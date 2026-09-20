#include "HeritageLayoutNumbers.h"
#include "LayoutService.h"
#include "HeritageImport.h"
#include "HeritageStyle.h"

#include <qgscategorizedsymbolrenderer.h>
#include <qgsexception.h>
#include <qgscallout.h>
#include <qgslabelpointsettings.h>
#include <qgslabelplacementsettings.h>
#include <qgslabelthinningsettings.h>
#include <qgslabelingresults.h>
#include <qgslinesymbol.h>
#include <qgscoordinatetransform.h>
#include <qgsexpression.h>
#include <qgsexpressioncontext.h>
#include <qgsfeatureiterator.h>
#include <qgsgeometry.h>
#include <qgslayout.h>
#include <qgslayoutitemlegend.h>
#include <qgslayoutitemmap.h>
#include <qgslayoutitempage.h>
#include <qgslayoutpagecollection.h>
#include <qgslayoutexporter.h>
#include <qgslayertree.h>
#include <qgslayertreemodel.h>
#include <qgslegendrenderer.h>
#include <qgsmaplayerlegend.h>
#include <qgsmaplayerstyle.h>
#include <qgsmarkersymbol.h>
#include <qgsmarkersymbollayer.h>
#include <qgspallabeling.h>
#include <qgsproject.h>
#include <qgsrendercontext.h>
#include <qgssinglesymbolrenderer.h>
#include <qgssymbollayer.h>
#include <qgstextbackgroundsettings.h>
#include <qgstextformat.h>
#include <qgsvectorlayer.h>
#include <qgsvectorlayerlabeling.h>

#include <QCryptographicHash>
#include <QDataStream>
#include <QTransform>
#include <memory>
#include <QScopedValueRollback>
#include <QSignalBlocker>
#include <QPainterPath>
#include <QSaveFile>
#include <QTemporaryDir>
#include <QTimer>

namespace {
std::optional<HeritageDataset> datasetFor(QgsVectorLayer* layer) {
  if (layer->project()) {
    for (QgsLayerTreeNode* node = layer->project()->layerTreeRoot()->findLayer(layer->id());
         node; node = node->parent()) {
      if (auto dataset = HeritageStyle::fromLayerName(node->name())) return dataset;
    }
  }
  return HeritageStyle::fromLayerName(layer->name());
}

QColor inkFor(const QColor& color) {
  return (color.redF() * .299 + color.greenF() * .587 + color.blueF() * .114) > .62
             ? QColor(Qt::black) : QColor(Qt::white);
}

double circleSize(int number) {
  return qMax(4.6, 2.0 + QString::number(number).size() * 1.45);
}

struct Site {
  QString name;
  QStringList ids;
  QVariantList values;
  int index = 0;
  QColor color;
  std::shared_ptr<QgsSymbol> symbol;
};

QPainterPath visibleMapOnPaper(QgsLayoutItemMap* map, bool exporting) {
  QPainterPath paper;
  auto* pages = map->layout()->pageCollection();
  for (int i = 0; i < pages->pageCount(); ++i) {
    if (exporting && !pages->shouldExportPage(i)) continue;
    QPainterPath page;
    page.addPolygon(pages->page(i)->mapToScene(pages->page(i)->rect()));
    paper = paper.united(page);
  }
  QPainterPath frame;
  frame.addPolygon(map->mapToScene(map->rect()));
  return paper.intersected(frame);
}

// Geographic footprint of the map that actually sits on the exported page.
// Scale changes this polygon; label collision does not.
QgsGeometry mapFootprintOnPaper(QgsLayoutItemMap* map, bool exporting) {
  const QgsGeometry extent = QgsGeometry::fromQPolygonF(map->visibleExtentPolygon());
  const QPainterPath onPaper = visibleMapOnPaper(map, exporting);
  if (onPaper.isEmpty()) return extent;
  const QgsGeometry clipped = QgsGeometry::fromQPolygonF(
      map->layoutToMapCoordsTransform().map(onPaper.toFillPolygon()));
  return clipped.isEmpty() ? extent : clipped;
}
}  // namespace

QString HeritageLayoutNumbers::entryKey(const QString& layerId, int number) {
  return layerId + QLatin1Char(':') + QString::number(number);
}

QSet<QString> HeritageLayoutNumbers::legendKeys() const {
  if (m_tracking) return m_visibleKeys;
  QSet<QString> keys;
  for (const auto& entry : m_entries) {
    if (entry.number > 0) keys.insert(entryKey(entry.layerId, entry.number));
  }
  return keys;
}

HeritageLayoutNumbers* HeritageLayoutNumbers::forMap(QgsLayoutItemMap* map) {
  return map ? qobject_cast<HeritageLayoutNumbers*>(map->property("ka_number_owner").value<QObject*>()) : nullptr;
}

QByteArray HeritageLayoutNumbers::renderSignature(QgsLayoutItemMap* map, bool includeStyles) const {
  QByteArray bytes;
  QDataStream stream(&bytes, QIODevice::WriteOnly);
  stream << m_revision << map->visibleExtentPolygon() << map->crs().toWkt()
         << map->scale() << map->rect() << map->sceneTransform() << map->layout()->renderContext().dpi();
  if (includeStyles) stream << map->layerStyleOverrides();
  for (auto* layer : map->layers())
    stream << layer->id() << m_layerRevisions.value(layer->id())
           << layer->customProperty(QStringLiteral("ka_hgis/layout_numbers_visible"), true).toBool();
  for (auto* page : map->layout()->pageCollection()->pages()) stream << page->rect() << page->sceneTransform();
  return QCryptographicHash::hash(bytes, QCryptographicHash::Sha256);
}

