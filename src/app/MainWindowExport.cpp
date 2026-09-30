#include "MainWindow.h"
#include "KaCrashGuard.h"
#include "KaIcons.h"
#include "KaTheme.h"
#include "core/ChecklistEngine.h"
#include "core/GeometryEditOps.h"  // [int W4] F083 merge conflict question
#include "core/LayerOps.h"
#include "core/LayoutService.h"
#include "core/MapGeoTiffExport.h"
#include "core/ProjectStateBuilder.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QProgressDialog>
#include <QSet>
#include <QStatusBar>
#include <QTabWidget>

#include "KaDrawingStudio.h"
#include "KaFeatureSelectTool.h"
#include "KaSectionDrawingStudio.h"
#include "KaTerrain3dLayoutStudio.h"
#include "KaTerrain3dStudio.h"
#include "core/Terrain3dLayoutService.h"
#include <QImage>
#include <QStandardPaths>
#include <algorithm>

#if KA_HGIS_HAS_QGIS
#include <qgis.h>
#include <qgscoordinatereferencesystem.h>
#include <qgsfeatureid.h>
#include <qgsgeometry.h>
#include <qgslayertree.h>
#include <qgslayertreeview.h>
#include <qgsmapcanvas.h>
#include <qgsmaplayer.h>
#include <qgsmapsettings.h>
#include <qgsproject.h>
#include <qgsrectangle.h>
#include <qgsvectorlayer.h>
#endif

int MainWindow::lastChecklistErrorCount() const {
  if (m_lastChecklistErrors >= 0) return m_lastChecklistErrors;
  if (!m_checklist) return -1;
  // A missing rule file blocks like an error; it must never read as "0 errors".
  if (m_checklist->ruleCount() == 0 && !m_checklist->loadRules(rulesPath())) return 1;
  return ChecklistEngine::failedCount(m_checklist->evaluate(buildProjectState()), QStringLiteral("error"));
}

void MainWindow::exportMapGeoTiff() {
#if KA_HGIS_HAS_QGIS
  if (!m_canvas || !m_viewTabs || m_viewTabs->currentWidget() != m_mapPage) return;
  // The canvas list leaves out survey shapes and heritage drawn above labels;
  // the GeoTIFF adds them back on top so the file matches the screen.
  const QList<QgsMapLayer*> aboveLabels = LayerOps::layersDrawnAboveLabels(QgsProject::instance());
  if (m_canvas->layers().isEmpty() && aboveLabels.isEmpty()) {
    QMessageBox::information(this, QStringLiteral("GeoTIFF 저장"),
                             QStringLiteral("지도에 저장할 레이어가 없습니다."));
    return;
  }
  QFileDialog dialog(this, QStringLiteral("현재 지도 GeoTIFF 저장"), preferredSurveyDir());
  dialog.setObjectName(QStringLiteral("mapGeoTiffSaveDialog"));
  dialog.setAcceptMode(QFileDialog::AcceptSave);
  dialog.setNameFilter(QStringLiteral("GeoTIFF (*.tif *.tiff)"));
  dialog.setDefaultSuffix(QStringLiteral("tif"));
  dialog.selectFile(QStringLiteral("지도_%1.tif")
                        .arg(m_canvas->mapSettings().destinationCrs().authid().replace(':', '_')));
  if (dialog.exec() != QDialog::Accepted || dialog.selectedFiles().isEmpty()) return;
  const QString path = dialog.selectedFiles().first();
  const QgsMapSettings snapshot = m_canvas->mapSettings();
  QProgressDialog progress(QStringLiteral("현재 지도를 GeoTIFF로 저장하고 있습니다…"),
                            QStringLiteral("취소"), 0, 0, this);
  progress.setWindowTitle(QStringLiteral("GeoTIFF 저장"));
  progress.setWindowModality(Qt::ApplicationModal);
  progress.setMinimumDuration(0);
  progress.setAutoClose(false);
  progress.show();
  QString error;
  const bool ok = MapGeoTiffExport::write(snapshot, path, &error,
                                         [&progress]() { return progress.wasCanceled(); }, aboveLabels);
  progress.hide();
  if (ok) {
    statusBar()->showMessage(QStringLiteral("GeoTIFF 저장 완료 · %1 · %2")
                                .arg(snapshot.destinationCrs().authid(), path), 15000);
  } else if (progress.wasCanceled()) {
    statusBar()->showMessage(QStringLiteral("GeoTIFF 저장을 취소했습니다."), 5000);
  } else {
    QMessageBox::warning(this, QStringLiteral("GeoTIFF 저장 실패"), error);
  }
#endif
}

