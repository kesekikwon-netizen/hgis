#include "KaAlignMapTool.h"
#include "core/GeorefQuality.h"
#include "core/LayerOps.h"

#include <qgsmapcanvas.h>
#include <qgsmaplayer.h>
#include <qgsvectorlayer.h>
#include <qgsrasterlayer.h>
#include <qgsrubberband.h>
#include <qgsvertexmarker.h>
#include <qgssnappingutils.h>
#include <qgspointlocator.h>
#include <qgsgeometry.h>
#include <qgsproject.h>
#include <qgslayertree.h>
#include <qgslayertreelayer.h>
#include <qgsmapmouseevent.h>
#include <qgsrectangle.h>
#include <qgsfeature.h>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QKeyEvent>
#include <QColor>

KaAlignPickTool::KaAlignPickTool(QgsMapCanvas* canvas)
    : QgsMapTool(canvas) {
  setCursor(Qt::CrossCursor);
}

KaAlignPickTool::~KaAlignPickTool() {
  delete m_snapMark;
  m_snapMark = nullptr;
}

QgsPointXY KaAlignPickTool::snapPoint(QgsMapMouseEvent* e, bool* snapped) {
  if (snapped) *snapped = false;
  QgsPointXY pt = e->mapPoint();
  if (canvas() && canvas()->snappingUtils()) {
    const QgsPointLocator::Match hit = canvas()->snappingUtils()->snapToMap(e->pos());
    if (hit.isValid()) {
      pt = hit.point();
      if (snapped) *snapped = true;
    }
  }
  return pt;
}

void KaAlignPickTool::updateSnapMark(const QgsPointXY& pt, bool snapped) {
  if (!canvas() || !snapped) {
    if (m_snapMark) m_snapMark->hide();
    return;
  }
  if (!m_snapMark) {
    m_snapMark = new QgsVertexMarker(canvas());
    m_snapMark->setIconType(QgsVertexMarker::ICON_CIRCLE);
    m_snapMark->setIconSize(16);
    m_snapMark->setPenWidth(2);
    m_snapMark->setColor(QColor(30, 103, 198));
    m_snapMark->setFillColor(QColor(255, 255, 255, 220));
  }
  m_snapMark->setCenter(pt);
  m_snapMark->show();
}

// canvasPressEvent / canvasReleaseEvent (pick or drag): KaAlignMapToolEdit.cpp

void KaAlignPickTool::canvasMoveEvent(QgsMapMouseEvent* e) {
  bool snapped = false;
  const QgsPointXY pt = snapPoint(e, &snapped);
  // While dragging a picked point the mark follows the cursor even without a snap.
  updateSnapMark(pt, snapped || m_dragIndex >= 0);
}

void KaAlignPickTool::deactivate() {
  if (m_snapMark) m_snapMark->hide();
  QgsMapTool::deactivate();
}

KaAlignMapTool::KaAlignMapTool(QgsMapCanvas* canvas)
    : QgsMapTool(canvas) {
  setCursor(Qt::CrossCursor);
}

KaAlignMapTool::~KaAlignMapTool() {
  clearMarks();
  delete m_snapMark;
  m_snapMark = nullptr;
}

QgsMapTool::Flags KaAlignMapTool::flags() const {
  return QgsMapTool::EditTool;
}

QgsCoordinateReferenceSystem KaAlignMapTool::workCrs() const {
  if (m_workCrs.isValid()) return m_workCrs;
  if (canvas()) return canvas()->mapSettings().destinationCrs();
  return {};
}

