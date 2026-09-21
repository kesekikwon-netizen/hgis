#include "MainWindow.h"
#include "KaIcons.h"
#include "core/ChecklistEngine.h"
#include "core/ExportService.h"
#include "core/LayerOps.h"
#include "core/LayoutService.h"
#include "core/MapGeoTiffExport.h"
#include "core/ProjectStateBuilder.h"

#include <QAction>
#include <QDateTime>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QProgressDialog>
#include <QRandomGenerator>
#include <QSet>
#include <QStatusBar>
#include <QTabWidget>
#include <QToolBar>

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
  int err = 0;
  for (const auto& r : m_checklist->evaluate(buildProjectState())) {
    if (!r.passed && r.severity == QLatin1String("error")) ++err;
  }
  return err;
}

void MainWindow::showSubToolsSubmit() {
#if KA_HGIS_HAS_QGIS
  if (!m_subToolbar) return;
  if (m_subToolsMode == QLatin1String("submit") && m_subToolbar->isVisible()) {
    hideSubTools();
    return;
  }
  clearSubToolbar();
  m_subToolsMode = QStringLiteral("submit");
  auto* lab = new QLabel(QStringLiteral("  제출 › "));
  lab->setObjectName(QStringLiteral("subToolbarCaption"));
  m_subToolbar->addWidget(lab);
  m_subToolbar->addAction(KaIcons::icon(QStringLiteral("check")), QStringLiteral("도면검수"),
                          this, &MainWindow::runChecklist);
  m_subToolbar->addAction(QStringLiteral("폴리곤 묶기"), this, &MainWindow::mergeFeaturePolygons);
  m_subToolbar->addAction(KaIcons::icon(QStringLiteral("export")), QStringLiteral("SHP패키지(5179)"),
                          this, &MainWindow::exportShpPackage);
  m_subToolbar->addAction(KaIcons::icon(QStringLiteral("pdf")), QStringLiteral("도면만들기"),
                          this, &MainWindow::openLayoutDesigner);
  m_subToolbar->addAction(KaIcons::icon(QStringLiteral("pdf")), QStringLiteral("도면PDF"),
                          this, &MainWindow::exportReportLayout);
  m_subToolbar->addAction(KaIcons::icon(QStringLiteral("upload")), QStringLiteral("5179변환"),
                          this, &MainWindow::convertSelectedTo5179);
  auto* closeAct = m_subToolbar->addAction(QStringLiteral("닫기"));
  connect(closeAct, &QAction::triggered, this, &MainWindow::hideSubTools);
  m_subToolbar->setVisible(true);
  statusBar()->showMessage(
      QStringLiteral("다 그렸으면 도면을 만들고, 필요할 때만 업로드용으로 보내세요."),
      12000);
#endif
}

void MainWindow::rebuildLayouts() {
#if KA_HGIS_HAS_QGIS
  openLayoutDesigner();
  statusBar()->showMessage(QStringLiteral("도면만들기에서 용지를 다시 배치하세요."), 6000);
#endif
}

void MainWindow::exportMapGeoTiff() {
#if KA_HGIS_HAS_QGIS
  if (!m_canvas || !m_viewTabs || m_viewTabs->currentWidget() != m_mapPage) return;
  if (m_canvas->layers().isEmpty()) {
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
                                         [&progress]() { return progress.wasCanceled(); });
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
    notify(Notice::Info, QStringLiteral("5179 변환"),
           QStringLiteral("지도 목록에서 변환할 레이어를 선택한 뒤 다시 누르세요."));
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
    notify(Notice::Warning, QStringLiteral("5179 변환"), QStringLiteral("저장하지 못했습니다."), err);
  else {
    statusBar()->showMessage(QStringLiteral("5179 파일만 저장: %1").arg(QDir::toNativeSeparators(out)), 8000);
    notify(Notice::Success, QStringLiteral("5179 변환"),
           QStringLiteral("파일로만 저장했습니다. 지도에는 올리지 않았습니다."),
           QDir::toNativeSeparators(out));
  }
#else
  QMessageBox::warning(this, QStringLiteral("CRS"), QStringLiteral("QGIS 빌드 필요"));
#endif
}

void MainWindow::convertSelected5186To5179() {
#if KA_HGIS_HAS_QGIS
  QgsMapLayer* cur = m_layerTree ? m_layerTree->currentLayer() : nullptr;
  auto* vl = qobject_cast<QgsVectorLayer*>(cur);
  if (!vl) {
    notify(Notice::Info, QStringLiteral("중부 → 업로드용"),
           QStringLiteral("보낼 면을 선택한 뒤 누르세요."));
    return;
  }
  if (!vl->crs().isValid() || vl->crs().authid() != QLatin1String("EPSG:5186"))
    vl->setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")));
  convertSelectedTo5179();
#endif
}

