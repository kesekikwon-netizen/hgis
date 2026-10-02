#include "KaCrashGuard.h"
#include "core/KaLogExcept.h"
#include "KaFeatureSelectTool.h"

#include "KaEditTolerance.h"
#include "KaPickLayers.h"
#include "KaVertexEditTool.h"
#include "core/FeaturePick.h"
#include "core/LayerOps.h"
#include "core/PolygonErase.h"

#include <qgsmapcanvas.h>
#include <qgsvectorlayer.h>
#include <qgsvectordataprovider.h>
#include <qgsabstractgeometry.h>
#include <qgsvertexid.h>
#include <qgsgeometry.h>
#include <qgsrubberband.h>
#include <qgsvertexmarker.h>
#include <qgsproject.h>
#include <qgscoordinatetransform.h>
#include <qgsfeaturerequest.h>
#include <qgsfeatureiterator.h>

#include <QApplication>
#include <QClipboard>
#include <QKeyEvent>
#include <QMenu>
#include <QTimer>
#include <QColor>
#include <algorithm>
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
  m_lastPickIndex = -1;  // the shapes under the last click may have changed
  // After an undo the vertex numbering may have changed under the highlight.
  if (m_vertex) m_vertex->setActiveVertex(-1);
  setActivePiece({});  // rings and parts may be numbered differently after the undo
  // Handles live only while 도형선택 is the map tool; activate() brings them back for the
  // selection then. Otherwise they would stay on the map while the user keeps drawing.
  if (!isActive()) {
    if (m_vertex) m_vertex->clearTarget();
    return;
  }
  syncVertexTarget();
}

bool KaFeatureSelectTool::isPickableLayer(const QgsVectorLayer* layer) {
  return FeaturePick::isSurveyLayer(layer);
}

bool KaFeatureSelectTool::deleteActiveVertex() {
  if (!m_vertex || !m_vertex->hasTarget() || m_vertexDragging) return false;
  const int idx = m_vertex->activeVertex();
  if (idx < 0) return false;
  if (m_vertex->deleteVertexAt(idx)) {
    m_vertex->showVertexMarkers();
    if (mCanvas) mCanvas->refresh();
    emit statusMessage(QStringLiteral("점을 지웠습니다. Ctrl+Z로 되돌릴 수 있습니다."));
  } else if (!m_vertex->lastEditError().isEmpty()) {
    emit statusMessage(m_vertex->lastEditError());
  }
  return true;
}

void KaFeatureSelectTool::showSnapMark(const QgsPointXY& mapPt, bool snapped) {
  if (!snapped || !mCanvas) {
    if (m_snapMark) m_snapMark->hide();
    return;
  }
  if (!m_snapMark) {
    m_snapMark = new QgsVertexMarker(mCanvas);
    m_snapMark->setIconType(QgsVertexMarker::ICON_CIRCLE);
    m_snapMark->setIconSize(14);
    m_snapMark->setPenWidth(2);
    m_snapMark->setColor(QColor(30, 103, 198));
    m_snapMark->setFillColor(QColor(255, 255, 255, 220));
  }
  m_snapMark->setCenter(mapPt);
  m_snapMark->show();
}

QgsPointXY KaFeatureSelectTool::dragPoint(QgsMapMouseEvent* e) {
  bool snapped = false;
  QgsPointXY pt = e->mapPoint();
  if (m_vertex && (e->modifiers() & Qt::ControlModifier))
    pt = m_vertex->snapMapPointExcludingTarget(e, &snapped);
  showSnapMark(pt, snapped);
  return pt;
}

void KaFeatureSelectTool::syncVertexTarget() {
  if (!m_vertex) return;
  const auto all = allSelectedFeatures(mCanvas);
  // Handles only for survey data: reference and cadastral shapes cannot be edited anyway.
  if (all.size() == 1 && all[0].layer && isPickableLayer(all[0].layer))
    m_vertex->setTarget(all[0].layer, all[0].fid);
  else
    m_vertex->clearTarget();
}