bool KaAlignMapTool::beginLayer(QgsMapLayer* layer, const QgsCoordinateReferenceSystem& workCrs,
                                QString* errorOut) {
  endSession();
  if (!GeorefService::isAlignableLayer(layer)) {
    if (errorOut) *errorOut = QStringLiteral("이 레이어는 맞출 수 없습니다");
    return false;
  }
  m_workCrs = workCrs;
  m_layer = layer;
  m_pairs.clear();
  m_affine = {};
  m_haveFrom = false;
  m_phase = Phase::WaitFrom;

  if (auto* rl = qobject_cast<QgsRasterLayer*>(layer)) {
    m_raster = true;
    m_rasterPath = rl->source();
    m_originalRasterCrs = rl->crs();
    m_pixelW = rl->width();
    m_pixelH = rl->height();
    LayerOps::setAlignPending(rl, true);
    if (m_pixelW < 2 || m_pixelH < 2) {
      if (errorOut) *errorOut = QStringLiteral("그림이 너무 작습니다");
      endSession();
      return false;
    }
  } else if (auto* vl = qobject_cast<QgsVectorLayer*>(layer)) {
    m_raster = false;
    if (vl->providerType() != QLatin1String("memory")) {
      QString err;
      auto* mem = GeorefService::cloneToMemory(
          vl, vl->name() + QStringLiteral(" 맞춤"), &err);
      if (!mem) {
        if (errorOut) *errorOut = err;
        endSession();
        return false;
      }
      LayerOps::markReferenceLayer(mem);
      if (vl->customProperty(QStringLiteral("ka_hgis/imported_reference")).toBool() ||
          GeorefService::isCadPath(vl->source().section(QLatin1Char('|'), 0, 0)))
        mem->setCustomProperty(QStringLiteral("ka_hgis/imported_reference"), true);
      LayerOps::applySimpleVectorStyle(mem, QColor(0, 0, 0, 0), QColor(0, 0, 0), 0.2, 3.5, true,
                                       false);
      if (QgsProject* proj = QgsProject::instance()) {
        proj->addMapLayer(mem, true);
        if (QgsLayerTreeLayer* node = proj->layerTreeRoot()->findLayer(vl->id()))
          node->setItemVisibilityChecked(false);
      }
      m_hiddenSource = vl;
      m_layer = mem;
      vl = mem;
    } else {
      QString err;
      m_displayClone = GeorefService::cloneToMemory(
          vl, vl->name() + QStringLiteral(" 원본"), &err);
    }
    LayerOps::markReferenceLayer(vl);
    captureOriginals(vl);
    if (m_originals.isEmpty()) {
      if (errorOut) *errorOut = QStringLiteral("도면에 도형이 없습니다");
      endSession();
      return false;
    }
  }
  if (m_layer) m_layer->setOpacity(0.72);
  emit statusChanged(statusText());
  return true;
}

void KaAlignMapTool::captureOriginals(QgsVectorLayer* vl) {
  m_originals.clear();
  if (!vl) return;
  QgsFeatureIterator it = vl->getFeatures();
  QgsFeature f;
  while (it.nextFeature(f)) {
    if (f.hasGeometry()) m_originals.insert(f.id(), f.geometry());
  }
}

void KaAlignMapTool::endSession() {
  clearMarks();
  if (m_displayClone) {
    delete m_displayClone;
    m_displayClone = nullptr;
  }
  m_layer.clear();
  m_hiddenSource.clear();
  m_pairs.clear();
  m_originals.clear();
  m_affine = {};
  m_haveFrom = false;
  m_phase = Phase::Idle;
  m_raster = false;
  m_rasterPath.clear();
  // The backup folder stays on disk; only this session's link to it ends.
  m_rasterBackup = GeorefBackup::RasterBackup();
  m_originalRasterCrs = QgsCoordinateReferenceSystem();
  m_savedVectorPath.clear();
  m_dragIndex = -1;
}

QgsMapLayer* KaAlignMapTool::sourceDisplayLayer() const {
  if (m_hiddenSource) return m_hiddenSource;
  if (m_displayClone) return m_displayClone;
  return nullptr;
}

void KaAlignMapTool::setMapHint(double mx, double my, bool valid) {
  m_hintX = mx;
  m_hintY = my;
  m_hasHint = valid;
}

