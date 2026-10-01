// Click picking of 도형선택, and the inner pieces of a polygon (a hole, or a part inside
// another part) that a click can pick and Delete can take out. Split from
// KaFeatureSelectTool.cpp, which holds the mouse/keyboard flow and the right-click menu.
#include "KaFeatureSelectTool.h"

#include "KaCaptureMapTool.h"
#include "KaEditTolerance.h"
#include "KaPickLayers.h"
#include "KaVertexEditTool.h"
#include "core/KaLogExcept.h"

#include <qgscoordinatetransform.h>
#include <qgsfeatureiterator.h>
#include <qgsfeaturerequest.h>
#include <qgsgeometry.h>
#include <qgsmapcanvas.h>
#include <qgsproject.h>
#include <qgsrubberband.h>
#include <qgsvectorlayer.h>

#include <QAbstractSpinBox>
#include <QAction>
#include <QApplication>
#include <QColor>
#include <QComboBox>
#include <QKeyEvent>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QTextEdit>
#include <algorithm>
#include <cmath>

bool KaFeatureSelectTool::hasActivePiece() const { return m_activePiece.isValid(); }

void KaFeatureSelectTool::setActivePiece(const PolygonPieces::Piece& piece) {
  m_activePiece = PolygonPieces::Piece();
  const QgsGeometry shape = (piece.isValid() && mCanvas && m_vertex && m_vertex->hasTarget())
                                ? PolygonPieces::outline(m_vertex->selectedGeometry(), piece)
                                : QgsGeometry();
  if (shape.isNull()) {
    delete m_pieceBand;
    m_pieceBand = nullptr;
    return;
  }
  m_activePiece = piece;
  if (!m_pieceBand) {
    // Same red as the picked vertex: this is what Delete removes.
    m_pieceBand = new QgsRubberBand(mCanvas, Qgis::GeometryType::Polygon);
    m_pieceBand->setColor(QColor(220, 38, 38));
    m_pieceBand->setWidth(3);
    m_pieceBand->setFillColor(QColor(220, 38, 38, 70));
  }
  m_pieceBand->setToGeometry(shape, m_vertex->layer());
}

std::optional<PolygonPieces::Piece> KaFeatureSelectTool::pieceUnder(const QgsPointXY& mapPt) const {
  if (!mCanvas || !m_vertex || !m_vertex->hasTarget()) return std::nullopt;
  const double tolerance = mCanvas->mapUnitsPerPixel() * KaEditTolerance::kFeaturePickPx;
  return PolygonPieces::innerPieceAt(m_vertex->selectedGeometry(), m_vertex->toLayer(mapPt), tolerance);
}

bool KaFeatureSelectTool::removeActivePiece(bool cutOut) {
  if (!m_activePiece.isValid() || !m_vertex || !m_vertex->hasTarget() || m_vertexDragging) return false;
  const QgsGeometry current = m_vertex->selectedGeometry();
  const QgsGeometry next = cutOut ? PolygonPieces::withPieceCutOut(current, m_activePiece)
                                  : PolygonPieces::withoutPiece(current, m_activePiece);
  setActivePiece({});
  // The key is taken either way: a failed piece edit must not fall through to deleting the whole shape.
  if (next.isNull() || next.isEmpty()) {
    emit statusMessage(QStringLiteral("안쪽 도형을 빼지 못했습니다. 도형을 다시 선택해 주세요."));
    return true;
  }
  const bool ok = m_vertex->replaceGeometry(next, cutOut ? QStringLiteral("안쪽 도형 빼기")
                                                         : QStringLiteral("안쪽 도형 삭제"));
  m_vertex->setActiveVertex(-1);  // vertex numbers changed with the rings
  m_vertex->showVertexMarkers();
  if (mCanvas) mCanvas->refresh();
  if (ok)
    emit statusMessage(cutOut ? QStringLiteral("안쪽 도형만큼 구멍을 냈습니다. Ctrl+Z로 되돌릴 수 있습니다.")
                              : QStringLiteral("안쪽 도형을 지웠습니다. Ctrl+Z로 되돌릴 수 있습니다."));
  else if (!m_vertex->lastEditError().isEmpty())
    emit statusMessage(m_vertex->lastEditError());
  return true;
}