void MainWindow::convertSelectedTo5179() {
#if KA_HGIS_HAS_QGIS
  QgsMapLayer* cur = m_layerTree ? m_layerTree->currentLayer() : nullptr;
  auto* vl = qobject_cast<QgsVectorLayer*>(cur);
  if (!vl || !vl->isValid()) {
    notify(Notice::Info, QStringLiteral("레이어 5179 변환"),
           QStringLiteral("지도 목록에서 변환할 레이어를 선택한 뒤 다시 누르세요. "
                          "SHP·조사도면.pdf·MANIFEST를 함께 내려면 「검수·제출」의 제출 꾸러미를 쓰세요."));
    return;
  }
  const QString startDir = preferredSurveyDir();
  const QString suggest = QDir(startDir.isEmpty() ? QDir::homePath() : startDir)
                              .filePath(vl->name() + QStringLiteral("_5179.shp"));
  const QString out = QFileDialog::getSaveFileName(
      this, QStringLiteral("EPSG:5179 SHP 저장 경로"),
      suggest, QStringLiteral("SHP (*.shp)"));
  if (out.isEmpty()) return;
  QString err;
  if (LayerOps::convertToShp5179(vl, out, QgsProject::instance(), &err, false).isEmpty())
    notify(Notice::Warning, QStringLiteral("레이어 5179 변환"), QStringLiteral("저장하지 못했습니다."), err);
  else {
    statusBar()->showMessage(QStringLiteral("5179 파일만 저장: %1").arg(QDir::toNativeSeparators(out)), 8000);
    notify(Notice::Success, QStringLiteral("레이어 5179 변환"),
           QStringLiteral("이 레이어만 파일로 저장했습니다. 지도에는 올리지 않았고, 검수·PDF·MANIFEST는 없습니다."),
           QDir::toNativeSeparators(out));
  }
#else
  QMessageBox::warning(this, QStringLiteral("CRS"), QStringLiteral("QGIS 빌드 필요"));
#endif
}

QJsonObject MainWindow::buildProjectState() const {
#if KA_HGIS_HAS_QGIS
  return ProjectStateBuilder::fromProject(QgsProject::instance());
#else
  QJsonObject st = ProjectStateBuilder::empty();
  st.insert(QStringLiteral("survey_area_count"), m_stubSurveyArea);
  st.insert(QStringLiteral("control_points_count"), m_stubGcp);
  st.insert(QStringLiteral("feature_poly_count"), m_stubFeatures);
  st.insert(QStringLiteral("project_crs_set"), true);
  st.insert(QStringLiteral("has_datum"), m_stubHasMeta);
  st.insert(QStringLiteral("has_ellipsoid"), m_stubHasMeta);
  st.insert(QStringLiteral("has_projection"), m_stubHasMeta);
  st.insert(QStringLiteral("has_kind_period"), true);
  st.insert(QStringLiteral("survey_is_polygon"), m_stubSurveyArea > 0);
  return st;
#endif
}

// Headless check (automatic QA and the dead work dock call it). The list with
// 「위치 보기」 lives in 「검수·제출」 (openSubmitReview).
void MainWindow::runChecklist() {
  if (!m_checklist) return;
  if (!ensureChecklistRules()) {
    m_lastChecklistErrors = 1;
    statusBar()->showMessage(QStringLiteral("검수 규칙 파일을 찾지 못했습니다. 제출 꾸러미를 만들 수 없습니다."), 8000);
    return;
  }
  const QVector<CheckResult> results = evaluateChecklist();
  statusBar()->showMessage(QStringLiteral("검수: 오류 %1 · 주의 %2 — 「검수·제출」에서 목록을 봅니다")
                               .arg(ChecklistEngine::failedCount(results, QStringLiteral("error")))
                               .arg(ChecklistEngine::failedCount(results, QStringLiteral("warn"))),
                           8000);
}