void KaAlignMapTool::setSourcePoint(double sx, double sy) {
  m_srcX = sx;
  m_srcY = sy;
  m_haveFrom = true;
  m_phase = Phase::WaitTo;
  emit statusChanged(statusText());
  emit pairsChanged();
}

bool KaAlignMapTool::hasSession() const {
  return m_layer && m_phase != Phase::Idle;
}

void KaAlignMapTool::clearMarks() {
  if (m_rubber) {
    m_rubber->reset(Qgis::GeometryType::Line);
    delete m_rubber;
    m_rubber = nullptr;
  }
  for (QgsVertexMarker* m : m_marks) delete m;
  m_marks.clear();
}

void KaAlignMapTool::rebuildPairMarks() {
  for (QgsVertexMarker* m : m_marks) delete m;
  m_marks.clear();
  QgsMapCanvas* c = canvas();
  if (!c) return;
  auto addMark = [&](const QgsPointXY& p, const QColor& col) {
    auto* mk = new QgsVertexMarker(c);
    mk->setIconType(QgsVertexMarker::ICON_CROSS);
    mk->setIconSize(12);
    mk->setPenWidth(2);
    mk->setColor(col);
    mk->setCenter(p);
    mk->show();
    m_marks.append(mk);
  };
  for (int i = 0; i < m_pairs.size(); ++i) {
    addMark(QgsPointXY(m_pairs[i].mapX, m_pairs[i].mapY), QColor(220, 38, 38));
  }
}

bool KaAlignMapTool::fitToDisplay() {
  if (!m_layer || !canvas()) return false;
  const QgsRectangle ext = canvas()->extent();
  if (ext.isEmpty() || !ext.isFinite()) return false;
  if (m_raster) {
    m_affine = GeorefService::fitRasterToExtent(m_pixelW, m_pixelH, ext);
  } else {
    QgsRectangle srcBox;
    bool first = true;
    for (auto it = m_originals.constBegin(); it != m_originals.constEnd(); ++it) {
      const QgsRectangle b = it.value().boundingBox();
      if (b.isEmpty()) continue;
      if (first) {
        srcBox = b;
        first = false;
      } else {
        srcBox.combineExtentWith(b);
      }
    }
    if (first) return false;
    m_affine = GeorefService::fitSrcBoxToExtent(srcBox.xMinimum(), srcBox.yMinimum(),
                                                srcBox.xMaximum(), srcBox.yMaximum(), ext);
  }
  applyPreview();
  emit statusChanged(statusText());
  return m_affine.valid;
}

bool KaAlignMapTool::applyPreview(QString* errorOut) {
  if (!m_layer || !m_affine.valid) {
    if (errorOut) *errorOut = QStringLiteral("옮길 변환이 없습니다");
    return false;
  }
  if (canvas())
    canvas()->freeze(true);
  QString err;
  bool ok = true;
  if (m_raster) {
    auto* rl = qobject_cast<QgsRasterLayer*>(m_layer.data());
    ok = rl && ensureRasterBackup(&err)
         && GeorefService::applyWorldFileToRaster(rl, m_affine, workCrs(), &err);
    if (ok && rl)
      GeorefService::styleAlignedRasterOverlay(rl);
  } else {
    auto* vl = qobject_cast<QgsVectorLayer*>(m_layer.data());
    ok = vl && GeorefService::applyAffineToVector(vl, m_affine, m_originals, workCrs(), &err);
    if (ok && vl) {
      LayerOps::applySimpleVectorStyle(vl, QColor(0, 0, 0, 0), QColor(30, 103, 198), 0.9, 4.0,
                                       true, false);
      vl->setOpacity(1.0);
    }
  }
  if (canvas()) {
    canvas()->freeze(false);
    LayerOps::refreshCanvasIfIdle(canvas());
  }
  rebuildPairMarks();
  if (!ok) {
    if (errorOut) *errorOut = err.isEmpty() ? QStringLiteral("도면을 옮기지 못했습니다") : err;
    if (!err.isEmpty()) emit statusChanged(err);
  }
  return ok;
}