QSet<QString> HeritageLayoutNumbers::placedKeys(QgsLayoutItemMap* map, const QgsLabelingResults* results) const {
  QSet<QString> keys;
  if (!map || !map->layout() || !results || !map->isVisible()) return keys;
  const QPainterPath onPaper = visibleMapOnPaper(map, m_exporting);
  bool invertible = false;
  const QTransform toPaper = map->layoutToMapCoordsTransform().inverted(&invertible);
  if (!invertible) return keys;
  QHash<QString, QHash<qint64, int>> featureNumbers;
  for (const auto& entry : m_entries)
    for (qint64 id : entry.featureIds) featureNumbers[entry.layerId].insert(id, entry.number);
  for (const auto& label : results->allLabels()) {
    if (label.isUnplaced || label.isDiagram) continue;
    const auto source = featureNumbers.constFind(label.layerID);
    if (source == featureNumbers.cend()) continue;
    const int number = source->value(label.featureId, 0);
    if (!number || label.labelText != QString::number(number)) continue;
    QPolygonF polygon;
    for (const auto& point : label.cornerPoints)
      polygon.append(toPaper.map(QPointF(point.x(), point.y())));
    QPainterPath text;
    text.addPolygon(polygon);
    if (!text.isEmpty() && onPaper.intersects(text)) keys.insert(entryKey(label.layerID, number));
  }
  return keys;
}

bool HeritageLayoutNumbers::acceptRenderedLabels(QgsLayoutItemMap* map, const QgsLabelingResults* results) {
  const auto keys = placedKeys(map, results);
  const bool renumbered = m_tracking && compactRenderedNumbers(map, results, keys);
  if (!renumbered && m_visibleKeys == keys && !m_legendPending) return false;
  if (!renumbered) m_visibleKeys = keys;
  ++m_placementRevision;
  m_legendPending = renumbered;
  // Preview must not keep the first-pass numbers (24, 30, …) after hidden
  // siblings are dropped. Update the legend as soon as the series is known.
  if (!m_exporting) emit visibleEntriesChanged();
  return true;
}

void HeritageLayoutNumbers::restoreCandidates(QgsLayoutItemMap* map) {
  if (!m_compacted) return;
  m_entries = m_candidateEntries;
  m_overrides = m_candidateOverrides;
  m_visibleKeys.clear();
  m_compacted = false;
  m_legendPending = true;
  ++m_placementRevision;
  const QScopedValueRollback<bool> applying(m_applying, true);
  map->setLayerStyleOverrides(m_overrides);
  map->invalidateCache();
}