void MainWindow::exportPdf() {
#if KA_HGIS_HAS_QGIS
  openLayoutDesigner();
  if (m_drawingStudio)
    m_drawingStudio->savePdf();
#else
  QMessageBox::warning(this, QStringLiteral("도면"), QStringLiteral("QGIS 빌드 필요"));
#endif
}

// Kept for the layer menu and the work dock: same flow as the 「검수·제출」 package button.
// The checklist runs first; with an error or no rule file the review list opens instead.
void MainWindow::exportShpPackage() { makeSubmitPackage(); }

void MainWindow::exportReportLayout() {
#if KA_HGIS_HAS_QGIS
  openLayoutDesigner();
#else
  QMessageBox::warning(this, QStringLiteral("도면"), QStringLiteral("QGIS 빌드 필요"));
#endif
}
void MainWindow::printDrawing() {
#if KA_HGIS_HAS_QGIS
  // 도면 화면이 이미 앞에 있으면 그대로 찍는다. 다시 열면 사용자가 옮겨 둔 용지 보기가 바뀐다.
  const bool studioShown = m_drawingStudio && m_viewTabs && m_viewTabs->currentWidget() == m_drawingStudio;
  if (!studioShown) openLayoutDesigner();
  if (m_drawingStudio && m_viewTabs && m_viewTabs->currentWidget() == m_drawingStudio)
    m_drawingStudio->printDrawing();
#endif
}

void MainWindow::openLayoutDesigner() {
#if KA_HGIS_HAS_QGIS
  if (!m_viewTabs)
    return;
  if (m_terrain3dStudio && m_viewTabs->currentWidget() == m_terrain3dStudio) {
    placeTerrain3dOnSheet();
    return;
  }
  if (m_drawingStudio && m_viewTabs->indexOf(m_drawingStudio) >= 0) {
    m_viewTabs->setCurrentWidget(m_drawingStudio);
    hideSubTools();
    m_drawingStudio->applyFieldPageGrow();
    m_drawingStudio->applyFieldEdge();
    m_drawingStudio->showSheetPage();
    m_drawingStudio->refreshMapFromProject();
    onCanvasScaleChanged(m_canvas->scale());
    return;
  }
  double w = KaDrawingStudio::kA4PortraitWidthMm;
  double h = KaDrawingStudio::kA4PortraitHeightMm;
  if (!KaDrawingStudio::promptPaper(this, &w, &h))
    return;
  if (!m_drawingStudio) {
    m_drawingStudio = new KaDrawingStudio(QgsProject::instance(), m_canvas, w, h, this);
    m_drawingStudio->setAttribute(Qt::WA_DeleteOnClose, false);
    connect(m_drawingStudio, &KaDrawingStudio::drawingScaleChanged, this, [this](double scale) {
      if (m_viewTabs && m_viewTabs->currentWidget() == m_drawingStudio)
        onCanvasScaleChanged(scale);
    });
    // 도면 탭의 안내는 메인 상태줄 한 줄에 보인다. 탭 안에 두 번째 줄을 두지 않는다.
    connect(m_drawingStudio, &KaDrawingStudio::statusMessage, this, [this](const QString& text) {
      if (!m_viewTabs || m_viewTabs->currentWidget() != m_drawingStudio) return;
      m_drawingStudio->setProperty("kaLastStatus", text);
      statusBar()->showMessage(text);
      statusBar()->repaint();
    });
    // 도면 안내는 도면 탭의 것이다. 떠나면 지운다. 「축척을 1 : 25000 로 맞췄습니다」가
    // 지도 탭(1:469)과 홈에 남아 지금 축척처럼 읽혔다.
    connect(m_viewTabs, &QTabWidget::currentChanged, this, [this](int) {
      if (!m_drawingStudio || m_viewTabs->currentWidget() == m_drawingStudio) return;
      const QString last = m_drawingStudio->property("kaLastStatus").toString();
      if (!last.isEmpty() && statusBar()->currentMessage() == last) statusBar()->clearMessage();
      m_drawingStudio->setProperty("kaLastStatus", QString());
    });
  } else {
    m_drawingStudio->resetPaper(w, h);
  }
  m_drawingStudio->setParent(m_viewTabs, Qt::Widget);
  if (m_viewTabs->indexOf(m_drawingStudio) < 0)
    m_viewTabs->addTab(m_drawingStudio, KaIcons::icon(QStringLiteral("pdf"), KaTheme::tokens().inkMuted),
                       QStringLiteral("도면"));
  m_viewTabs->setCurrentWidget(m_drawingStudio);
  hideSubTools();
  m_drawingStudio->showSheetPage();
  m_drawingStudio->refreshMapFromProject();
  m_drawingStudio->centerOnMapCanvas();
  const QString welcome = QStringLiteral("도면 화면입니다. 좌표점은 용지 아래 아이콘으로 찍습니다.");
  m_drawingStudio->setProperty("kaLastStatus", welcome);
  statusBar()->showMessage(welcome);
#endif
}

