// MainWindow's drawing-session UI: on-screen 완료·되돌리기·취소 for pen and touch,
// 연속 그리기, the current-tool chip in the status bar, and keeping an unfinished sketch
// while another tab is open. Drawing itself lives in KaCaptureMapTool.
#include "KaAlignMapTool.h"
#include "KaAttributeMapTool.h"
#include "KaCaptureMapTool.h"
#include "KaDrawGuideBand.h"
#include "KaDrawRowRules.h"
#include "KaFeatureSelectTool.h"
#include "KaIcons.h"
#include "KaMeasureMapTool.h"
#include "KaSnapSettingsWidget.h"
#include "KaTrenchMoveTool.h"
#include "MainWindow.h"
#include "core/LayerOps.h"
#include "core/SnapTraceOps.h"

#include <QAction>
#include <QLabel>
#include <QMenu>
#include <QSignalBlocker>
#include <QStatusBar>
#include <QTabWidget>
#include <QToolBar>
#include <QToolButton>

#if KA_HGIS_HAS_QGIS
#include <qgsmapcanvas.h>
#include <qgsmaptoolemitpoint.h>
#include <qgsmaptoolpan.h>
#include <qgsvectorlayer.h>
#endif

void MainWindow::addDrawSketchButtons() {
#if KA_HGIS_HAS_QGIS
  if (!m_subToolbar) return;
  if (!property("kaSnapTraceInit").toBool())  // both snap helpers start on
    for (const char* name : {"kaSnapTraceInit", "kaSnapTrace", "kaSnapPreview"}) setProperty(name, true);
  QAction* easyAct = nullptr;
  for (QAction* act : m_subToolbar->actions())
    if (!easyAct && act && act->property("kaSubTool").toString() == QLatin1String("easy")) easyAct = act;
  auto runSnapToLines = [this]() -> bool {
    QString msg;
    const int n = SnapTraceOps::snapCanvasShapesToNearestLines(m_canvas, m_lastDrawnLayer, m_lastDrawnFid, &msg);
    if (m_featureSelectTool) m_featureSelectTool->refreshSelectedGeometry();
    LayerOps::refreshCanvasIfIdle(m_canvas);
    if (!msg.isEmpty()) statusBar()->showMessage(msg, 6000);
    return n > 0;
  };
  // 자석 보조 셋은 「자석」 옆 좁은 ▾(눌러서 여는 메뉴)에: 글자 단추 셋(409 px)이면 1904 px 창에서 줄 글자가 다 숨었다.
  auto* assist = new QToolButton(m_subToolbar);
  assist->setObjectName(QStringLiteral("btnSnapAssist"));
  assist->setArrowType(Qt::DownArrow);
  assist->setStyleSheet(QStringLiteral("QToolButton { padding-left: 2px; padding-right: 2px; }"));
  assist->setToolTip(QStringLiteral("자석 보조 — 선 따라 자동채움 · 근접 자석점 표시 · 가까운 선에 붙이기"));
  auto* assistMenu = new QMenu(assist);
  assistMenu->setToolTipsVisible(true);
  connect(assist, &QToolButton::clicked, assistMenu, [assist, assistMenu] { assistMenu->popup(assist->mapToGlobal(QPoint(0, assist->height()))); });
  (easyAct ? m_subToolbar->insertWidget(easyAct, assist) : m_subToolbar->addWidget(assist))->setProperty("kaSnapAssist", true);
  auto* snapNowAct = new QAction(QStringLiteral("가까운 선에 붙이기"), assistMenu);
  snapNowAct->setObjectName(QStringLiteral("actSnapToLines"));
  snapNowAct->setToolTip(QStringLiteral(
      "지적도·사업자 도면 위에 그린 도형(방금 그린 도형 또는 선택한 도형)을\n"
      "가장 가까운 지적선·도면선에 자동으로 붙이고 빠진 사이 꺾임점도 채웁니다 (Ctrl+Z 되돌리기 가능)."));
  connect(snapNowAct, &QAction::triggered, this, runSnapToLines);
  auto addSnapToggle = [&](const QString& text, const char* objName, const char* propName, bool isTrace,
                           const QString& tip, const QString& onMsg) {
    auto* act = new QAction(text, assistMenu);
    act->setObjectName(QString::fromLatin1(objName));
    act->setProperty(propName, true);
    act->setCheckable(true);
    assistMenu->addAction(act);
    act->setToolTip(tip);
    connect(act, &QAction::toggled, this, [this, propName, isTrace, text, onMsg, runSnapToLines](bool on) {
      setProperty(propName, on);
      if (on && !m_snapEnabled) {
        m_snapEnabled = true;
        applySnapConfig();
        if (auto* w = m_subToolbar ? m_subToolbar->findChild<KaSnapSettingsWidget*>() : nullptr) w->syncFromProject();
      }
      if (m_captureTool) isTrace ? m_captureTool->setSnapTrace(on) : m_captureTool->setSnapPreview(on);
      if (isTrace && runSnapToLines()) return;
      statusBar()->showMessage(on ? onMsg : (text + QStringLiteral(" 꺼짐")), 5000);
    });
  };
  addSnapToggle(QStringLiteral("선 따라 자동채움"), "actSnapTrace", "kaSnapTrace", true,
      QStringLiteral("1번 기능(선 따라 빠진 사이 자석점 자동 채우기)\n"
                     "· 그릴 때 자석점 한두 칸을 건너뛰어도 사이 꼭짓점을 선을 따라 자동으로 채우며,\n"
                     "  이미 그린 도형이 있을 때 누르면 가장 가까운 선에 바로 붙입니다."),
      QStringLiteral("선 따라 자동채움 켜짐 — 자석점을 한두 칸 건너뛰어 찍어도 사이 꼭짓점을 자동으로 채웁니다."));
  addSnapToggle(QStringLiteral("근접 자석점 표시"), "actSnapPreview", "kaSnapPreview", false,
                QStringLiteral("3번 기능(마우스가 가까이 갔을 때만 주변 자석점 미리 표시)\n"
                               "· 마우스가 선·꼭짓점에 가까이 갔을 때만 주변 자석점을 미리 보여 줍니다."),
                QStringLiteral("근접 자석점 표시 켜짐 — 마우스가 가까이 갔을 때만 주변 자석점을 미리 보여 줍니다."));
  assistMenu->addAction(snapNowAct);
  auto* finishAct = m_subToolbar->addAction(KaIcons::icon(QStringLiteral("check")), QStringLiteral("완료"));
  finishAct->setProperty("kaSketch", QStringLiteral("finish"));
  finishAct->setToolTip(QStringLiteral("그리던 도형을 마칩니다. 우클릭·Enter·더블클릭과 같습니다."));
  connect(finishAct, &QAction::triggered, this, [this]() {
    if (m_captureTool) m_captureTool->finishSketch();
  });
  auto* undoAct = m_subToolbar->addAction(KaIcons::icon(QStringLiteral("undo")), QStringLiteral("되돌리기"));
  undoAct->setProperty("kaSketch", QStringLiteral("undo"));
  undoAct->setToolTip(QStringLiteral("마지막 점 하나를 지웁니다. Ctrl+Z·Backspace와 같습니다."));
  connect(undoAct, &QAction::triggered, this, [this]() {
    if (m_captureTool && m_captureTool->undoLastVertex())
      statusBar()->showMessage(QStringLiteral("꼭짓점 하나를 되돌렸습니다."), 4000);
  });
  auto* cancelAct = m_subToolbar->addAction(KaIcons::icon(QStringLiteral("stop")), QStringLiteral("취소"));
  cancelAct->setProperty("kaSketch", QStringLiteral("cancel"));
  cancelAct->setToolTip(QStringLiteral("그리던 도형을 지웁니다. Esc와 같습니다. 이미 넣은 도형은 그대로입니다."));
  connect(cancelAct, &QAction::triggered, this, [this]() {
    if (m_captureTool) m_captureTool->cancelSketch();
  });
  auto* continuousAct = m_subToolbar->addAction(QStringLiteral("연속 그리기"));
  continuousAct->setProperty("kaSketch", QStringLiteral("continuous"));
  continuousAct->setCheckable(true);
  if (QWidget* toggle = m_subToolbar->widgetForAction(continuousAct))
    toggle->setProperty("kaToggle", true);  // oat when on, not slate
  continuousAct->setToolTip(
      QStringLiteral("켜면 도형을 마칠 때마다 이름·번호 창을 띄우지 않고 바로 다음 도형을 그립니다.\n"
                     "이름·번호는 나중에 도형을 우클릭해 「이 도형 기록 입력」으로 넣습니다.\n"
                     "끄면 도형을 마친 뒤 창이 뜨고, Esc로 건너뛸 수 있습니다."));
  connect(continuousAct, &QAction::toggled, this, [this](bool on) {
    m_continuousDraw = on;
    statusBar()->showMessage(on ? QStringLiteral("연속 그리기 — 이름·번호 창 없이 이어서 그립니다.")
                                : QStringLiteral("도형을 마칠 때마다 이름·번호 창을 띄웁니다(Esc로 건너뜀)."),
                             5000);
  });
  KaDrawRowRules::apply(m_subToolbar);  // 새 모양: a rule between the clusters of the drawing row
  syncDrawSketchButtons();
#endif
}