bool KaAlignMapTool::removeLastPair() {
  if (m_haveFrom) {
    m_haveFrom = false;
    m_phase = Phase::WaitFrom;
    emit statusChanged(statusText());
    emit pairsChanged();
    return true;
  }
  if (m_pairs.isEmpty()) return false;
  return removePairAt(m_pairs.size() - 1);
}

bool KaAlignMapTool::removePairAt(int index) {
  if (index < 0 || index >= m_pairs.size()) return false;
  m_pairs.removeAt(index);
  m_haveFrom = false;
  m_phase = Phase::WaitFrom;
  m_affine = {};
  rebuildPairMarks();
  emit statusChanged(statusText());
  emit pairsChanged();
  return true;
}

// restoreOriginals: KaAlignMapToolEdit.cpp (restores the raster from its backup)

bool KaAlignMapTool::applyMove(QString* errorOut) {
  if (m_pairs.size() < 2) {
    if (errorOut) *errorOut = QStringLiteral("점을 2개 이상 찍은 뒤 「이동」을 누르세요");
    return false;
  }
  m_affine = GeorefService::fromPairs(m_pairs, m_raster);
  if (!m_affine.valid) {
    if (errorOut) *errorOut = QStringLiteral("점 배치로 변환을 만들 수 없습니다");
    return false;
  }
  if (m_raster) {
    auto* rl = qobject_cast<QgsRasterLayer*>(m_layer.data());
    if (!rl) {
      if (errorOut) *errorOut = QStringLiteral("그림 레이어가 없습니다");
      return false;
    }
    if (!ensureRasterBackup(errorOut)) return false;
    if (!GeorefService::persistAlignedRaster(rl, m_affine, workCrs(), errorOut)) {
      const QString src = rl->source();
      const QString name = rl->name();
      const bool importedReference =
          rl->customProperty(QStringLiteral("ka_hgis/imported_reference")).toBool();
      if (!QFile::exists(GeorefService::worldFilePathFor(src)))
        return false;
      QgsProject* proj = QgsProject::instance();
      if (!proj) return false;
      LayerOps::setAlignPending(rl, false);
      proj->removeMapLayer(rl->id());
      m_layer = nullptr;
      auto* neu = new QgsRasterLayer(src, name, QStringLiteral("gdal"));
      if (!neu->isValid() || GeorefService::looksUnreferencedRaster(neu)) {
        delete neu;
        if (errorOut)
          *errorOut = QStringLiteral("그림을 지도 좌표로 붙이지 못했습니다. 점을 다시 찍고 이동하세요.");
        return false;
      }
      if (workCrs().isValid()) neu->setCrs(workCrs());
      LayerOps::markReferenceLayer(neu);
      if (importedReference)
        neu->setCustomProperty(QStringLiteral("ka_hgis/imported_reference"), true);
      LayerOps::setAlignPending(neu, false);
      LayerOps::applyLegendCrsLabel(neu);
      GeorefService::styleAlignedRasterOverlay(neu);
      proj->addMapLayer(neu, true);
      m_layer = neu;
    } else {
      LayerOps::markReferenceLayer(rl);
      LayerOps::applyLegendCrsLabel(rl);
      GeorefService::styleAlignedRasterOverlay(rl);
    }
  } else if (!applyPreview(errorOut)) {
    return false;
  }
  if (auto* l = m_layer.data()) {
    LayerOps::setAlignPending(l, false);
    if (QgsProject* proj = QgsProject::instance()) {
      if (QgsLayerTreeLayer* n = proj->layerTreeRoot()->findLayer(l->id()))
        n->setItemVisibilityChecked(true);
    }
  }
  if (canvas()) {
    canvas()->freeze(false);
    LayerOps::refreshCanvasIfIdle(canvas());
  }
  rebuildPairMarks();
  emit statusChanged(QStringLiteral("이동 완료 · %1점").arg(m_pairs.size())
                     + (m_pairs.size() >= 4
                            ? QStringLiteral(" · 평균 어긋남 %1 m").arg(m_affine.rmsMeters, 0, 'f', 2)
                            : QString()));
  emit pairsChanged();
  return true;
}