int MainWindow::runUiStressLoop(int iterations) {
#if KA_HGIS_HAS_QGIS
  if (iterations <= 0 || !m_canvas || !m_viewTabs || !m_mapPage)
    return 1;
  loadBootBasemaps();
  QCoreApplication::processEvents();

  // 종이 선택 창 없이 A4로 조판 탭만 연다(자동 반복용).
  if (!m_drawingStudio) {
    m_drawingStudio =
        new KaDrawingStudio(QgsProject::instance(), m_canvas, KaDrawingStudio::kA4PortraitWidthMm,
                            KaDrawingStudio::kA4PortraitHeightMm, this);
    m_drawingStudio->setAttribute(Qt::WA_DeleteOnClose, false);
  }
  m_drawingStudio->setParent(m_viewTabs, Qt::Widget);
  if (m_viewTabs->indexOf(m_drawingStudio) < 0)
    m_viewTabs->addTab(m_drawingStudio, KaIcons::icon(QStringLiteral("pdf"), KaTheme::tokens().inkMuted),
                       QStringLiteral("도면"));

  auto toggleBasemaps = []() {
    QgsProject* proj = QgsProject::instance();
    QgsLayerTree* root = proj ? proj->layerTreeRoot() : nullptr;
    if (!root) return;
    for (QgsLayerTreeLayer* node : root->findLayers()) {
      if (!node || !node->layer() || !LayerOps::isBasemapLayer(node->layer())) continue;
      node->setItemVisibilityChecked(!node->itemVisibilityChecked());
    }
  };

  for (int i = 0; i < iterations; ++i) {
    toggleBasemaps();
    QCoreApplication::processEvents();

    m_viewTabs->setCurrentWidget(m_drawingStudio);
    hideSubTools();
    m_drawingStudio->showSheetPage();
    m_drawingStudio->refreshMapFromProject();
    QCoreApplication::processEvents();

    m_viewTabs->setCurrentWidget(m_mapPage);
    QCoreApplication::processEvents();

    persistSurveyWork();
    QCoreApplication::processEvents();

    m_canvas->zoomByFactor(1.25);
    QCoreApplication::processEvents();
    m_canvas->zoomByFactor(0.8);
    QCoreApplication::processEvents();

    KaCrashGuard::logLine(
        QStringLiteral("[stress-ui] %1/%2").arg(i + 1).arg(iterations));
  }
  return 0;
#else
  Q_UNUSED(iterations);
  return 1;
#endif
}