void MainWindow::syncDrawSketchButtons() {
#if KA_HGIS_HAS_QGIS
  const bool snapTrace = property("kaSnapTrace").toBool();
  const bool snapPreview = property("kaSnapPreview").toBool();
  if (m_captureTool) {
    if (m_captureTool->snapTrace() != snapTrace) m_captureTool->setSnapTrace(snapTrace);
    if (m_captureTool->snapPreview() != snapPreview) m_captureTool->setSnapPreview(snapPreview);
  }
  if (!m_subToolbar) return;
  const bool sketching =
      m_captureTool && m_canvas && m_canvas->mapTool() == m_captureTool && m_captureTool->hasSketch();
  for (QAction* action : m_subToolbar->actions()) {
    if (!action) continue;
    if (action->property("kaSnapAssist").toBool()) {  // the ▾ menu next to 「자석」
      auto* assist = qobject_cast<QToolButton*>(m_subToolbar->widgetForAction(action));
      auto* menu = assist ? assist->findChild<QMenu*>() : nullptr;
      for (QAction* item : menu ? menu->actions() : QList<QAction*>()) {
        const QSignalBlocker block(item);
        if (item->property("kaSnapTrace").toBool()) item->setChecked(snapTrace);
        if (item->property("kaSnapPreview").toBool()) item->setChecked(snapPreview);
      }
      continue;
    }
    const QString id = action->property("kaSketch").toString();
    if (id.isEmpty()) continue;
    if (id == QLatin1String("continuous")) {
      const QSignalBlocker block(action);
      action->setChecked(m_continuousDraw);
      continue;
    }
    // 완료·되돌리기·취소 show only while a shape is being drawn. Kept on the row as disabled
    // buttons they took 270 px, and at 1920 px the row folded 「겹친 곳 지우기」 and 「닫기」 away.
    action->setEnabled(sketching);
    action->setVisible(sketching);
  }
#endif
}

