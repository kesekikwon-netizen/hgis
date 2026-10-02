#include "MainWindow.h"
#include "KaFeatureSelectTool.h"
#include "core/LayerOps.h"
#include "core/MeasureOps.h"
#include "core/PolygonErase.h"

#include <QCursor>
#include <QMenu>
#include <QMessageBox>
#include <QStatusBar>
#include <QUndoStack>

#if KA_HGIS_HAS_QGIS
#include <qgsfeature.h>
#include <qgslayertreeview.h>
#include <qgsmapcanvas.h>
#include <qgsproject.h>
#include <qgsvectorlayer.h>
#endif

// 「겹친 곳 지우기」: 위에 그린 면 모양대로 아래 면에서 그 자리만 지우고, 위에 그린 면은 없앤다.
// 조사구역 안의 제외 구역처럼 「안에 있는 도형을 빼는」 일에 쓴다.
void MainWindow::eraseOverlapWithShape() {
#if KA_HGIS_HAS_QGIS
  auto* project = QgsProject::instance();
  if (!project || !m_canvas) return;
  const QString title = QStringLiteral("겹친 곳 지우기");
  const auto say = [this, &title](const QString& text) {
    statusBar()->showMessage(text, 10000);
    notify(Notice::Info, title, text);
  };

  QList<PolygonErase::Shape> selected;
  const auto items = KaFeatureSelectTool::allSelectedFeatures(m_canvas);
  for (const auto& item : items) {
    PolygonErase::Shape shape;
    shape.layer = item.layer;
    shape.fid = item.fid;
    selected.append(shape);
  }

  // 아무것도 고르지 않고 눌렀으면 방금 그린 면을 골라서 보여 주기만 한다. 지우는 것은
  // 사용자가 그 도형이 맞는지 보고 한 번 더 눌렀을 때다.
  if (selected.isEmpty() && m_lastDrawnLayer && !FID_IS_NULL(m_lastDrawnFid) &&
      PolygonErase::editable(m_lastDrawnLayer) && m_lastDrawnLayer->getFeature(m_lastDrawnFid).isValid()) {
    m_lastDrawnLayer->selectByIds({m_lastDrawnFid});
    m_lastDrawnLayer->triggerRepaint();
    if (m_featureSelectTool) m_featureSelectTool->refreshSelectedGeometry();
    LayerOps::refreshCanvasIfIdle(m_canvas);
    say(QStringLiteral("방금 그린 도형을 골랐습니다. 이 모양대로 아래 도형에서 지우려면 "
                       "[겹친 곳 지우기]를 한 번 더 누르세요."));
    return;
  }

  const PolygonErase::Plan plan = PolygonErase::plan(selected, project);
  if (!plan.ready()) {
    say(plan.hint);
    return;
  }
  // 도형 하나로 지우는 것은 바로 한다. 여러 개를 한꺼번에 없애는 것은 끌어서 고르다 딸려 온
  // 도형까지 사라질 수 있으므로 먼저 묻는다.
  if (plan.cutters.size() > 1 &&
      QMessageBox::question(
          this, title,
          QStringLiteral("고른 도형 %1개의 자리를 「%2」의 가장 큰 도형에서 지우고, 그 %1개 도형은 없앱니다.\n"
                         "Ctrl+Z로 되돌릴 수 있습니다. 계속할까요?")
              .arg(plan.cutters.size())
              .arg(plan.targets.first().layer->name()),
          QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
    return;
  runErasePlan(plan);
#else
  statusBar()->showMessage(QStringLiteral("스텁: 겹친 곳 지우기"), 3000);
#endif
}

#if KA_HGIS_HAS_QGIS
bool MainWindow::runErasePlan(const PolygonErase::Plan& plan) {
  auto* project = QgsProject::instance();
  if (!project || !plan.ready()) return false;
  const QString title = QStringLiteral("겹친 곳 지우기");
  const auto areaOf = [project](const PolygonErase::Shape& shape) {
    const QgsFeature feature = shape.layer ? shape.layer->getFeature(shape.fid) : QgsFeature();
    return feature.hasGeometry() ? MeasureOps::geometryAreaSquareMeters(feature.geometry(), shape.layer->crs(),
                                                                         project->transformContext())
                                 : 0.0;
  };
  const PolygonErase::Shape first = plan.targets.first();
  const QPointer<QgsVectorLayer> firstLayer = first.layer;
  const QString layerName = firstLayer ? firstLayer->name() : QString();
  const double before = areaOf(first);

  forgetStaleLinkedEdits();
  const PolygonErase::Outcome outcome = PolygonErase::apply(plan, project);
  if (!outcome.ok) {
    statusBar()->showMessage(QStringLiteral("지우지 못했습니다. ") + outcome.error, 10000);
    notify(Notice::Warning, title, QStringLiteral("지우지 못했습니다."), outcome.error);
    return false;
  }
  rememberLinkedEdit(outcome.layers);

  project->setDirty(true);
  // Ctrl+Z 가 방금 한 일부터 되돌리도록 고친 레이어를 현재 레이어로 둔다.
  if (m_layerTree && firstLayer) m_layerTree->setCurrentLayer(firstLayer);
  if (m_featureSelectTool) m_featureSelectTool->refreshSelectedGeometry();
  if (m_canvas) LayerOps::refreshCanvasIfIdle(m_canvas);
  updateUndoRedoActions();

  QString text = QStringLiteral("「%1」 도형 %2개에서 겹친 자리를 지웠습니다.").arg(layerName).arg(outcome.erased);
  if (outcome.erased == 1 && outcome.added == 0 && firstLayer)
    text += QStringLiteral(" 면적 %L1 ㎡ → %L2 ㎡.").arg(before, 0, 'f', 2).arg(areaOf(first), 0, 'f', 2);
  if (outcome.added > 0)
    text += QStringLiteral(" 떨어진 조각 %1개는 따로 도형이 되었습니다.").arg(outcome.added);
  text += QStringLiteral(" 위에 그린 도형은 없앴습니다. Ctrl+Z로 되돌립니다.");
  statusBar()->showMessage(text, 10000);
  notify(Notice::Success, title, text);
  return true;
}

// 도형을 다 그렸을 때(우클릭·Enter), 같은 레이어의 도형 위에 겹쳐 그렸으면 그 자리에 선택창을
// 띄운다. 조사구역 안에 유구를 그리는 것처럼 다른 레이어의 도형 위에 그리는 것은 늘 하는
// 그리기라서 묻지 않는다. 선택창을 그냥 닫으면 「그대로 두기」와 같다. 지우기 직전에 beforeErase 를 부른다
// (그리기 묶음을 닫아, 지우기를 따로 한 단계로 되돌리게 한다).
bool MainWindow::offerEraseWithDrawnShape(QgsVectorLayer* layer, QgsFeatureId fid,
                                          const std::function<void()>& beforeErase) {
  auto* project = QgsProject::instance();
  if (!project || !layer) return false;
  PolygonErase::Shape drawn;
  drawn.layer = layer;
  drawn.fid = fid;
  const PolygonErase::Plan plan = PolygonErase::plan({drawn}, project);
  if (!plan.ready()) return false;
  for (const PolygonErase::Shape& target : plan.targets) {
    if (target.layer != layer) return false;
  }
  QMenu menu(this);
  menu.setObjectName(QStringLiteral("drawnShapeEraseMenu"));
  QAction* erase = menu.addAction(QStringLiteral("겹친 곳 지우기"));
  erase->setObjectName(QStringLiteral("actEraseDrawnOverlap"));
  QAction* keep = menu.addAction(QStringLiteral("그대로 두기"));
  keep->setObjectName(QStringLiteral("actKeepDrawnShape"));
  bool chosen = false;
  connect(erase, &QAction::triggered, &menu, [&chosen] { chosen = true; });
  QAction* picked = menu.exec(QCursor::pos());
  if (!chosen && picked != erase) return false;
  if (beforeErase) beforeErase();
  return runErasePlan(plan);
}

namespace {
// 그 편집이 아직 그 자리에 그대로 있는가. 저장하거나 다른 편집이 덮으면 달라진다.
bool sameCommandAt(const QUndoStack* stack, int position, const QUndoCommand* command, const QString& text) {
  return stack && position >= 0 && position < stack->count() && stack->command(position) == command &&
         stack->text(position) == text;
}
}  // namespace

// 새 편집을 넣기 전에 부른다. 저장하거나 다른 편집이 덮어 자리가 바뀐 묶음은 버리고,
// 되돌려 둔 묶음은 새 편집이 들어가면 다시 실행할 수 없으므로 모두 버린다.
void MainWindow::forgetStaleLinkedEdits() {
  auto* project = QgsProject::instance();
  m_linkedEdits.removeIf([project](const KaLinkedEdit& edit) {
    for (const auto& step : edit.steps) {
      auto* layer = qobject_cast<QgsVectorLayer*>(project->mapLayer(step.layerId));
      if (!layer || !sameCommandAt(layer->undoStack(), step.index - 1, step.command, step.text)) return true;
    }
    return false;
  });
  m_linkedRedos.clear();
}

// 여러 레이어를 한 번에 고친 편집을 묶어 둔다. 레이어마다 되돌리기 기록이 따로라서
// 묶지 않으면 Ctrl+Z 한 번에 반만 돌아온다(구멍은 메워졌는데 위 도형은 안 돌아오는 식).
void MainWindow::rememberLinkedEdit(const QList<QPointer<QgsVectorLayer>>& layers) {
  if (layers.size() < 2) return;
  KaLinkedEdit edit;
  for (const QPointer<QgsVectorLayer>& layer : layers) {
    QUndoStack* stack = layer ? layer->undoStack() : nullptr;
    if (!stack || stack->index() < 1) return;
    KaLinkedEdit::Step step;
    step.layerId = layer->id();
    step.index = stack->index();
    step.command = stack->command(step.index - 1);
    step.text = stack->text(step.index - 1);
    edit.steps.append(step);
  }
  m_linkedEdits.append(edit);
}

// layer 가 지금 되돌릴 편집이 묶음의 하나면 묶음 전체를 되돌린다.
bool MainWindow::undoLinkedEdit(QgsVectorLayer* layer) {
  auto* project = QgsProject::instance();
  if (!layer || !project) return false;
  for (int i = m_linkedEdits.size() - 1; i >= 0; --i) {
    const KaLinkedEdit edit = m_linkedEdits.at(i);
    QList<QgsVectorLayer*> members;
    bool mine = false;
    bool intact = true;
    for (const auto& step : edit.steps) {
      auto* member = qobject_cast<QgsVectorLayer*>(project->mapLayer(step.layerId));
      QUndoStack* stack = member ? member->undoStack() : nullptr;
      const bool onTop = stack && stack->index() == step.index &&
                         sameCommandAt(stack, step.index - 1, step.command, step.text);
      if (!onTop) intact = false;
      else members.append(member);
      if (member == layer && onTop) mine = true;
    }
    if (!mine) continue;
    m_linkedEdits.removeAt(i);
    if (!intact) return false;  // 짝이 이미 달라졌다. 이 레이어 것만 되돌린다.
    for (QgsVectorLayer* member : members) LayerOps::undoLayerEdits(member);
    m_linkedRedos.append(edit);
    return true;
  }
  return false;
}

// 되돌린 묶음을 다시 실행한다.
bool MainWindow::redoLinkedEdit(QgsVectorLayer* layer) {
  auto* project = QgsProject::instance();
  if (!layer || !project) return false;
  for (int i = m_linkedRedos.size() - 1; i >= 0; --i) {
    const KaLinkedEdit edit = m_linkedRedos.at(i);
    QList<QgsVectorLayer*> members;
    bool mine = false;
    bool intact = true;
    for (const auto& step : edit.steps) {
      auto* member = qobject_cast<QgsVectorLayer*>(project->mapLayer(step.layerId));
      QUndoStack* stack = member ? member->undoStack() : nullptr;
      const bool next = stack && stack->index() == step.index - 1 &&
                        sameCommandAt(stack, step.index - 1, step.command, step.text);
      if (!next) intact = false;
      else members.append(member);
      if (member == layer && next) mine = true;
    }
    if (!mine) continue;
    m_linkedRedos.removeAt(i);
    if (!intact) return false;
    for (QgsVectorLayer* member : members) LayerOps::redoLayerEdits(member);
    m_linkedEdits.append(edit);
    return true;
  }
  return false;
}
#endif
