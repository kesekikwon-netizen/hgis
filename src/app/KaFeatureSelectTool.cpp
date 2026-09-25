#include "KaCrashGuard.h"
#include "KaFeatureSelectTool.h"

#include "KaVertexEditTool.h"
#include "core/LayerOps.h"
#include "core/MeasureOps.h"

#include <qgsmapcanvas.h>
#include <qgsvectorlayer.h>
#include <qgsvectordataprovider.h>
#include <qgsabstractgeometry.h>
#include <qgsvertexid.h>
#include <qgsgeometry.h>
#include <qgsrubberband.h>
#include <qgsproject.h>
#include <qgscoordinatetransform.h>
#include <qgsfeaturerequest.h>
#include <qgsfeatureiterator.h>

#include <QApplication>
#include <QClipboard>
#include <QMenu>
#include <QTimer>
#include <QColor>
#include <cmath>
#include <limits>

KaFeatureSelectTool::KaFeatureSelectTool(QgsMapCanvas* canvas)
    : QgsMapTool(canvas) {
  // 「도형수정」을 따로 켜지 않고, 도형을 고르면 곧바로 수정점이 나오게 한다.
  // 편집기는 지도 도구로 걸지 않고 기능만 빌려 쓴다.
  m_vertex = new KaVertexEditTool(canvas);
  m_vertex->setParent(this);
  connect(m_vertex, &KaVertexEditTool::statusMessage, this,
          [this](const QString& t) { emit statusMessage(t); });
  connect(m_vertex, &KaVertexEditTool::featureGeometryEdited, this,
          &KaFeatureSelectTool::featureGeometryEdited);
}

void KaFeatureSelectTool::setSnapEnabled(bool on) {
  if (m_vertex) m_vertex->setSnapEnabled(on);
}

void KaFeatureSelectTool::refreshSelectedGeometry() {
  m_vertexDragging = false;
  m_vertexIndex = -1;
  syncVertexTarget();
}

void KaFeatureSelectTool::syncVertexTarget() {
  if (!m_vertex) return;
  const auto all = allSelectedFeatures(mCanvas);
  if (all.size() == 1 && all[0].layer)
    m_vertex->setTarget(all[0].layer, all[0].fid);
  else
    m_vertex->clearTarget();
}

KaFeatureSelectTool::~KaFeatureSelectTool() {
  if (m_rubberBand) {
    delete m_rubberBand;
    m_rubberBand = nullptr;
  }
}

void KaFeatureSelectTool::activate() {
  QgsMapTool::activate();
  if (mCanvas) {
    mCanvas->setCursor(Qt::ArrowCursor);
  }
  emit statusMessage(QStringLiteral(
      "도형선택: 도형을 클릭하면 수정점이 나옵니다 — 점을 끌어 옮기고, 우클릭하면 "
      "점추가·점삭제·면적입니다. Shift+클릭은 추가 선택."));
}

void KaFeatureSelectTool::deactivate() {
  if (m_rubberBand) {
    delete m_rubberBand;
    m_rubberBand = nullptr;
  }
  m_dragging = false;
  m_vertexDragging = false;
  m_vertexIndex = -1;
  if (m_vertex) m_vertex->clearTarget();
  QgsMapTool::deactivate();
}

void KaFeatureSelectTool::canvasPressEvent(QgsMapMouseEvent* e) {
  if (e->button() != Qt::LeftButton) return;
  // 이미 고른 도형의 수정점을 눌렀으면 선택을 바꾸지 말고 그 점을 끈다.
  if (m_vertex && m_vertex->hasTarget()) {
    const int idx = m_vertex->vertexNear(e->mapPoint(), 20);
    if (idx >= 0) {
      m_vertexDragging = true;
      m_vertexIndex = idx;
      return;
    }
  }
  m_pressPos = e->pos();
  m_pressMapPt = e->mapPoint();
  m_dragging = false;
}