void MainWindow::placeTerrain3dOnSheet() {
#if KA_HGIS_HAS_QGIS
  if (!m_viewTabs || !m_terrain3dStudio)
    return;
  if (!m_terrain3dStudio->hasScene()) {
    QMessageBox::information(this, QStringLiteral("입체지형 도면출력"),
                             QStringLiteral("먼저 「화면을 입체로」로 지금 지도를 만드세요."));
    return;
  }
  const QString png = terrain3dSheetPngPath();
  const QImage view = m_terrain3dStudio->renderView(1600, 1000);
  if (view.isNull() || !view.save(png)) {
    QMessageBox::warning(this, QStringLiteral("입체지형 도면출력"),
                         QStringLiteral("입체지형 그림을 만들지 못했습니다."));
    return;
  }
  Terrain3dLayoutService::SheetSpec spec;
  spec.pngPath = png;
  double x0 = 0, y0 = 0, x1 = 0, y1 = 0;
  if (m_terrain3dStudio->groundExtent(&x0, &y0, &x1, &y1))
    spec.groundExtent = QgsRectangle(x0, y0, x1, y1);
  spec.crs.createFromUserInput(m_terrain3dStudio->workCrsLabel());
  spec.visibleWidthM = m_terrain3dStudio->visibleWidthM(1600, 1000);
  spec.yawDegFromNorth = m_terrain3dStudio->northYawDeg();
  spec.crsLabel = m_terrain3dStudio->workCrsLabel();
  spec.demName = m_terrain3dStudio->demDisplayName();
  spec.zMin = m_terrain3dStudio->zMin();
  spec.zMax = m_terrain3dStudio->zMax();
  if (m_terrain3dLayoutStudio)
    m_terrain3dLayoutStudio->detachSheet();
  QString err;
  if (Terrain3dLayoutService::buildSheet(QgsProject::instance(), spec, &err).isEmpty()) {
    QMessageBox::warning(this, QStringLiteral("입체지형 도면출력"),
                         err.isEmpty() ? QStringLiteral("입체지형 도면을 만들지 못했습니다.") : err);
    return;
  }
  openTerrain3dLayout();
  statusBar()->showMessage(QStringLiteral("입체지형 도면입니다. 범례·방위·축척이 있습니다."), 6000);
#endif
}

void MainWindow::openTerrain3dLayout() {
#if KA_HGIS_HAS_QGIS
  if (!m_viewTabs)
    return;
  if (!m_terrain3dLayoutStudio) {
    m_terrain3dLayoutStudio = new KaTerrain3dLayoutStudio(QgsProject::instance(), this);
    m_terrain3dLayoutStudio->setAttribute(Qt::WA_DeleteOnClose, false);
    connect(m_terrain3dLayoutStudio, &KaTerrain3dLayoutStudio::requestScale, this,
            &MainWindow::applyTerrain3dSheetScale);
    connect(m_terrain3dLayoutStudio, &KaTerrain3dLayoutStudio::overlaysChanged, this,
            &MainWindow::refreshTerrain3dDrapeAndSheet);
  }
  m_terrain3dLayoutStudio->setParent(m_viewTabs, Qt::Widget);
  if (m_viewTabs->indexOf(m_terrain3dLayoutStudio) < 0)
    m_viewTabs->addTab(m_terrain3dLayoutStudio, KaIcons::icon(QStringLiteral("terrain_3d"), KaTheme::tokens().inkMuted),
                       QStringLiteral("입체지형 도면"));
  m_viewTabs->setCurrentWidget(m_terrain3dLayoutStudio);
  hideSubTools();
  m_terrain3dLayoutStudio->attachSheet();
#endif
}

QString MainWindow::terrain3dSheetPngPath() const {
  QString dir = m_surveyPath.isEmpty() ? QString() : QFileInfo(m_surveyPath).absolutePath();
#if KA_HGIS_HAS_QGIS
  if (dir.isEmpty() && QgsProject::instance())
    dir = QgsProject::instance()->homePath();
#endif
  if (dir.isEmpty())
    dir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
  return QDir(dir).filePath(QStringLiteral("입체지형_도면.png"));
}

