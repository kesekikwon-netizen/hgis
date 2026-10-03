#include <QtTest>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QFontDatabase>
#include <QPointer>
#include <QPainterPath>
#include <QSet>
#include <QTimer>
#include <QTemporaryDir>
#include <QDomDocument>
#include <algorithm>
#include <qgsapplication.h>
#include <qgscallout.h>
#include <qgscategorizedsymbolrenderer.h>
#include <qgscoordinatetransform.h>
#include <qgsexpression.h>
#include <qgsexpressioncontextutils.h>
#include <qgsfeature.h>
#include <qgsfillsymbollayer.h>
#include <qgsgeometry.h>
#include <qgsgeometrygeneratorsymbollayer.h>
#include <qgslabelingresults.h>
#include <qgslayertree.h>
#include <qgslayertreelayer.h>
#include <qgslayertreemodellegendnode.h>
#include <qgslayoutitemlegend.h>
#include <qgslayoutitemmap.h>
#include <qgslayoutitempage.h>
#include <qgslayoutmanager.h>
#include <qgslayoutexporter.h>
#include <qgslayoutpagecollection.h>
#include <qgsmaplayerstyle.h>
#include <qgsmarkersymbol.h>
#include <qgsmarkersymbollayer.h>
#include <qgspallabeling.h>
#include <qgslabelplacementsettings.h>
#include <qgsprintlayout.h>
#include <qgsproject.h>
#include <qgsrendercontext.h>
#include <qgssinglesymbolrenderer.h>
#include <qgssymbol.h>
#include <qgstextbackgroundsettings.h>
#include <qgsvectorlayer.h>
#include <qgsvectorlayerlabeling.h>

#include "core/HeritageSiteLegend.h"
#include "core/HeritageImport.h"
#include "core/HeritageLayoutNumbers.h"
#include "core/HeritageStyle.h"
#include "core/LayerOps.h"
#include "core/LayoutService.h"

// 계획서 2026-09-11-heritage-intranet-nearby-sites.md 7.4 · 7.5 를 붙잡는다.
class HeritageStyleTest : public QObject {
  Q_OBJECT

  static QgsVectorLayer* makeLayer(const QStringList& names, bool addEmpty = false) {
    auto* layer = new QgsVectorLayer(
        QStringLiteral("Polygon?crs=EPSG:5186&field=nm:string(80)"),
        QStringLiteral("매장유산유존지역"), QStringLiteral("memory"));
    if (!layer->isValid()) return layer;
    layer->startEditing();
    double x = 190000.;
    auto addOne = [&](const QVariant& value) {
      QgsFeature f(layer->fields());
      QgsPolylineXY ring;
      ring << QgsPointXY(x, 550000.) << QgsPointXY(x + 100., 550000.)
           << QgsPointXY(x + 100., 550100.) << QgsPointXY(x, 550100.)
           << QgsPointXY(x, 550000.);
      f.setGeometry(QgsGeometry::fromPolygonXY(QgsPolygonXY() << ring));
      f.setAttribute(QStringLiteral("nm"), value);
      layer->addFeature(f);
      x += 200.;
    };
    for (const QString& n : names) addOne(n);
    if (addEmpty) addOne(QVariant(QString()));
    layer->commitChanges();
    return layer;
  }

  static QStringList nodeLabels(const QList<QgsLayerTreeModelLegendNode*>& nodes) {
    QStringList out;
    for (QgsLayerTreeModelLegendNode* n : nodes) {
      if (n) out << n->data(Qt::DisplayRole).toString();
    }
    return out;
  }

  static QgsVectorLayer* addHeritage(QgsProject& project, HeritageDataset dataset,
                                     const QStringList& names) {
    auto* layer = makeLayer(names);
    HeritageStyle::apply(layer, dataset, QStringLiteral("nm"));
    // Simulate saved/reopened sources: identify the original dataset group, not
    // an in-memory HeritageSiteLegend instance or the imported file's title.
    layer->setName(QStringLiteral("원본_자료_%1").arg(project.mapLayers().size()));
    LayerOps::markReferenceLayer(layer);
    project.addMapLayer(layer, false);
    auto* reference = project.layerTreeRoot()->findGroup(QStringLiteral("참조 지도"));
    if (!reference) reference = project.layerTreeRoot()->addGroup(QStringLiteral("참조 지도"));
    const QString datasetName = HeritageStyle::layerName(dataset);
    auto* group = reference->findGroup(datasetName);
    if (!group) group = reference->addGroup(datasetName);
    group->addLayer(layer);
    return layer;
  }

  static QgsLayoutItemMap* makeLayoutMap(QgsPrintLayout& layout,
                                        const QList<QgsMapLayer*>& layers,
                                        const QgsRectangle& extent) {
    layout.initializeDefaults();
    auto* map = new QgsLayoutItemMap(&layout);
    layout.addLayoutItem(map);
    map->attemptSetSceneRect(QRectF(10., 10., 160., 160.));
    map->setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")));
    map->setKeepLayerSet(true);
    map->setLayers(layers);
    map->setExtent(extent);
    return map;
  }

  static QSet<QString> numberedEntryKeys(const QVector<HeritageLayoutNumbers::Entry>& entries) {
    QSet<QString> keys;
    for (const auto& entry : entries) {
      if (entry.number > 0) keys.insert(HeritageLayoutNumbers::entryKey(entry.layerId, entry.number));
    }
    return keys;
  }

  static QSet<QString> legendNumberKeys(QgsLayoutItemLegend* legend) {
    QSet<QString> keys;
    for (auto* layer : legend->model()->rootGroup()->findLayers()) {
      for (auto* node : legend->model()->layerLegendNodes(layer)) {
        auto* symbol = dynamic_cast<QgsSymbolLegendNode*>(node);
        if (!symbol || !symbol->customSymbol()) continue;
        for (int i = 0; i < symbol->customSymbol()->symbolLayerCount(); ++i) {
          auto* glyph = dynamic_cast<QgsFontMarkerSymbolLayer*>(symbol->customSymbol()->symbolLayer(i));
          if (glyph) keys.insert(HeritageLayoutNumbers::entryKey(layer->layerId(), glyph->character().toInt()));
        }
      }
    }
    return keys;
  }

  static QVector<HeritageLayoutNumbers::Entry> legendEntries(
      QgsLayoutItemLegend* legend, QVector<HeritageLayoutNumbers::Entry> entries) {
    for (auto& entry : entries) entry.number = 0;
    for (auto* layer : legend->model()->rootGroup()->findLayers()) {
      for (auto* node : legend->model()->layerLegendNodes(layer)) {
        auto* symbol = dynamic_cast<QgsSymbolLegendNode*>(node);
        if (!symbol || !symbol->customSymbol()) continue;
        for (int i = 0; i < symbol->customSymbol()->symbolLayerCount(); ++i) {
          auto* glyph = dynamic_cast<QgsFontMarkerSymbolLayer*>(symbol->customSymbol()->symbolLayer(i));
          if (!glyph) continue;
          for (auto& entry : entries)
            if (entry.layerId == layer->layerId() && entry.name == node->data(Qt::DisplayRole).toString())
              entry.number = glyph->character().toInt();
        }
      }
    }
    return entries;
  }

  static QString consecutiveLegendError(QgsLayoutItemLegend* legend,
                                         const QVector<HeritageLayoutNumbers::Entry>& entries) {
    QMap<QString, QList<int>> numbers;
    for (auto* layer : legend->model()->rootGroup()->findLayers()) {
      for (auto* node : legend->model()->layerLegendNodes(layer)) {
        auto* symbol = dynamic_cast<QgsSymbolLegendNode*>(node);
        if (!symbol || !symbol->customSymbol()) continue;
        int number = 0;
        for (int i = 0; i < symbol->customSymbol()->symbolLayerCount(); ++i)
          if (auto* glyph = dynamic_cast<QgsFontMarkerSymbolLayer*>(symbol->customSymbol()->symbolLayer(i)))
            number = glyph->character().toInt();
        if (number <= 0) return QStringLiteral("A rendered legend badge has no positive number");
        bool found = false;
        for (const auto& entry : entries) {
          if (entry.layerId != layer->layerId() || entry.name != node->data(Qt::DisplayRole).toString()) continue;
          if (entry.color != symbol->customSymbol()->color()) return QStringLiteral("A numbered site's color changed");
          numbers[entry.dataset].append(number);
          found = true;
          break;
        }
        if (!found) return QStringLiteral("A numbered site's layer/name identity changed");
      }
    }
    for (auto it = numbers.begin(); it != numbers.end(); ++it) {
      auto values = it.value();
      std::sort(values.begin(), values.end());
      for (int i = 0; i < values.size(); ++i)
        if (values.at(i) != i + 1) return QStringLiteral("%1 is not consecutive from 1: %2 at position %3").arg(it.key()).arg(values.at(i)).arg(i + 1);
    }
    return {};
  }

  static QSet<QString> placedNumberKeys(const QgsLabelingResults* results,
                                        QgsLayoutItemMap* map,
                                        const QVector<HeritageLayoutNumbers::Entry>& entries) {
    QSet<QString> keys;
    if (!results) return keys;
    QPainterPath paper;
    for (int i = 0; i < map->layout()->pageCollection()->pageCount(); ++i) {
      auto* page = map->layout()->pageCollection()->page(i);
      if (!map->layout()->pageCollection()->shouldExportPage(i)) continue;
      paper.addPolygon(page->mapToScene(page->rect()));
    }
    QPainterPath mapFrame;
    mapFrame.addPolygon(map->mapToScene(map->rect()));
    const QPainterPath clippedPage = paper.intersected(mapFrame);
    bool transformValid = false;
    const QTransform mapToLayout = map->layoutToMapCoordsTransform().inverted(&transformValid);
    if (!transformValid) return keys;
    for (const auto& label : results->allLabels()) {
      if (label.isUnplaced || label.isDiagram) continue;
      bool numeric = false;
      const int number = label.labelText.toInt(&numeric);
      if (!numeric) continue;
      bool matched = false;
      for (const auto& entry : entries)
        if (entry.layerId == label.layerID && entry.number == number && entry.featureIds.contains(label.featureId))
          matched = true;
      if (!matched) continue;
      QPolygonF corners;
      for (const auto& point : label.cornerPoints)
        corners.append(mapToLayout.map(QPointF(point.x(), point.y())));
      QPainterPath labelPath;
      labelPath.addPolygon(corners);
      if (clippedPage.intersects(labelPath))
        keys.insert(HeritageLayoutNumbers::entryKey(label.layerID, number));
    }
    return keys;
  }

  static const QgsLabelingResults* sheetNumberResults(QgsLayoutExporter& exporter, QgsLayoutItemMap* map) {
    if (auto* numbers = HeritageLayoutNumbers::numbersMapOf(map)) {
      if (const auto* results = exporter.labelingResults().value(numbers->uuid()))
        return results;
    }
    return exporter.labelingResults().value(map->uuid());
  }

private slots:
  void canvasNamesHideWhenZoomedOutPastTenThousand() {
    auto* layer = makeLayer({QStringLiteral("유적 A")});
    QVERIFY(HeritageStyle::apply(layer, HeritageDataset::BuriedHeritageArea, QStringLiteral("nm")).ok);
    QVERIFY(layer->labelsEnabled());
    const QgsPalLayerSettings settings = layer->labeling()->settings();
    QVERIFY(settings.scaleVisibility);
    QCOMPARE(settings.minimumScale, HeritageStyle::nameLabelMinScale());
    QCOMPARE(settings.maximumScale, 0.);
    delete layer;
  }

  void mixedThematicLegendFiltersMapExtentAndKeepsNumberBadges() {
    QgsProject project;
    auto* heritage = addHeritage(project, HeritageDataset::SurfaceSurveyArea,
                                {QStringLiteral("유적 A"), QStringLiteral("유적 B")});
    auto* geology = makeLayer({QStringLiteral("도면 안 암석"), QStringLiteral("도면 밖 암석")});
    geology->setName(QStringLiteral("지질 분류 검사"));
    QgsCategoryList categories;
    for (const auto& name : {QStringLiteral("도면 안 암석"), QStringLiteral("도면 밖 암석")})
      categories.append(QgsRendererCategory(name, std::unique_ptr<QgsSymbol>(QgsSymbol::defaultSymbol(Qgis::GeometryType::Polygon)).release(), name));
    for (int i = 0; i < 60; ++i) {
      const QString name = QStringLiteral("다른 지역 암석 %1").arg(i);
      categories.append(QgsRendererCategory(name, std::unique_ptr<QgsSymbol>(QgsSymbol::defaultSymbol(Qgis::GeometryType::Polygon)).release(), name));
    }
    geology->setRenderer(new QgsCategorizedSymbolRenderer(QStringLiteral("nm"), categories));
    project.addMapLayer(geology);
    QgsMapLayerStyle original;
    original.readFromLayer(geology);
    QgsPrintLayout layout(&project);
    auto* map = makeLayoutMap(layout, {heritage}, QgsRectangle(189950., 549950., 190150., 550150.));
    auto* legend = new QgsLayoutItemLegend(&layout);
    layout.addLayoutItem(legend);
    legend->attemptSetSceneRect(QRectF(175., 10., 100., 70.));
    legend->setLinkedMap(map);
    HeritageLayoutNumbers numbers;
    // Start with the fast numbered-only path, then add/remove a thematic layer.
    for (int pass = 0; pass < 4; ++pass) {
      const bool mixed = pass % 2;
      map->setLayers(mixed ? QList<QgsMapLayer*>{heritage, geology} : QList<QgsMapLayer*>{heritage});
      QVERIFY(numbers.update(map));
      LayoutService::tuneSheetLegend(legend);
      numbers.applyLegend(legend);
      QCOMPARE(legend->legendFilterByMapEnabled(), mixed);
      QgsLayoutExporter exporter(&layout);
      if (pass == 1) {
        // No preview or event-loop wait before this first mixed PDF.
        LayoutService::settleSheetLegendsForExport(&layout);
        QVERIFY(legend->rect().height() < 80.);
        QTemporaryDir pdfDirectory;
        QgsLayoutExporter::PdfExportSettings pdfSettings;
        QCOMPARE(exporter.exportToPdf(pdfDirectory.filePath(QStringLiteral("first-mixed.pdf")), pdfSettings), QgsLayoutExporter::Success);
        QVERIFY(legend->rect().height() < 80.);
      }
      QVERIFY(!exporter.renderPageToImage(0, QSize(1000, 700), 96).isNull());
      legend->model()->waitForHitTestBlocking();
      if (mixed) {
        auto* geologyNode = legend->model()->rootGroup()->findLayer(geology->id());
        QVERIFY(geologyNode);
        QCOMPARE(nodeLabels(legend->model()->layerLegendNodes(geologyNode)), QStringList{QStringLiteral("도면 안 암석")});
        QTRY_VERIFY_WITH_TIMEOUT(legend->rect().height() < 80., 3000);
      }
      auto* heritageNode = legend->model()->rootGroup()->findLayer(heritage->id());
      QVERIFY(heritageNode);
      const auto nodes = legend->model()->layerLegendNodes(heritageNode);
      QCOMPARE(nodes.size(), 1);
      auto* badge = dynamic_cast<QgsSymbolLegendNode*>(nodes.first());
      QVERIFY(badge && badge->customSymbol());
      QCOMPARE(badge->customSymbol()->type(), Qgis::SymbolType::Marker);
      if (pass == 3) {
        map->setExtent(QgsRectangle(190150., 549950., 190350., 550150.));
        QVERIFY(numbers.update(map));
        LayoutService::tuneSheetLegend(legend);
        numbers.applyLegend(legend);
        QVERIFY(!exporter.renderPageToImage(0, QSize(), 96).isNull());
        legend->model()->waitForHitTestBlocking();
        auto* changedGeology = legend->model()->rootGroup()->findLayer(geology->id());
        QVERIFY(changedGeology);
        QCOMPARE(nodeLabels(legend->model()->layerLegendNodes(changedGeology)), QStringList{QStringLiteral("도면 밖 암석")});
        auto* changedHeritage = legend->model()->rootGroup()->findLayer(heritage->id());
        QVERIFY(changedHeritage);
        const auto changedNodes = legend->model()->layerLegendNodes(changedHeritage);
        QCOMPARE(changedNodes.size(), 1);
        auto* changedBadge = dynamic_cast<QgsSymbolLegendNode*>(changedNodes.first());
        QVERIFY(changedBadge && changedBadge->customSymbol());
        QCOMPARE(changedBadge->data(Qt::DisplayRole).toString(), QStringLiteral("유적 B"));
      }
    }
    QgsMapLayerStyle after;
    after.readFromLayer(geology);
    QCOMPARE(after.xmlData(), original.xmlData());
  }

  void colorTableMatchesPlan() {
    QCOMPARE(HeritageStyle::color(HeritageDataset::DesignatedHeritage).name(), QStringLiteral("#8e44ad"));
    QCOMPARE(HeritageStyle::color(HeritageDataset::AlterationStandard).name(), QStringLiteral("#e67e22"));
    QCOMPARE(HeritageStyle::color(HeritageDataset::BuriedHeritageArea).name(), QStringLiteral("#2e86c1"));
    QCOMPARE(HeritageStyle::color(HeritageDataset::HeritageDistributionMap).name(), QStringLiteral("#27ae60"));
    QCOMPARE(HeritageStyle::color(HeritageDataset::SurfaceSurveyArea).name(), QStringLiteral("#d4aa00"));
    QCOMPARE(HeritageStyle::color(HeritageDataset::ExcavationSurveyArea).name(), QStringLiteral("#c2187d"));
    QCOMPARE(HeritageStyle::allDatasets().size(), 6);
  }