KaFeatureSelectTool::~KaFeatureSelectTool() {
  if (m_rubberBand) {
    delete m_rubberBand;
    m_rubberBand = nullptr;
  }
  delete m_snapMark;
  delete m_pieceBand;
}

void KaFeatureSelectTool::activate() {
  QgsMapTool::activate();
  if (mCanvas) {
    mCanvas->setCursor(Qt::ArrowCursor);
  }
  if (mCanvas) mCanvas->setContextMenuPolicy(Qt::PreventContextMenu);
  syncVertexTarget();
  emit statusMessage(QStringLiteral(
      "도형선택: 도형을 클릭하면 수정점이 나옵니다. 점을 끌어 옮기세요(Ctrl=자석). "
      "점 우클릭은 삭제, 선 우클릭은 점추가입니다."));
}

void KaFeatureSelectTool::deactivate() {
  if (m_rubberBand) {
    delete m_rubberBand;
    m_rubberBand = nullptr;
  }
  m_dragging = false;
  m_vertexDragging = false;
  m_vertexIndex = -1;
  m_lastPickIndex = -1;
  delete m_snapMark;
  m_snapMark = nullptr;
  setActivePiece({});
  if (m_vertex) m_vertex->clearTarget();
  QgsMapTool::deactivate();
}

void KaFeatureSelectTool::keyPressEvent(QKeyEvent* e) {
  if (!e) return;
  if ((e->key() == Qt::Key_Delete || e->key() == Qt::Key_Backspace) && deleteActivePick()) {
    e->accept();
    return;
  }
  if (e->key() == Qt::Key_Escape && m_vertex && (m_vertex->activeVertex() >= 0 || m_activePiece.isValid())) {
    m_vertex->setActiveVertex(-1);
    setActivePiece({});
    m_lastPickIndex = -1;
    e->accept();
    return;
  }
  QgsMapTool::keyPressEvent(e);
}

void KaFeatureSelectTool::canvasPressEvent(QgsMapMouseEvent* e) {
  if (e->button() != Qt::LeftButton) return;
  // 이미 고른 도형의 수정점을 눌렀으면 선택을 바꾸지 말고 그 점을 끈다.
  if (m_vertex && m_vertex->hasTarget()) {
    const int idx = m_vertex->vertexNear(e->mapPoint());
    if (idx >= 0) {
      m_vertexDragging = true;
      m_vertexMoved = false;
      m_vertexIndex = idx;
      m_vertexPressPos = e->pos();
      m_vertex->setActiveVertex(idx);
      setActivePiece({});  // one picked thing at a time: the vertex replaces the inner piece
      return;
    }
  }
  // Clicking anywhere but a handle lets go of the picked vertex.
  if (m_vertex) m_vertex->setActiveVertex(-1);
  m_pressPos = e->pos();
  m_pressMapPt = e->mapPoint();
  m_dragging = false;
}