bool HeritageLayoutNumbers::compactRenderedNumbers(QgsLayoutItemMap* map, const QgsLabelingResults* results,
                                                    const QSet<QString>& keys) {
  if (!map || !results) return false;
  QVector<Entry> numbered = m_entries;
  QMap<QString, int> counters;
  bool needsRewrite = false;
  for (auto& entry : numbered) {
    const bool visible = entry.number > 0 && keys.contains(entryKey(entry.layerId, entry.number));
    const int next = visible ? ++counters[entry.dataset] : 0;
    if (entry.number != next) needsRewrite = true;
    entry.number = next;
  }
  if (!needsRewrite || keys.isEmpty()) return false;

  // Keep the already collision-tested placement. Hidden siblings stay hidden
  // so map numbers and legend numbers stay the same consecutive series.
  QHash<QString, QHash<qint64, int>> indices;
  for (int i = 0; i < m_entries.size(); ++i)
    for (auto id : m_entries[i].featureIds) indices[m_entries[i].layerId].insert(id, i);
  QHash<QString, QMap<qint64, QPointF>> anchors;
  const QPainterPath onPaper = visibleMapOnPaper(map, m_exporting);
  const QTransform toPaper = map->layoutToMapCoordsTransform().inverted();
  for (const auto& label : results->allLabels()) {
    if (label.isUnplaced || label.isDiagram || label.cornerPoints.size() < 2) continue;
    const auto layer = indices.constFind(label.layerID);
    if (layer == indices.cend()) continue;
    const int index = layer->value(label.featureId, -1);
    if (index < 0 || !numbered[index].number || label.labelText != QString::number(m_entries[index].number)) continue;
    QPolygonF polygon;
    for (const auto& corner : label.cornerPoints) polygon.append(toPaper.map(QPointF(corner.x(), corner.y())));
    QPainterPath text;
    text.addPolygon(polygon);
    if (!onPaper.intersects(text)) continue;
    const auto& a = label.cornerPoints[0];
    const auto& b = label.cornerPoints[1];
    anchors[label.layerID].insert(label.featureId, QPointF((a.x() + b.x()) / 2., (a.y() + b.y()) / 2.));
  }
  auto overrides = m_overrides;
  try {
    for (auto it = indices.cbegin(); it != indices.cend(); ++it) {
      const auto drawing = m_drawingSources.value(it.key()).layer;
      if (!drawing || !drawing->labeling()) continue;
      QgsPalLayerSettings labels = drawing->labeling()->settings();
      QString numberCase = QStringLiteral("CASE");
      QString xCase = QStringLiteral("CASE");
      QString yCase = QStringLiteral("CASE");
      QString sizeCase = QStringLiteral("CASE");
      const auto positions = anchors.value(it.key());
      const QgsCoordinateTransform toSource(map->crs(), drawing->crs(), map->layout()->project()->transformContext());
      for (auto pos = positions.cbegin(); pos != positions.cend(); ++pos) {
        const int number = numbered[it->value(pos.key())].number;
        const QgsPointXY source = toSource.transform(QgsPointXY(pos.value()));
        const QString condition = QStringLiteral(" WHEN $id = %1 THEN ").arg(pos.key());
        numberCase += condition + QString::number(number);
        xCase += condition + QString::number(source.x(), 'g', 17);
        yCase += condition + QString::number(source.y(), 'g', 17);
        sizeCase += condition + QString::number(circleSize(number));
      }
      labels.fieldName = positions.isEmpty() ? QStringLiteral("NULL") : numberCase + QStringLiteral(" ELSE NULL END");
      if (!positions.isEmpty()) {
        auto& properties = labels.dataDefinedProperties();
        properties.setProperty(QgsPalLayerSettings::Property::PositionX, QgsProperty::fromExpression(xCase + QStringLiteral(" END")));
        properties.setProperty(QgsPalLayerSettings::Property::PositionY, QgsProperty::fromExpression(yCase + QStringLiteral(" END")));
        properties.setProperty(QgsPalLayerSettings::Property::Hali, QgsProperty::fromValue(QStringLiteral("Center")));
        properties.setProperty(QgsPalLayerSettings::Property::Vali, QgsProperty::fromValue(QStringLiteral("Bottom")));
        properties.setProperty(QgsPalLayerSettings::Property::ShapeSizeX, QgsProperty::fromExpression(sizeCase + QStringLiteral(" END")));
        properties.setProperty(QgsPalLayerSettings::Property::ShapeSizeY, QgsProperty::fromExpression(sizeCase + QStringLiteral(" END")));
      }
      drawing->setLabeling(new QgsVectorLayerSimpleLabeling(labels));
      QgsMapLayerStyle style;
      style.readFromLayer(drawing.get());
      overrides.insert(it.key(), style.xmlData());
    }
  } catch (const QgsCsException& exception) {
    m_error = QStringLiteral("연번 위치를 변환하지 못했습니다: %1").arg(exception.what());
    return false;
  }
  m_entries = std::move(numbered);
  m_overrides = std::move(overrides);
  m_visibleKeys.clear();
  for (const auto& entry : m_entries)
    if (entry.number > 0) m_visibleKeys.insert(entryKey(entry.layerId, entry.number));
  m_compacted = true;
  m_pinnedSignature = renderSignature(map, false);
  const QScopedValueRollback<bool> applying(m_applying, true);
  map->setLayerStyleOverrides(m_overrides);
  map->invalidateCache();
  map->update();
  return true;
}

void HeritageLayoutNumbers::followRenderedLabels(QgsLayoutItemMap* map) {
  if (m_tracking && m_followedMap == map) return;
  if (m_followedMap && forMap(m_followedMap) == this)
    m_followedMap->setProperty("ka_number_owner", QVariant());
  for (const auto& connection : m_renderConnections) disconnect(connection);
  m_renderConnections.clear();
  m_followedMap = map;
  m_tracking = true;
  m_visibleKeys.clear();
  ++m_placementRevision;
  m_previewBusy = false;
  m_previewDirty = false;
  if (!map) return;
  map->setProperty("ka_number_owner", QVariant::fromValue<QObject*>(this));
  m_renderConnections.append(connect(this, &QObject::destroyed, map, [map, owner = static_cast<QObject*>(this)]() {
    if (map->property("ka_number_owner").value<QObject*>() == owner)
      map->setProperty("ka_number_owner", QVariant());
  }));
  auto* timer = new QTimer(map);
  timer->setSingleShot(true);
  timer->setInterval(120);
  m_renderConnections.append(connect(map, &QgsLayoutItem::backgroundTaskCountChanged, this, [this, map](int count) {
    if (count <= 0) return;
    const bool interacting = map->property("ka_interacting").toBool();
    if (!interacting && m_compacted && m_pinnedSignature != renderSignature(map, false)) restoreCandidates(map);
    m_previewBusy = true;
    m_previewDirty = m_exporting || interacting;
    m_previewSignature = renderSignature(map);
  }));
  auto changed = [this]() { if (m_previewBusy) m_previewDirty = true; };
  m_renderConnections.append(connect(map, &QgsLayoutItemMap::extentChanged, this, changed));
  m_renderConnections.append(connect(map, &QgsLayoutItemMap::mapRotationChanged, this, changed));
  m_renderConnections.append(connect(map, &QgsLayoutItemMap::crsChanged, this, changed));
  m_renderConnections.append(connect(map, &QgsLayoutItemMap::layerStyleOverridesChanged, this, changed));
  // Moving the map or changing paper can clip placed numbers without changing
  // the geographic extent. Coalesce those changes; legend movement is unrelated.
  m_renderConnections.append(connect(timer, &QTimer::timeout, this, [this, map, timer]() {
    if (m_followedMap != map) return;
    if (map->property("ka_interacting").toBool() || m_exporting) { timer->start(); return; }
    map->invalidateCache();
    map->update();
  }));
  auto paperChanged = [changed, timer]() { changed(); timer->start(); };
  m_renderConnections.append(connect(map, &QgsLayoutItem::sizePositionChanged, this, paperChanged));
  m_renderConnections.append(connect(map->layout()->pageCollection(), &QgsLayoutPageCollection::changed, this, paperChanged));
  m_renderConnections.append(connect(map, &QgsLayoutItemMap::previewRefreshed, this, [this, map, timer]() {
    const bool current = m_previewBusy && !m_previewDirty && !m_exporting
        && m_previewSignature == renderSignature(map);
    m_previewBusy = false;
    if (m_exporting) return;
    if (map->property("ka_interacting").toBool()) { timer->start(); return; }
    if (current) {
      acceptRenderedLabels(map, map->previewLabelingResults());
    } else {
      // QGIS can replace an active job without a second start signal. Its old
      // results have no render ID; discard them and request one known fresh job.
      map->invalidateCache();
      map->update();
    }
  }));
  map->invalidateCache();
  map->update();
}