QString MainWindow::currentToolLabel() const {
#if KA_HGIS_HAS_QGIS
  if (!m_canvas) return {};
  QgsMapTool* tool = m_canvas->mapTool();
  if (m_captureTool && tool == m_captureTool) {
    QString target = m_editLayer ? m_editLayer->name() : QString();
    const QString key = m_editLayer ? LayerOps::layerKeyOf(m_editLayer) : QString();
    if (m_isSplittingPolygon)
      target = QStringLiteral("나누기 선");
    else if (m_captureTool->easyDraw())
      target = QStringLiteral("쉽게그리기");
    else if (key == QLatin1String("survey_area"))
      target = QStringLiteral("조사구역");
    else if (key == QLatin1String("feature_poly"))
      target = QStringLiteral("유구 면");
    else if (key == QLatin1String("feature_line"))
      target = QStringLiteral("유구 선");
    else if (key == QLatin1String("section_line"))
      target = QStringLiteral("단면선");
    else if (key == QLatin1String("control_points"))
      target = QStringLiteral("기준점");
    else if (key.startsWith(QLatin1String("artifact")))
      target = QStringLiteral("유물 위치");
    QString text = target.isEmpty() ? QStringLiteral("그리기") : QStringLiteral("그리기 · %1").arg(target);
    if (m_captureTool->pointCount() > 0) text += QStringLiteral(" · 점 %1").arg(m_captureTool->pointCount());
    return text;
  }
  if (m_featureSelectTool && tool == m_featureSelectTool) return QStringLiteral("도형선택");
  if (m_measureTool && tool == m_measureTool) return QStringLiteral("줄자");
  if (m_attributeTool && tool == m_attributeTool) return QStringLiteral("속성 편집");
  if ((m_alignTool && tool == m_alignTool) || (m_alignPickTool && tool == m_alignPickTool))
    return QStringLiteral("정합");
  if ((m_trenchOriginTool && tool == m_trenchOriginTool) || (m_trenchMoveTool && tool == m_trenchMoveTool))
    return QStringLiteral("시굴 격자");
  if (!tool || (m_panTool && tool == m_panTool)) return QStringLiteral("이동");
  return {};
#else
  return {};
#endif
}