bool KaAlignMapTool::saveAligned(QString* savedPath, QString* errorOut) {
  if (!m_layer) {
    if (errorOut) *errorOut = QStringLiteral("맞출 도면이 없습니다");
    return false;
  }
  if (m_pairs.size() < 2 && !m_affine.valid) {
    if (errorOut) *errorOut = QStringLiteral("점을 두 곳 이상 찍은 뒤 저장하세요");
    return false;
  }
  if (m_pairs.size() >= 2) {
    m_affine = GeorefService::fromPairs(m_pairs, m_raster);
    applyPreview();
  }
  if (m_raster) {
    auto* rl = qobject_cast<QgsRasterLayer*>(m_layer.data());
    if (!rl) return false;
    if (!ensureRasterBackup(errorOut)) return false;
    if (!GeorefService::persistAlignedRaster(rl, m_affine, workCrs(), errorOut))
      return false;
    LayerOps::markReferenceLayer(rl);
    LayerOps::applyLegendCrsLabel(rl);
    GeorefService::styleAlignedRasterOverlay(rl);
    LayerOps::setAlignPending(rl, false);
    if (QgsProject* proj = QgsProject::instance()) {
      if (QgsLayerTree* root = proj->layerTreeRoot()) {
        if (QgsLayerTreeLayer* n = root->findLayer(rl->id()))
          n->setItemVisibilityChecked(true);
      }
    }
    if (savedPath) *savedPath = GeorefService::worldFilePathFor(rl->source());
    return true;
  }
  auto* vl = qobject_cast<QgsVectorLayer*>(m_layer.data());
  if (!vl) return false;
  QString base = vl->name();
  base.replace(QStringLiteral(" 맞춤"), QString());
  QString dir;
  if (m_hiddenSource && !m_hiddenSource->source().isEmpty())
    dir = QFileInfo(m_hiddenSource->source().section(QLatin1Char('|'), 0, 0)).absolutePath();
  if (dir.isEmpty()) dir = QFileInfo(vl->source()).absolutePath();
  if (dir.isEmpty() || dir == QLatin1String(".")) dir = QDir::tempPath();
  // Never delete a file this session did not write: an older <name>_aligned.gpkg gets a
  // numbered sibling instead; saving again in the same session rewrites our own copy.
  if (m_savedVectorPath.isEmpty())
    m_savedVectorPath = GeorefBackup::uniqueOutputPath(
        dir + QLatin1Char('/') + QFileInfo(base).completeBaseName() + QStringLiteral("_aligned.gpkg"));
  const QString out = m_savedVectorPath;
  if (QFile::exists(out)) QFile::remove(out);
  const QString written = GeorefService::saveVectorCopyGpkg(vl, out, workCrs(), errorOut);
  if (written.isEmpty()) return false;
  if (savedPath) *savedPath = written;
  LayerOps::markReferenceLayer(vl);
  LayerOps::applyLegendCrsLabel(vl);
  return true;
}

QString KaAlignMapTool::statusText() const {
  if (!m_layer) return QStringLiteral("맞출 도면을 고르세요");
  if (m_phase == Phase::WaitTo)
    return QStringLiteral("오른쪽 지도에서 같은 모서리를 찍으세요");
  if (m_pairs.isEmpty())
    return QStringLiteral("왼쪽 도면 모서리 → 오른쪽 지적 모서리");
  // Residuals come from the points themselves, so they show before 「이동」 too.
  return QStringLiteral("왼쪽 → 오른쪽 · %1점").arg(m_pairs.size())
         + GeorefQuality::summary(GeorefQuality::assess(m_pairs, m_raster));
}