bool KaFeatureSelectTool::deleteActivePick() { return deleteActiveVertex() || removeActivePiece(false); }

bool KaFeatureSelectTool::cutOutActivePiece() { return !m_activePiece.isHole() && removeActivePiece(true); }

void KaFeatureSelectTool::installKeyShortcut(QWidget* window, QAction* selectAction) {
  if (!window || !selectAction) return;
  selectAction->setToolTip(
      QStringLiteral("그린 도형을 선택합니다 (A 또는 Ctrl+1). 단추를 다시 누르면 이동으로 돌아갑니다"));
  auto* key = new QAction(QStringLiteral("도형선택"), window);
  key->setObjectName(QStringLiteral("actSelectShapeKey"));
  key->setShortcut(QKeySequence(Qt::Key_A));
  key->setShortcutContext(Qt::WindowShortcut);
  QObject::connect(key, &QAction::triggered, window, [window, selectAction]() {
    auto* canvas = window->findChild<QgsMapCanvas*>(QStringLiteral("mapCanvas"));
    if (!canvas || !canvas->isVisibleTo(window) || !selectAction->isEnabled()) return;
    // A letter typed into a field is text, never a tool change.
    const QWidget* focus = QApplication::focusWidget();
    const auto* combo = qobject_cast<const QComboBox*>(focus);
    if (qobject_cast<const QLineEdit*>(focus) || qobject_cast<const QAbstractSpinBox*>(focus) ||
        qobject_cast<const QTextEdit*>(focus) || qobject_cast<const QPlainTextEdit*>(focus) ||
        (combo && combo->isEditable()))
      return;
    QgsMapTool* tool = canvas->mapTool();
    if (qobject_cast<KaFeatureSelectTool*>(tool)) return;  // already on: A never turns it off
    if (tool && tool->inherits("KaMeasureMapTool")) {       // the tape keeps its own A (면적)
      QKeyEvent press(QEvent::KeyPress, Qt::Key_A, Qt::NoModifier);
      tool->keyPressEvent(&press);
      return;
    }
    // A half-drawn shape is never dropped by a stray key.
    if (const auto* capture = qobject_cast<KaCaptureMapTool*>(tool); capture && capture->hasSketch()) return;
    selectAction->trigger();
  });
  window->addAction(key);
}