void KaFeatureSelectTool::canvasMoveEvent(QgsMapMouseEvent* e) {
  if (m_vertexDragging && m_vertex) {
    m_vertex->previewVertexMove(m_vertexIndex, m_vertex->toLayer(m_vertex->snapMapPoint(e)));
    return;
  }
  if (!(e->buttons() & Qt::LeftButton)) return;

  if (!m_dragging) {
    if ((e->pos() - m_pressPos).manhattanLength() > 4) {
      m_dragging = true;
      if (!m_rubberBand && mCanvas) {
        m_rubberBand = new QgsRubberBand(mCanvas, Qgis::GeometryType::Polygon);
        m_rubberBand->setColor(QColor(30, 103, 198, 180));
        m_rubberBand->setWidth(1);
        m_rubberBand->setFillColor(QColor(30, 103, 198, 40));
      }
    }
  }

  if (m_dragging && m_rubberBand) {
    QgsRectangle rect(m_pressMapPt, e->mapPoint());
    m_rubberBand->setToGeometry(QgsGeometry::fromRect(rect), nullptr);
  }
}

void KaFeatureSelectTool::canvasReleaseEvent(QgsMapMouseEvent* e) {
  if (e->button() == Qt::RightButton) {
    handleContextMenu(e);
    return;
  }
  if (e->button() != Qt::LeftButton) return;

  // 끌던 수정점을 놓았다 — 여기서 한 번만 저장한다.
  if (m_vertexDragging && m_vertex) {
    const int idx = m_vertexIndex;
    m_vertexDragging = false;
    m_vertexIndex = -1;
    if (idx >= 0) {
      const bool ok = m_vertex->moveVertexTo(idx, m_vertex->toLayer(m_vertex->snapMapPoint(e)));
      m_vertex->showVertexMarkers();
      if (mCanvas) mCanvas->refresh();
      emit statusMessage(ok ? QStringLiteral("수정점을 옮겼습니다.")
                            : (m_vertex->lastEditError().isEmpty()
                                   ? QStringLiteral("수정점을 옮기지 못했습니다.")
                                   : m_vertex->lastEditError()));
    }
    return;
  }

  const bool shift = (e->modifiers() & Qt::ShiftModifier);

  if (m_dragging) {
    m_dragging = false;
    if (m_rubberBand) {
      delete m_rubberBand;
      m_rubberBand = nullptr;
    }
    QgsRectangle mapRect(m_pressMapPt, e->mapPoint());
    selectInRect(mapRect, shift);
  } else {
    selectAtPoint(e->mapPoint(), shift);
  }
}