// [P6] Glyph id for the guide band (KaDrawGuideBand); empty for 이동 / no tool, when the band
// folds to undo·redo. Same dispatch as currentToolLabel(); the hint words are in MainWindowChrome.cpp.
QString MainWindow::currentToolIconId() const {
#if KA_HGIS_HAS_QGIS
  if (!m_canvas) return {};
  QgsMapTool* tool = m_canvas->mapTool();
  if (m_captureTool && tool == m_captureTool) {
    const QString key = m_editLayer ? LayerOps::layerKeyOf(m_editLayer) : QString();
    if (key == QLatin1String("control_points")) return QStringLiteral("gps");
    if (key.startsWith(QLatin1String("artifact"))) return QStringLiteral("artifact");
    if (m_isSplittingPolygon || key == QLatin1String("feature_line") || key == QLatin1String("section_line"))
      return QStringLiteral("draw_line");
    return QStringLiteral("draw_poly");
  }
  if (m_featureSelectTool && tool == m_featureSelectTool) return QStringLiteral("select");
  if (m_measureTool && tool == m_measureTool) return QStringLiteral("measure");
  if (m_attributeTool && tool == m_attributeTool) return QStringLiteral("note");
  if ((m_alignTool && tool == m_alignTool) || (m_alignPickTool && tool == m_alignPickTool))
    return QStringLiteral("georef");
  if ((m_trenchOriginTool && tool == m_trenchOriginTool) || (m_trenchMoveTool && tool == m_trenchMoveTool))
    return QStringLiteral("trench_grid");
#endif
  return {};
}

void MainWindow::updateToolChip() {
#if KA_HGIS_HAS_QGIS
  if (!m_toolChip) {
    m_toolChip = new QLabel(statusBar());
    m_toolChip->setObjectName(QStringLiteral("toolChip"));
    m_toolChip->setToolTip(QStringLiteral("지금 켜져 있는 지도 도구입니다."));
    m_toolChip->setStyleSheet(QStringLiteral("QLabel#toolChip { padding: 0 8px; font-weight: 600; }"));
    statusBar()->insertPermanentWidget(0, m_toolChip);
    // The chip is the context: it goes before the canvas and the tools when the window closes.
    if (m_canvas)
      connect(m_canvas, &QgsMapCanvas::mapToolSet, m_toolChip, [this](QgsMapTool*, QgsMapTool*) { updateToolChip(); });
  }
  const bool onMap = m_viewTabs && m_mapPage && m_viewTabs->currentWidget() == m_mapPage;
  const QString label = onMap ? currentToolLabel() : QString();
  m_toolChip->setText(label);
  m_toolChip->setVisible(!label.isEmpty());
  // [P6] The guide band over the map says the same thing with a sentence; 이동 (no hint)
  // and other tabs fold it to the undo/redo buttons.
  if (m_drawGuide) {
    const QString hint = currentToolHint();
    m_drawGuide->setTool(currentToolIconId(), hint.isEmpty() ? QString() : label, hint);
  }
  syncDrawSketchButtons();
#endif
}

bool MainWindow::parkSketchForOtherTab() {
#if KA_HGIS_HAS_QGIS
  const bool keep = m_subToolsMode == QLatin1String("draw") && m_captureTool && m_canvas &&
                    m_canvas->mapTool() == m_captureTool && m_captureTool->hasSketch();
  if (keep) {
    // The capture tool stays on the hidden map with its points; only the map-only
    // drawing row goes away until the map tab is shown again. Nothing is saved.
    if (m_subToolbar) m_subToolbar->setVisible(false);
    statusBar()->showMessage(
        QStringLiteral("그리던 도형(점 %1개)은 지도 탭에 그대로 있습니다.").arg(m_captureTool->pointCount()), 5000);
  }
  updateToolChip();
  return keep;
#else
  return false;
#endif
}

void MainWindow::resumeParkedSketch() {
#if KA_HGIS_HAS_QGIS
  if (m_subToolbar && m_subToolsMode == QLatin1String("draw") && !m_subToolbar->isVisible()) {
    if (m_captureTool && m_canvas && m_canvas->mapTool() == m_captureTool) {
      m_subToolbar->setVisible(true);
      syncDrawSketchButtons();
      if (m_captureTool->hasSketch())
        statusBar()->showMessage(
            QStringLiteral("그리던 도형에 이어서 점을 찍으세요 (점 %1개).").arg(m_captureTool->pointCount()), 6000);
    } else {
      // The parked tool was stopped meanwhile (another survey opened): a hidden row
      // must not stay registered as the open drawing mode.
      hideSubTools();
    }
  }
  updateToolChip();
#endif
}