  void everyDatasetHasItsOwnColorAndName() {
    QSet<QString> colors;
    QSet<QString> names;
    for (HeritageDataset ds : HeritageStyle::allDatasets()) {
      colors.insert(HeritageStyle::color(ds).name());
      names.insert(HeritageStyle::layerName(ds));
      QVERIFY(!HeritageStyle::layerName(ds).isEmpty());
      QCOMPARE(HeritageStyle::fromLayerName(HeritageStyle::layerName(ds)).value(), ds);
    }
    QCOMPARE(colors.size(), 6);
    QCOMPARE(names.size(), 6);
  }

  void layoutPaletteOnlyUpgradesLegacyDefaultsAndPreservesAlpha() {
    const QColor custom(QStringLiteral("#3178cc"));
    for (HeritageDataset dataset : HeritageStyle::allDatasets()) {
      QCOMPARE(HeritageStyle::layoutColor(dataset, custom), custom);
      QCOMPARE(HeritageStyle::layoutColor(dataset, HeritageStyle::color(dataset)), HeritageStyle::color(dataset));
    }
    QColor oldSurface(QStringLiteral("#16a085"));
    oldSurface.setAlpha(128);
    QColor expected = HeritageStyle::color(HeritageDataset::SurfaceSurveyArea);
    expected.setAlpha(128);
    QCOMPARE(HeritageStyle::layoutColor(HeritageDataset::SurfaceSurveyArea, oldSurface), expected);
    QCOMPARE(HeritageStyle::layoutColor(HeritageDataset::ExcavationSurveyArea, QColor(QStringLiteral("#a0522d"))),
             HeritageStyle::color(HeritageDataset::ExcavationSurveyArea));
    // A different dataset using these RGBs is an explicit/custom choice.
    QCOMPARE(HeritageStyle::layoutColor(HeritageDataset::DesignatedHeritage, oldSurface), oldSurface);
  }

  void legacyDatasetColorsUpgradeOnlyInLayoutCopies() {
    QgsProject project;
    auto* surface = addHeritage(project, HeritageDataset::SurfaceSurveyArea, {QStringLiteral("옛 지표 유적")});
    auto* excavation = addHeritage(project, HeritageDataset::ExcavationSurveyArea, {QStringLiteral("옛 발굴 유적")});
    auto* custom = addHeritage(project, HeritageDataset::SurfaceSurveyArea, {QStringLiteral("사용자 색 유적")});
    const QColor customColor(QStringLiteral("#3178cc"));
    const QColor customStroke(QStringLiteral("#5938be"));
    QMap<QString, QColor> expectedColors;
    QMap<QString, QString> sourceStyles;
    const QList<QgsVectorLayer*> sources{surface, excavation, custom};
    for (auto* source : sources) {
      const QColor color = source == surface ? QColor(QStringLiteral("#16a085"))
                            : source == excavation ? QColor(QStringLiteral("#a0522d")) : customColor;
      auto* renderer = dynamic_cast<QgsSingleSymbolRenderer*>(source->renderer());
      QVERIFY(renderer);
      auto* symbol = renderer->symbol()->clone();
      symbol->setColor(color);
      auto* fill = dynamic_cast<QgsSimpleFillSymbolLayer*>(symbol->symbolLayer(0));
      QVERIFY(fill);
      fill->setStrokeColor(source == custom ? customStroke : color);
      renderer->setSymbol(symbol);
      source->triggerRepaint();
      QgsMapLayerStyle style;
      style.readFromLayer(source);
      sourceStyles.insert(source->id(), style.xmlData());
      expectedColors.insert(source->id(), source == surface ? HeritageStyle::color(HeritageDataset::SurfaceSurveyArea)
                                        : source == excavation ? HeritageStyle::color(HeritageDataset::ExcavationSurveyArea)
                                        : customColor);
    }
    QgsPrintLayout layout(&project);
    auto* map = makeLayoutMap(layout, {surface, excavation, custom}, QgsRectangle(189950., 549950., 190150., 550150.));
    HeritageLayoutNumbers numbers;
    QVERIFY(numbers.update(map));
    QCOMPARE(numbers.entries().size(), 3);
    for (const auto& entry : numbers.entries()) {
      auto* source = qobject_cast<QgsVectorLayer*>(project.mapLayer(entry.layerId));
      QVERIFY(source);
      QCOMPARE(entry.color, expectedColors.value(entry.layerId));
      auto* pins = numbers.numberLayer();
      QVERIFY(pins);
      bool foundPin = false;
      QgsFeature pinFeature;
      auto pinRows = pins->getFeatures();
      while (pinRows.nextFeature(pinFeature)) {
        if (pinFeature.attribute(QStringLiteral("layer")).toString() != entry.layerId) continue;
        QCOMPARE(pinFeature.attribute(QStringLiteral("fill")).toString(), entry.color.name(QColor::HexArgb));
        foundPin = true;
      }
      QVERIFY(foundPin);
      std::unique_ptr<QgsVectorLayer> drawing(source->clone());
      QgsMapLayerStyle(numbers.overrides().value(entry.layerId)).writeToLayer(drawing.get());
      QVERIFY(!drawing->labelsEnabled());
      auto* renderer = dynamic_cast<QgsCategorizedSymbolRenderer*>(drawing->renderer());
      QVERIFY(renderer);
      const auto* fill = dynamic_cast<QgsSimpleFillSymbolLayer*>(renderer->categories().first().symbol()->symbolLayer(0));
      QVERIFY(fill);
      QCOMPARE(fill->color(), entry.color);
      QCOMPARE(fill->strokeColor(), source == custom ? customStroke : entry.color);
      QgsMapLayerStyle after;
      after.readFromLayer(source);
      QCOMPARE(after.xmlData(), sourceStyles.value(source->id()));
    }
    auto* legend = new QgsLayoutItemLegend(&layout);
    layout.addLayoutItem(legend);
    legend->setLinkedMap(map);
    legend->setSyncMode(Qgis::LegendSyncMode::Manual);
    legend->resetManualLayers(Qgis::LegendSyncMode::VisibleLayers);
    numbers.applyLegend(legend);
    for (auto* node : legend->model()->rootGroup()->findLayers()) {
      const auto items = legend->model()->layerLegendNodes(node);
      QCOMPARE(items.size(), 1);
      auto* item = dynamic_cast<QgsSymbolLegendNode*>(items.first());
      QVERIFY(item && item->customSymbol());
      QCOMPARE(item->customSymbol()->color(), expectedColors.value(node->layerId()));
    }
  }



  void reservedColorsAreRefused() {
    // 빨강은 조사구역 팔레트가 쓴다.
    QVERIFY(HeritageStyle::isReservedColor(QColor(QStringLiteral("#DC2626"))));
    QVERIFY(HeritageStyle::isReservedColor(QColor(QStringLiteral("#FF0000"))));
    // 회색은 수치지형도 밑그림이 쓴다.
    QVERIFY(HeritageStyle::isReservedColor(QColor(QStringLiteral("#808080"))));
    QVERIFY(HeritageStyle::isReservedColor(QColor(QStringLiteral("#000000"))));
    // 표의 여섯 색은 어느 것도 걸리지 않는다.
    for (HeritageDataset ds : HeritageStyle::allDatasets())
      QVERIFY2(!HeritageStyle::isReservedColor(HeritageStyle::color(ds)),
               qPrintable(HeritageStyle::layerName(ds)));
  }

  void legendGetsOneRowPerSiteNameInOneColor() {
    std::unique_ptr<QgsVectorLayer> layer(
        makeLayer({QStringLiteral("안동 저전리유적"), QStringLiteral("안동 마애리유적"),
                   QStringLiteral("안동 조탑리유적")}));
    QVERIFY(layer->isValid());
    const HeritageStyleResult r =
        HeritageStyle::apply(layer.get(), HeritageDataset::BuriedHeritageArea, QStringLiteral("nm"));
    QVERIFY(r.ok);
    QVERIFY(!r.overCap);
    QCOMPARE(r.categoryCount, 1);

    auto* single = dynamic_cast<QgsSingleSymbolRenderer*>(layer->renderer());
    QVERIFY(single);
    QCOMPARE(single->symbol()->color(), HeritageStyle::color(HeritageDataset::BuriedHeritageArea));
    QVERIFY(layer->labelsEnabled());
    QCOMPARE(layer->labeling()->settings().fieldName, QStringLiteral("\"nm\""));
  }

  void emptySiteNameStillDraws() {
    std::unique_ptr<QgsVectorLayer> layer(makeLayer({QStringLiteral("안동 저전리유적")}, true));
    QVERIFY(layer->isValid());
    QVERIFY(HeritageStyle::apply(layer.get(), HeritageDataset::BuriedHeritageArea,
                                 QStringLiteral("nm")).ok);
    QVERIFY(dynamic_cast<QgsSingleSymbolRenderer*>(layer->renderer()));
    QCOMPARE(layer->featureCount(), 2LL);
  }

  void missingNameFieldSaysSoInsteadOfPretending() {
    std::unique_ptr<QgsVectorLayer> layer(makeLayer({QStringLiteral("안동 저전리유적")}));
    const HeritageStyleResult r = HeritageStyle::apply(
        layer.get(), HeritageDataset::BuriedHeritageArea, QStringLiteral("없는필드"));
    QVERIFY(r.ok);
    QVERIFY(!r.message.isEmpty());
    QVERIFY(dynamic_cast<QgsSingleSymbolRenderer*>(layer->renderer()));
  }

  void nameFieldIsChosenFromRealFieldsNotGuessed() {
    auto makeWith = [](const QString& uriFields) {
      return new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5186&%1").arg(uriFields),
                                QStringLiteral("t"), QStringLiteral("memory"));
    };
    // 있는 것 중에서 고른다.
    std::unique_ptr<QgsVectorLayer> a(makeWith(QStringLiteral("field=id:integer&field=유적명:string(80)")));
    QVERIFY(a->isValid());
    QCOMPARE(HeritageImport::chooseNameField(a.get()), QStringLiteral("유적명"));

    // 지표조사구역·발굴조사구역은 사업명으로 온다.
    std::unique_ptr<QgsVectorLayer> b(makeWith(QStringLiteral("field=id:integer&field=사업명:string(80)")));
    QCOMPARE(HeritageImport::chooseNameField(b.get()), QStringLiteral("사업명"));

    // 「명」이 들어간 글자 필드로 떨어진다.
    std::unique_ptr<QgsVectorLayer> c(makeWith(QStringLiteral("field=id:integer&field=지정명칭:string(80)")));
    QCOMPARE(HeritageImport::chooseNameField(c.get()), QStringLiteral("지정명칭"));