bool HeritageLayoutNumbers::exportPdf(QgsLayoutItemMap* map, QgsLayoutItemLegend* legend,
                                     const QString& path, double dpi, QString* error, bool forceVectorOutput) {
  auto fail = [error](const QString& message) { if (error) *error = message; return false; };
  if (!map || !map->layout()) return fail(QStringLiteral("출력할 도면이 없습니다."));
  followRenderedLabels(map);
  const QScopedValueRollback<bool> exporting(m_exporting, true);
  // Print-DPI placement must start from the full footprint candidates, not
  // from the subset pinned by a screen preview at a different resolution.
  restoreCandidates(map);
  auto* layout = map->layout();
  const double previousDpi = layout->renderContext().dpi();
  struct RestoreDpi {
    QgsLayout* layout;
    QgsLayoutItemMap* map;
    double dpi;
    ~RestoreDpi() {
      layout->renderContext().setDpi(dpi);
      map->invalidateCache();
      map->update();
    }
  } restore{layout, map, previousDpi};
  layout->renderContext().setDpi(dpi);
  QTemporaryDir temporary;
  if (!temporary.isValid()) return fail(QStringLiteral("PDF 임시 폴더를 만들지 못했습니다."));
  const QString draft = temporary.filePath(QStringLiteral("drawing.pdf"));
  QByteArray signature = renderSignature(map);
  auto syncLegend = [&]() {
    if (!legend) return;
    LayoutService::tuneSheetLegend(legend);
    applyLegend(legend);
    m_legendPending = false;
  };
  syncLegend();
  // The exporter owns actual print-DPI PAL results only after a PDF pass.
  // Normally the second pass verifies pinned consecutive numbers. One bounded
  // retry also handles a placement rejected by QGIS after fixing its anchor.
  for (int pass = 0; pass < 3; ++pass) {
    LayoutService::settleSheetLegendsForExport(layout);
    QgsLayoutExporter exporter(layout);
    QgsLayoutExporter::PdfExportSettings settings;
    settings.dpi = dpi;
    settings.forceVectorOutput = forceVectorOutput;
    if (exporter.exportToPdf(draft, settings) != QgsLayoutExporter::Success)
      return fail(QStringLiteral("PDF 임시 출력에 실패했습니다."));
    if (signature != renderSignature(map))
      return fail(QStringLiteral("출력 중 도면이 변경되었습니다. 다시 내보내세요."));
    const auto* results = exporter.labelingResults().value(map->uuid());
    if (!results && !m_entries.isEmpty())
      return fail(QStringLiteral("PDF 번호 배치 결과를 확인하지 못했습니다."));
    const auto printedKeys = placedKeys(map, results);
    const auto shownKeys = m_visibleKeys;
    acceptRenderedLabels(map, results);
    if (!m_error.isEmpty()) return fail(m_error);
    if (printedKeys == shownKeys && signature == renderSignature(map)) {
      QFile source(draft);
      QSaveFile destination(path);
      if (!source.open(QIODevice::ReadOnly) || !destination.open(QIODevice::WriteOnly))
        return fail(QStringLiteral("PDF 저장 파일을 열지 못했습니다."));
      while (!source.atEnd()) {
        const QByteArray bytes = source.read(1024 * 1024);
        if (source.error() != QFileDevice::NoError || destination.write(bytes) != bytes.size())
          return fail(QStringLiteral("PDF 파일을 저장하지 못했습니다."));
      }
      if (!destination.commit()) return fail(QStringLiteral("PDF 파일을 확정하지 못했습니다."));
      return true;
    }
    syncLegend();
    signature = renderSignature(map);
  }
  return fail(QStringLiteral("PDF 번호와 범례가 안정적으로 일치하지 않아 저장하지 않았습니다. 다시 내보내세요."));
}

