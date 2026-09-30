// MainWindow's drawing-session UI: on-screen 완료·되돌리기·취소 for pen and touch,
// 연속 그리기, the current-tool chip in the status bar, and keeping an unfinished sketch
// while another tab is open. Drawing itself lives in KaCaptureMapTool.
#include "MainWindow.h"
#include "KaAlignMapTool.h"
#include "KaAttributeMapTool.h"
#include "KaCaptureMapTool.h"
#include "KaDrawGuideBand.h"
#include "KaFeatureSelectTool.h"
#include "KaIcons.h"
#include "KaMeasureMapTool.h"
#include "KaTrenchMoveTool.h"
#include "core/LayerOps.h"

#include <QAction>
#include <QLabel>
#include <QSignalBlocker>
#include <QStatusBar>
#include <QTabWidget>
#include <QToolBar>

#if KA_HGIS_HAS_QGIS
#include <qgsmapcanvas.h>
#include <qgsmaptoolemitpoint.h>
#include <qgsmaptoolpan.h>
#include <qgsvectorlayer.h>
#endif

void MainWindow::addDrawSketchButtons() {
#if KA_HGIS_HAS_QGIS
  if (!m_subToolbar) return;
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
  continuousAct->setToolTip(QStringLiteral(
      "켜면 도형을 마칠 때마다 이름·번호 창을 띄우지 않고 바로 다음 도형을 그립니다.\n"
      "이름·번호는 나중에 도형을 우클릭해 「이 도형 기록 입력」으로 넣습니다.\n"
      "끄면 도형을 마친 뒤 창이 뜨고, Esc로 건너뛸 수 있습니다."));
  connect(continuousAct, &QAction::toggled, this, [this](bool on) {
    m_continuousDraw = on;
    statusBar()->showMessage(on ? QStringLiteral("연속 그리기 — 이름·번호 창 없이 이어서 그립니다.")
                                : QStringLiteral("도형을 마칠 때마다 이름·번호 창을 띄웁니다(Esc로 건너뜀)."),
                             5000);
  });
  syncDrawSketchButtons();
#endif
}

void MainWindow::syncDrawSketchButtons() {
#if KA_HGIS_HAS_QGIS
  if (!m_subToolbar) return;
  const bool sketching = m_captureTool && m_canvas && m_canvas->mapTool() == m_captureTool &&
                         m_captureTool->hasSketch();
  for (QAction* action : m_subToolbar->actions()) {
    const QString id = action ? action->property("kaSketch").toString() : QString();
    if (id.isEmpty()) continue;
    if (id == QLatin1String("continuous")) {
      const QSignalBlocker block(action);
      action->setChecked(m_continuousDraw);
      continue;
    }
    action->setEnabled(sketching);
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
    if (m_isSplittingPolygon) target = QStringLiteral("나누기 선");
    else if (m_captureTool->easyDraw()) target = QStringLiteral("쉽게그리기");
    else if (key == QLatin1String("survey_area")) target = QStringLiteral("조사구역");
    else if (key == QLatin1String("feature_poly")) target = QStringLiteral("유구 면");
    else if (key == QLatin1String("feature_line")) target = QStringLiteral("유구 선");
    else if (key == QLatin1String("section_line")) target = QStringLiteral("단면선");
    else if (key == QLatin1String("control_points")) target = QStringLiteral("기준점");
    else if (key.startsWith(QLatin1String("artifact"))) target = QStringLiteral("유물 위치");
    QString text = target.isEmpty() ? QStringLiteral("그리기") : QStringLiteral("그리기 · %1").arg(target);
    if (m_captureTool->pointCount() > 0)
      text += QStringLiteral(" · 점 %1").arg(m_captureTool->pointCount());
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
      connect(m_canvas, &QgsMapCanvas::mapToolSet, m_toolChip,
              [this](QgsMapTool*, QgsMapTool*) { updateToolChip(); });
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
    statusBar()->showMessage(QStringLiteral("그리던 도형(점 %1개)은 지도 탭에 그대로 있습니다.")
                                 .arg(m_captureTool->pointCount()),
                             5000);
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
        statusBar()->showMessage(QStringLiteral("그리던 도형에 이어서 점을 찍으세요 (점 %1개).")
                                     .arg(m_captureTool->pointCount()),
                                 6000);
    } else {
      // The parked tool was stopped meanwhile (another survey opened): a hidden row
      // must not stay registered as the open drawing mode.
      hideSubTools();
    }
  }
  updateToolChip();
#endif
}