void MainWindow::convertSelected5187To5179() {
#if KA_HGIS_HAS_QGIS
  QgsMapLayer* cur = m_layerTree ? m_layerTree->currentLayer() : nullptr;
  auto* vl = qobject_cast<QgsVectorLayer*>(cur);
  if (!vl) {
    notify(Notice::Info, QStringLiteral("동부 → 업로드용"),
           QStringLiteral("보낼 면을 선택한 뒤 누르세요."));
    return;
  }
  if (!vl->crs().isValid() || vl->crs().authid() != QLatin1String("EPSG:5187"))
    vl->setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5187")));
  convertSelectedTo5179();
#endif
}

void MainWindow::convertShpFileTo5179() {
#if KA_HGIS_HAS_QGIS
  const QString in = QFileDialog::getOpenFileName(
      this, QStringLiteral("5186/5187 SHP 선택"), QString(),
      QStringLiteral("Vector (*.shp *.gpkg *.geojson)"));
  if (in.isEmpty()) return;
  const QString out = QFileDialog::getSaveFileName(
      this, QStringLiteral("5179 SHP 저장"),
      QFileInfo(in).completeBaseName() + QStringLiteral("_5179.shp"),
      QStringLiteral("SHP (*.shp)"));
  if (out.isEmpty()) return;
  QString err;
  if (LayerOps::convertFileToShp5179(in, out, QgsProject::instance(), &err, false).isEmpty())
    notify(Notice::Warning, QStringLiteral("5179 변환"), QStringLiteral("변환하지 못했습니다."), err);
  else
    notify(Notice::Success, QStringLiteral("5179 변환"),
           QStringLiteral("업로드용 EPSG:5179 SHP 파일만 만들었습니다. 지도에는 올리지 않았습니다."),
           QDir::toNativeSeparators(out));
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

void MainWindow::runChecklist() {
  if (!m_checklist) return;
  if (m_checklist->ruleCount() == 0) m_checklist->loadRules(rulesPath());
  const auto results = m_checklist->evaluate(buildProjectState());
  int err = 0, warn = 0;
  for (const auto& r : results) {
    if (r.passed) continue;
    if (r.severity == QLatin1String("error")) err++; else warn++;
  }
  m_lastChecklistErrors = err;
  statusBar()->showMessage(QStringLiteral("검수: error %1 / warn %2").arg(err).arg(warn), 8000);
  refreshWorkPanel();
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

void MainWindow::exportShpPackage() {
  const auto results = m_checklist->evaluate(buildProjectState());
  bool hasErr = false;
  QString summary;
  for (const auto& r : results) {
    if (!r.passed) {
      summary += QStringLiteral("- [%1] %2\n").arg(r.severity, r.messageKo);
      if (r.severity == QLatin1String("error")) hasErr = true;
    }
  }
  if (summary.isEmpty()) summary = QStringLiteral("OK\n");
  const QString enc = QInputDialog::getItem(this, QStringLiteral("인코딩"), QStringLiteral("SHP 인코딩"),
                                      {QStringLiteral("UTF-8"), QStringLiteral("EUC-KR")}, 0, false);
  if (enc.isEmpty()) return;
  const QString dir = QFileDialog::getExistingDirectory(this, QStringLiteral("제출 결과를 저장할 위치"));
  if (dir.isEmpty()) return;
  m_packageCreated = false;
  refreshWorkPanel();
  if (hasErr) {
    QMessageBox::warning(
        this, QStringLiteral("제출 차단"),
        QStringLiteral("도면 검수 error가 있어 제출 패키지를 만들 수 없습니다.\n"
                       "「도면검수」로 항목을 고친 뒤 다시 시도하세요.\n\n%1")
            .arg(summary));
    statusBar()->showMessage(QStringLiteral("제출 차단: 검수 error 잔존"), 8000);
    return;
  }
  QString err;
#if KA_HGIS_HAS_QGIS
  const QString packageName = QStringLiteral("제출_%1_%2")
      .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss_zzz")),
           QString::number(QRandomGenerator::global()->generate(), 16));
  const QString out = ExportService::exportSubmissionPackage(
      QgsProject::instance(), QDir(dir).filePath(packageName), enc, summary, /*blockOnError=*/true, hasErr, &err);
#else
  const QString out;
  err = QStringLiteral("QGIS required");
#endif
  if (out.isEmpty())
    notify(Notice::Warning, QStringLiteral("내보내기"),
           QStringLiteral("제출 패키지를 만들지 못했습니다."), err);
  else {
    m_packageCreated = true;
    statusBar()->showMessage(QStringLiteral("제출 패키지: %1").arg(out), 6000);
    notify(Notice::Success, QStringLiteral("내보내기"),
           QStringLiteral("제출 패키지를 만들었습니다."), QDir::toNativeSeparators(out));
    refreshWorkPanel();
  }
}

void MainWindow::crsDefineOnly() {
  QMessageBox::warning(this, QStringLiteral("위험"),
    QStringLiteral("「이름만 지정」은 좌표값을 바꾸지 않습니다.\n실제 이동이 필요하면 「좌표 변환」을 쓰세요."));
#if KA_HGIS_HAS_QGIS
  QgsMapLayer* cur = m_layerTree ? m_layerTree->currentLayer() : nullptr;
  auto* l = qobject_cast<QgsVectorLayer*>(cur);
  if (!l) {
    statusBar()->showMessage(QStringLiteral("CRS 이름만 지정 — 벡터 레이어를 선택하세요"), 5000);
    return;
  }
  const QString auth = QInputDialog::getText(this, QStringLiteral("CRS 이름만 지정"),
      QStringLiteral("EPSG 코드 (예: EPSG:5179)"), QLineEdit::Normal, QStringLiteral("EPSG:5179"));
  if (auth.isEmpty()) return;
  const QgsCoordinateReferenceSystem crs(auth);
  if (!crs.isValid()) {
    QMessageBox::warning(this, QStringLiteral("CRS"), QStringLiteral("잘못된 CRS"));
    return;
  }
  l->setCrs(crs);
  statusBar()->showMessage(QStringLiteral("CRS 라벨만 변경: %1 (좌표 미변환)").arg(auth), 6000);
#endif
}

void MainWindow::crsReproject() {
#if KA_HGIS_HAS_QGIS
  QgsMapLayer* cur = m_layerTree ? m_layerTree->currentLayer() : nullptr;
  auto* vl = qobject_cast<QgsVectorLayer*>(cur);
  if (!vl) {
    QMessageBox::information(this, QStringLiteral("좌표 변환"), QStringLiteral("레이어 트리에서 벡터 레이어를 선택하세요."));
    return;
  }
  const QString auth = QInputDialog::getText(this, QStringLiteral("좌표 변환(재투영)"),
      QStringLiteral("대상 CRS"), QLineEdit::Normal, QStringLiteral("EPSG:4326"));
  if (auth.isEmpty()) return;
  const QString out = QFileDialog::getSaveFileName(this, QStringLiteral("재투영 저장"),
      vl->name() + QStringLiteral("_reproj.gpkg"), QStringLiteral("GPKG (*.gpkg);;SHP (*.shp)"));
  if (out.isEmpty()) return;
  QString err;
  if (LayerOps::reprojectVectorLayer(vl, auth, out, QgsProject::instance(), &err).isEmpty())
    QMessageBox::warning(this, QStringLiteral("재투영 실패"), err);
  else {
    if (m_canvas) m_canvas->refresh();
    statusBar()->showMessage(QStringLiteral("재투영 완료: %1").arg(out), 6000);
  }
#else
  QMessageBox::warning(this, QStringLiteral("CRS"), QStringLiteral("QGIS 빌드 필요"));
#endif
}

void MainWindow::exportReportLayout() {
#if KA_HGIS_HAS_QGIS
  openLayoutDesigner();
#else
  QMessageBox::warning(this, QStringLiteral("도면"), QStringLiteral("QGIS 빌드 필요"));
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
  } else {
    m_drawingStudio->resetPaper(w, h);
  }
  m_drawingStudio->setParent(m_viewTabs, Qt::Widget);
  if (m_viewTabs->indexOf(m_drawingStudio) < 0)
    m_viewTabs->addTab(m_drawingStudio, KaIcons::icon(QStringLiteral("pdf")),
                       QStringLiteral("레이아웃"));
  m_viewTabs->setCurrentWidget(m_drawingStudio);
  hideSubTools();
  m_drawingStudio->refreshMapFromProject();
  m_drawingStudio->centerOnMapCanvas();
  statusBar()->showMessage(QStringLiteral("조판입니다. 좌표점은 용지 아래 아이콘으로 찍습니다."), 6000);
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
                         err.isEmpty() ? QStringLiteral("입체지형 조판을 만들지 못했습니다.") : err);
    return;
  }
  openTerrain3dLayout();
  statusBar()->showMessage(QStringLiteral("입체지형 조판입니다. 범례·방위·축척이 있습니다."), 6000);
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
    m_viewTabs->addTab(m_terrain3dLayoutStudio, KaIcons::icon(QStringLiteral("terrain_3d")),
                       QStringLiteral("입체지형 조판"));
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
  return QDir(dir).filePath(QStringLiteral("입체지형_조판.png"));
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
    m_viewTabs->addTab(m_sectionStudio, KaIcons::icon(QStringLiteral("section")),
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

  QString err;
  const bool ok = LayerOps::mergePolygonFeatures(targetLayer, selectedIds, &err);
  if (!ok) {
    QMessageBox::warning(this, QStringLiteral("폴리곤 묶기"), err);
    return;
  }
  if (m_canvas) m_canvas->refresh();
  const QString msg =
      QStringLiteral("「%1」의 폴리곤 %2개를 1개로 묶었습니다")
          .arg(targetLayer->name())
          .arg(selectedIds.size());
  statusBar()->showMessage(msg, 10000);
  notify(Notice::Success, QStringLiteral("폴리곤 묶기 완료"),
         QStringLiteral("선택된 폴리곤들을 하나의 지오메트리로 합쳤습니다."),
         QStringLiteral("문화재 인트라넷 제출 시 「SHP내보내기」하면 "
                        "feature_poly.shp 한 파일(EPSG:5179)로 등록하면 됩니다."));
#else
  statusBar()->showMessage(QStringLiteral("스텁: 폴리곤 묶기"), 3000);
#endif
}