void KaAlignMapTool::updateSnapMark(const QgsPointXY& pt, bool snapped) {
  QgsMapCanvas* c = canvas();
  if (!c || !snapped) {
    if (m_snapMark) m_snapMark->hide();
    return;
  }
  if (!m_snapMark) {
    m_snapMark = new QgsVertexMarker(c);
    m_snapMark->setIconType(QgsVertexMarker::ICON_CIRCLE);
    m_snapMark->setIconSize(16);
    m_snapMark->setPenWidth(2);
    m_snapMark->setColor(QColor(30, 103, 198));
    m_snapMark->setFillColor(QColor(255, 255, 255, 220));
  }
  m_snapMark->setCenter(pt);
  m_snapMark->show();
}

bool KaAlignMapTool::mapPointFromEvent(QgsMapMouseEvent* e, QgsPointXY* out, bool* snapped) {
  if (!out) return false;
  if (snapped) *snapped = false;
  *out = e->mapPoint();
  bool did = false;
  QgsMapCanvas* c = canvas();
  if (c && c->snappingUtils()) {
    const QgsPointLocator::Match m = c->snappingUtils()->snapToMap(e->pos());
    if (m.isValid()) {
      *out = m.point();
      did = true;
      if (snapped) *snapped = true;
    }
  }
  updateSnapMark(*out, did);
  return did;
}

void KaAlignMapTool::canvasPressEvent(QgsMapMouseEvent* e) {
  if (!m_layer || e->button() != Qt::LeftButton) {
    if (e->button() == Qt::RightButton) {
      m_haveFrom = false;
      m_phase = Phase::WaitFrom;
      emit statusChanged(statusText());
    }
    return;
  }
  // Pressing on a finished pair's mark (while no left point waits) drags it.
  if (beginPairDrag(e)) return;
  if (m_phase != Phase::WaitTo || !m_haveFrom) {
    emit statusChanged(QStringLiteral("왼쪽 도면에서 먼저 모서리를 찍으세요"));
    return;
  }
  QgsPointXY mapPt;
  mapPointFromEvent(e, &mapPt, nullptr);
  GeorefService::Pair pr;
  pr.srcX = m_srcX;
  pr.srcY = m_srcY;
  pr.mapX = mapPt.x();
  pr.mapY = mapPt.y();
  m_pairs.append(pr);
  m_haveFrom = false;
  m_phase = Phase::WaitFrom;
  rebuildPairMarks();
  emit statusChanged(statusText());
  emit pairsChanged();
}

void KaAlignMapTool::canvasMoveEvent(QgsMapMouseEvent* e) {
  QgsPointXY mapPt;
  mapPointFromEvent(e, &mapPt, nullptr);
  if (m_dragIndex >= 0 && m_dragIndex < m_marks.size()) m_marks[m_dragIndex]->setCenter(mapPt);
  if (m_haveFrom && m_rubber) m_rubber->movePoint(mapPt);
  emit cursorMoved(mapPt);
}

void KaAlignMapTool::keyPressEvent(QKeyEvent* e) {
  if (e->key() == Qt::Key_Escape) {
    if (m_haveFrom) {
      m_haveFrom = false;
      m_phase = Phase::WaitFrom;
      if (m_rubber) m_rubber->reset(Qgis::GeometryType::Line);
      emit statusChanged(statusText());
      e->accept();
      return;
    }
  }
  if (e->key() == Qt::Key_Backspace || e->key() == Qt::Key_Delete) {
    removeLastPair();
    e->accept();
    return;
  }
  QgsMapTool::keyPressEvent(e);
}

void KaAlignMapTool::deactivate() {
  if (m_rubber) m_rubber->reset(Qgis::GeometryType::Line);
  QgsMapTool::deactivate();
}