void KaFeatureSelectTool::canvasMoveEvent(QgsMapMouseEvent* e) {
  if (m_vertexDragging && m_vertex) {
    // 기본은 커서 위치다. 자석을 그대로 쓰면 놓은 점이 원래 꼭짓점으로 다시 붙는다.
    // Ctrl을 누르고 있을 때만 끄는 도형 자신을 뺀 다른 도형에 붙인다.
    if (!m_vertexMoved &&
        (e->pos() - m_vertexPressPos).manhattanLength() <= KaEditTolerance::kClickSlopPx)
      return;
    m_vertexMoved = true;
    m_vertex->previewVertexMove(m_vertexIndex, m_vertex->toLayer(dragPoint(e)));
    return;
  }
  if (!(e->buttons() & Qt::LeftButton)) return;

  if (!m_dragging) {
    if ((e->pos() - m_pressPos).manhattanLength() > KaEditTolerance::kClickSlopPx) {
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

  // 끌던 수정점을 놓았다 — 여기서 한 번만 편집 버퍼에 넣는다.
  if (m_vertexDragging && m_vertex) {
    const int idx = m_vertexIndex;
    const bool moved = m_vertexMoved;
    m_vertexDragging = false;
    m_vertexMoved = false;
    m_vertexIndex = -1;
    if (idx >= 0 && !moved) {
      // A click on a handle only picks the vertex; it must not add a no-op edit.
      if (m_snapMark) m_snapMark->hide();
      emit statusMessage(QStringLiteral("점을 골랐습니다. Delete로 지우고, 끌면 옮깁니다."));
      return;
    }
    if (idx >= 0) {
      const QgsPointXY target = m_vertex->toLayer(dragPoint(e));
      if (m_snapMark) m_snapMark->hide();
      const bool ok = m_vertex->moveVertexTo(idx, target);
      m_vertex->showVertexMarkers();
      if (mCanvas) mCanvas->refresh();
      emit statusMessage(ok ? QStringLiteral("수정점을 옮겼습니다. Ctrl+Z로 되돌릴 수 있습니다.")
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
    selectAtPoint(e->mapPoint(), shift, e->pos(), true);
  }
}

void KaFeatureSelectTool::handleContextMenu(QgsMapMouseEvent* e) {
  if (!e || !mCanvas || !QgsProject::instance()) return;

  // 선택된 도형이 없으면 우클릭한 위치의 도형을 선택
  auto all = allSelectedFeatures(mCanvas);
  if (all.isEmpty()) {
    selectAtPoint(e->mapPoint(), false, e->pos(), false);
    all = allSelectedFeatures(mCanvas);
  }
  if (all.isEmpty()) {
    emit requestMapContextMenu(e->pos());
    return;
  }

  // Same rule as the handles and Delete (isPickableLayer): every survey layer, including
  // user_poly_*, paleo_landform and imported survey layers, gets the vertex and piece actions.
  bool hasEditableShapeType = false;
  const QPointer<QgsVectorLayer> firstLayer = all.first().layer;
  bool sameLayer = true;

  for (const auto& item : all) {
    if (item.layer != firstLayer) sameLayer = false;
    if (!item.layer || !item.layer->isValid()) continue;
    const auto type = item.layer->geometryType();
    if (isPickableLayer(item.layer))
      hasEditableShapeType |= type == Qgis::GeometryType::Line || type == Qgis::GeometryType::Polygon;
  }

  // 점추가·점삭제는 선에서도 써야 하므로 면적 계산과 따로 판단한다.
  syncVertexTarget();
  int vtxIdx = -1;
  int segAfter = -1;
  QgsPointXY onLine;
  if (hasEditableShapeType && all.size() == 1 && m_vertex && m_vertex->hasTarget()) {
    const QgsPointXY mapPt = e->mapPoint();
    vtxIdx = m_vertex->vertexNear(mapPt, KaEditTolerance::kVertexMenuPx);
    if (vtxIdx < 0)
      segAfter = m_vertex->segmentNear(mapPt, &onLine);
  }

  const auto editReason = [&firstLayer, sameLayer](Qgis::VectorProviderCapabilities required) {
    if (!sameLayer) return QStringLiteral("같은 레이어의 도형만 선택해 주세요.");
    if (!firstLayer || !firstLayer->isValid()) return QStringLiteral("도형이 있는 레이어를 다시 열어 주세요.");
    if (!isPickableLayer(firstLayer)) return QStringLiteral("조사 데이터의 도형을 선택해 주세요.");
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

  // An inner piece under the cursor (a hole, or a part inside another part) can be taken out whole.
  std::optional<PolygonPieces::Piece> piece;
  if (hasEditableShapeType && all.size() == 1 && m_vertex && m_vertex->hasTarget()) piece = pieceUnder(e->mapPoint());

  QMenu menu(mCanvas);
  QAction* vertexAct = nullptr;
  if (vtxIdx >= 0 || segAfter >= 0) {
    const QString& reason = vtxIdx >= 0 ? deleteReason : addReason;
    vertexAct = menu.addAction(vtxIdx >= 0 ? QStringLiteral("점삭제") : QStringLiteral("점추가"));
    vertexAct->setEnabled(reason.isEmpty());
    if (!reason.isEmpty()) vertexAct->setToolTip(reason);
  }
  QAction* removeAct = nullptr;
  QAction* cutAct = nullptr;
  if (piece) {
    if (vertexAct) menu.addSeparator();
    removeAct = menu.addAction(QStringLiteral("안쪽 도형 지우기"));
    if (!piece->isHole()) cutAct = menu.addAction(QStringLiteral("안쪽 도형만큼 구멍 내기"));
    for (QAction* act : {removeAct, cutAct}) {
      if (!act) continue;
      act->setEnabled(vertexReason.isEmpty());
      if (!vertexReason.isEmpty()) act->setToolTip(vertexReason);
    }
  }
  // 「겹친 곳 지우기」 lives on the draw row, which 도형선택 (A, ribbon) does not open; offer it
  // here too when no point or piece action is under the cursor.
  QAction* eraseAct = nullptr;
  bool eraseChosen = false;
  if (!vertexAct && !piece) {
    QList<PolygonErase::Shape> shapes;
    bool anyPolygon = false;
    for (const auto& item : all) {
      shapes.append({item.layer, item.fid});
      anyPolygon |= item.layer && item.layer->geometryType() == Qgis::GeometryType::Polygon;
    }
    const PolygonErase::Plan plan = anyPolygon ? PolygonErase::plan(shapes, QgsProject::instance())
                                               : PolygonErase::Plan();
    if (anyPolygon && (plan.ready() || all.size() >= 2)) {
      eraseAct = menu.addAction(QStringLiteral("겹친 곳 지우기"));
      eraseAct->setObjectName(QStringLiteral("actSelectEraseOverlap"));
      eraseAct->setEnabled(plan.ready());
      if (!plan.ready()) eraseAct->setToolTip(plan.hint);
      connect(eraseAct, &QAction::triggered, &menu, [&eraseChosen] { eraseChosen = true; });
    }
  }
  if (menu.isEmpty()) {
    emit statusMessage(QStringLiteral("점 위에서 우클릭하면 점삭제, 선 위에서 우클릭하면 점추가입니다."));
    return;
  }
  const QAction* chosen = menu.exec(mCanvas->mapToGlobal(e->pos()));
  if (eraseAct && (eraseChosen || chosen == eraseAct)) {
    emit requestEraseOverlap();
    return;
  }
  if (!chosen || !m_vertex) return;
  if (chosen == removeAct || chosen == cutAct) {
    setActivePiece(*piece);
    removeActivePiece(chosen == cutAct);
    return;
  }
  const bool deleting = vtxIdx >= 0;
  if (deleting ? m_vertex->deleteVertexAt(vtxIdx) : m_vertex->insertVertexAt(segAfter, onLine)) {
    m_vertex->showVertexMarkers();
    if (mCanvas) mCanvas->refresh();
    emit statusMessage(deleting ? QStringLiteral("점을 지웠습니다.") : QStringLiteral("점을 넣었습니다."));
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

  m_lastPickIndex = -1;  // a box selection is never the start of a click cycle

  // Same tiers as a click (core/FeaturePick): survey shapes first; reference and cadastral
  // shapes only when the box holds no survey shape. They are read-only and get no handles.
  const QgsGeometry mapGeom = QgsGeometry::fromRect(mapRect);
  QList<QPair<QgsVectorLayer*, QgsFeatureIds>> survey;
  QList<QPair<QgsVectorLayer*, QgsFeatureIds>> reference;
  for (QgsMapLayer* ml : kaPickLayers(mCanvas)) {
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
        KA_LOG_EXCEPT();
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
    if (!toSelect.isEmpty()) (isPickableLayer(vl) ? survey : reference).append({vl, toSelect});
  }
  for (const auto& [vl, toSelect] : survey.isEmpty() ? reference : survey) {
    vl->selectByIds(toSelect, Qgis::SelectBehavior::AddToSelection);
    vl->triggerRepaint();
  }

  mCanvas->refresh();
  auto all = allSelectedFeatures(mCanvas);
  syncVertexTarget();
  setActivePiece({});
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