void KaFeatureSelectTool::handleContextMenu(QgsMapMouseEvent* e) {
  if (!e || !mCanvas || !QgsProject::instance()) return;

  // 선택된 도형이 없으면 우클릭한 위치의 도형을 선택
  auto all = allSelectedFeatures(mCanvas);
  if (all.isEmpty()) {
    selectAtPoint(e->mapPoint(), false);
    all = allSelectedFeatures(mCanvas);
  }
  if (all.isEmpty()) {
    emit requestMapContextMenu(e->pos());
    return;
  }

  const auto isSurveyLayer = [](const QgsVectorLayer* layer) {
    const QString key = LayerOps::layerKeyOf(layer);
    return key == QLatin1String("survey_area") || key == QLatin1String("feature_poly") ||
           key == QLatin1String("feature_line") || key == QLatin1String("section_line") ||
           key == QLatin1String("control_points") || key == QLatin1String("artifact_point") ||
           key == QLatin1String("trial_trench");
  };
  double totalAreaM2 = 0.0;
  int polyCount = 0;
  bool hasEditableShapeType = false;
  bool hasEditablePolygonType = false;
  const QPointer<QgsVectorLayer> firstLayer = all.first().layer;
  bool sameLayer = true;

  for (const auto& item : all) {
    if (item.layer != firstLayer) sameLayer = false;
    if (!item.layer || !item.layer->isValid()) continue;
    const auto type = item.layer->geometryType();
    if (isSurveyLayer(item.layer)) {
      hasEditableShapeType |= type == Qgis::GeometryType::Line || type == Qgis::GeometryType::Polygon;
      hasEditablePolygonType |= type == Qgis::GeometryType::Polygon;
    }
    if (type != Qgis::GeometryType::Polygon) continue;
    QgsFeature f;
    if (!item.layer->getFeatures(QgsFeatureRequest(item.fid)).nextFeature(f) || !f.hasGeometry())
      continue;
    QgsGeometry geom = f.geometry();
    if (geom.isEmpty()) continue;
    // Planimetric work-CRS area — same as tape / area($geometry) labels.
    totalAreaM2 += MeasureOps::geometryAreaSquareMeters(
        geom, item.layer->crs(), QgsProject::instance()->transformContext());
    polyCount++;
  }

  // 점추가·점삭제는 선에서도 써야 하므로 면적 계산과 따로 판단한다.
  syncVertexTarget();
  int vtxIdx = -1;
  int segAfter = -1;
  QgsPointXY onLine;
  if (hasEditableShapeType && all.size() == 1 && m_vertex && m_vertex->hasTarget()) {
    const QgsPointXY mapPt = m_vertex->snapMapPoint(e);
    vtxIdx = m_vertex->vertexNear(mapPt, 24);
    segAfter = m_vertex->segmentNear(mapPt, &onLine, 16);
  }

  const auto editReason = [&firstLayer, sameLayer, &isSurveyLayer](Qgis::VectorProviderCapabilities required) {
    if (!sameLayer) return QStringLiteral("같은 레이어의 도형만 선택해 주세요.");
    if (!firstLayer || !firstLayer->isValid()) return QStringLiteral("도형이 있는 레이어를 다시 열어 주세요.");
    if (!isSurveyLayer(firstLayer)) return QStringLiteral("조사 데이터의 도형을 선택해 주세요.");
    if (firstLayer->readOnly()) return QStringLiteral("읽기 전용 레이어는 수정할 수 없습니다.");
    const auto* provider = firstLayer->dataProvider();
    if (!provider || (provider->capabilities() & required) != required)
      return QStringLiteral("이 자료는 해당 편집 기능을 지원하지 않습니다.");
    return QString();
  };
  QString vertexReason = editReason(Qgis::VectorProviderCapability::ChangeGeometries);
  if (vertexReason.isEmpty() && all.size() != 1)
    vertexReason = QStringLiteral("꼭짓점을 고칠 도형 하나만 선택해 주세요.");
  QString addReason = vertexReason;
  if (addReason.isEmpty() && segAfter < 0)
    addReason = QStringLiteral("꼭짓점을 넣을 선이나 면의 가장자리에서 우클릭하세요.");
  QString deleteReason = vertexReason;
  if (deleteReason.isEmpty() && vtxIdx < 0)
    deleteReason = QStringLiteral("지울 꼭짓점 가까이에서 우클릭하세요.");
  if (deleteReason.isEmpty()) {
    const QgsGeometry geometry = m_vertex->selectedGeometry();
    QgsVertexId vertexId;
    if (!geometry.constGet() || !geometry.vertexIdFromVertexNr(vtxIdx, vertexId)) {
      deleteReason = QStringLiteral("지울 꼭짓점을 다시 선택해 주세요.");
    } else {
      const int count = geometry.constGet()->vertexCount(vertexId.part, vertexId.ring);
      const bool polygon = geometry.type() == Qgis::GeometryType::Polygon;
      if (count < (polygon ? 5 : 3))
        deleteReason = polygon ? QStringLiteral("면은 꼭짓점 3개보다 줄일 수 없습니다.")
                               : QStringLiteral("선은 꼭짓점 2개보다 줄일 수 없습니다.");
    }
  }

  const double pyeong = totalAreaM2 * 0.3025;
  QString areaStr;
  if (polyCount == 0) {
    areaStr.clear();
  } else if (polyCount == 1) {
    areaStr = QStringLiteral("면적: %L1 ㎡ (약 %L2평)")
                  .arg(totalAreaM2, 0, 'f', 1)
                  .arg(pyeong, 0, 'f', 1);
  } else {
    areaStr = QStringLiteral("선택한 면도형 %1개 총 면적: %L2 ㎡ (약 %L3평)")
                  .arg(polyCount)
                  .arg(totalAreaM2, 0, 'f', 1)
                  .arg(pyeong, 0, 'f', 1);
  }
  if (!areaStr.isEmpty())
    emit statusMessage(areaStr);

  QMenu menu(mCanvas);
  menu.setToolTipsVisible(true);
  const auto configure = [](QAction* action, const QString& reason, const QString& help) {
    action->setEnabled(reason.isEmpty());
    action->setToolTip(reason.isEmpty() ? help : reason);
  };

  if (!areaStr.isEmpty()) {
    auto* actArea = menu.addAction(areaStr);
    QFont boldFont = actArea->font();
    boldFont.setBold(true);
    actArea->setFont(boldFont);
    actArea->setEnabled(false);
    actArea->setToolTip(QStringLiteral("선택한 면도형의 면적입니다. 아래의 「면적 복사」를 사용하세요."));

    auto* actCopy = menu.addAction(QStringLiteral("면적 복사"));
    actCopy->setToolTip(QStringLiteral("선택한 면도형의 면적을 복사합니다."));
    connect(actCopy, &QAction::triggered, [areaStr]() {
      QApplication::clipboard()->setText(areaStr);
    });
  }
  auto* actDeselect = menu.addAction(QStringLiteral("선택 해제"));
  actDeselect->setToolTip(QStringLiteral("선택만 해제합니다. 도형은 그대로 남습니다."));
  connect(actDeselect, &QAction::triggered, [this]() {
    for (QgsMapLayer* l : QgsProject::instance()->mapLayers()) {
      if (auto* vl = qobject_cast<QgsVectorLayer*>(l)) {
        vl->removeSelection();
        vl->triggerRepaint();
      }
    }
    if (m_vertex) m_vertex->clearTarget();
    if (mCanvas) mCanvas->refresh();
    emit selectionChanged(0);
    emit statusMessage(QStringLiteral("선택 해제됨"));
  });

  if (hasEditableShapeType) {
    menu.addSeparator();
    auto* actAddVtx = menu.addAction(QStringLiteral("꼭짓점 추가"));
    configure(actAddVtx, addReason, QStringLiteral("우클릭한 가장자리에 꼭짓점을 넣습니다."));
    connect(actAddVtx, &QAction::triggered, this, [this, segAfter, onLine]() {
      if (m_vertex && m_vertex->insertVertexAt(segAfter, onLine)) {
        m_vertex->showVertexMarkers();
        if (mCanvas) mCanvas->refresh();
        emit statusMessage(QStringLiteral("꼭짓점을 넣었습니다."));
      }
    });
  }
  if (hasEditablePolygonType) {
    QString polygonReason = editReason(Qgis::VectorProviderCapability::ChangeGeometries |
        Qgis::VectorProviderCapability::AddFeatures | Qgis::VectorProviderCapability::DeleteFeatures);
    if (polygonReason.isEmpty() && polyCount != all.size())
      polygonReason = QStringLiteral("면도형만 선택해 주세요.");
    QString mergeReason = editReason(Qgis::VectorProviderCapability::AddFeatures |
        Qgis::VectorProviderCapability::DeleteFeatures);
    if (mergeReason.isEmpty() && polyCount != all.size())
      mergeReason = QStringLiteral("면도형만 선택해 주세요.");
    if (mergeReason.isEmpty() && polyCount < 2)
      mergeReason = QStringLiteral("합칠 면도형을 같은 레이어에서 2개 이상 선택해 주세요.");
    auto* actMerge = menu.addAction(QStringLiteral("면도형 합치기"));
    configure(actMerge, mergeReason, QStringLiteral("같은 레이어의 선택한 면도형을 하나로 합칩니다."));
    connect(actMerge, &QAction::triggered, this, [this]() {
      QTimer::singleShot(0, this, [this]() { emit requestMerge(); });
    });
    QString splitReason = polygonReason;
    if (splitReason.isEmpty() && (polyCount < 1 || polyCount > 2))
      splitReason = QStringLiteral("면도형 하나 또는 겹친 면도형 두 개를 선택해 주세요.");
    auto* actSplit = menu.addAction(QStringLiteral("면도형 나누기"));
    configure(actSplit, splitReason,
        QStringLiteral("묶인 면은 분리하고, 면 하나는 선으로 자릅니다. 면 두 개는 겹친 부분을 나눕니다."));
    connect(actSplit, &QAction::triggered, this, [this]() {
      QTimer::singleShot(0, this, [this]() { emit requestSplit(); });
    });
  }
  if (hasEditableShapeType) {
    menu.addSeparator();
    auto* actDelVtx = menu.addAction(QStringLiteral("꼭짓점 삭제"));
    configure(actDelVtx, deleteReason, QStringLiteral("우클릭한 꼭짓점을 지웁니다."));
    connect(actDelVtx, &QAction::triggered, this, [this, vtxIdx]() {
      if (m_vertex && m_vertex->deleteVertexAt(vtxIdx)) {
        m_vertex->showVertexMarkers();
        if (mCanvas) mCanvas->refresh();
        emit statusMessage(QStringLiteral("꼭짓점을 지웠습니다."));
      }
    });
  }

  menu.exec(QCursor::pos());
}