void KaFeatureSelectTool::selectAtPoint(const QgsPointXY& mapPt, bool addToSelection,
                                        const QPoint& screenPos, bool allowCycle) {
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

  // Every survey shape under the cursor, not just the first one of the top layer:
  // lines and points by distance, then polygons by area so a pit inside a house wins.
  // An inner piece of a polygon (a hole, or a part inside another part) is a hit of its own.
  struct Hit {
    QgsVectorLayer* layer = nullptr;
    QgsFeatureId fid = FID_NULL;
    int tier = 0;
    double key = 0.0;
    PolygonPieces::Piece piece;
  };
  QVector<Hit> hits;
  const double mapTol = mCanvas->mapUnitsPerPixel() * KaEditTolerance::kFeaturePickPx;

  for (QgsMapLayer* ml : kaPickLayers(mCanvas)) {
    auto* vl = qobject_cast<QgsVectorLayer*>(ml);
    if (!isPickableLayer(vl)) continue;

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
        KA_LOG_EXCEPT();
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
    const bool polygonLayer = vl->geometryType() == Qgis::GeometryType::Polygon;

    while (it.nextFeature(f)) {
      if (!f.hasGeometry() || f.geometry().isEmpty()) continue;
      const QgsGeometry g = f.geometry();
      if (polygonLayer) {
        // Inside a hole nothing of the shape is under the cursor, yet the hole must be pickable.
        if (const auto piece = PolygonPieces::innerPieceAt(g, layerPt, layerTol))
          hits.append({vl, f.id(), 1, std::abs(PolygonPieces::outline(g, *piece).area()), *piece});
        if (!g.contains(layerProbe) && g.distance(layerProbe) > layerTol) continue;
        hits.append({vl, f.id(), 1, std::abs(g.area()), {}});
      } else {
        const double d = g.distance(layerProbe);
        if (d > layerTol) continue;
        hits.append({vl, f.id(), 0, d, {}});
      }
    }
  }
  std::stable_sort(hits.begin(), hits.end(), [](const Hit& a, const Hit& b) {
    return a.tier != b.tier ? a.tier < b.tier : a.key < b.key;
  });

  QList<Pick> candidates;
  for (const Hit& hit : hits) candidates.append({hit.layer->id(), hit.fid, hit.piece});
  int pick = 0;
  const bool samePlace = allowCycle && !addToSelection && !candidates.isEmpty() &&
                         m_lastPickIndex >= 0 && candidates == m_lastPickCandidates &&
                         (screenPos - m_lastPickPos).manhattanLength() <= KaEditTolerance::kClickSlopPx;
  if (samePlace) pick = (m_lastPickIndex + 1) % candidates.size();
  m_lastPickPos = screenPos;
  m_lastPickCandidates = candidates;
  m_lastPickIndex = candidates.isEmpty() ? -1 : pick;

  QgsVectorLayer* hitLayer = hits.isEmpty() ? nullptr : hits.at(pick).layer;
  // Any id counts, negative ones too: a shape drawn but not saved yet lives in the edit buffer.
  const QgsFeatureId hitFid = hits.isEmpty() ? FID_NULL : hits.at(pick).fid;
  if (hitLayer && !FID_IS_NULL(hitFid)) {
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
  // The piece is picked only with its own shape as the single selection.
  const bool pieceHit = !hits.isEmpty() && hits.at(pick).piece.isValid() && !addToSelection && all.size() == 1 &&
                        all[0].layer == hitLayer && all[0].fid == hitFid;
  setActivePiece(pieceHit ? hits.at(pick).piece : PolygonPieces::Piece());
  emit selectionChanged(all.size());

  if (all.isEmpty()) {
    emit statusMessage(QStringLiteral("선택된 도형 없음"));
  } else if (m_activePiece.isValid()) {
    emit statusMessage(QStringLiteral("안쪽 도형을 골랐습니다. Delete를 누르면 안쪽 도형만 지워집니다(바깥은 그대로). "
                                      "우클릭하면 구멍으로 뺄 수도 있습니다.") +
                       (candidates.size() > 1 ? QStringLiteral(" 같은 자리를 다시 누르면 도형 전체를 고릅니다.")
                                              : QString()));
  } else if (all.size() == 1 && candidates.size() > 1 && !addToSelection) {
    emit statusMessage(QStringLiteral("겹친 도형 %1/%2 — 같은 자리를 다시 누르면 다음 도형을 고릅니다.")
                           .arg(pick + 1)
                           .arg(candidates.size()));
  } else if (all.size() == 1) {
    emit statusMessage(QStringLiteral("수정점이 나왔습니다. 점을 끌어 옮기세요(Ctrl=자석). 점 우클릭은 삭제, 선 우클릭은 추가입니다."));
  } else if (all.size() == 2) {
    emit statusMessage(QStringLiteral("도형 2개 선택됨 (%1, %2) — [폴리곤 나누기] 클릭 시 겹치는 구간이 자동 분할됩니다!").arg(all[0].layer->name(), all[1].layer->name()));
  } else {
    emit statusMessage(QStringLiteral("도형 %1개 선택됨").arg(all.size()));
  }
}