bool HeritageLayoutNumbers::update(QgsLayoutItemMap* map, bool force) {
  if (!map || !map->layout() || !map->layout()->project()) return false;
  QgsProject* project = map->layout()->project();
  const auto layers = map->layers();
  QByteArray signature;
  QDataStream stream(&signature, QIODevice::WriteOnly);
  stream << map->uuid() << map->visibleExtentPolygon() << map->crs().toWkt() << map->scale();
  // Do not serialize thousands of symbols merely to discover a cache hit.
  // Track source changes with lifetime-bound connections, including in-place
  // renderer check state changes (repaintRequested) and uncommitted edits.
  for (auto* layer : layers) {
    const QString id = layer->id();
    if (!m_layerRevisions.contains(id)) {
      m_layerRevisions.insert(id, 0);
      auto invalidate = [this, id]() {
        if (!m_applying) ++m_layerRevisions[id];
      };
      connect(layer, &QgsMapLayer::styleChanged, this, invalidate);
      connect(layer, &QgsMapLayer::dataChanged, this, invalidate);
      connect(layer, &QgsMapLayer::dataSourceChanged, this, invalidate);
      connect(layer, &QgsMapLayer::crsChanged, this, invalidate);
      connect(layer, &QgsMapLayer::nameChanged, this, invalidate);
      connect(layer, &QgsMapLayer::repaintRequested, this, invalidate);
      connect(layer, &QObject::destroyed, this, [this, id]() {
        m_layerRevisions.remove(id);
        m_drawingSources.remove(id);
        m_signature.clear();
      });
      if (auto* vector = qobject_cast<QgsVectorLayer*>(layer)) {
        connect(vector, &QgsVectorLayer::updatedFields, this, invalidate);
        connect(vector, &QgsVectorLayer::featureAdded, this, invalidate);
        connect(vector, &QgsVectorLayer::featureDeleted, this, invalidate);
        connect(vector, &QgsVectorLayer::geometryChanged, this, invalidate);
        connect(vector, &QgsVectorLayer::attributeValueChanged, this, invalidate);
        connect(vector, &QgsVectorLayer::afterRollBack, this, invalidate);
      }
    }
    stream << id << m_layerRevisions.value(id) << layer->name()
           << layer->customProperty(QStringLiteral("ka_hgis/layout_numbers_visible"), true).toBool();
  }
  // Number assignment follows project tree order, not paint order alone.
  for (auto* node : project->layerTreeRoot()->findLayers()) {
    if (!layers.contains(node->layer())) continue;
    stream << node->layerId();
    for (auto* ancestor = node->parent(); ancestor; ancestor = ancestor->parent())
      stream << ancestor->name();
  }
  const QString digest = QString::fromLatin1(QCryptographicHash::hash(signature, QCryptographicHash::Sha256).toHex());
  if (!force && digest == m_signature) return m_error.isEmpty();
  m_error.clear();
  QVector<Entry> entries;
  QMap<QString, QString> overrides = map->layerStyleOverrides();
  // Remove our previous styles when a heritage layer is hidden or removed.
  for (auto it = m_drawingSources.cbegin(); it != m_drawingSources.cend(); ++it)
    overrides.remove(it.key());
  for (auto* layer : layers) {
    if (auto* vector = qobject_cast<QgsVectorLayer*>(layer); vector && datasetFor(vector))
      overrides.remove(layer->id());
  }
  QMap<QString, int> counters;
  // Project tree order is stable and agrees with the user's layer panel.
  for (QgsLayerTreeLayer* treeLayer : project->layerTreeRoot()->findLayers()) {
    auto* layer = qobject_cast<QgsVectorLayer*>(treeLayer->layer());
    if (!layer || !layers.contains(layer) || !layer->renderer()) continue;
    const auto dataset = datasetFor(layer);
    if (!dataset) continue;
    const QString datasetName = HeritageStyle::layerName(*dataset);
    auto& source = m_drawingSources[layer->id()];
    const auto sourceRevision = m_layerRevisions.value(layer->id());
    if (force || !source.layer || source.revision != sourceRevision) {
      source.layer.reset(layer->clone());
      source.revision = sourceRevision;
    } else {
      // Reuse the provider/style shell while restoring unpruned source symbols.
      source.layer->setRenderer(layer->renderer()->clone());
    }
    const auto& drawing = source.layer;
    if (!layer->customProperty(QStringLiteral("ka_hgis/layout_numbers_visible"), true).toBool()) {
      // Omitting the numeric override would expose the source's name labels.
      // Keep its geometry renderer intact and disable labels only in this
      // layout clone, without scanning features or changing the main map.
      drawing->setLabelsEnabled(false);
      QgsMapLayerStyle style;
      style.readFromLayer(drawing.get());
      overrides.insert(layer->id(), style.xmlData());
      continue;
    }
    auto* renderer = drawing->renderer();
    auto* categorized = dynamic_cast<QgsCategorizedSymbolRenderer*>(renderer);
    const bool single = dynamic_cast<QgsSingleSymbolRenderer*>(renderer) != nullptr;
    if (!categorized && !single) {
      m_error = QStringLiteral("'%1'의 조판 번호는 단일 심볼 또는 분류 스타일에서 사용할 수 있습니다.").arg(layer->name());
      return false;
    }
    QgsGeometry footprint = mapFootprintOnPaper(map, m_exporting);
    try {
      footprint.transform(QgsCoordinateTransform(map->crs(), layer->crs(), project->transformContext()));
    } catch (const QgsCsException& e) {
      m_error = QStringLiteral("'%1'의 도면 범위를 변환하지 못했습니다: %2").arg(layer->name(), e.what());
      return false;
    }
    QgsRenderContext context = QgsRenderContext::fromMapSettings(
        map->mapSettings(map->extent(), QSizeF(1000, 1000), 96, false));
    context.setRendererScale(map->scale());
    context.setExpressionContext(drawing->createExpressionContext());
    // Old projects retain their original palette. Upgrade simple default
    // strokes/fills in this disposable layout clone only; custom and
    // data-defined colors retain their existing meaning.
    for (QgsSymbol* symbol : renderer->symbols(context)) {
      if (!symbol || symbol->hasDataDefinedProperties()) continue;
      for (int i = 0; i < symbol->symbolLayerCount(); ++i) {
        auto* part = symbol->symbolLayer(i);
        if (!part || (part->layerType() != QLatin1String("SimpleFill") &&
                      part->layerType() != QLatin1String("SimpleLine") &&
                      part->layerType() != QLatin1String("SimpleMarker"))) continue;
        const QColor color = part->color();
        const QColor stroke = part->strokeColor();
        const QColor mappedColor = HeritageStyle::layoutColor(*dataset, color);
        const QColor mappedStroke = HeritageStyle::layoutColor(*dataset, stroke);
        if (mappedColor != color) part->setColor(mappedColor);
        if (mappedStroke != stroke) part->setStrokeColor(mappedStroke);
      }
    }
    QMap<QString, Site> sites;
    QMap<QString, int> legendIndices;
    const auto symbols = renderer->legendSymbolItems();
    for (int i = 0; i < symbols.size(); ++i) legendIndices.insert(symbols[i].ruleKey(), i);
    const QString nameField = HeritageImport::chooseNameField(layer);
    renderer->startRender(context, drawing->fields());
    if (layer->isInScaleRange(map->scale())) {
      QgsFeatureRequest request;
      request.setFilterRect(footprint.boundingBox());
      auto features = layer->getFeatures(request);
      QgsFeature feature;
      while (features.nextFeature(feature)) {
        if (!feature.hasGeometry() || !footprint.intersects(feature.geometry())) continue;
        context.expressionContext().setFeature(feature);
        QgsSymbol* symbol = renderer->symbolForFeature(feature, context);
        if (!symbol) continue;
        QString key;
        QString name;
        int index = 0;
        if (categorized) {
          const auto keys = renderer->legendKeysForFeature(feature, context);
          if (keys.isEmpty() || !legendIndices.contains(*keys.begin())) continue;
          index = legendIndices.value(*keys.begin());
          key = QString::number(index).rightJustified(10, QLatin1Char('0'));
          name = categorized->categories().at(index).label();
        } else {
          name = nameField.isEmpty() ? QString() : feature.attribute(nameField).toString().trimmed();
          if (name.isEmpty()) name = HeritageStyle::unnamedLabel();
          key = name;
        }
        auto& site = sites[key];
        if (!site.symbol) {
          site.name = name;
          site.index = index;
          site.color = symbol->color().isValid() ? symbol->color() : HeritageStyle::color(*dataset);
          site.symbol.reset(symbol->clone());
        }
        site.ids.append(QString::number(feature.id()));
        site.values.append(QVariant::fromValue(feature.id()));
      }
    }
    renderer->stopRender(context);
    if (categorized) {
      // Keep only categories actually visible on this sheet in the clone.
      // Serializing/reloading thousands of off-sheet symbols for each scale
      // change dominated both map overrides and legend construction.
      QgsCategoryList visibleCategories;
      for (auto it = sites.begin(); it != sites.end(); ++it) {
        visibleCategories.append(categorized->categories().at(it->index));
        it->index = visibleCategories.size() - 1;
      }
      categorized->deleteAllCategories();
      for (const auto& category : visibleCategories) categorized->addCategory(category);
    }
    QString numberExpression = QStringLiteral("CASE");
    QString fillExpression = QStringLiteral("CASE");
    QString inkExpression = QStringLiteral("CASE");
    QString sizeExpression = QStringLiteral("CASE");
    QgsCategoryList fallbackCategories;
    for (auto it = sites.cbegin(); it != sites.cend(); ++it) {
      const Site& site = it.value();
      const int number = ++counters[datasetName];
      const int index = single ? fallbackCategories.size() : site.index;
      QSet<qint64> featureIds;
      for (const auto& id : site.ids) featureIds.insert(id.toLongLong());
      entries.append({layer->id(), datasetName, site.name, number, index, site.color, featureIds});
      const QString condition = QStringLiteral(" WHEN $id IN (%1) THEN ").arg(site.ids.join(QLatin1Char(',')));
      numberExpression += condition + QString::number(number);
      fillExpression += condition + QgsExpression::quotedString(site.color.name(QColor::HexArgb));
      inkExpression += condition + QgsExpression::quotedString(inkFor(site.color).name());
      sizeExpression += condition + QString::number(circleSize(number));
      if (single) fallbackCategories.append(QgsRendererCategory(site.values, site.symbol->clone(), site.name));
    }
    if (single && !sites.isEmpty()) {
      // The drawing clone alone expands a single symbol into visible site rows.
      fallbackCategories.append(QgsRendererCategory(QVariant(), renderer->symbols(context).first()->clone(), HeritageStyle::unnamedLabel()));
      drawing->setRenderer(new QgsCategorizedSymbolRenderer(QStringLiteral("$id"), fallbackCategories));
    }
    QgsPalLayerSettings labels;
    labels.isExpression = true;
    labels.fieldName = sites.isEmpty() ? QStringLiteral("NULL") : numberExpression + QStringLiteral(" ELSE NULL END");
    labels.geometryGeneratorEnabled = true;
    labels.geometryGenerator = QStringLiteral("point_on_surface($geometry)");
    labels.geometryGeneratorType = Qgis::GeometryType::Point;
    labels.placement = Qgis::LabelPlacement::OrderedPositionsAroundPoint;
    labels.centroidInside = true;
    labels.placementSettings().setOverlapHandling(Qgis::LabelOverlapHandling::PreventOverlap);
    labels.placementSettings().setAllowDegradedPlacement(true);
    // Prefer a small offset from the site. Farther rings are allowed so two
    // badges can sit side by side instead of stacking on the same point.
    labels.dist = 1.8;
    labels.distUnits = Qgis::RenderUnit::Millimeters;
    labels.pointSettings().setMaximumDistance(28.);
    labels.pointSettings().setMaximumDistanceUnit(Qgis::RenderUnit::Millimeters);
    labels.pointSettings().setPredefinedPositionOrder({
        Qgis::LabelPredefinedPointPosition::TopRight,
        Qgis::LabelPredefinedPointPosition::TopLeft,
        Qgis::LabelPredefinedPointPosition::BottomRight,
        Qgis::LabelPredefinedPointPosition::BottomLeft,
        Qgis::LabelPredefinedPointPosition::MiddleRight,
        Qgis::LabelPredefinedPointPosition::MiddleLeft,
        Qgis::LabelPredefinedPointPosition::TopMiddle,
        Qgis::LabelPredefinedPointPosition::BottomMiddle,
        Qgis::LabelPredefinedPointPosition::TopSlightlyRight,
        Qgis::LabelPredefinedPointPosition::TopSlightlyLeft,
        Qgis::LabelPredefinedPointPosition::BottomSlightlyRight,
        Qgis::LabelPredefinedPointPosition::BottomSlightlyLeft,
        Qgis::LabelPredefinedPointPosition::OverPoint,
    });
    // PAL collides the text box, not the colored disc. Keep the full badge clear.
    labels.thinningSettings().setLimitNumberLabelsEnabled(false);
    labels.thinningSettings().setMinimumFeatureSize(0);
    labels.thinningSettings().setLabelMarginDistance(circleSize(counters.value(datasetName)) + .35);
    labels.thinningSettings().setLabelMarginDistanceUnit(Qgis::RenderUnit::Millimeters);
    auto* callout = new QgsSimpleLineCallout;
    callout->setEnabled(true);
    callout->setOffsetFromLabel(2.);
    callout->setOffsetFromLabelUnit(Qgis::RenderUnit::Millimeters);
    callout->setLineSymbol(QgsLineSymbol::createSimple({
        {QStringLiteral("line_color"), QStringLiteral("#777777")},
        {QStringLiteral("line_width"), QStringLiteral("0.15")}}).release());
    labels.setCallout(callout);
    QgsTextFormat format;
    format.setFont(QFont(QStringLiteral("Malgun Gothic"), 8, QFont::Bold));
    format.setSize(8);
    format.setSizeUnit(Qgis::RenderUnit::Points);
    const QColor firstColor = sites.isEmpty() ? HeritageStyle::color(*dataset) : sites.first().color;
    format.setColor(inkFor(firstColor));
    QgsTextBackgroundSettings background;
    background.setEnabled(true);
    background.setType(QgsTextBackgroundSettings::ShapeCircle);
    background.setSizeType(QgsTextBackgroundSettings::SizeFixed);
    background.setSize(QSizeF(4.6, 4.6));
    background.setSizeUnit(Qgis::RenderUnit::Millimeters);
    background.setFillColor(firstColor);
    background.setStrokeColor(firstColor);
    background.setStrokeWidth(0);
    format.setBackground(background);
    labels.setFormat(format);
    if (!sites.isEmpty()) {
      auto& properties = labels.dataDefinedProperties();
      properties.setProperty(QgsPalLayerSettings::Property::ShapeFillColor, QgsProperty::fromExpression(fillExpression + QStringLiteral(" END")));
      properties.setProperty(QgsPalLayerSettings::Property::ShapeStrokeColor, QgsProperty::fromExpression(fillExpression + QStringLiteral(" END")));
      properties.setProperty(QgsPalLayerSettings::Property::Color, QgsProperty::fromExpression(inkExpression + QStringLiteral(" END")));
      properties.setProperty(QgsPalLayerSettings::Property::ShapeSizeX, QgsProperty::fromExpression(sizeExpression + QStringLiteral(" END")));
      properties.setProperty(QgsPalLayerSettings::Property::ShapeSizeY, QgsProperty::fromExpression(sizeExpression + QStringLiteral(" END")));
    }
    drawing->setLabeling(new QgsVectorLayerSimpleLabeling(labels));
    drawing->setLabelsEnabled(true);
    QgsMapLayerStyle style;
    style.readFromLayer(drawing.get());
    overrides.insert(layer->id(), style.xmlData());
  }
  m_entries = std::move(entries);
  m_overrides = std::move(overrides);
  m_candidateEntries = m_entries;
  m_candidateOverrides = m_overrides;
  m_compacted = false;
  m_legendPending = false;
  m_signature = digest;
  ++m_revision;
  if (m_tracking) {
    m_visibleKeys.clear();
    ++m_placementRevision;
    if (m_previewBusy) m_previewDirty = true;
  }
  const QScopedValueRollback<bool> applying(m_applying, true);
  map->setKeepLayerStyles(true);
  map->setLayerStyleOverrides(m_overrides);
  map->invalidateCache();
  return true;
}

