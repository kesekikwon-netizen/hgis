// Click picking of 도형선택, and the inner pieces of a polygon (a hole, or a part inside
// another part) that a click can pick and Delete can take out. Split from
// KaFeatureSelectTool.cpp, which holds the mouse/keyboard flow and the right-click menu.
#include "KaFeatureSelectTool.h"

#include "KaCaptureMapTool.h"
#include "KaEditTolerance.h"
#include "KaPickLayers.h"
#include "KaVertexEditTool.h"
#include "core/FeaturePick.h"
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

void KaFeatureSelectTool::installKeyShortcut(QWidget* window, QAction* selectAction,
                                             std::function<bool()> keyOwnedElsewhere) {
  if (!window || !selectAction) return;
  // One key, one action: a second QAction or QShortcut on A in this window would make Qt report
  // an ambiguous shortcut, and then neither fires.
  if (window->findChild<QAction*>(QStringLiteral("actSelectShapeKey"))) return;
  selectAction->setToolTip(
      QStringLiteral("그린 도형을 선택합니다 (A 또는 Ctrl+1). 단추를 다시 누르면 이동으로 돌아갑니다"));
  auto* key = new QAction(QStringLiteral("도형선택"), window);
  key->setObjectName(QStringLiteral("actSelectShapeKey"));
  key->setShortcut(QKeySequence(Qt::Key_A));
  key->setShortcutContext(Qt::WindowShortcut);
  QObject::connect(key, &QAction::triggered, window, [window, selectAction, keyOwnedElsewhere]() {
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
    // Tools that read keys themselves get the key: the tape keeps its own A (면적), and
    // 맞추기 (image alignment) is never left by a stray letter.
    if (tool && (tool->inherits("KaMeasureMapTool") || tool->inherits("KaAlignMapTool"))) {
      QKeyEvent press(QEvent::KeyPress, Qt::Key_A, Qt::NoModifier);
      tool->keyPressEvent(&press);
      return;
    }
    if (keyOwnedElsewhere && keyOwnedElsewhere()) return;
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

  // Every shape under the cursor in pick order (core/FeaturePick): survey shapes before
  // reference and cadastral ones, nearby lines and points, then the smallest polygon or inner
  // piece (a hole, or a part inside another part) holding the click, then nearby outlines.
  // Unsaved shapes (negative edit-buffer ids) and above-labels overlay layers are included.
  const QList<FeaturePick::Hit> hits = FeaturePick::candidates(
      kaPickLayers(mCanvas), mapPt, mCanvas->mapUnitsPerPixel() * KaEditTolerance::kFeaturePickPx,
      mCanvas->mapSettings().destinationCrs(), QgsProject::instance()->transformContext());

  QList<Pick> candidates;
  for (const FeaturePick::Hit& hit : hits) candidates.append({hit.layer->id(), hit.fid, hit.piece});
  int pick = 0;
  const bool samePlace = allowCycle && !addToSelection && !candidates.isEmpty() &&
                         m_lastPickIndex >= 0 && candidates == m_lastPickCandidates &&
                         (screenPos - m_lastPickPos).manhattanLength() <= KaEditTolerance::kClickSlopPx;
  if (samePlace) pick = (m_lastPickIndex + 1) % candidates.size();
  // Only a cycling click starts or continues a cycle: after a right-click pick the next left
  // click on the same spot must pick the best shape again, not the second one.
  m_lastPickPos = screenPos;
  m_lastPickCandidates = allowCycle ? candidates : QList<Pick>();
  m_lastPickIndex = (allowCycle && !candidates.isEmpty()) ? pick : -1;

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
  } else if (all.size() == 1 && !(m_vertex && m_vertex->hasTarget())) {
    emit statusMessage(QStringLiteral("참고 자료 도형은 고칠 수 없습니다(보기만)."));
  } else if (all.size() == 1) {
    emit statusMessage(QStringLiteral("수정점이 나왔습니다. 점을 끌어 옮기세요(Ctrl=자석). 점 우클릭은 삭제, 선 우클릭은 추가입니다."));
  } else if (all.size() == 2) {
    emit statusMessage(QStringLiteral("도형 2개 선택됨 (%1, %2) — [겹친 곳 지우기](우클릭)는 큰 도형에서 작은 도형 자리를 지우고, "
                                      "[폴리곤 나누기]는 겹친 자리를 새 도형으로 나눕니다.")
                           .arg(all[0].layer->name(), all[1].layer->name()));
  } else {
    emit statusMessage(QStringLiteral("도형 %1개 선택됨").arg(all.size()));
  }
}