void MainWindow::applyTerrain3dSheetScale(int denominator) {
#if KA_HGIS_HAS_QGIS
  if (!m_terrain3dStudio || !m_terrain3dStudio->hasScene())
    return;
  const int denom = std::max(10, denominator);
  double widthMm = Terrain3dLayoutService::pictureWidthMm(QgsProject::instance());
  if (widthMm < 8.0)
    widthMm = 300.0;
  const double targetGroundM = static_cast<double>(denom) * (widthMm / 1000.0);
  m_terrain3dStudio->setVisibleWidthM(targetGroundM, 1600, 1000);
  const QString png = terrain3dSheetPngPath();
  const QImage view = m_terrain3dStudio->renderView(1600, 1000);
  if (view.isNull() || !view.save(png)) {
    statusBar()->showMessage(QStringLiteral("입체지형 그림을 다시 만들지 못했습니다."), 5000);
    return;
  }
  QString err;
  if (!Terrain3dLayoutService::replacePicture(QgsProject::instance(), png, &err)) {
    statusBar()->showMessage(err.isEmpty() ? QStringLiteral("그림을 바꾸지 못했습니다.") : err, 5000);
    return;
  }
  if (!Terrain3dLayoutService::applyScale(QgsProject::instance(), denom, &err)) {
    statusBar()->showMessage(err.isEmpty() ? QStringLiteral("축척을 맞추지 못했습니다.") : err, 5000);
    return;
  }
  if (m_terrain3dLayoutStudio)
    m_terrain3dLayoutStudio->attachSheet();
  statusBar()->showMessage(QStringLiteral("입체지형을 축척 1 : %1에 맞췄습니다.").arg(denom), 5000);
#else
  Q_UNUSED(denominator);
#endif
}

void MainWindow::refreshTerrain3dDrapeAndSheet() {
#if KA_HGIS_HAS_QGIS
  if (!m_terrain3dStudio || !m_terrain3dStudio->hasScene())
    return;
  m_terrain3dStudio->refreshDrape();
  if (!m_terrain3dLayoutStudio)
    return;
  const QString png = terrain3dSheetPngPath();
  const QImage view = m_terrain3dStudio->renderView(1600, 1000);
  if (view.isNull() || !view.save(png))
    return;
  QString err;
  Terrain3dLayoutService::replacePicture(QgsProject::instance(), png, &err);
  if (m_terrain3dLayoutStudio)
    m_terrain3dLayoutStudio->attachSheet();
#endif
}

void MainWindow::openSectionDesigner() {
#if KA_HGIS_HAS_QGIS
  if (!m_viewTabs)
    return;
  if (m_sectionStudio && m_viewTabs->indexOf(m_sectionStudio) >= 0) {
    m_viewTabs->setCurrentWidget(m_sectionStudio);
    hideSubTools();
    m_sectionStudio->refreshLayers();
    return;
  }
  if (!m_sectionStudio) {
    m_sectionStudio = new KaSectionDrawingStudio(QgsProject::instance(), this);
    m_sectionStudio->setAttribute(Qt::WA_DeleteOnClose, false);
    connect(m_sectionStudio, &KaSectionDrawingStudio::statusMessage, this, [this](const QString& text) {
      if (!m_viewTabs || m_viewTabs->currentWidget() != m_sectionStudio) return;
      statusBar()->showMessage(text);
    });
    connect(m_sectionStudio, &KaSectionDrawingStudio::geoTiffAddRequested, this,
            [this](const QString& path) {
              if (path.isEmpty()) return;
              const QString crs = m_sectionStudio
                  ? m_sectionStudio->selectedCrsAuthId()
                  : QStringLiteral("EPSG:5187");
              if (!addSectionGeoTiffFromPath(path, crs)) {
                QMessageBox::warning(this, QStringLiteral("GeoTIFF 추가"),
                                     QStringLiteral("단면 GeoTIFF를 열지 못했습니다.\n%1").arg(path));
                return;
              }
            });
  }
  m_sectionStudio->setParent(m_viewTabs, Qt::Widget);
  if (m_viewTabs->indexOf(m_sectionStudio) < 0)
    m_viewTabs->addTab(m_sectionStudio, KaIcons::icon(QStringLiteral("section"), KaTheme::tokens().inkMuted),
                       QStringLiteral("단면도"));
  m_viewTabs->setCurrentWidget(m_sectionStudio);
  hideSubTools();
  m_sectionStudio->refreshLayers();
  statusBar()->showMessage(QStringLiteral("용지 눈금이 준비되었습니다. GeoTIFF 추가로 단면을 맞추세요."), 6000);
#endif
}