void HeritageLayoutNumbers::applyLegend(QgsLayoutItemLegend* legend) const {
  if (!legend || !legend->model() || !legend->model()->rootGroup()) return;
  auto* model = legend->model();
  const QString stamp = QString::number(reinterpret_cast<quintptr>(this)) + QLatin1Char(':') + QString::number(m_revision)
      + QLatin1Char(':') + QString::number(m_placementRevision);
  const QString key = QStringLiteral("ka_hgis/layout_numbers_revision");
  bool current = legend->customProperty(key).toString() == stamp && model->layerStyleOverrides() == m_overrides;
  bool needsMapFilter = m_entries.isEmpty();
  for (auto* node : model->rootGroup()->findLayers()) {
    if (auto* layer = qobject_cast<QgsVectorLayer*>(node->layer())) {
      if (datasetFor(layer)) current = current && node->customProperty(key).toString() == stamp;
      else needsMapFilter = true;
    }
  }
  // Heritage rows are already limited to the on-paper map. Other vectors
  // (e.g. geology/soil) still need QGIS symbol hit testing.
  legend->setLegendFilterByMapEnabled(needsMapFilter);
  if (current) return;
  const QScopedValueRollback<bool> applying(m_applying, true);
  // Numbered rows already passed the paper-footprint intersection test.
  if (!m_entries.isEmpty()) {
    // This service applies the complete override snapshot below. Letting the
    // linked map also reload it rebuilds all rows once before our own update.
    if (auto* linked = legend->linkedMap())
      QObject::disconnect(linked, &QgsLayoutItemMap::layerStyleOverridesChanged, legend, nullptr);
  }
  if (model->layerStyleOverrides() != m_overrides) model->setLayerStyleOverrides(m_overrides);
  for (auto* node : model->rootGroup()->findLayers()) {
    auto* layer = qobject_cast<QgsVectorLayer*>(node->layer());
    if (!layer || !datasetFor(layer)) continue;
    // Batch badge properties; otherwise every property refreshes the legend.
    QSignalBlocker nodeSignals(node);
    QList<int> order;
    for (const Entry& entry : m_entries) {
      if (entry.layerId != layer->id() || entry.number <= 0) continue;
      if (m_tracking && !m_visibleKeys.contains(entryKey(entry.layerId, entry.number))) continue;
      order.append(entry.legendIndex);
      auto marker = QgsMarkerSymbol::createSimple({
          {QStringLiteral("name"), QStringLiteral("circle")},
          {QStringLiteral("color"), entry.color.name(QColor::HexArgb)},
          {QStringLiteral("outline_style"), QStringLiteral("no")},
          {QStringLiteral("size"), QString::number(circleSize(entry.number))}});
      auto* number = new QgsFontMarkerSymbolLayer(QStringLiteral("Malgun Gothic"),
          QString::number(entry.number), 2.8, inkFor(entry.color));
      number->setFontStyle(QStringLiteral("Bold"));
      number->setSizeUnit(Qgis::RenderUnit::Millimeters);
      marker->appendSymbolLayer(number);
      QgsMapLayerLegendUtils::setLegendNodeCustomSymbol(node, entry.legendIndex, marker.get());
      QgsMapLayerLegendUtils::setLegendNodeUserLabel(node, entry.legendIndex, entry.name);
      QgsMapLayerLegendUtils::setLegendNodeSymbolSize(node, entry.legendIndex, QSizeF(circleSize(entry.number), circleSize(entry.number)));
    }
    if (order.isEmpty()) {
      nodeSignals.unblock();
      if (auto* parent = qobject_cast<QgsLayerTreeGroup*>(node->parent())) parent->removeChildNode(node);
      continue;
    }
    QgsMapLayerLegendUtils::setLegendNodeOrder(node, order);
    QgsLegendRenderer::setNodeLegendStyle(node, Qgis::LegendComponent::Hidden);
    model->refreshLayerLegend(node);
    node->setCustomProperty(key, stamp);
  }
  // The native override-change connection is suppressed above to avoid two
  // rebuilds. Refresh its filter explicitly after the complete model snapshot.
  if (needsMapFilter) legend->updateFilterByMap(false);
  LayoutService::flowSheetLegend(legend);
  legend->update();
  legend->setCustomProperty(key, stamp);
}