void KaFeatureSelectTool::selectAtPoint(const QgsPointXY& mapPt, bool addToSelection) {
  if (!mCanvas || !QgsProject::instance()) return;

  if (!addToSelection) {
    for (QgsMapLayer* l : QgsProject::instance()->mapLayers()) {
      if (auto* vl = qobject_cast<QgsVectorLayer*>(l)) {
        if (!vl->selectedFeatureIds().isEmpty()) {
          vl->removeSelection();
          vl->triggerRepaint();
        }
      }
    }
  }

  QList<QgsMapLayer*> layers = mCanvas->layers();
  QgsVectorLayer* hitLayer = nullptr;
  QgsFeatureId hitFid = -1;

  const double mapTol = mCanvas->mapUnitsPerPixel() * 10.0;

  for (QgsMapLayer* ml : layers) {
    auto* vl = qobject_cast<QgsVectorLayer*>(ml);
    if (!vl || !vl->isValid()) continue;

    QgsCoordinateTransform xf;
    const bool needXf = (mCanvas->mapSettings().destinationCrs() != vl->crs());
    if (needXf) {
      xf = QgsCoordinateTransform(mCanvas->mapSettings().destinationCrs(), vl->crs(),
                                  QgsProject::instance()->transformContext());
      xf.setBallparkTransformsAreAppropriate(true);
    }

    QgsPointXY layerPt = mapPt;
    if (needXf) {
      try {
        layerPt = xf.transform(mapPt);
      } catch (...) {
        KaCrashGuard::logLine(QStringLiteral("[except] app/KaFeatureSelectTool.cpp:405"));
        continue;
      }
    }

    const double layerTol = needXf ? (mapTol * (vl->crs().mapUnits() == Qgis::DistanceUnit::Degrees ? 0.00001 : 1.0)) : mapTol;
    const QgsRectangle searchBox(layerPt.x() - layerTol, layerPt.y() - layerTol,
                                 layerPt.x() + layerTol, layerPt.y() + layerTol);

    QgsFeatureRequest req;
    req.setFilterRect(searchBox);
    QgsFeatureIterator it = vl->getFeatures(req);
    QgsFeature f;
    const QgsGeometry layerProbe = QgsGeometry::fromPointXY(layerPt);

    while (it.nextFeature(f)) {
      if (!f.hasGeometry() || f.geometry().isEmpty()) continue;
      QgsGeometry g = f.geometry();

      if (vl->geometryType() == Qgis::GeometryType::Polygon) {
        if (g.contains(layerProbe) || g.distance(layerProbe) <= layerTol) {
          hitLayer = vl;
          hitFid = f.id();
          break;
        }
      } else {
        if (g.distance(layerProbe) <= layerTol) {
          hitLayer = vl;
          hitFid = f.id();
          break;
        }
      }
    }

    if (hitLayer && hitFid >= 0) break;
  }

  if (hitLayer && hitFid >= 0) {
    if (addToSelection && hitLayer->selectedFeatureIds().contains(hitFid)) {
      hitLayer->deselect(hitFid);
    } else {
      hitLayer->select(hitFid);
    }
    hitLayer->triggerRepaint();
  }

  mCanvas->refresh();

  auto all = allSelectedFeatures(mCanvas);
  syncVertexTarget();
  emit selectionChanged(all.size());

  if (all.isEmpty()) {
    emit statusMessage(QStringLiteral("선택된 도형 없음"));
  } else if (all.size() == 1) {
    emit statusMessage(QStringLiteral("도형 1개 선택됨 (%1) — Shift+클릭으로 다른 도형도 선택 가능").arg(all[0].layer->name()));
  } else if (all.size() == 2) {
    emit statusMessage(QStringLiteral("도형 2개 선택됨 (%1, %2) — [폴리곤 나누기] 클릭 시 겹치는 구간이 자동 분할됩니다!").arg(all[0].layer->name(), all[1].layer->name()));
  } else {
    emit statusMessage(QStringLiteral("도형 %1개 선택됨").arg(all.size()));
  }
}