    // 숫자만 있으면 고르지 않는다. 없는 이름을 지어내지 않는다.
    std::unique_ptr<QgsVectorLayer> d(makeWith(QStringLiteral("field=id:integer&field=area:double")));
    QVERIFY(HeritageImport::chooseNameField(d.get()).isEmpty());
  }

  void oneZipCanCarryManyShapefiles() {
    // 지정유산 ZIP 하나에 6종이 들어온다(2026-09-12 실제 파일).
    // 색은 종류(지정유산) 하나로 통일하되, 레이어 이름은 파일 이름을 살려야 구분된다.
    const QStringList inZip = {QStringLiteral("국가지정유산"), QStringLiteral("시도지정유산"),
                               QStringLiteral("국가등록문화유산"), QStringLiteral("시도등록문화유산"),
                               QStringLiteral("국가지정유산보호구역"),
                               QStringLiteral("시도지정유산보호구역")};
    for (const QString& name : inZip) {
      // 이 이름들은 종류 이름과 다르다. fromLayerName 이 종류로 오인하면 안 된다.
      QVERIFY2(!HeritageStyle::fromLayerName(name).has_value() ||
                   HeritageStyle::fromLayerName(name).value() == HeritageDataset::DesignatedHeritage,
               qPrintable(name));
    }
  }

  void referenceGroupKeepsHeritageOutOfSurveyData() {
    QCOMPARE(HeritageImport::referenceGroupName(), QStringLiteral("참조 지도"));
  }

  void panelShowsTypeOnlyAndSheetShowsEverySiteName() {
    QgsProject project;
    auto* layer = makeLayer({QStringLiteral("안동 저전리유적"), QStringLiteral("안동 마애리유적")});
    QVERIFY(layer->isValid());
    project.addMapLayer(layer);
    QVERIFY(HeritageStyle::apply(layer, HeritageDataset::BuriedHeritageArea,
                                 QStringLiteral("nm")).ok);
    QVERIFY(HeritageSiteLegend::install(layer));
    QVERIFY(HeritageSiteLegend::isInstalled(layer));

    // 레이어창: 프로젝트 트리에 달린 노드
    QgsLayerTreeLayer* panelNode = project.layerTreeRoot()->findLayer(layer->id());
    QVERIFY(panelNode);
    QVERIFY(HeritageSiteLegend::isPanelNode(panelNode, layer));
    QList<QgsLayerTreeModelLegendNode*> panel =
        layer->legend()->createLayerTreeModelLegendNodes(panelNode);
    QCOMPARE(panel.size(), 1);
    QVERIFY(panel.first()->isEmbeddedInParent());
    qDeleteAll(panel);

    // 도면 범례: tuneSheetLegend 가 Manual 동기화로 만드는 복제 트리
    QgsLayerTreeLayer sheetNode(layer);
    QVERIFY(!HeritageSiteLegend::isPanelNode(&sheetNode, layer));
    QList<QgsLayerTreeModelLegendNode*> sheet =
        layer->legend()->createLayerTreeModelLegendNodes(&sheetNode);
    QCOMPARE(sheet.size(), 1);
    QVERIFY(layer->labelsEnabled());
    QCOMPARE(layer->labeling()->settings().fieldName, QStringLiteral("\"nm\""));
    qDeleteAll(sheet);
  }

  void layoutNumbersShareCategoriesButKeepLayersAndDatasetsDistinct() {
    QgsProject project;
    auto* first = addHeritage(project, HeritageDataset::DesignatedHeritage,
                              {QStringLiteral("같은 유적"), QStringLiteral("같은 유적")});
    auto* second = addHeritage(project, HeritageDataset::DesignatedHeritage,
                               {QStringLiteral("같은 유적")});
    auto* third = addHeritage(project, HeritageDataset::SurfaceSurveyArea,
                              {QStringLiteral("같은 유적")});
    QgsPrintLayout layout(&project);
    auto* map = makeLayoutMap(layout, {first, second, third},
                              QgsRectangle(189950., 549800., 190450., 550300.));
    HeritageLayoutNumbers numbers;
    numbers.update(map, true);
    QVERIFY2(numbers.error().isEmpty(), qPrintable(numbers.error()));
    QCOMPARE(numbers.entries().size(), 3);
    QSet<int> designatedNumbers;
    QSet<QString> layerIds;
    for (const auto& entry : numbers.entries()) {
      layerIds.insert(entry.layerId);
      QCOMPARE(entry.name, QStringLiteral("같은 유적"));
      if (entry.dataset == HeritageStyle::layerName(HeritageDataset::DesignatedHeritage)) {
        designatedNumbers.insert(entry.number);
        QCOMPARE(entry.color, HeritageStyle::color(HeritageDataset::DesignatedHeritage));
      } else {
        QCOMPARE(entry.dataset, HeritageStyle::layerName(HeritageDataset::SurfaceSurveyArea));
        QCOMPARE(entry.number, 1);
        QCOMPARE(entry.color, HeritageStyle::color(HeritageDataset::SurfaceSurveyArea));
      }
    }
    QCOMPARE(layerIds.size(), 3);
    QCOMPARE(designatedNumbers, (QSet<int>{1, 2}));
  }

  void layoutLegendNumbersMatchEntriesAfterTuneAndFilter() {
    QgsProject project;
    auto* first = addHeritage(project, HeritageDataset::DesignatedHeritage,
                              {QStringLiteral("평양리제요10호"), QStringLiteral("이도리제1호"),
                               QStringLiteral("평양리제요8호")});
    auto* second = addHeritage(project, HeritageDataset::DesignatedHeritage,
                               {QStringLiteral("평양리제요10호"), QStringLiteral("이도리제2호")});
    QgsPrintLayout layout(&project);
    auto* map = makeLayoutMap(layout, {first, second},
                              QgsRectangle(189950., 549800., 190450., 550300.));
    HeritageLayoutNumbers numbers;
    QVERIFY(numbers.update(map, true));
    QCOMPARE(numbers.entries().size(), 5);
    auto* legend = new QgsLayoutItemLegend(&layout);
    layout.addLayoutItem(legend);
    legend->setLinkedMap(map);
    LayoutService::tuneSheetLegend(legend);
    numbers.applyLegend(legend);
    QMap<QString, int> badges;
    for (auto* node : legend->model()->rootGroup()->findLayers()) {
      for (auto* item : legend->model()->layerLegendNodes(node)) {
        auto* symbol = dynamic_cast<QgsSymbolLegendNode*>(item);
        QVERIFY(symbol);
        QVERIFY2(symbol->customSymbol(), qPrintable(item->data(Qt::DisplayRole).toString()));
        QString glyph;
        for (int i = 0; i < symbol->customSymbol()->symbolLayerCount(); ++i) {
          if (auto* font = dynamic_cast<QgsFontMarkerSymbolLayer*>(symbol->customSymbol()->symbolLayer(i)))
            glyph = font->character();
        }
        QVERIFY2(!glyph.isEmpty(), qPrintable(item->data(Qt::DisplayRole).toString()));
        badges.insert(node->layerId() + QLatin1Char('\t') + item->data(Qt::DisplayRole).toString(),
                      glyph.toInt());
      }
    }
    QCOMPARE(badges.size(), numbers.entries().size());
    QSet<int> seen;
    for (const auto& entry : numbers.entries()) {
      const int badge = badges.value(entry.layerId + QLatin1Char('\t') + entry.name);
      QCOMPARE(badge, entry.number);
      QVERIFY(!seen.contains(entry.number));
      seen.insert(entry.number);
    }
    for (auto* node : legend->model()->rootGroup()->findLayers())
      legend->model()->refreshLayerLegend(node);
    numbers.applyLegend(legend);
    QCOMPARE(legendNumberKeys(legend), numbers.legendKeys());
  }

  void layoutLegendKeepsNamesWhenHeritageIsOnlyOnTheOverlay() {
    QgsProject project;
    auto* heritage = addHeritage(project, HeritageDataset::DesignatedHeritage,
                                 {QStringLiteral("경주 가"), QStringLiteral("경주 나")});
    auto* survey = new QgsVectorLayer(
        QStringLiteral("Polygon?crs=EPSG:5186&field=nm:string(40)"),
        QStringLiteral("조사구역"), QStringLiteral("memory"));
    QVERIFY(survey->isValid());
    QVERIFY(survey->startEditing());
    QgsFeature area(survey->fields());
    area.setGeometry(QgsGeometry::fromRect(QgsRectangle(190010., 550010., 190040., 550040.)));
    area.setAttribute(QStringLiteral("nm"), QStringLiteral("조사"));
    QVERIFY(survey->addFeature(area));
    QVERIFY(survey->commitChanges());
    project.addMapLayer(survey, false);
    QgsPrintLayout layout(&project);
    const QgsRectangle extent(189950., 549950., 190450., 550250.);
    auto* base = makeLayoutMap(layout, {survey}, extent);
    base->setId(QStringLiteral("ka_map"));
    auto* above = new QgsLayoutItemMap(&layout);
    above->setId(QStringLiteral("ka_map_above"));
    layout.addLayoutItem(above);
    above->attemptSetSceneRect(base->rect().translated(base->pos()));
    above->setCrs(base->crs());
    above->setKeepLayerSet(true);
    above->setLayers({heritage});
    above->setExtent(extent);
    HeritageLayoutNumbers numbers;
    QVERIFY(numbers.update(base, true));
    QCOMPARE(numbers.entries().size(), 2);
    auto* legend = new QgsLayoutItemLegend(&layout);
    layout.addLayoutItem(legend);
    legend->setLinkedMap(base);
    legend->setLegendFilterByMapEnabled(true);
    LayoutService::tuneSheetLegend(legend);
    numbers.applyLegend(legend);
    const QList<QgsLayoutItemMap*> filters = legend->filterByMapItems();
    QVERIFY2(filters.contains(above), "legend filter must include the heritage overlay");
    QStringList names;
    for (auto* node : legend->model()->rootGroup()->findLayers()) {
      for (auto* item : legend->model()->layerLegendNodes(node))
        names << item->data(Qt::DisplayRole).toString();
    }
    QVERIFY2(names.contains(QStringLiteral("경주 가")), qPrintable(names.join(QLatin1Char(','))));
    QVERIFY2(names.contains(QStringLiteral("경주 나")), qPrintable(names.join(QLatin1Char(','))));
  }

  void layoutNumbersFollowPaperClipNotOffPageCentroid() {
    QgsProject project;
    auto* layer = new QgsVectorLayer(
        QStringLiteral("Polygon?crs=EPSG:5186&field=nm:string(80)"),
        QStringLiteral("매장유산유존지역"), QStringLiteral("memory"));
    QVERIFY(layer->isValid());
    QVERIFY(layer->startEditing());
    QgsFeature onPage(layer->fields());
    onPage.setGeometry(QgsGeometry::fromWkt(QStringLiteral(
        "POLYGON((190000 550000,190080 550000,190080 550080,190000 550080,190000 550000))")));
    onPage.setAttribute(QStringLiteral("nm"), QStringLiteral("페이지 안"));
    QVERIFY(layer->addFeature(onPage));
    QgsFeature clip(layer->fields());
    clip.setGeometry(QgsGeometry::fromWkt(QStringLiteral(
        "POLYGON((100000 400000,190040 400000,190040 550040,100000 550040,100000 400000))")));
    clip.setAttribute(QStringLiteral("nm"), QStringLiteral("걸친 유적"));
    QVERIFY(layer->addFeature(clip));
    QgsFeature outside(layer->fields());
    outside.setGeometry(QgsGeometry::fromWkt(QStringLiteral(
        "POLYGON((191000 551000,191080 551000,191080 551080,191000 551080,191000 551000))")));
    outside.setAttribute(QStringLiteral("nm"), QStringLiteral("페이지 밖"));
    QVERIFY(layer->addFeature(outside));
    QVERIFY(layer->commitChanges());
    HeritageStyle::apply(layer, HeritageDataset::BuriedHeritageArea, QStringLiteral("nm"));
    LayerOps::markReferenceLayer(layer);
    project.addMapLayer(layer, false);
    auto* reference = project.layerTreeRoot()->addGroup(QStringLiteral("참조 지도"));
    auto* group = reference->addGroup(HeritageStyle::layerName(HeritageDataset::BuriedHeritageArea));
    group->addLayer(layer);
    QgsPrintLayout layout(&project);
    auto* map = makeLayoutMap(layout, {layer}, QgsRectangle(189950., 549950., 190200., 550200.));
    HeritageLayoutNumbers numbers;
    numbers.update(map, true);
    QVERIFY2(numbers.error().isEmpty(), qPrintable(numbers.error()));
    QSet<QString> names;
    for (const auto& entry : numbers.entries()) names.insert(entry.name);
    QVERIFY(names.contains(QStringLiteral("페이지 안")));
    QVERIFY(names.contains(QStringLiteral("걸친 유적")));
    QVERIFY(!names.contains(QStringLiteral("페이지 밖")));
    QCOMPARE(numbers.entries().size(), 2);
    auto* pins = numbers.numberLayer();
    QVERIFY(pins);
    QCOMPARE(int(pins->featureCount()), 2);
    QVERIFY(!pins->labelsEnabled());
    auto* legend = new QgsLayoutItemLegend(&layout);
    layout.addLayoutItem(legend);
    legend->setLinkedMap(map);
    legend->setSyncMode(Qgis::LegendSyncMode::Manual);
    legend->resetManualLayers(Qgis::LegendSyncMode::VisibleLayers);
    numbers.applyLegend(legend);
    QCOMPARE(legendNumberKeys(legend), numberedEntryKeys(numbers.entries()));
  }

  void layoutNumbersIncludeHeritageMissingFromBaseMap() {
    QgsProject project;
    auto* dummy = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5186"),
                                     QStringLiteral("배경"), QStringLiteral("memory"));
    QVERIFY(dummy->isValid());
    project.addMapLayer(dummy);
    auto* first = addHeritage(project, HeritageDataset::DesignatedHeritage,
                              {QStringLiteral("지정 가"), QStringLiteral("지정 나")});
    auto* second = addHeritage(project, HeritageDataset::SurfaceSurveyArea,
                               {QStringLiteral("지표 가")});
    QgsPrintLayout layout(&project);
    auto* map = makeLayoutMap(layout, {dummy}, QgsRectangle(189950., 549950., 190550., 550250.));
    auto* overlay = new QgsLayoutItemMap(&layout);
    overlay->setId(QStringLiteral("ka_map_above"));
    layout.addLayoutItem(overlay);
    overlay->setKeepLayerSet(true);
    overlay->setLayers({first, second});
    overlay->setExtent(map->extent());
    HeritageLayoutNumbers numbers;
    QVERIFY(numbers.update(map));
    QCOMPARE(numbers.entries().size(), 3);
    QSet<QString> names;
    for (const auto& entry : numbers.entries()) names.insert(entry.name);
    QVERIFY(names.contains(QStringLiteral("지정 가")));
    QVERIFY(names.contains(QStringLiteral("지정 나")));
    QVERIFY(names.contains(QStringLiteral("지표 가")));
    numbers.raiseAboveGeometries(map);
    auto* raised = HeritageLayoutNumbers::numbersMapOf(map);
    QVERIFY(raised);
    auto* pins = numbers.numberLayer();
    QVERIFY(pins);
    QCOMPARE(raised->layers(), QList<QgsMapLayer*>{pins});
    QCOMPARE(int(pins->featureCount()), 3);
    QVERIFY(!raised->layers().contains(dummy));
  }

  void layoutNumbersKeepEverySiblingDrawing() {
    QgsProject project;
    auto* layer = addHeritage(project, HeritageDataset::DesignatedHeritage,
                              {QStringLiteral("같은 유적"), QStringLiteral("같은 유적")});
    QgsPrintLayout layout(&project);
    auto* map = makeLayoutMap(layout, {layer}, QgsRectangle(189950., 549950., 190550., 550250.));
    HeritageLayoutNumbers numbers;
    QVERIFY(numbers.update(map));
    QCOMPARE(numbers.entries().size(), 1);
    QCOMPARE(numbers.entries().first().featureIds.size(), 2);
    std::unique_ptr<QgsVectorLayer> drawing(layer->clone());
    QgsMapLayerStyle(numbers.overrides().value(layer->id())).writeToLayer(drawing.get());
    auto* categorized = dynamic_cast<QgsCategorizedSymbolRenderer*>(drawing->renderer());
    QVERIFY(categorized);
    QSet<qint64> renderedIds;
    bool hasCatchAll = false;
    for (const QgsRendererCategory& category : categorized->categories()) {
      const QVariant value = category.value();
      if (!value.isValid() || value.isNull()) {
        hasCatchAll = true;
        continue;
      }
      if (value.userType() == QMetaType::QVariantList) {
        for (const QVariant& item : value.toList())
          renderedIds.insert(item.toLongLong());
      } else {
        renderedIds.insert(value.toLongLong());
      }
    }
    for (qint64 id : numbers.entries().first().featureIds) {
      QVERIFY2(renderedIds.contains(id) || hasCatchAll,
               "같은 이름 형제 도형은 조판에서 빠져서는 안 된다");
    }
  }

  void layoutNumbersKeepOnPagePoints() {
    QgsProject project;
    auto* layer = new QgsVectorLayer(
        QStringLiteral("Point?crs=EPSG:5186&field=nm:string(80)"),
        QStringLiteral("문화유적분포지도"), QStringLiteral("memory"));
    QVERIFY(layer->isValid());
    QVERIFY(layer->startEditing());
    QgsFeature a(layer->fields());
    a.setGeometry(QgsGeometry::fromPointXY(QgsPointXY(190020., 550020.)));
    a.setAttribute(QStringLiteral("nm"), QStringLiteral("점 가"));
    QVERIFY(layer->addFeature(a));
    QgsFeature b(layer->fields());
    b.setGeometry(QgsGeometry::fromPointXY(QgsPointXY(190080., 550080.)));
    b.setAttribute(QStringLiteral("nm"), QStringLiteral("점 나"));
    QVERIFY(layer->addFeature(b));
    QgsFeature c(layer->fields());
    c.setGeometry(QgsGeometry::fromPointXY(QgsPointXY(191000., 551000.)));
    c.setAttribute(QStringLiteral("nm"), QStringLiteral("점 밖"));
    QVERIFY(layer->addFeature(c));
    QVERIFY(layer->commitChanges());
    HeritageStyle::apply(layer, HeritageDataset::HeritageDistributionMap, QStringLiteral("nm"));
    LayerOps::markReferenceLayer(layer);
    project.addMapLayer(layer, false);
    auto* reference = project.layerTreeRoot()->addGroup(QStringLiteral("참조 지도"));
    auto* group = reference->addGroup(HeritageStyle::layerName(HeritageDataset::HeritageDistributionMap));
    group->addLayer(layer);
    QgsPrintLayout layout(&project);
    auto* map = makeLayoutMap(layout, {layer}, QgsRectangle(189950., 549950., 190200., 550200.));
    HeritageLayoutNumbers numbers;
    numbers.update(map, true);
    QVERIFY2(numbers.error().isEmpty(), qPrintable(numbers.error()));
    QSet<QString> names;
    for (const auto& entry : numbers.entries()) names.insert(entry.name);
    QCOMPARE(names, (QSet<QString>{QStringLiteral("점 가"), QStringLiteral("점 나")}));
    QCOMPARE(numbers.entries().size(), 2);
    auto* keepPins = numbers.numberLayer();
    QVERIFY(keepPins);
    int keepOffset = 0;
    QgsFeature keepPin;
    auto keepRows = keepPins->getFeatures();
    while (keepRows.nextFeature(keepPin)) {
      const QgsPointXY pin = keepPin.geometry().asPoint();
      const QgsPointXY origin(keepPin.attribute(QStringLiteral("ox")).toDouble(),
                              keepPin.attribute(QStringLiteral("oy")).toDouble());
      if (pin.distance(origin) > 1.) ++keepOffset;
    }
    QCOMPARE(keepOffset, 0);
    auto* legend = new QgsLayoutItemLegend(&layout);
    layout.addLayoutItem(legend);
    legend->setLinkedMap(map);
    legend->setSyncMode(Qgis::LegendSyncMode::Manual);
    legend->resetManualLayers(Qgis::LegendSyncMode::VisibleLayers);
    numbers.applyLegend(legend);
    QCOMPARE(legendNumberKeys(legend), numberedEntryKeys(numbers.entries()));
  }

  void layoutNumbersOffsetStackedAnchorsWithCallout() {
    QgsProject project;
    auto* layer = new QgsVectorLayer(
        QStringLiteral("Point?crs=EPSG:5186&field=nm:string(80)"),
        QStringLiteral("문화유적분포지도"), QStringLiteral("memory"));
    QVERIFY(layer->isValid());
    QVERIFY(layer->startEditing());
    const QgsPointXY stack(190040., 550040.);
    for (const QString& name : {QStringLiteral("가"), QStringLiteral("나"),
                                QStringLiteral("다"), QStringLiteral("라")}) {
      QgsFeature feature(layer->fields());
      feature.setGeometry(QgsGeometry::fromPointXY(stack));
      feature.setAttribute(QStringLiteral("nm"), name);
      QVERIFY(layer->addFeature(feature));
    }
    QVERIFY(layer->commitChanges());
    HeritageStyle::apply(layer, HeritageDataset::HeritageDistributionMap, QStringLiteral("nm"));
    LayerOps::markReferenceLayer(layer);
    project.addMapLayer(layer, false);
    auto* reference = project.layerTreeRoot()->addGroup(QStringLiteral("참조 지도"));
    auto* group = reference->addGroup(HeritageStyle::layerName(HeritageDataset::HeritageDistributionMap));
    group->addLayer(layer);
    QgsPrintLayout layout(&project);
    auto* map = makeLayoutMap(layout, {layer}, QgsRectangle(186040., 546040., 194040., 554040.));
    HeritageLayoutNumbers numbers;
    numbers.update(map, true);
    QVERIFY2(numbers.error().isEmpty(), qPrintable(numbers.error()));
    QCOMPARE(numbers.entries().size(), 4);
    auto* pinLayer = numbers.numberLayer();
    QVERIFY(pinLayer);
    auto* pinSymbol = dynamic_cast<QgsSingleSymbolRenderer*>(pinLayer->renderer());
    QVERIFY(pinSymbol && dynamic_cast<QgsGeometryGeneratorSymbolLayer*>(pinSymbol->symbol()->symbolLayer(2)));
    QSet<QString> pins;
    int offset = 0;
    QgsFeature feature;
    auto features = pinLayer->getFeatures();
    while (features.nextFeature(feature)) {
      const QgsPointXY pin = feature.geometry().asPoint();
      pins.insert(QStringLiteral("%1,%2").arg(pin.x(), 0, 'g', 12).arg(pin.y(), 0, 'g', 12));
      if (qAbs(pin.x() - stack.x()) > 1. || qAbs(pin.y() - stack.y()) > 1.) ++offset;
    }
    QCOMPARE(pins.size(), 4);
    QCOMPARE(offset, 3);
  }

  void layoutNumbersKeepDenseClusterOnShortRings() {
    QgsProject project;
    auto* layer = new QgsVectorLayer(
        QStringLiteral("Point?crs=EPSG:5186&field=nm:string(80)"),
        QStringLiteral("문화유적분포지도"), QStringLiteral("memory"));
    QVERIFY(layer->isValid());
    QVERIFY(layer->startEditing());
    const QgsPointXY stack(190040., 550040.);
    for (int i = 0; i < 20; ++i) {
      QgsFeature feature(layer->fields());
      feature.setGeometry(QgsGeometry::fromPointXY(stack));
      feature.setAttribute(QStringLiteral("nm"), QStringLiteral("밀집 %1").arg(i));
      QVERIFY(layer->addFeature(feature));
    }
    QVERIFY(layer->commitChanges());
    HeritageStyle::apply(layer, HeritageDataset::HeritageDistributionMap, QStringLiteral("nm"));
    LayerOps::markReferenceLayer(layer);
    project.addMapLayer(layer, false);
    project.layerTreeRoot()->addGroup(QStringLiteral("참조 지도"))
        ->addGroup(HeritageStyle::layerName(HeritageDataset::HeritageDistributionMap))
        ->addLayer(layer);
    QgsPrintLayout layout(&project);
    auto* map = makeLayoutMap(layout, {layer}, QgsRectangle(186040., 546040., 194040., 554040.));
    HeritageLayoutNumbers numbers;
    numbers.update(map, true);
    QVERIFY2(numbers.error().isEmpty(), qPrintable(numbers.error()));
    QCOMPARE(numbers.entries().size(), 20);
    auto* pinLayer = numbers.numberLayer();
    QVERIFY(pinLayer);
    const double scale = map->scale() > 0. ? map->scale() : 50000.;
    const double minSep = (4.6 / 1000.0) * scale * 0.96 - 1.;
    const double maxSep = (4.6 / 1000.0) * scale * 3.0 + 1.;
    QVector<QgsPointXY> pins;
    int onSite = 0;
    QgsFeature feature;
    auto features = pinLayer->getFeatures();
    while (features.nextFeature(feature)) {
      const QgsPointXY pin = feature.geometry().asPoint();
      pins.append(pin);
      QVERIFY2(pin.distance(stack) <= maxSep, "dense cluster must stay on the near rings");
      if (pin.distance(stack) < 1.) ++onSite;
    }
    for (int i = 0; i < pins.size(); ++i) {
      for (int j = i + 1; j < pins.size(); ++j)
        QVERIFY2(pins.at(i).distance(pins.at(j)) >= minSep, "number circles must not cover each other");
    }
    QVERIFY2(onSite >= 1, "at least one badge stays on the site");
  }

  void layoutNumbersSitOnNearbyDistinctSites() {
    QgsProject project;
    auto* layer = new QgsVectorLayer(
        QStringLiteral("Point?crs=EPSG:5186&field=nm:string(80)"),
        QStringLiteral("문화유적분포지도"), QStringLiteral("memory"));
    QVERIFY(layer->isValid());
    QVERIFY(layer->startEditing());
    const QgsPointXY center(190040., 550040.);
    const QList<QPointF> deltas = {
        {40., 0.}, {40., 40.}, {0., 40.}, {-40., 40.},
        {-40., 0.}, {-40., -40.}, {0., -40.}, {40., -40.},
    };
    for (int i = 0; i < deltas.size(); ++i) {
      QgsFeature feature(layer->fields());
      feature.setGeometry(QgsGeometry::fromPointXY(
          QgsPointXY(center.x() + deltas.at(i).x(), center.y() + deltas.at(i).y())));
      feature.setAttribute(QStringLiteral("nm"), QStringLiteral("근처 %1").arg(i));
      QVERIFY(layer->addFeature(feature));
    }
    QVERIFY(layer->commitChanges());
    HeritageStyle::apply(layer, HeritageDataset::HeritageDistributionMap, QStringLiteral("nm"));
    LayerOps::markReferenceLayer(layer);
    project.addMapLayer(layer, false);
    project.layerTreeRoot()->addGroup(QStringLiteral("참조 지도"))
        ->addGroup(HeritageStyle::layerName(HeritageDataset::HeritageDistributionMap))
        ->addLayer(layer);
    QgsPrintLayout layout(&project);
    // 160 mm map over 4000 m → 1:25000. 40 m is 1.6 mm on paper, inside a
    // 4.6 mm badge, so the circles move apart instead of stacking.
    auto* map = makeLayoutMap(layout, {layer}, QgsRectangle(188040., 548040., 192040., 552040.));
    HeritageLayoutNumbers numbers;
    numbers.update(map, true);
    QVERIFY2(numbers.error().isEmpty(), qPrintable(numbers.error()));
    QCOMPARE(numbers.entries().size(), 8);
    auto* pinLayer = numbers.numberLayer();
    QVERIFY(pinLayer);
    const double scale = map->scale() > 0. ? map->scale() : 25000.;
    const double minSep = (4.6 / 1000.0) * scale * 0.96 - 1.;
    const double maxLeader = (4.6 / 1000.0) * scale * 6.0 + 1.;
    QVector<QgsPointXY> pins;
    QgsFeature feature;
    auto features = pinLayer->getFeatures();
    while (features.nextFeature(feature)) {
      const QgsPointXY pin = feature.geometry().asPoint();
      const QgsPointXY site(feature.attribute(QStringLiteral("ox")).toDouble(),
                            feature.attribute(QStringLiteral("oy")).toDouble());
      QVERIFY2(pin.distance(site) <= maxLeader, "nearby badges stay on a short ring");
      pins.append(pin);
    }
    QCOMPARE(pins.size(), 8);
    for (int i = 0; i < pins.size(); ++i) {
      for (int j = i + 1; j < pins.size(); ++j)
        QVERIFY2(pins.at(i).distance(pins.at(j)) >= minSep, "nearby number circles must clear");
    }
  }

  void layoutNumbersExcludeOutsideAndDisabledCategoriesAndRefreshExtent() {
    QgsProject project;
    auto* layer = addHeritage(project, HeritageDataset::SurfaceSurveyArea,
                              {QStringLiteral("보이는 유적"), QStringLiteral("범위 밖 유적")});
    QgsPrintLayout layout(&project);
    auto* map = makeLayoutMap(layout, {layer}, QgsRectangle(189950., 549875., 190150., 550275.));
    HeritageLayoutNumbers numbers;
    numbers.update(map, true);
    QVERIFY2(numbers.error().isEmpty(), qPrintable(numbers.error()));
    QCOMPARE(numbers.entries().size(), 1);
    QCOMPARE(numbers.entries().first().name, QStringLiteral("보이는 유적"));
    map->setExtent(QgsRectangle(190175., 549975., 190325., 550125.));
    numbers.update(map);
    QVERIFY2(numbers.error().isEmpty(), qPrintable(numbers.error()));
    QCOMPARE(numbers.entries().size(), 1);
    QCOMPARE(numbers.entries().first().name, QStringLiteral("범위 밖 유적"));
    QCOMPARE(numbers.entries().first().number, 1);
  }

  void layoutNumbersTransformExtentToSourceCrs() {
    QgsProject project;
    auto* layer = addHeritage(project, HeritageDataset::ExcavationSurveyArea,
                              {QStringLiteral("범위 안 유적"), QStringLiteral("범위 밖 유적")});
    QgsPrintLayout layout(&project);
    auto* map = makeLayoutMap(layout, {layer}, QgsRectangle(189975., 549975., 190125., 550125.));
    const QgsCoordinateReferenceSystem target(QStringLiteral("EPSG:5187"));
    QgsCoordinateTransform transform(layer->crs(), target, project.transformContext());
    const QgsRectangle transformed = transform.transformBoundingBox(map->extent());
    map->setCrs(target);
    map->setExtent(transformed);
    map->setMapRotation(20.);
    HeritageLayoutNumbers numbers;
    numbers.update(map, true);
    QVERIFY2(numbers.error().isEmpty(), qPrintable(numbers.error()));
    QCOMPARE(numbers.entries().size(), 1);
    QCOMPARE(numbers.entries().first().name, QStringLiteral("범위 안 유적"));
    QCOMPARE(layer->crs().authid(), QStringLiteral("EPSG:5186"));
  }

  void layoutNumbersExcludeRotatedBoundingBoxCorners() {
    QgsProject project;
    auto* layer = addHeritage(project, HeritageDataset::SurfaceSurveyArea,
                              {QStringLiteral("도면 안"), QStringLiteral("회전 도면 밖")});
    layer->startEditing();
    auto features = layer->getFeatures();
    QgsFeature feature;
    while (features.nextFeature(feature)) {
      if (feature.attribute(QStringLiteral("nm")).toString() == QStringLiteral("회전 도면 밖")) {
        QgsGeometry outside = QgsGeometry::fromWkt(QStringLiteral(
            "POLYGON((189830 549830,189850 549830,189850 549850,189830 549850,189830 549830))"));
        QVERIFY(layer->changeGeometry(feature.id(), outside));
      }
    }
    QVERIFY(layer->commitChanges());
    QgsPrintLayout layout(&project);
    auto* map = makeLayoutMap(layout, {layer}, QgsRectangle(189900., 549900., 190300., 550300.));
    map->setMapRotation(45.);
    HeritageLayoutNumbers numbers;
    numbers.update(map, true);
    QVERIFY2(numbers.error().isEmpty(), qPrintable(numbers.error()));
    QCOMPARE(numbers.entries().size(), 1);
    QCOMPARE(numbers.entries().first().name, QStringLiteral("도면 안"));
  }

  void layoutNumbersAlsoHandleSingleSymbolSources() {
    QgsProject project;
    auto* layer = addHeritage(project, HeritageDataset::HeritageDistributionMap,
                              {QStringLiteral("첫 유적"), QStringLiteral("둘째 유적"),
                               QStringLiteral("첫 유적")});
    QVERIFY(dynamic_cast<QgsSingleSymbolRenderer*>(layer->renderer()));
    QgsMapLayerStyle original;
    original.readFromLayer(layer);
    QgsPrintLayout layout(&project);
    auto* map = makeLayoutMap(layout, {layer}, QgsRectangle(189950., 549750., 190550., 550350.));
    HeritageLayoutNumbers numbers;
    numbers.update(map, true);
    QVERIFY2(numbers.error().isEmpty(), qPrintable(numbers.error()));
    QCOMPARE(numbers.entries().size(), 2);
    QSet<QString> names;
    QSet<int> assigned;
    for (const auto& entry : numbers.entries()) {
      names.insert(entry.name);
      assigned.insert(entry.number);
    }
    QCOMPARE(names, (QSet<QString>{QStringLiteral("첫 유적"), QStringLiteral("둘째 유적")}));
    QCOMPARE(assigned, (QSet<int>{1, 2}));
    QVERIFY(numbers.overrides().contains(layer->id()));
    QgsMapLayerStyle after;
    after.readFromLayer(layer);
    QCOMPARE(after.xmlData(), original.xmlData());
    QVERIFY(dynamic_cast<QgsSingleSymbolRenderer*>(layer->renderer()));
  }

  void layoutNumberCirclesAndLegendMatchWithoutChangingSourceStyle() {
    QgsProject project;
    auto* layer = addHeritage(project, HeritageDataset::BuriedHeritageArea,
                              {QStringLiteral("긴 유적명"), QStringLiteral("긴 유적명")});
    QVERIFY(LayerOps::applyNameAttributeLabels(layer, QStringLiteral("nm"), 5., false));
    QgsMapLayerStyle original;
    original.readFromLayer(layer);
    const QString source = layer->source();
    QgsPrintLayout layout(&project);
    auto* map = makeLayoutMap(layout, {layer}, QgsRectangle(189950., 549800., 190450., 550300.));
    HeritageLayoutNumbers numbers;
    numbers.update(map, true);
    QVERIFY2(numbers.error().isEmpty(), qPrintable(numbers.error()));
    QCOMPARE(numbers.entries().size(), 1);
    QVERIFY(numbers.overrides().contains(layer->id()));
    const auto entry = numbers.entries().first();

    std::unique_ptr<QgsVectorLayer> drawing(layer->clone());
    QgsMapLayerStyle(numbers.overrides().value(layer->id())).writeToLayer(drawing.get());
    QVERIFY(!drawing->labelsEnabled());
    auto* pins = numbers.numberLayer();
    QVERIFY(pins);
    QCOMPARE(int(pins->featureCount()), 1);
    QVERIFY(!pins->labelsEnabled());
    auto* pinSymbol = dynamic_cast<QgsSingleSymbolRenderer*>(pins->renderer());
    QVERIFY(pinSymbol);
    auto* glyph = dynamic_cast<QgsFontMarkerSymbolLayer*>(pinSymbol->symbol()->symbolLayer(1));
    QVERIFY(glyph);
    auto* leader = dynamic_cast<QgsGeometryGeneratorSymbolLayer*>(pinSymbol->symbol()->symbolLayer(2));
    QVERIFY(leader);
    QCOMPARE(leader->symbolType(), Qgis::SymbolType::Line);
    QgsFeature pin;
    QVERIFY(pins->getFeatures().nextFeature(pin));
    QCOMPARE(pin.attribute(QStringLiteral("num")).toInt(), entry.number);
    QCOMPARE(pin.attribute(QStringLiteral("fill")).toString(), entry.color.name(QColor::HexArgb));
    QVERIFY(pin.attribute(QStringLiteral("size")).toDouble() >= 2.3);
    QCOMPARE(entry.featureIds.size(), 2);
    QgsLayoutItemMap* raised = nullptr;
    for (QGraphicsItem* item : layout.items()) {
      auto* candidate = dynamic_cast<QgsLayoutItemMap*>(item);
      if (candidate && candidate->id() == QLatin1String("ka_map_numbers")) raised = candidate;
    }
    QVERIFY(raised);
    QVERIFY(raised->zValue() > map->zValue());
    QCOMPARE(raised->layers(), QList<QgsMapLayer*>{pins});
    QDomDocument baseStyle;
    QVERIFY(baseStyle.setContent(map->layerStyleOverrides().value(layer->id())));
    QCOMPARE(baseStyle.documentElement().attribute(QStringLiteral("labelsEnabled")), QStringLiteral("0"));

    auto* legend = new QgsLayoutItemLegend(&layout);
    layout.addLayoutItem(legend);
    legend->setLinkedMap(map);
    legend->setSyncMode(Qgis::LegendSyncMode::Manual);
    legend->resetManualLayers(Qgis::LegendSyncMode::VisibleLayers);
    numbers.applyLegend(legend);
    auto* treeLayer = legend->model()->rootGroup()->findLayer(layer->id());
    QVERIFY(treeLayer);
    for (int pass = 0; pass < 2; ++pass) {
      // QGIS regenerates nodes after filtering/styles change. Number glyphs
      // must survive that regeneration, not only the first preview.
      if (pass == 1) legend->model()->refreshLayerLegend(treeLayer);
      int matched = 0;
      for (auto* node : legend->model()->layerLegendNodes(treeLayer)) {
        auto* symbolNode = dynamic_cast<QgsSymbolLegendNode*>(node);
        if (!symbolNode || !symbolNode->customSymbol()) continue;
        QgsSymbol* marker = symbolNode->customSymbol();
        QString number;
        for (int i = 0; i < marker->symbolLayerCount(); ++i) {
          if (auto* glyph = dynamic_cast<QgsFontMarkerSymbolLayer*>(marker->symbolLayer(i)))
            number = glyph->character();
        }
        if (number.isEmpty()) continue;
        QCOMPARE(number, QString::number(entry.number));
        QVERIFY(symbolNode->data(Qt::DisplayRole).toString().contains(entry.name));
        QCOMPARE(marker->type(), Qgis::SymbolType::Marker);
        QCOMPARE(marker->color(), entry.color);
        ++matched;
      }
      QCOMPARE(matched, 1);
    }
    QgsMapLayerStyle after;
    after.readFromLayer(layer);
    QCOMPARE(after.xmlData(), original.xmlData());
    QCOMPARE(layer->source(), source);
    QCOMPARE(layer->featureCount(), 2LL);
  }

  void layoutNumberedPdfFixture() {
    const QString qaDir = qEnvironmentVariable("KA_HGIS_QA_DIR");
    if (qaDir.isEmpty()) QSKIP("Set KA_HGIS_QA_DIR to export the synthetic numbered drawing.");
    QVERIFY(QDir().mkpath(qaDir));
    QgsProject project;
    auto* designated = addHeritage(project, HeritageDataset::DesignatedHeritage,
                                   {QStringLiteral("합성 가람 유적"), QStringLiteral("합성 나루 유적")});
    auto* surface = addHeritage(project, HeritageDataset::SurfaceSurveyArea,
                                {QStringLiteral("합성 다솔 지표조사")});
    surface->startEditing();
    auto features = surface->getFeatures();
    QgsFeature feature;
    while (features.nextFeature(feature)) {
      QgsGeometry shifted = feature.geometry();
      shifted.translate(100., 300.);
      QVERIFY(surface->changeGeometry(feature.id(), shifted));
    }
    QVERIFY(surface->commitChanges());
    QVERIFY(LayerOps::applyNameAttributeLabels(designated, QStringLiteral("nm"), 8., false));
    QVERIFY(LayerOps::applyNameAttributeLabels(surface, QStringLiteral("nm"), 8., false));
    QgsPrintLayout layout(&project);
    auto* map = makeLayoutMap(layout, {designated, surface},
                              QgsRectangle(189900., 549900., 190700., 550700.));
    layout.pageCollection()->page(0)->setPageSize(QStringLiteral("A4"), QgsLayoutItemPage::Portrait);
    map->attemptSetSceneRect(QRectF(15., 15., 180., 180.));
    map->setFrameEnabled(true);
    HeritageLayoutNumbers numbers;
    numbers.update(map, true);
    QVERIFY2(numbers.error().isEmpty(), qPrintable(numbers.error()));
    QCOMPARE(numbers.entries().size(), 3);
    auto* legend = new QgsLayoutItemLegend(&layout);
    layout.addLayoutItem(legend);
    legend->setTitle(QStringLiteral("주변유적 범례"));
    legend->setLinkedMap(map);
    legend->setSyncMode(Qgis::LegendSyncMode::Manual);
    legend->resetManualLayers(Qgis::LegendSyncMode::VisibleLayers);
    numbers.applyLegend(legend);
    legend->attemptMove(QgsLayoutPoint(15., 205.));
    legend->adjustBoxSize();
    const QString pdfPath = QDir(qaDir).filePath(QStringLiteral("numbered-heritage-synthetic.pdf"));
    QgsLayoutExporter exporter(&layout);
    QgsLayoutExporter::PdfExportSettings settings;
    settings.dpi = 150.;
    QCOMPARE(exporter.exportToPdf(pdfPath, settings), QgsLayoutExporter::Success);
    QVERIFY(QFileInfo(pdfPath).size() > 1000);
    const QImage preview = exporter.renderPageToImage(0, QSize(), 150.);
    QVERIFY(!preview.isNull());
    QVERIFY(preview.save(QDir(qaDir).filePath(QStringLiteral("numbered-heritage-synthetic.png"))));
  }

  void layoutSixDatasetPalettePdfFixture() {
    const QString qaDir = qEnvironmentVariable("KA_HGIS_QA_DIR");
    if (qaDir.isEmpty()) QSKIP("Set KA_HGIS_QA_DIR to export the six-dataset palette drawing.");
    QVERIFY(QDir().mkpath(qaDir));
    QgsProject project;
    QList<QgsMapLayer*> layers;
    QMap<QString, QColor> expected;
    QMap<QString, QString> originalStyles;
    const auto datasets = HeritageStyle::allDatasets();
    for (int i = 0; i < datasets.size(); ++i) {
      const auto dataset = datasets.at(i);
      auto* layer = addHeritage(project, dataset,
          {QStringLiteral("합성 %1 유적").arg(HeritageStyle::layerName(dataset))});
      QVERIFY(layer->startEditing());
      auto features = layer->getFeatures();
      QgsFeature feature;
      QVERIFY(features.nextFeature(feature));
      QgsGeometry geometry = feature.geometry();
      geometry.translate((i % 3) * 300., (1 - i / 3) * 400.);
      QVERIFY(layer->changeGeometry(feature.id(), geometry));
      QVERIFY(layer->commitChanges());
      // Include old saved colors in this visible proof of the layout upgrade.
      if (dataset == HeritageDataset::SurfaceSurveyArea || dataset == HeritageDataset::ExcavationSurveyArea) {
        auto* renderer = dynamic_cast<QgsSingleSymbolRenderer*>(layer->renderer());
        QVERIFY(renderer);
        const QColor legacy(dataset == HeritageDataset::SurfaceSurveyArea
                                ? QStringLiteral("#16a085") : QStringLiteral("#a0522d"));
        auto* symbol = renderer->symbol()->clone();
        symbol->setColor(legacy);
        auto* fill = dynamic_cast<QgsSimpleFillSymbolLayer*>(symbol->symbolLayer(0));
        QVERIFY(fill);
        fill->setStrokeColor(legacy);
        renderer->setSymbol(symbol);
      }
      QVERIFY(LayerOps::applyNameAttributeLabels(layer, QStringLiteral("nm"), 8., false));
      QgsMapLayerStyle style;
      style.readFromLayer(layer);
      originalStyles.insert(layer->id(), style.xmlData());
      expected.insert(layer->id(), HeritageStyle::color(dataset));
      layers.append(layer);
    }
    QgsPrintLayout layout(&project);
    auto* map = makeLayoutMap(layout, layers, QgsRectangle(189900., 549900., 190900., 550900.));
    layout.pageCollection()->page(0)->setPageSize(QStringLiteral("A4"), QgsLayoutItemPage::Portrait);
    map->attemptSetSceneRect(QRectF(15., 15., 180., 180.));
    map->setFrameEnabled(true);
    HeritageLayoutNumbers numbers;
    QVERIFY(numbers.update(map));
    QCOMPARE(numbers.entries().size(), 6);
    QSet<QString> distinctColors;
    for (const auto& entry : numbers.entries()) {
      QCOMPARE(entry.color, expected.value(entry.layerId));
      QCOMPARE(entry.number, 1);
      distinctColors.insert(entry.color.name());
      auto* source = qobject_cast<QgsVectorLayer*>(project.mapLayer(entry.layerId));
      QVERIFY(source);
      auto* pins = numbers.numberLayer();
      QVERIFY(pins);
      bool foundPin = false;
      QgsFeature pinFeature;
      auto pinRows = pins->getFeatures();
      while (pinRows.nextFeature(pinFeature)) {
        if (pinFeature.attribute(QStringLiteral("layer")).toString() != entry.layerId) continue;
        QCOMPARE(pinFeature.attribute(QStringLiteral("fill")).toString(), entry.color.name(QColor::HexArgb));
        foundPin = true;
      }
      QVERIFY(foundPin);
      std::unique_ptr<QgsVectorLayer> drawing(source->clone());
      QgsMapLayerStyle(numbers.overrides().value(entry.layerId)).writeToLayer(drawing.get());
      QVERIFY(!drawing->labelsEnabled());
      auto* renderer = dynamic_cast<QgsCategorizedSymbolRenderer*>(drawing->renderer());
      QVERIFY(renderer && !renderer->categories().isEmpty());
      const auto* fill = dynamic_cast<QgsSimpleFillSymbolLayer*>(renderer->categories().first().symbol()->symbolLayer(0));
      QVERIFY(fill);
      QCOMPARE(fill->strokeColor(), entry.color);
    }
    QCOMPARE(distinctColors.size(), 6);
    auto* legend = new QgsLayoutItemLegend(&layout);
    layout.addLayoutItem(legend);
    legend->setTitle(QStringLiteral("여섯 자료 종류 · 색과 번호"));
    legend->setLinkedMap(map);
    legend->setSyncMode(Qgis::LegendSyncMode::Manual);
    legend->resetManualLayers(Qgis::LegendSyncMode::VisibleLayers);
    numbers.applyLegend(legend);
    legend->attemptMove(QgsLayoutPoint(15., 205.));
    legend->attemptResize(QgsLayoutSize(180., 65.));
    LayoutService::flowSheetLegend(legend);
    int legendEntries = 0;
    for (auto* treeLayer : legend->model()->rootGroup()->findLayers()) {
      const auto nodes = legend->model()->layerLegendNodes(treeLayer);
      QCOMPARE(nodes.size(), 1);
      auto* node = dynamic_cast<QgsSymbolLegendNode*>(nodes.first());
      QVERIFY(node && node->customSymbol());
      QCOMPARE(node->customSymbol()->color(), expected.value(treeLayer->layerId()));
      ++legendEntries;
    }
    QCOMPARE(legendEntries, 6);
    const QString base = QDir(qaDir).filePath(QStringLiteral("six-dataset-palette"));
    QgsLayoutExporter exporter(&layout);
    QgsLayoutExporter::PdfExportSettings settings;
    settings.dpi = 150.;
    QCOMPARE(exporter.exportToPdf(base + QStringLiteral(".pdf"), settings), QgsLayoutExporter::Success);
    QVERIFY(exporter.renderPageToImage(0, QSize(), 150.).save(base + QStringLiteral(".png")));
    for (auto* layer : layers) {
      QgsMapLayerStyle after;
      after.readFromLayer(layer);
      QCOMPARE(after.xmlData(), originalStyles.value(layer->id()));
    }
  }

  void denseDatasetNumberCirclesDoNotOverlapInExport() {
    QgsProject project;
    QList<QgsMapLayer*> layers;
    QMap<QString, QString> originalStyles;
    const auto datasets = HeritageStyle::allDatasets();
    for (int i = 0; i < datasets.size(); ++i) {
      auto* layer = addHeritage(project, datasets.at(i),
          {QStringLiteral("밀집 %1 가유적").arg(i), QStringLiteral("밀집 %1 나유적").arg(i)});
      QVERIFY(layer->startEditing());
      auto features = layer->getFeatures();
      QgsFeature feature;
      int j = 0;
      while (features.nextFeature(feature)) {
        const double x = 190000. + i * .3 + j * 3.;
        const double y = 550000. + i * .2;
        QgsGeometry geometry = QgsGeometry::fromRect(QgsRectangle(x, y, x + 5., y + 5.));
        QVERIFY(layer->changeGeometry(feature.id(), geometry));
        ++j;
      }
      QCOMPARE(j, 2);
      QVERIFY(layer->commitChanges());
      QVERIFY(LayerOps::applyNameAttributeLabels(layer, QStringLiteral("nm"), 8., false));
      QgsMapLayerStyle sourceStyle;
      sourceStyle.readFromLayer(layer);
      originalStyles.insert(layer->id(), sourceStyle.xmlData());
      layers.append(layer);
    }
    QgsPrintLayout layout(&project);
    auto* map = makeLayoutMap(layout, layers, QgsRectangle(189915., 549913., 190095., 550093.));
    layout.pageCollection()->page(0)->setPageSize(QStringLiteral("A4"), QgsLayoutItemPage::Portrait);
    map->attemptSetSceneRect(QRectF(15., 15., 180., 180.));
    map->setScale(1000.);
    map->setFrameEnabled(true);
    HeritageLayoutNumbers numbers;
    QVERIFY(numbers.update(map));
    QCOMPARE(numbers.entries().size(), 12);
    QMap<QString, double> badgeDiameterMm;
    auto* pins = numbers.numberLayer();
    QVERIFY(pins);
    QgsFeature pinFeature;
    auto pinRows = pins->getFeatures();
    while (pinRows.nextFeature(pinFeature)) {
      const QString layerId = pinFeature.attribute(QStringLiteral("layer")).toString();
      const double size = pinFeature.attribute(QStringLiteral("size")).toDouble();
      badgeDiameterMm.insert(layerId, size);
      QVERIFY(size >= 2.3);
      QVERIFY(size < 4.6);
    }
    QCOMPARE(badgeDiameterMm.size(), layers.size());
    auto* legend = new QgsLayoutItemLegend(&layout);
    layout.addLayoutItem(legend);
    legend->setTitle(QStringLiteral("밀집 유적 · 페이지 안 번호"));
    legend->setLinkedMap(map);
    legend->setSyncMode(Qgis::LegendSyncMode::Manual);
    legend->resetManualLayers(Qgis::LegendSyncMode::VisibleLayers);
    numbers.applyLegend(legend);
    legend->attemptMove(QgsLayoutPoint(15., 205.));
    legend->attemptResize(QgsLayoutSize(180., 65.));
    LayoutService::flowSheetLegend(legend);
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const QString qaDir = qEnvironmentVariable("KA_HGIS_QA_DIR");
    const QString directory = qaDir.isEmpty() ? temporary.path() : qaDir;
    QVERIFY(QDir().mkpath(directory));
    const QString base = QDir(directory).filePath(QStringLiteral("dense-number-labels"));
    QString exportError;
    QVERIFY2(numbers.exportPdf(map, legend, base + QStringLiteral(".pdf"), 150., &exportError), qPrintable(exportError));
    QVERIFY(QFileInfo(base + QStringLiteral(".pdf")).size() > 0);
    const QSet<QString> deliveredKeys = legendNumberKeys(legend);
    QCOMPARE(deliveredKeys, numbers.visibleKeys());
    QCOMPARE(deliveredKeys, numbers.legendKeys());
    const QString sequenceError = consecutiveLegendError(legend, numbers.entries());
    QVERIFY2(sequenceError.isEmpty(), qPrintable(sequenceError));
    QgsLayoutExporter exporter(&layout);
    QgsLayoutExporter::PdfExportSettings settings;
    settings.dpi = 150.;
    QCOMPARE(exporter.exportToPdf(temporary.filePath(QStringLiteral("independent-dense-check.pdf")), settings), QgsLayoutExporter::Success);
    QCOMPARE(deliveredKeys, numberedEntryKeys(numbers.entries()));
    QVERIFY(!deliveredKeys.isEmpty());
    QCOMPARE(deliveredKeys.size(), numbers.entries().size());
    int placed = 0;
    QgsFeature placedPin;
    auto placedRows = pins->getFeatures();
    while (placedRows.nextFeature(placedPin)) {
      if (!badgeDiameterMm.contains(placedPin.attribute(QStringLiteral("layer")).toString())) continue;
      const int number = placedPin.attribute(QStringLiteral("num")).toInt();
      QVERIFY(number >= 1 && number <= 2);
      ++placed;
    }
    QVERIFY2(placed > 0, "On-page number labels must draw");
    qInfo() << "DENSE_NUMBER_EXPORT placed=" << placed << "of12";
    if (!qaDir.isEmpty())
      QVERIFY(exporter.renderPageToImage(0, QSize(), 150.).save(base + QStringLiteral(".png")));
    for (auto* source : layers) {
      QgsMapLayerStyle after;
      after.readFromLayer(source);
      QCOMPARE(after.xmlData(), originalStyles.value(source->id()));
    }
  }

  void widerScaleAddsMapExtentSitesToLegend() {
    QgsProject project;
    QStringList nearNames;
    for (int i = 0; i < 8; ++i) nearNames.append(QStringLiteral("근거리 %1").arg(i));
    auto* layer = addHeritage(project, HeritageDataset::DesignatedHeritage, nearNames);
    QVERIFY(layer->startEditing());
    auto features = layer->getFeatures();
    QgsFeature feature;
    int i = 0;
    while (features.nextFeature(feature)) {
      const double x = 190000. + i * 40.;
      QgsGeometry nearGeom = QgsGeometry::fromRect(QgsRectangle(x, 550000., x + 20., 550020.));
      QVERIFY(layer->changeGeometry(feature.id(), nearGeom));
      ++i;
    }
    QCOMPARE(i, 8);
    for (int j = 0; j < 6; ++j) {
      QgsFeature extra(layer->fields());
      extra.setAttribute(QStringLiteral("nm"), QStringLiteral("원거리 %1").arg(j));
      const double x = 191800. + j * 40.;
      extra.setGeometry(QgsGeometry::fromRect(QgsRectangle(x, 550000., x + 20., 550020.)));
      QVERIFY(layer->addFeature(extra));
    }
    QVERIFY(layer->commitChanges());
    HeritageStyle::apply(layer, HeritageDataset::DesignatedHeritage, QStringLiteral("nm"));

    QgsPrintLayout layout(&project);
    auto* map = makeLayoutMap(layout, {layer}, QgsRectangle(189900., 549900., 190700., 550700.));
    layout.pageCollection()->page(0)->setPageSize(QStringLiteral("A4"), QgsLayoutItemPage::Portrait);
    map->attemptSetSceneRect(QRectF(20., 20., 160., 160.));
    map->setScale(5000., true);
    HeritageLayoutNumbers numbers;
    QVERIFY(numbers.update(map));
    const int tightCandidates = numbers.entries().size();
    QVERIFY2(tightCandidates >= 6 && tightCandidates <= 8,
             qPrintable(QStringLiteral("1:5000 candidates=%1").arg(tightCandidates)));
    auto* legend = new QgsLayoutItemLegend(&layout);
    layout.addLayoutItem(legend);
    legend->setLinkedMap(map);
    legend->setResizeToContents(false);
    legend->attemptSetSceneRect(QRectF(15., 200., 180., 70.));
    LayoutService::tuneSheetLegend(legend);
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    QString error;
    QVERIFY2(numbers.exportPdf(map, legend, temporary.filePath(QStringLiteral("scale-5000.pdf")), 150., &error),
             qPrintable(error));
    const auto tightKeys = legendNumberKeys(legend);
    QCOMPARE(tightKeys, numbers.visibleKeys());
    QCOMPARE(tightKeys, numbers.legendKeys());
    QgsLayoutExporter exporter(&layout);
    QgsLayoutExporter::PdfExportSettings settings;
    settings.dpi = 150.;
    QCOMPARE(exporter.exportToPdf(temporary.filePath(QStringLiteral("scale-5000-check.pdf")), settings),
             QgsLayoutExporter::Success);
    QCOMPARE(tightKeys, numberedEntryKeys(numbers.entries()));

    map->setScale(25000., true);
    QVERIFY(numbers.update(map));
    const int wideCandidates = numbers.entries().size();
    QVERIFY2(wideCandidates > tightCandidates,
             qPrintable(QStringLiteral("1:5000 candidates=%1 1:25000=%2").arg(tightCandidates).arg(wideCandidates)));
    QVERIFY(wideCandidates >= 12);
    QVERIFY2(numbers.exportPdf(map, legend, temporary.filePath(QStringLiteral("scale-25000.pdf")), 150., &error),
             qPrintable(error));
    const auto wideKeys = legendNumberKeys(legend);
    QCOMPARE(wideKeys, numbers.visibleKeys());
    QCOMPARE(wideKeys, numbers.legendKeys());
    QCOMPARE(exporter.exportToPdf(temporary.filePath(QStringLiteral("scale-25000-check.pdf")), settings),
             QgsLayoutExporter::Success);
    QCOMPARE(wideKeys.size(), wideCandidates);
    QCOMPARE(wideKeys, numberedEntryKeys(numbers.entries()));
    QCOMPARE(wideKeys, numberedEntryKeys(numbers.entries()));
    QVERIFY2(consecutiveLegendError(legend, numbers.entries()).isEmpty(),
             "On-page numbers must stay consecutive and match the legend");
    QSet<int> mapNumbers;
    auto* widePins = numbers.numberLayer();
    QVERIFY(widePins);
    QgsFeature widePin;
    auto wideRows = widePins->getFeatures();
    while (wideRows.nextFeature(widePin)) {
      if (widePin.attribute(QStringLiteral("layer")).toString() != layer->id()) continue;
      mapNumbers.insert(widePin.attribute(QStringLiteral("num")).toInt());
    }
    QCOMPARE(mapNumbers.size(), wideKeys.size());
    for (int n = 1; n <= mapNumbers.size(); ++n)
      QVERIFY2(mapNumbers.contains(n),
               qPrintable(QStringLiteral("Map kept a hole: missing %1").arg(n)));
  }

  void savedStudioSheetGenericPdfUsesActuallyPlacedNumberLegend() {
    QgsProject project;
    QList<QgsMapLayer*> layers;
    QMap<QString, QString> originalStyles;
    const auto datasets = HeritageStyle::allDatasets();
    for (int i = 0; i < datasets.size(); ++i) {
      auto* layer = addHeritage(project, datasets.at(i),
          {QStringLiteral("저장 도면 %1 가유적").arg(i), QStringLiteral("저장 도면 %1 나유적").arg(i)});
      QVERIFY(layer->startEditing());
      auto features = layer->getFeatures();
      QgsFeature feature;
      int j = 0;
      while (features.nextFeature(feature)) {
        const double x = 190000. + i * .3 + j * 3.;
        const double y = 550000. + i * .2;
        QgsGeometry geometry = QgsGeometry::fromRect(QgsRectangle(x, y, x + 5., y + 5.));
        QVERIFY(layer->changeGeometry(feature.id(), geometry));
        ++j;
      }
      QCOMPARE(j, 2);
      QVERIFY(layer->commitChanges());
      QgsMapLayerStyle style;
      style.readFromLayer(layer);
      originalStyles.insert(layer->id(), style.xmlData());
      layers.append(layer);
    }
    auto layoutOwner = std::make_unique<QgsPrintLayout>(&project);
    auto* layout = layoutOwner.get();
    auto* map = makeLayoutMap(*layout, layers, QgsRectangle(189915., 549913., 190095., 550093.));
    layout->setName(QStringLiteral("user_sheet"));
    layout->pageCollection()->page(0)->setPageSize(QStringLiteral("A4"), QgsLayoutItemPage::Portrait);
    map->setId(QStringLiteral("ka_map"));
    map->attemptSetSceneRect(QRectF(15., 15., 180., 180.));
    map->setScale(1000.);
    auto* legend = new QgsLayoutItemLegend(layout);
    layout->addLayoutItem(legend);
    legend->setId(QStringLiteral("ka_legend"));
    legend->setLinkedMap(map);
    legend->setResizeToContents(false);
    legend->attemptSetSceneRect(QRectF(15., 205., 180., 65.));
    LayoutService::tuneSheetLegend(legend);
    HeritageLayoutNumbers candidateNumbers;
    QVERIFY(candidateNumbers.update(map));
    QCOMPARE(candidateNumbers.entries().size(), 12);
    candidateNumbers.applyLegend(legend);
    QCOMPARE(legendNumberKeys(legend).size(), 12);
    QVERIFY(project.layoutManager()->addLayout(layout));
    layoutOwner.release();  // QgsLayoutManager owns the persisted sheet.

    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const QString qa = qEnvironmentVariable("KA_HGIS_QA_DIR");
    const QString directory = qa.isEmpty() ? temporary.path() : qa;
    QVERIFY(QDir().mkpath(directory));
    const QString path = QDir(directory).filePath(QStringLiteral("saved-studio-visible-number-legend.pdf"));
    QString error;
    const double originalDpi = layout->renderContext().dpi();
    QVERIFY(!HeritageLayoutNumbers::forMap(map));
    const QString exported = LayoutService::exportLayoutPdf(&project, layout->name(), path, &error);
    QVERIFY2(!exported.isEmpty(), qPrintable(error));
    QCOMPARE(exported, path);
    QVERIFY(QFileInfo(path).size() > 0);
    QVERIFY(!HeritageLayoutNumbers::forMap(map));
    QCOMPARE(layout->renderContext().dpi(), originalDpi);
    const auto deliveredKeys = legendNumberKeys(legend);
    QVERIFY(!deliveredKeys.isEmpty());
    QVERIFY(deliveredKeys.size() <= 12);
    QgsLayoutExporter check(layout);
    QgsLayoutExporter::PdfExportSettings settings;
    settings.dpi = 300.;
    settings.forceVectorOutput = true;
    QCOMPARE(check.exportToPdf(temporary.filePath(QStringLiteral("saved-studio-independent.pdf")), settings), QgsLayoutExporter::Success);
    const auto* results = sheetNumberResults(check, map);
    QVERIFY(results);
    QCOMPARE(deliveredKeys, numberedEntryKeys(candidateNumbers.entries()));
    const QString sequenceError = consecutiveLegendError(legend, candidateNumbers.entries());
    QVERIFY2(sequenceError.isEmpty(), qPrintable(sequenceError));
    {
      HeritageLayoutNumbers live;
      QVERIFY(live.update(map));
      live.followRenderedLabels(map);
      QCOMPARE(HeritageLayoutNumbers::forMap(map), &live);
      const QString livePath = temporary.filePath(QStringLiteral("live-studio-generic-export.pdf"));
      QVERIFY2(!LayoutService::exportLayoutPdf(&project, layout->name(), livePath, &error).isEmpty(), qPrintable(error));
      QCOMPARE(HeritageLayoutNumbers::forMap(map), &live);
      QCOMPARE(legendNumberKeys(legend), live.legendKeys());
      QVERIFY(!live.legendKeys().isEmpty());
      QCOMPARE(check.exportToPdf(temporary.filePath(QStringLiteral("live-studio-independent.pdf")), settings), QgsLayoutExporter::Success);
      const auto* liveResults = sheetNumberResults(check, map);
      QVERIFY(liveResults);
      QCOMPARE(live.visibleKeys(), numberedEntryKeys(live.entries()));
      QCOMPARE(legendNumberKeys(legend), live.legendKeys());
    }
    QVERIFY(!HeritageLayoutNumbers::forMap(map));
    for (auto* layer : layers) {
      QgsMapLayerStyle after;
      after.readFromLayer(layer);
      QCOMPARE(after.xmlData(), originalStyles.value(layer->id()));
    }
    if (!qa.isEmpty())
      QVERIFY(check.renderPageToImage(0, QSize(), 150.).save(QDir(directory).filePath(QStringLiteral("saved-studio-visible-number-legend.png"))));
  }

  void layoutNumberVisibilityPreservesSourceLabelsAndInvalidatesCache() {
    QgsProject project;
    auto* layer = addHeritage(project, HeritageDataset::DesignatedHeritage,
                              {QStringLiteral("첫 유적"), QStringLiteral("둘째 유적")});
    QgsPalLayerSettings sourceLabels;
    sourceLabels.fieldName = QStringLiteral("nm");
    layer->setLabeling(new QgsVectorLayerSimpleLabeling(sourceLabels));
    layer->setLabelsEnabled(true);
    QgsMapLayerStyle original;
    original.readFromLayer(layer);
    QgsPrintLayout layout(&project);
    auto* map = makeLayoutMap(layout, {layer}, QgsRectangle(189950., 549950., 190550., 550550.));
    HeritageLayoutNumbers numbers;
    QVERIFY(numbers.update(map));
    QCOMPARE(numbers.entries().size(), 2);
    const auto initial = numbers.revision();

    // No repaint signal: a persisted preference alone must invalidate the cache.
    layer->setCustomProperty(QStringLiteral("ka_hgis/layout_numbers_visible"), false);
    QVERIFY(numbers.update(map));
    QVERIFY(numbers.revision() > initial);
    QVERIFY(numbers.entries().isEmpty());
    QVERIFY(numbers.overrides().contains(layer->id()));
    std::unique_ptr<QgsVectorLayer> hidden(layer->clone());
    QgsMapLayerStyle(numbers.overrides().value(layer->id())).writeToLayer(hidden.get());
    QVERIFY(!hidden->labelsEnabled());
    QCOMPARE(hidden->featureCount(), layer->featureCount());
    auto* sourceRenderer = dynamic_cast<QgsSingleSymbolRenderer*>(layer->renderer());
    auto* hiddenRenderer = dynamic_cast<QgsSingleSymbolRenderer*>(hidden->renderer());
    QVERIFY(sourceRenderer);
    QVERIFY(hiddenRenderer);
    QCOMPARE(hiddenRenderer->symbol()->color(), sourceRenderer->symbol()->color());
    QVERIFY(layer->labelsEnabled());
    QCOMPARE(layer->labeling()->settings().fieldName, QStringLiteral("nm"));
    const auto hiddenRevision = numbers.revision();
    QVERIFY(numbers.update(map));
    QCOMPARE(numbers.revision(), hiddenRevision);

    layer->setCustomProperty(QStringLiteral("ka_hgis/layout_numbers_visible"), true);
    layer->triggerRepaint();
    QVERIFY(numbers.update(map));
    QVERIFY(numbers.revision() > hiddenRevision);
    QCOMPARE(numbers.entries().size(), 2);
    QCOMPARE(numbers.entries().first().number, 1);
    QCOMPARE(numbers.entries().last().number, 2);
    QgsMapLayerStyle(numbers.overrides().value(layer->id())).writeToLayer(hidden.get());
    QVERIFY(!hidden->labelsEnabled());
    QCOMPARE(int(numbers.numberLayer()->featureCount()), 2);
    // Restore the originally absent preference before comparing all style XML.
    layer->removeCustomProperty(QStringLiteral("ka_hgis/layout_numbers_visible"));
    QgsMapLayerStyle after;
    after.readFromLayer(layer);
    QCOMPARE(after.xmlData(), original.xmlData());

    // A hidden layer removed from the drawing must not leave our override behind.
    layer->setCustomProperty(QStringLiteral("ka_hgis/layout_numbers_visible"), false);
    QVERIFY(numbers.update(map));
    map->setLayers({});
    QVERIFY(numbers.update(map));
    QVERIFY(!numbers.overrides().contains(layer->id()));
  }

  void layoutNumberVisibilityKeepsMixedLegendAndPdfConsecutive() {
    QgsProject project;
    auto* first = addHeritage(project, HeritageDataset::DesignatedHeritage,
                              {QStringLiteral("첫 분류 유적 가"), QStringLiteral("첫 분류 유적 나")});
    auto* second = addHeritage(project, HeritageDataset::DesignatedHeritage,
                               {QStringLiteral("같은 분류 유적 다"), QStringLiteral("같은 분류 유적 라")});
    auto* third = addHeritage(project, HeritageDataset::SurfaceSurveyArea,
                              {QStringLiteral("다른 분류 유적 마"), QStringLiteral("다른 분류 유적 바")});
    const QList<QgsVectorLayer*> sources{first, second, third};
    for (int i = 0; i < sources.size(); ++i) {
      auto* source = sources[i];
      QVERIFY(source->startEditing());
      auto features = source->getFeatures();
      QgsFeature feature;
      while (features.nextFeature(feature)) {
        auto geometry = feature.geometry();
        geometry.translate(0., i * 300.);
        QVERIFY(source->changeGeometry(feature.id(), geometry));
      }
      QVERIFY(source->commitChanges());
      QgsPalLayerSettings labels;
      labels.fieldName = QStringLiteral("nm");
      source->setLabeling(new QgsVectorLayerSimpleLabeling(labels));
      source->setLabelsEnabled(true);
    }
    QgsPrintLayout layout(&project);
    auto* map = makeLayoutMap(layout, {first, second, third}, QgsRectangle(189900., 549900., 190800., 550800.));
    HeritageLayoutNumbers numbers;
    QVERIFY(numbers.update(map));
    QCOMPARE(numbers.entries().size(), 6);
    auto* legend = new QgsLayoutItemLegend(&layout);
    layout.addLayoutItem(legend);
    legend->setLinkedMap(map);
    legend->setResizeToContents(false);
    legend->attemptSetSceneRect(QRectF(10., 190., 185., 60.));
    LayoutService::tuneSheetLegend(legend);
    numbers.applyLegend(legend);
    QCOMPARE(legendNumberKeys(legend).size(), 6);
    QTemporaryDir output;
    QVERIFY(output.isValid());

    for (const bool visible : {false, true}) {
      first->setCustomProperty(QStringLiteral("ka_hgis/layout_numbers_visible"), visible);
      first->triggerRepaint();
      QVERIFY(numbers.update(map));
      QCOMPARE(numbers.entries().size(), visible ? 6 : 4);
      LayoutService::tuneSheetLegend(legend);
      numbers.applyLegend(legend);
      QString error;
      const QString path = output.filePath(visible ? QStringLiteral("numbers-on.pdf") : QStringLiteral("numbers-off.pdf"));
      QVERIFY2(numbers.exportPdf(map, legend, path, 150., &error), qPrintable(error));
      const auto keys = legendNumberKeys(legend);
      QCOMPARE(keys, numbers.visibleKeys());
      QCOMPARE(keys, numbers.legendKeys());
      QCOMPARE(keys.size(), visible ? 6 : 4);
      QVERIFY2(consecutiveLegendError(legend, numbers.entries()).isEmpty(), "Each visible classification must restart at 1 without gaps");
      QCOMPARE(bool(legend->model()->rootGroup()->findLayer(first->id())), visible);

      QgsLayoutExporter check(&layout);
      QgsLayoutExporter::PdfExportSettings settings;
      settings.dpi = 150.;
      QCOMPARE(check.exportToPdf(output.filePath(QStringLiteral("independent.pdf")), settings), QgsLayoutExporter::Success);
      const auto* results = sheetNumberResults(check, map);
      QVERIFY(results);
      QCOMPARE(keys, numberedEntryKeys(numbers.entries()));
      for (const auto& label : results->allLabels()) {
        if (label.isUnplaced || label.isDiagram) continue;
        if (!visible) QVERIFY(label.layerID != first->id());
        bool numeric = false;
        label.labelText.toInt(&numeric);
        QVERIFY2(numeric, "Source site names must not reappear when layout numbers are hidden");
      }
      for (auto* source : sources) {
        QVERIFY(source->labelsEnabled());
        QCOMPARE(source->labeling()->settings().fieldName, QStringLiteral("nm"));
      }
    }
  }

  void renderedLegendDeduplicatesCategoriesAndSeparatesLayers() {
    QgsProject project;
    const QString shared = QStringLiteral("같은 유적");
    auto* first = addHeritage(project, HeritageDataset::DesignatedHeritage,
                              {shared, shared, QStringLiteral("다른 유적")});
    auto* second = addHeritage(project, HeritageDataset::SurfaceSurveyArea,
                               {shared, shared, QStringLiteral("다른 유적")});
    QVERIFY(second->startEditing());
    auto features = second->getFeatures();
    QgsFeature feature;
    while (features.nextFeature(feature)) {
      QgsGeometry geometry = feature.geometry();
      geometry.translate(0., 300.);
      QVERIFY(second->changeGeometry(feature.id(), geometry));
    }
    QVERIFY(second->commitChanges());
    QgsMapLayerStyle firstStyle, secondStyle;
    firstStyle.readFromLayer(first);
    secondStyle.readFromLayer(second);
    QgsPrintLayout layout(&project);
    auto* map = makeLayoutMap(layout, {first, second}, QgsRectangle(189950., 549950., 190550., 550550.));
    HeritageLayoutNumbers numbers;
    QVERIFY(numbers.update(map));
    numbers.followRenderedLabels(map);
    QCOMPARE(numbers.entries().size(), 4);
    for (const auto& entry : numbers.entries())
      QCOMPARE(entry.featureIds.size(), entry.name == shared ? 2 : 1);
    auto* legend = new QgsLayoutItemLegend(&layout);
    layout.addLayoutItem(legend);
    legend->setLinkedMap(map);
    legend->setResizeToContents(false);
    legend->attemptSetSceneRect(QRectF(180., 10., 90., 100.));
    LayoutService::tuneSheetLegend(legend);
    QTemporaryDir output;
    QVERIFY(output.isValid());
    QgsLayoutExporter exporter(&layout);
    QgsLayoutExporter::PdfExportSettings settings;
    settings.dpi = 150.;
    QCOMPARE(exporter.exportToPdf(output.filePath(QStringLiteral("duplicate-sites.pdf")), settings), QgsLayoutExporter::Success);
    const auto* results = sheetNumberResults(exporter, map);
    QVERIFY(results);
    QMap<QString, int> placedCounts;
    for (const auto& label : results->allLabels()) {
      if (!label.isUnplaced && !label.isDiagram)
        ++placedCounts[HeritageLayoutNumbers::entryKey(label.layerID, label.labelText.toInt())];
    }
    bool repeatedNumber = false;
    for (int count : placedCounts) repeatedNumber |= count > 1;
    QVERIFY2(!repeatedNumber, "같은 유적은 번호 하나만 두고 선으로 뺀다");
    numbers.acceptRenderedLabels(map, results);
    QCOMPARE(numbers.visibleKeys(), numberedEntryKeys(numbers.entries()));
    QCOMPARE(numbers.visibleKeys().size(), 4);
    numbers.applyLegend(legend);
    QCOMPARE(legendNumberKeys(legend), numbers.visibleKeys());
    QCOMPARE(legendNumberKeys(legend), numbers.legendKeys());
    QVERIFY(!numbers.acceptRenderedLabels(map, results));
    for (auto* node : legend->model()->rootGroup()->findLayers())
      QCOMPARE(legend->model()->layerLegendNodes(node).size(), 2);
    QgsMapLayerStyle after;
    after.readFromLayer(first);
    QCOMPARE(after.xmlData(), firstStyle.xmlData());
    after.readFromLayer(second);
    QCOMPARE(after.xmlData(), secondStyle.xmlData());
    map->setExtent(QgsRectangle(189950., 549950., 190250., 550250.));
    QVERIFY(numbers.update(map));
    QCOMPARE(numbers.visibleKeys(), numberedEntryKeys(numbers.entries()));
    QCOMPARE(numbers.legendKeys(), numbers.visibleKeys());
    QVERIFY(!numbers.visibleKeys().isEmpty());
    numbers.applyLegend(legend);
  }

  void renderedLegendRejectsPreviousFeatureWithSameLayerAndNumber() {
    QgsProject project;
    auto* layer = addHeritage(project, HeritageDataset::DesignatedHeritage, {QStringLiteral("같은 이름과 번호")});
    QgsFeature original;
    QVERIFY(layer->getFeatures().nextFeature(original));
    QgsPrintLayout layout(&project);
    auto* map = makeLayoutMap(layout, {layer}, QgsRectangle(189950., 549950., 190150., 550150.));
    HeritageLayoutNumbers numbers;
    QVERIFY(numbers.update(map));
    numbers.followRenderedLabels(map);
    QTemporaryDir output;
    QVERIFY(output.isValid());
    QgsLayoutExporter firstExport(&layout);
    QgsLayoutExporter::PdfExportSettings settings;
    settings.dpi = 150.;
    QCOMPARE(firstExport.exportToPdf(output.filePath(QStringLiteral("original-feature.pdf")), settings), QgsLayoutExporter::Success);
    const auto* previousResults = sheetNumberResults(firstExport, map);
    QVERIFY(previousResults);
    numbers.acceptRenderedLabels(map, previousResults);
    QCOMPARE(numbers.visibleKeys().size(), 1);

    // A real replacement feature keeps its name, geometry, layer ID and number.
    // The completed older render must not authorize this different feature.
    QVERIFY(layer->startEditing());
    QgsFeature replacement(layer->fields());
    replacement.setAttributes(original.attributes());
    replacement.setGeometry(original.geometry());
    QVERIFY(layer->addFeature(replacement));
    QVERIFY(layer->commitChanges());
    QVERIFY(layer->startEditing());
    QVERIFY(layer->deleteFeature(original.id()));
    QVERIFY(layer->commitChanges());
    QgsFeature current;
    QVERIFY(layer->getFeatures().nextFeature(current));
    QVERIFY(current.id() != original.id());
    QVERIFY(numbers.update(map));
    QCOMPARE(numbers.entries().size(), 1);
    QCOMPARE(numbers.entries().first().number, 1);
    QVERIFY(numbers.entries().first().featureIds.contains(current.id()));
    QVERIFY(!numbers.entries().first().featureIds.contains(original.id()));
    QCOMPARE(numbers.visibleKeys().size(), 1);
    QVERIFY(!numbers.acceptRenderedLabels(map, previousResults));
    QCOMPARE(numbers.visibleKeys().size(), 1);

    QgsLayoutExporter freshExport(&layout);
    QCOMPARE(freshExport.exportToPdf(output.filePath(QStringLiteral("replacement-feature.pdf")), settings), QgsLayoutExporter::Success);
    const auto* freshResults = sheetNumberResults(freshExport, map);
    QVERIFY(freshResults);
    numbers.acceptRenderedLabels(map, freshResults);
    QCOMPARE(numbers.visibleKeys().size(), 1);
  }

  void renderedLegendClipsToActualPageWithMapRotation_data() {
    QTest::addColumn<double>("rotation");
    QTest::newRow("page-clip") << 0.;
    QTest::newRow("rotated-page-clip") << 30.;
  }
  void renderedLegendClipsToActualPageWithMapRotation() {
    QFETCH(double, rotation);
    QgsProject project;
    auto* layer = addHeritage(project, HeritageDataset::DesignatedHeritage,
        {QStringLiteral("페이지 안"), QStringLiteral("페이지 밖 가운데"), QStringLiteral("페이지 밖 오른쪽")});
    QVERIFY(layer->startEditing());
    auto features = layer->getFeatures();
    QgsFeature feature;
    while (features.nextFeature(feature)) {
      QgsGeometry geometry = feature.geometry();
      geometry.translate(0., 250.);
      QVERIFY(layer->changeGeometry(feature.id(), geometry));
    }
    QVERIFY(layer->commitChanges());
    QgsMapLayerStyle original;
    original.readFromLayer(layer);
    QgsPrintLayout layout(&project);
    auto* map = makeLayoutMap(layout, {layer}, QgsRectangle(189950., 549950., 190550., 550550.));
    layout.pageCollection()->page(0)->setPageSize(QStringLiteral("A4"), QgsLayoutItemPage::Portrait);
    map->attemptMove(QgsLayoutPoint(160., 10.));
    map->setMapRotation(rotation);
    HeritageLayoutNumbers numbers;
    QVERIFY(numbers.update(map));
    numbers.followRenderedLabels(map);
    QVERIFY(numbers.entries().size() >= 1);
    QVERIFY(numbers.entries().size() <= 3);
    auto* legend = new QgsLayoutItemLegend(&layout);
    layout.addLayoutItem(legend);
    legend->setLinkedMap(map);
    legend->setResizeToContents(false);
    legend->attemptSetSceneRect(QRectF(10., 190., 190., 65.));
    LayoutService::tuneSheetLegend(legend);
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const QString qa = qEnvironmentVariable("KA_HGIS_QA_DIR");
    const QString directory = qa.isEmpty() ? temporary.path() : qa;
    QVERIFY(QDir().mkpath(directory));
    const QString base = QDir(directory).filePath(QStringLiteral("number-page-clip-%1").arg(rotation));
    QgsLayoutExporter rawExport(&layout);
    QgsLayoutExporter::PdfExportSettings rawSettings;
    rawSettings.dpi = 150.;
    QCOMPARE(rawExport.exportToPdf(temporary.filePath(QStringLiteral("page-clip-before-compaction.pdf")), rawSettings), QgsLayoutExporter::Success);
    const auto* rawResults = sheetNumberResults(rawExport, map);
    QVERIFY(rawResults);
    QSet<QString> allOriginalPlaced;
    for (const auto& label : rawResults->allLabels())
      if (!label.isUnplaced && label.layerID == layer->id())
        allOriginalPlaced.insert(HeritageLayoutNumbers::entryKey(label.layerID, label.labelText.toInt()));
    QString error;
    QVERIFY2(numbers.exportPdf(map, legend, base + QStringLiteral(".pdf"), 150., &error), qPrintable(error));
    const auto deliveredKeys = legendNumberKeys(legend);
    QCOMPARE(deliveredKeys, numbers.visibleKeys());
    QCOMPARE(deliveredKeys, numbers.legendKeys());
    QVERIFY(!deliveredKeys.isEmpty());
    const QString sequenceError = consecutiveLegendError(legend, numbers.entries());
    QVERIFY2(sequenceError.isEmpty(), qPrintable(sequenceError));
    QgsLayoutExporter exporter(&layout);
    QgsLayoutExporter::PdfExportSettings settings;
    settings.dpi = 150.;
    QCOMPARE(exporter.exportToPdf(temporary.filePath(QStringLiteral("page-clip-independent.pdf")), settings), QgsLayoutExporter::Success);
    const auto* results = sheetNumberResults(exporter, map);
    QVERIFY(results);
    QCOMPARE(deliveredKeys, numberedEntryKeys(numbers.entries()));
    QVERIFY2(!allOriginalPlaced.isEmpty() || !deliveredKeys.isEmpty(),
             "Page-clipped map must keep at least the on-paper site in the legend");
    if (!qa.isEmpty()) QVERIFY(exporter.renderPageToImage(0, QSize(), 120.).save(base + QStringLiteral(".png")));
    QgsMapLayerStyle after;
    after.readFromLayer(layer);
    QCOMPARE(after.xmlData(), original.xmlData());
  }

  void compactTailNumbersPreservePrintedPositionsAcrossCrsAndRotation() {
    QgsProject project;
    QStringList names;
    for (int i = 1; i <= 12; ++i) names.append(QStringLiteral("유적 %1").arg(i, 2, 10, QLatin1Char('0')));
    names.append(names.at(10));  // Same category, but this extra feature stays off paper.
    auto* layer = addHeritage(project, HeritageDataset::DesignatedHeritage, names);
    QgsPrintLayout layout(&project);
    auto* map = makeLayoutMap(layout, {layer}, QgsRectangle(189950., 549950., 190550., 550550.));
    layout.pageCollection()->page(0)->setPageSize(QStringLiteral("A4"), QgsLayoutItemPage::Portrait);
    map->attemptSetSceneRect(QRectF(100., 10., 160., 160.));
    const QgsCoordinateReferenceSystem mapCrs(QStringLiteral("EPSG:5187"));
    const QgsCoordinateTransform toMap(layer->crs(), mapCrs, project.transformContext());
    const QgsPointXY center = toMap.transform(QgsPointXY(190000., 550000.));
    map->setCrs(mapCrs);
    map->setExtent(QgsRectangle(center.x() - 400., center.y() - 400., center.x() + 400., center.y() + 400.));
    map->setMapRotation(30.);
    const QgsCoordinateTransform toSource(mapCrs, layer->crs(), project.transformContext());
    const QTransform layoutToMap = map->layoutToMapCoordsTransform();
    QVERIFY(layer->startEditing());
    auto features = layer->getFeatures();
    QgsFeature feature;
    int index = 0;
    QSet<qint64> expectedVisibleFeatures;
    qint64 hiddenDuplicate = -1;
    while (features.nextFeature(feature)) {
      QPointF target(230. + (index % 2) * 20., 30. + (index / 2) * 25.);
      if (index == 10) target = QPointF(140., 65.);
      if (index == 11) target = QPointF(170., 120.);
      if (index == 12) target = QPointF(235., 155.);
      if (index == 10 || index == 11) expectedVisibleFeatures.insert(feature.id());
      if (index == 12) hiddenDuplicate = feature.id();
      const QPointF mapPoint = layoutToMap.map(target);
      const QgsPointXY point = toSource.transform(QgsPointXY(mapPoint));
      QgsGeometry geometry = QgsGeometry::fromRect(QgsRectangle(point.x() - 2., point.y() - 2., point.x() + 2., point.y() + 2.));
      QVERIFY(layer->changeGeometry(feature.id(), geometry));
      ++index;
    }
    QCOMPARE(index, 13);
    QVERIFY(layer->commitChanges());
    QgsMapLayerStyle original;
    original.readFromLayer(layer);
    HeritageLayoutNumbers numbers;
    QVERIFY(numbers.update(map));
    QVERIFY(numbers.entries().size() >= 2);
    QVERIFY(numbers.entries().size() <= 12);
    auto* legend = new QgsLayoutItemLegend(&layout);
    layout.addLayoutItem(legend);
    legend->setLinkedMap(map);
    legend->setResizeToContents(false);
    legend->attemptSetSceneRect(QRectF(10., 190., 190., 65.));
    LayoutService::tuneSheetLegend(legend);
    numbers.applyLegend(legend);
    QTemporaryDir output;
    QVERIFY(output.isValid());
    QgsLayoutExporter rawExport(&layout);
    QgsLayoutExporter::PdfExportSettings settings;
    settings.dpi = 300.;
    settings.forceVectorOutput = true;
    QCOMPARE(rawExport.exportToPdf(output.filePath(QStringLiteral("tail-original.pdf")), settings), QgsLayoutExporter::Success);
    auto* rawPins = numbers.numberLayer();
    QVERIFY(rawPins);
    const QTransform mapToLayout = layoutToMap.inverted();
    QMap<qint64, QPointF> originalCenters;
    QgsFeature rawPin;
    auto rawRows = rawPins->getFeatures();
    while (rawRows.nextFeature(rawPin)) {
      const qint64 sourceId = rawPin.attribute(QStringLiteral("fid")).toString().toLongLong();
      if (!expectedVisibleFeatures.contains(sourceId)) continue;
      QVERIFY(rawPin.attribute(QStringLiteral("num")).toInt() >= 1);
      const QgsPointXY pin = rawPin.geometry().asPoint();
      originalCenters.insert(sourceId, mapToLayout.map(QPointF(pin.x(), pin.y())));
    }
    QCOMPARE(originalCenters.size(), 2);
    QVERIFY(rawPins->featureCount() >= 2);
    QString error;
    const QString qa = qEnvironmentVariable("KA_HGIS_QA_DIR");
    const QString directory = qa.isEmpty() ? output.path() : qa;
    QVERIFY(QDir().mkpath(directory));
    const QString path = QDir(directory).filePath(QStringLiteral("compact-tail-crs-rotation.pdf"));
    QVERIFY2(numbers.exportPdf(map, legend, path, 300., &error), qPrintable(error));
    const QString sequenceError = consecutiveLegendError(legend, numbers.entries());
    QVERIFY2(sequenceError.isEmpty(), qPrintable(sequenceError));
    QCOMPARE(legendNumberKeys(legend), numbers.visibleKeys());
    QCOMPARE(legendNumberKeys(legend), numbers.legendKeys());
    QVERIFY(numbers.visibleKeys().size() >= 2);
    QgsLayoutExporter finalExport(&layout);
    QCOMPARE(finalExport.exportToPdf(output.filePath(QStringLiteral("tail-final-independent.pdf")), settings), QgsLayoutExporter::Success);
    auto* finalPins = numbers.numberLayer();
    QVERIFY(finalPins);
    QCOMPARE(numbers.visibleKeys(), numberedEntryKeys(numbers.entries()));
    QSet<qint64> finalFeatures;
    QgsFeature finalPin;
    auto finalRows = finalPins->getFeatures();
    while (finalRows.nextFeature(finalPin)) {
      const qint64 sourceId = finalPin.attribute(QStringLiteral("fid")).toString().toLongLong();
      QVERIFY(sourceId != hiddenDuplicate);
      if (originalCenters.contains(sourceId)) {
        const QgsPointXY pin = finalPin.geometry().asPoint();
        const QPointF position = mapToLayout.map(QPointF(pin.x(), pin.y()));
        const double movement = QLineF(position, originalCenters.value(sourceId)).length();
        QVERIFY2(movement < 2., qPrintable(QStringLiteral("On-paper badge moved by %1 mm").arg(movement)));
      }
      finalFeatures.insert(sourceId);
    }
    QVERIFY(finalFeatures.contains(*expectedVisibleFeatures.begin()));
    QgsMapLayerStyle after;
    after.readFromLayer(layer);
    QCOMPARE(after.xmlData(), original.xmlData());
    if (!qa.isEmpty()) QVERIFY(finalExport.renderPageToImage(0, QSize(), 150.).save(QDir(directory).filePath(QStringLiteral("compact-tail-crs-rotation.png"))));
  }

  void layoutNumberCachePerformance() {
    // A realistic category count, with only the first hundred sites on paper.
    // Keep input and each stage separate so a faster entry cannot hide stale
    // numbering or move expensive work into legend reconstruction.
    constexpr int categories = 1500;
    constexpr int visible = 100;
    constexpr int repeats = 3;
    QStringList names;
    names.reserve(categories);
    for (int i = 0; i < categories; ++i)
      names.append(QStringLiteral("성능검사 유적 %1").arg(i, 4, 10, QLatin1Char('0')));
    QgsProject project;
    auto* layer = addHeritage(project, HeritageDataset::SurfaceSurveyArea, names);
    QgsMapLayerStyle originalStyle;
    originalStyle.readFromLayer(layer);
    QVERIFY(dynamic_cast<QgsSingleSymbolRenderer*>(layer->renderer()));
    QgsPrintLayout layout(&project);
    auto* map = makeLayoutMap(layout, {layer},
        QgsRectangle(189950., 540050., 209950., 560050.));
    HeritageLayoutNumbers numbers;
    QElapsedTimer timer;
    timer.start();
    QVERIFY(numbers.update(map, true));
    const qint64 firstUpdateNs = timer.nsecsElapsed();
    QVERIFY2(numbers.error().isEmpty(), qPrintable(numbers.error()));
    QCOMPARE(numbers.entries().size(), visible);
    auto* legend = new QgsLayoutItemLegend(&layout);
    layout.addLayoutItem(legend);
    legend->setLinkedMap(map);
    legend->setSyncMode(Qgis::LegendSyncMode::Manual);
    legend->resetManualLayers(Qgis::LegendSyncMode::VisibleLayers);
    timer.restart();
    numbers.applyLegend(legend);
    const qint64 firstLegendNs = timer.nsecsElapsed();
    timer.restart();
    for (int i = 0; i < repeats; ++i) QVERIFY(numbers.update(map));
    const qint64 cachedUpdateNs = timer.nsecsElapsed();
    timer.restart();
    for (int i = 0; i < repeats; ++i) numbers.applyLegend(legend);
    const qint64 cachedLegendNs = timer.nsecsElapsed();
    QCOMPARE(numbers.entries().size(), visible);
    qInfo().noquote() << QStringLiteral(
        "LAYOUT_NUMBER_PERF categories=%1 visible=%2 repeats=%3 first_update_ms=%4 first_legend_ms=%5 cached_update_mean_ms=%6 cached_legend_mean_ms=%7")
        .arg(categories).arg(visible).arg(repeats)
        .arg(firstUpdateNs / 1000000., 0, 'f', 3)
        .arg(firstLegendNs / 1000000., 0, 'f', 3)
        .arg(cachedUpdateNs / (1000000. * repeats), 0, 'f', 3)
        .arg(cachedLegendNs / (1000000. * repeats), 0, 'f', 3);

    std::unique_ptr<QgsVectorLayer> firstDrawing(layer->clone());
    QgsMapLayerStyle(numbers.overrides().value(layer->id())).writeToLayer(firstDrawing.get());
    auto* firstRenderer = dynamic_cast<QgsCategorizedSymbolRenderer*>(firstDrawing->renderer());
    QVERIFY(firstRenderer);
    QCOMPARE(firstRenderer->categories().size(), visible + 1);
    QVERIFY(dynamic_cast<QgsSingleSymbolRenderer*>(layer->renderer()));
    for (int i = 0; i < visible; ++i)
      QCOMPARE(numbers.entries().at(i).legendIndex, i);

    constexpr int firstMovedSite = 1000;
    map->setExtent(QgsRectangle(389950., 540050., 409950., 560050.));
    timer.restart();
    QVERIFY(numbers.update(map));
    const qint64 movedUpdateNs = timer.nsecsElapsed();
    QCOMPARE(numbers.entries().size(), visible);
    std::unique_ptr<QgsVectorLayer> movedDrawing(layer->clone());
    QgsMapLayerStyle(numbers.overrides().value(layer->id())).writeToLayer(movedDrawing.get());
    auto* movedRenderer = dynamic_cast<QgsCategorizedSymbolRenderer*>(movedDrawing->renderer());
    QVERIFY(movedRenderer);
    QCOMPARE(movedRenderer->categories().size(), visible + 1);
    for (int i = 0; i < visible; ++i) {
      const auto& entry = numbers.entries().at(i);
      QCOMPARE(entry.name, names.at(firstMovedSite + i));
      QCOMPARE(entry.number, i + 1);
      QCOMPARE(entry.legendIndex, i);
      QCOMPARE(movedRenderer->categories().at(i).label(), entry.name);
    }
    auto* movedPins = numbers.numberLayer();
    QVERIFY(movedPins);
    QCOMPARE(int(movedPins->featureCount()), visible);
    QgsFeature pin;
    auto pinRows = movedPins->getFeatures();
    int evaluated = 0;
    while (pinRows.nextFeature(pin)) {
      const int sourceIndex = names.indexOf(pin.attribute(QStringLiteral("nm")).toString());
      QVERIFY(sourceIndex >= firstMovedSite && sourceIndex < firstMovedSite + visible);
      QCOMPARE(pin.attribute(QStringLiteral("num")).toInt(), sourceIndex - firstMovedSite + 1);
      ++evaluated;
    }
    QCOMPARE(evaluated, visible);
    timer.restart();
    numbers.applyLegend(legend);
    const qint64 movedLegendNs = timer.nsecsElapsed();
    qInfo() << "LAYOUT_CHANGED_VIEW_MS update=" << movedUpdateNs / 1000000.
            << "legend=" << movedLegendNs / 1000000.;
    auto* treeLayer = legend->model()->rootGroup()->findLayer(layer->id());
    QVERIFY(treeLayer);
    const auto nodes = legend->model()->layerLegendNodes(treeLayer);
    QCOMPARE(nodes.size(), visible);
    for (int i = 0; i < visible; ++i) {
      QCOMPARE(nodes.at(i)->data(Qt::DisplayRole).toString(), names.at(firstMovedSite + i));
      auto* symbol = dynamic_cast<QgsSymbolLegendNode*>(nodes.at(i));
      QVERIFY(symbol && symbol->customSymbol());
      QString badge;
      for (int j = 0; j < symbol->customSymbol()->symbolLayerCount(); ++j) {
        if (auto* font = dynamic_cast<QgsFontMarkerSymbolLayer*>(symbol->customSymbol()->symbolLayer(j)))
          badge = font->character();
      }
      QCOMPARE(badge, QString::number(i + 1));
    }
    QgsMapLayerStyle after;
    after.readFromLayer(layer);
    QCOMPARE(after.xmlData(), originalStyle.xmlData());
    QVERIFY(dynamic_cast<QgsSingleSymbolRenderer*>(layer->renderer()));
  }

  void layoutNumberCacheInvalidatesStyleAndGeometryWithoutForce() {
    QgsProject project;
    auto* layer = addHeritage(project, HeritageDataset::SurfaceSurveyArea,
                              {QStringLiteral("움직이는 유적")});
    QgsPrintLayout layout(&project);
    auto* map = makeLayoutMap(layout, {layer}, QgsRectangle(189950., 549950., 190150., 550150.));
    HeritageLayoutNumbers numbers;
    QVERIFY(numbers.update(map));
    QCOMPARE(numbers.entries().size(), 1);
    const auto initialRevision = numbers.revision();
    QVERIFY(initialRevision > 0);
    QVERIFY(numbers.update(map));
    QCOMPARE(numbers.revision(), initialRevision);

    auto* changedRenderer = dynamic_cast<QgsSingleSymbolRenderer*>(layer->renderer()->clone());
    QVERIFY(changedRenderer);
    auto* coloredSymbol = changedRenderer->symbol()->clone();
    const QColor changedColor(QStringLiteral("#318ad1"));
    coloredSymbol->setColor(changedColor);
    changedRenderer->setSymbol(coloredSymbol);
    layer->setRenderer(changedRenderer);
    QVERIFY(numbers.update(map));
    QVERIFY(numbers.revision() > initialRevision);
    QCOMPARE(numbers.entries().first().color, changedColor);
    const auto styleRevision = numbers.revision();
    QVERIFY(numbers.update(map));
    QCOMPARE(numbers.revision(), styleRevision);

    // Geometry changes leave featureCount and renderer untouched. The next
    // normal update still has to invalidate cached spatial membership.
    auto features = layer->getFeatures();
    QgsFeature feature;
    QVERIFY(features.nextFeature(feature));
    QgsGeometry outside = feature.geometry();
    outside.translate(1000., 0.);
    QVERIFY(layer->startEditing());
    QVERIFY(layer->changeGeometry(feature.id(), outside));
    QVERIFY(numbers.update(map));
    QVERIFY(numbers.revision() > styleRevision);
    QCOMPARE(numbers.entries().size(), 0);
    const auto editedRevision = numbers.revision();
    QVERIFY(layer->rollBack());
    QVERIFY(numbers.update(map));
    QVERIFY(numbers.revision() > editedRevision);
    QCOMPARE(numbers.entries().size(), 1);
    QCOMPARE(layer->featureCount(), 1LL);
  }

  void layoutNumberCacheInvalidatesNameEditsAndTreeOrder() {
    QgsProject project;
    auto* first = addHeritage(project, HeritageDataset::SurfaceSurveyArea,
                              {QStringLiteral("첫 유적")});
    auto* second = addHeritage(project, HeritageDataset::SurfaceSurveyArea,
                               {QStringLiteral("둘째 유적")});
    QVERIFY(dynamic_cast<QgsSingleSymbolRenderer*>(first->renderer()));
    QgsPrintLayout layout(&project);
    auto* map = makeLayoutMap(layout, {first, second}, QgsRectangle(189950., 549950., 190150., 550150.));
    HeritageLayoutNumbers numbers;
    QVERIFY(numbers.update(map));
    QCOMPARE(numbers.entries().size(), 2);
    QCOMPARE(numbers.entries().first().layerId, first->id());
    QCOMPARE(numbers.entries().first().number, 1);
    const auto initialRevision = numbers.revision();
    QgsFeature feature;
    auto features = first->getFeatures();
    QVERIFY(features.nextFeature(feature));
    QVERIFY(first->startEditing());
    QVERIFY(first->changeAttributeValue(feature.id(), first->fields().indexOf(QStringLiteral("nm")),
                                        QStringLiteral("바뀐 유적명")));
    QVERIFY(numbers.update(map));
    QVERIFY(numbers.revision() > initialRevision);
    QCOMPARE(numbers.entries().first().name, QStringLiteral("바뀐 유적명"));
    QVERIFY(first->commitChanges());
    QVERIFY(numbers.update(map));
    const auto nameRevision = numbers.revision();
    auto* secondNode = project.layerTreeRoot()->findLayer(second->id());
    QVERIFY(secondNode);
    auto* group = qobject_cast<QgsLayerTreeGroup*>(secondNode->parent());
    QVERIFY(group);
    QVERIFY(group->takeChild(secondNode));
    group->insertChildNode(0, secondNode);
    QVERIFY(numbers.update(map));
    QVERIFY(numbers.revision() > nameRevision);
    QCOMPARE(numbers.entries().first().layerId, second->id());
    QCOMPARE(numbers.entries().first().number, 1);
    QCOMPARE(numbers.entries().last().layerId, first->id());
    QCOMPARE(numbers.entries().last().number, 2);
  }

  void layoutNumberLegendReusePreservesNodesAndRebuildRestoresBadges() {
    QgsProject project;
    auto* layer = addHeritage(project, HeritageDataset::DesignatedHeritage,
                              {QStringLiteral("첫 유적"), QStringLiteral("둘째 유적")});
    QgsPrintLayout layout(&project);
    auto* map = makeLayoutMap(layout, {layer}, QgsRectangle(189950., 549800., 190450., 550300.));
    HeritageLayoutNumbers numbers;
    QVERIFY(numbers.update(map));
    const auto revision = numbers.revision();
    auto* legend = new QgsLayoutItemLegend(&layout);
    layout.addLayoutItem(legend);
    legend->setLinkedMap(map);
    legend->setSyncMode(Qgis::LegendSyncMode::Manual);
    legend->resetManualLayers(Qgis::LegendSyncMode::VisibleLayers);
    numbers.applyLegend(legend);
    auto* treeLayer = legend->model()->rootGroup()->findLayer(layer->id());
    QVERIFY(treeLayer);
    auto nodes = legend->model()->layerLegendNodes(treeLayer);
    QCOMPARE(nodes.size(), 2);
    QPointer<QgsLayerTreeModelLegendNode> retained(nodes.first());
    numbers.applyLegend(legend);
    QVERIFY(!retained.isNull());
    QCOMPARE(legend->model()->layerLegendNodes(treeLayer).first(), retained.data());
    QCOMPARE(numbers.revision(), revision);

    legend->resetManualLayers(Qgis::LegendSyncMode::VisibleLayers);
    numbers.applyLegend(legend);
    treeLayer = legend->model()->rootGroup()->findLayer(layer->id());
    QVERIFY(treeLayer);
    nodes = legend->model()->layerLegendNodes(treeLayer);
    QCOMPARE(nodes.size(), 2);
    QSet<QString> badgeNumbers;
    for (auto* node : nodes) {
      auto* symbol = dynamic_cast<QgsSymbolLegendNode*>(node);
      QVERIFY(symbol);
      QVERIFY(symbol->customSymbol());
      for (int i = 0; i < symbol->customSymbol()->symbolLayerCount(); ++i) {
        if (auto* font = dynamic_cast<QgsFontMarkerSymbolLayer*>(symbol->customSymbol()->symbolLayer(i)))
          badgeNumbers.insert(font->character());
      }
    }
    QCOMPARE(badgeNumbers, (QSet<QString>{QStringLiteral("1"), QStringLiteral("2")}));
    QCOMPARE(numbers.revision(), revision);
  }

  void numberedLegendFlowsAcrossColumnsWhenWidthChanges() {
    QgsProject project;
    QStringList names;
    for (int i = 0; i < 40; ++i) {
      names.append(i % 13 == 0
          ? QStringLiteral("합성 긴 유적명 %1 문화유산 지표조사 상세 구역 및 주변 유적 분포 확인 지역").arg(i, 2, 10, QLatin1Char('0'))
          : QStringLiteral("합성 유적 %1").arg(i, 2, 10, QLatin1Char('0')));
    }
    auto* largeLayer = addHeritage(project, HeritageDataset::DesignatedHeritage, names);
    auto* secondLayer = addHeritage(project, HeritageDataset::SurfaceSurveyArea,
        {QStringLiteral("합성 지표조사 가구역"), QStringLiteral("합성 지표조사 나구역"),
         QStringLiteral("합성 지표조사 다구역")});
    QgsMapLayerStyle originalLarge;
    originalLarge.readFromLayer(largeLayer);
    QgsMapLayerStyle originalSecond;
    originalSecond.readFromLayer(secondLayer);
    QgsPrintLayout layout(&project);
    auto* map = makeLayoutMap(layout, {largeLayer, secondLayer},
                              QgsRectangle(189900., 545900., 198100., 554100.));
    // Optional exported fixtures isolate the legend on a page large enough to
    // show both its narrow single column and its wider multiple columns.
    layout.pageCollection()->page(0)->setPageSize(QStringLiteral("A3"), QgsLayoutItemPage::Portrait);
    map->setVisibility(false);
    HeritageLayoutNumbers numbers;
    QVERIFY(numbers.update(map));
    QCOMPARE(numbers.entries().size(), 43);
    const auto revision = numbers.revision();
    auto* legend = new QgsLayoutItemLegend(&layout);
    layout.addLayoutItem(legend);
    legend->setTitle(QStringLiteral("주변유적 번호 범례"));
    legend->setLinkedMap(map);
    legend->setSyncMode(Qgis::LegendSyncMode::Manual);
    legend->resetManualLayers(Qgis::LegendSyncMode::VisibleLayers);
    for (const auto component : {Qgis::LegendComponent::Title, Qgis::LegendComponent::Group,
                                 Qgis::LegendComponent::Subgroup, Qgis::LegendComponent::SymbolLabel})
      legend->setStyleFont(component, QFont(QStringLiteral("Malgun Gothic"), 9));
    numbers.applyLegend(legend);
    QVector<QPointer<QgsLayerTreeModelLegendNode>> retained;
    for (auto* treeLayer : legend->model()->rootGroup()->findLayers()) {
      for (auto* node : legend->model()->layerLegendNodes(treeLayer)) retained.append(node);
    }
    QCOMPARE(retained.size(), 43);
    const QPointF position(20., 20.);
    legend->attemptMove(QgsLayoutPoint(position.x(), position.y()));
    legend->setResizeToContents(false);
    legend->attemptResize(QgsLayoutSize(70., 350.));
    QSignalSpy overridesChanged(map, &QgsLayoutItemMap::layerStyleOverridesChanged);
    LayoutService::flowSheetLegend(legend);
    const int narrowColumns = legend->columnCount();
    const double narrowHeight = legend->rect().height();
    QCOMPARE(legend->boxSpace(), 1.);
    QVERIFY(narrowColumns >= 2);
    QVERIFY(narrowHeight > 80.);
    QVERIFY(qAbs(legend->rect().width() - 70.) < .1);
    QVERIFY(QLineF(legend->pos(), position).length() < .01);
    const QString qaDir = qEnvironmentVariable("KA_HGIS_QA_DIR");
    const auto exportFixture = [&](const QString& name) {
      if (qaDir.isEmpty()) return true;
      if (!QDir().mkpath(qaDir)) return false;
      QgsLayoutExporter exporter(&layout);
      QgsLayoutExporter::PdfExportSettings pdfSettings;
      pdfSettings.dpi = 120.;
      const QString base = QDir(qaDir).filePath(name);
      if (exporter.exportToPdf(base + QStringLiteral(".pdf"), pdfSettings) != QgsLayoutExporter::Success)
        return false;
      return exporter.renderPageToImage(0, QSize(), 120.).save(base + QStringLiteral(".png"));
    };
    QVERIFY(exportFixture(QStringLiteral("numbered-legend-narrow")));

    // Only the first call installs flow handling. User resizes must work through
    // the real sizePositionChanged signal and its coalescing timer thereafter.
    legend->attemptResize(QgsLayoutSize(190., narrowHeight));
    QTRY_VERIFY_WITH_TIMEOUT(legend->columnCount() > narrowColumns, 3000);
    QTRY_VERIFY_WITH_TIMEOUT(legend->rect().height() < narrowHeight, 3000);
    const int wideColumns = legend->columnCount();
    const double wideHeight = legend->rect().height();
    QVERIFY(wideColumns > narrowColumns);
    QVERIFY(wideHeight < narrowHeight);
    QVERIFY(legend->splitLayer());
    QVERIFY(legend->autoWrapLinesAfter() > 0.);
    QVERIFY(legend->autoWrapLinesAfter() < 190. / wideColumns);
    QVERIFY(qAbs(legend->rect().width() - 190.) < .1);
    QVERIFY(QLineF(legend->pos(), position).length() < .01);
    QVERIFY(exportFixture(QStringLiteral("numbered-legend-wide")));

    legend->attemptResize(QgsLayoutSize(70., wideHeight));
    QTRY_COMPARE_WITH_TIMEOUT(legend->columnCount(), narrowColumns, 3000);
    QTRY_VERIFY_WITH_TIMEOUT(legend->rect().height() > wideHeight, 3000);
    QCOMPARE(legend->columnCount(), narrowColumns);
    QVERIFY(legend->rect().height() > wideHeight);
    QVERIFY(qAbs(legend->rect().height() - narrowHeight) < 1.);
    QVERIFY(qAbs(legend->rect().width() - 70.) < .1);
    QVERIFY(QLineF(legend->pos(), position).length() < .01);
    auto* flowTimer = legend->findChild<QTimer*>(QStringLiteral("ka_legend_flow_timer"));
    QVERIFY(flowTimer);
    QSignalSpy flowTimeouts(flowTimer, &QTimer::timeout);
    const double beforeMoveHeight = legend->rect().height();
    const QPointF movedPosition(35., 28.);
    legend->attemptMove(QgsLayoutPoint(movedPosition.x(), movedPosition.y()));
    QTest::qWait(250);
    QCOMPARE(flowTimeouts.count(), 0);
    QCOMPARE(legend->columnCount(), narrowColumns);
    QVERIFY(qAbs(legend->rect().height() - beforeMoveHeight) < .01);
    QVERIFY(QLineF(legend->pos(), movedPosition).length() < .01);
    int checked = 0;
    for (auto* treeLayer : legend->model()->rootGroup()->findLayers()) {
      for (auto* node : legend->model()->layerLegendNodes(treeLayer)) {
        QVERIFY(retained.contains(QPointer<QgsLayerTreeModelLegendNode>(node)));
        auto* symbolNode = dynamic_cast<QgsSymbolLegendNode*>(node);
        QVERIFY(symbolNode && symbolNode->customSymbol());
        QString badge;
        for (int i = 0; i < symbolNode->customSymbol()->symbolLayerCount(); ++i) {
          if (auto* font = dynamic_cast<QgsFontMarkerSymbolLayer*>(symbolNode->customSymbol()->symbolLayer(i)))
            badge = font->character();
        }
        bool matches = false;
        for (const auto& entry : numbers.entries()) {
          if (entry.layerId == treeLayer->layerId() && entry.name == node->data(Qt::DisplayRole).toString()) {
            QCOMPARE(badge, QString::number(entry.number));
            QCOMPARE(symbolNode->customSymbol()->color(), entry.color);
            matches = true;
            break;
          }
        }
        QVERIFY(matches);
        ++checked;
      }
    }
    QCOMPARE(checked, 43);
    for (const auto& node : retained) QVERIFY(!node.isNull());
    QCOMPARE(overridesChanged.count(), 0);
    QCOMPARE(numbers.revision(), revision);
    QgsMapLayerStyle afterLarge;
    afterLarge.readFromLayer(largeLayer);
    QgsMapLayerStyle afterSecond;
    afterSecond.readFromLayer(secondLayer);
    QCOMPARE(afterLarge.xmlData(), originalLarge.xmlData());
    QCOMPARE(afterSecond.xmlData(), originalSecond.xmlData());
    qInfo() << "LEGEND_FLOW widths_mm=70,190,70 columns=" << narrowColumns << wideColumns
            << "heights_mm=" << narrowHeight << wideHeight;
    // Removing a legend while its resize is pending must dispose of the timer
    // and disconnect callbacks that captured that layout item.
    QPointer<QgsLayoutItemLegend> deletedLegend(legend);
    QPointer<QTimer> deletedTimer(flowTimer);
    legend->attemptResize(QgsLayoutSize(190., beforeMoveHeight));
    QVERIFY(flowTimer->isActive());
    layout.removeLayoutItem(legend);
    QTRY_VERIFY_WITH_TIMEOUT(deletedLegend.isNull(), 1000);
    QVERIFY(deletedTimer.isNull());
    QTest::qWait(250);
    QCOMPARE(flowTimeouts.count(), 0);
  }
};

int main(int argc, char** argv) {
  QgsApplication app(argc, argv, false);
  const QString fonts =
      qEnvironmentVariable("WINDIR", QStringLiteral("C:/Windows")) + QStringLiteral("/Fonts/");
  for (const QString& file : {QStringLiteral("malgun.ttf"), QStringLiteral("malgunbd.ttf")})
    QFontDatabase::addApplicationFont(fonts + file);
  app.setFont(QFont(QStringLiteral("Malgun Gothic"), 9));
  QgsApplication::setPrefixPath(
      qEnvironmentVariable("QGIS_PREFIX_PATH", "D:/OSGeo4W/apps/qgis-dev"), true);
  QgsApplication::initQgis();
  HeritageStyleTest test;
  const int result = QTest::qExec(&test, argc, argv);
  QgsApplication::exitQgis();
  return result;
}
#include "test_heritage_style.moc"