void MainWindow::mergeFeaturePolygons() {
#if KA_HGIS_HAS_QGIS
  auto selected = KaFeatureSelectTool::allSelectedFeatures(m_canvas);
  QgsVectorLayer* targetLayer = nullptr;
  QgsFeatureIds selectedIds;
  QSet<QString> otherLayers;
  if (!selected.isEmpty()) {
    targetLayer = selected[0].layer.data();
    for (const auto& item : selected) {
      if (item.layer == targetLayer)
        selectedIds.insert(item.fid);
      else if (item.layer)
        otherLayers.insert(item.layer->name());
    }
  }
  // 예전에는 다른 레이어에서 고른 면을 조용히 버리고 첫 레이어 것만 묶은 뒤
  // "선택한 폴리곤 N개를 묶었습니다"라고만 알렸다. 무엇이 빠졌는지 밝힌다.
  if (!otherLayers.isEmpty()) {
    QMessageBox::warning(
        this, QStringLiteral("폴리곤 묶기"),
        QStringLiteral("한 번에 한 레이어만 묶을 수 있습니다.\n"
                       "「%1」의 면만 묶고 다음 레이어의 선택은 쓰지 않습니다: %2")
            .arg(targetLayer ? targetLayer->name() : QStringLiteral("?"),
                 QStringList(otherLayers.begin(), otherLayers.end()).join(QStringLiteral(", "))));
  }
  // 아무것도 고르지 않고 누르면 예전에는 유구면 전체가 통째로 하나가 됐다.
  // 되돌리려면 다시 나누어야 해서 사고가 컸다. 이제는 멈추고 알려 준다.
  if (selectedIds.size() < 2) {
    QMessageBox::information(
        this, QStringLiteral("폴리곤 묶기"),
        QStringLiteral("묶을 면을 2개 이상 고른 뒤 누르세요.\n"
                       "[도형선택]으로 면을 클릭하고, Shift를 누른 채 다른 면을 더 고릅니다."));
    return;
  }
  if (!targetLayer) {
    targetLayer = m_layerTree ? qobject_cast<QgsVectorLayer*>(m_layerTree->currentLayer()) : nullptr;
  }
  if (!targetLayer || targetLayer->geometryType() != Qgis::GeometryType::Polygon) {
    targetLayer = ensureDomainLayerForEdit(QStringLiteral("feature_poly"), QStringLiteral("유구면"));
  }
  if (!targetLayer) return;

  // [pkg E1] F083: say which records differ before the merge keeps the first-drawn one.
  const auto conflicts = GeometryEditOps::mergeConflicts(targetLayer, selectedIds);
  if (!conflicts.isEmpty() &&
      QMessageBox::question(this, QStringLiteral("폴리곤 묶기"),
                            GeometryEditOps::mergeConflictSummary(conflicts) + QStringLiteral("\n\n묶을까요?"),
                            QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
    return;
  QString err;
  const bool ok = LayerOps::mergePolygonFeatures(targetLayer, selectedIds, &err);
  if (!ok) {
    QMessageBox::warning(this, QStringLiteral("폴리곤 묶기"), err);
    return;
  }
  // The merge stays in the edit buffer (Ctrl+Z); the survey save writes it.
  QgsProject::instance()->setDirty(true);
  if (m_canvas) m_canvas->refresh();
  const QString msg =
      QStringLiteral("「%1」의 폴리곤 %2개를 1개로 묶었습니다. Ctrl+Z로 되돌릴 수 있습니다.")
          .arg(targetLayer->name())
          .arg(selectedIds.size());
  statusBar()->showMessage(msg, 10000);
  notify(Notice::Success, QStringLiteral("폴리곤 묶기 완료"),
         QStringLiteral("고른 면을 하나로 합쳤습니다."),
         QStringLiteral("「검수·제출」의 제출 꾸러미에서 feature_poly.shp 한 파일(EPSG:5179)로 나갑니다."));
#else
  statusBar()->showMessage(QStringLiteral("스텁: 폴리곤 묶기"), 3000);
#endif
}