void KaFeatureSelectTool::selectInRect(const QgsRectangle& mapRect, bool addToSelection) {
  if (!mCanvas || !QgsProject::instance()) return;

  if (!addToSelection) {
    for (QgsMapLayer* l : QgsProject::instance()->mapLayers()) {
      if (auto* vl = qobject_cast<QgsVectorLayer*>(l)) {
        if (!vl->selectedFeatureIds().isEmpty()) {
          vl->removeSelection();
          vl->triggerRepaint();
        }
      }
    }
  }

  const QgsGeometry mapGeom = QgsGeometry::fromRect(mapRect);
  for (QgsMapLayer* ml : mCanvas->layers()) {
    auto* vl = qobject_cast<QgsVectorLayer*>(ml);
    if (!vl || !vl->isValid()) continue;

    QgsCoordinateTransform xf;
    const bool needXf = (mCanvas->mapSettings().destinationCrs() != vl->crs());
    if (needXf) {
      xf = QgsCoordinateTransform(mCanvas->mapSettings().destinationCrs(), vl->crs(),
                                  QgsProject::instance()->transformContext());
      xf.setBallparkTransformsAreAppropriate(true);
    }

    QgsGeometry layerGeom = mapGeom;
    if (needXf) {
      try {
        layerGeom.transform(xf);
      } catch (...) {
        KaCrashGuard::logLine(QStringLiteral("[except] app/KaFeatureSelectTool.cpp:499"));
        continue;
      }
    }

    QgsFeatureRequest req;
    req.setFilterRect(layerGeom.boundingBox());
    QgsFeatureIterator it = vl->getFeatures(req);
    QgsFeature f;
    QgsFeatureIds toSelect;
    while (it.nextFeature(f)) {
      if (!f.hasGeometry()) continue;
      if (f.geometry().intersects(layerGeom)) {
        toSelect.insert(f.id());
      }
    }
    if (!toSelect.isEmpty()) {
      vl->selectByIds(toSelect, Qgis::SelectBehavior::AddToSelection);
      vl->triggerRepaint();
    }
  }

  mCanvas->refresh();
  auto all = allSelectedFeatures(mCanvas);
  syncVertexTarget();
  emit selectionChanged(all.size());
  emit statusMessage(QStringLiteral("도형 %1개 선택됨").arg(all.size()));
}

QList<KaFeatureSelectTool::SelectedItem> KaFeatureSelectTool::allSelectedFeatures(QgsMapCanvas* canvas) {
  QList<SelectedItem> list;
  Q_UNUSED(canvas);
  if (!QgsProject::instance()) return list;

  for (QgsMapLayer* l : QgsProject::instance()->mapLayers()) {
    auto* vl = qobject_cast<QgsVectorLayer*>(l);
    if (!vl || !vl->isValid()) continue;
    for (QgsFeatureId fid : vl->selectedFeatureIds()) {
      list.append({vl, fid});
    }
  }
  return list;
}
