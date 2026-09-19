#include "MainWindow.h"
#include "KaBeginnerRibbon.h"
#include "KaCrashGuard.h"
#include "KaDrawingStudio.h"
#include "KaReferenceDownloadJob.h"
#include "KaSectionDrawingStudio.h"
#include "KaStartPage.h"
#include "KaStatusBar.h"
#include "KaTerrain3dLayoutStudio.h"
#include "KaTerrain3dStudio.h"
#include "KaTopographicBrowser.h"
#include "core/DemPresentation.h"
#include "core/KaSafeQgis.h"
#include "core/LayerOps.h"
#include "core/RecentSurveys.h"
#include "core/SurveyProjectFactory.h"
#include "core/SurveySession.h"
#include "core/SurveyStorage.h"
#include "core/VworldSettings.h"
#include "core/WorkflowGuide.h"

#include <QAbstractButton>
#include <QCoreApplication>
#include <QDialog>
#include <QDir>
#include <QDockWidget>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPointer>
#include <QProgressDialog>
#include <QPushButton>
#include <QScopeGuard>
#include <QScopedValueRollback>
#include <QSettings>
#include <QStatusBar>
#include <QTabWidget>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#if KA_HGIS_HAS_QGIS
#include <qgscoordinatereferencesystem.h>
#include <qgsfeature.h>
#include <qgsgeometry.h>
#include <qgslayertree.h>
#include <qgslayertreeview.h>
#include <qgsmapcanvas.h>
#include <qgsmaplayerstyle.h>
#include <qgsproject.h>
#include <qgsrasterlayer.h>
#include <qgsrectangle.h>
#include <qgsvectorlayer.h>
#endif

void MainWindow::finishOpenedProject(const QString& gpkgPath, const QString& sourceLabel,
                                     qint64 elapsedMs) {
#if KA_HGIS_HAS_QGIS
  LayerOps::restoreThematicOverlayVisibility(QgsProject::instance());
  LayerOps::pruneDuplicateSatelliteLayers(QgsProject::instance());
  LayerOps::addNonEmptyDomainLayers(QgsProject::instance(), gpkgPath);
  m_workspaceRestoreSuppressesAutosave = false;
  m_surveySessionReady = true;
  m_surveyPath = gpkgPath;
  if (QgsProject::instance()->crs().isValid())
    m_workCrs = QgsProject::instance()->crs().authid();
  LayerOps::ensureOtfEnabled(QgsProject::instance(), m_canvas, m_workCrs);

  LayerOps::restoreMissingLayerTreeNodes(QgsProject::instance());
  auto* project = QgsProject::instance();
  for (auto* layer : project->mapLayers()) {
    auto* dem = qobject_cast<QgsRasterLayer*>(layer);
    if (!dem || dem->name() != QLatin1String("DEM") || !dem->isValid() ||
        !DemPresentation::restore(dem)) continue;
    DemPresentation::followCanvas(dem, m_canvas);
    const auto* node = project->layerTreeRoot()->findLayer(dem->id());
    if (node && node->isVisible()) LayerOps::ensureDemRelief(project, dem);
  }
  // QgsProject는 읽기 후에도 같은 트리 루트를 유지한다. 기존 모델과 연결을 보존한다.
  if (m_layerTree && m_layerTree->selectionModel())
    m_layerTree->selectionModel()->clear();
  refreshLayerEmptyState();

  LayerOps::syncMapCanvas(QgsProject::instance(), m_canvas, false);
  if (!LayerOps::zoomToProjectDataLayers(m_canvas, QgsProject::instance()))
    LayerOps::zoomToKorea(m_canvas, m_workCrs, false);
  m_startupViewApplied = true;
  if (m_canvas) {
    m_canvas->setParallelRenderingEnabled(false);
    m_canvas->setPreviewJobsEnabled(false);
    m_canvas->freeze(false);
    LayerOps::refreshXyzBasemapTiles(m_canvas);
    QTimer::singleShot(2500, this, [this]() {
      if (m_canvas) m_canvas->setPreviewJobsEnabled(true);
    });
  }
  ensureDefaultBasemaps();
  rememberSurvey(gpkgPath, QFileInfo(gpkgPath).completeBaseName());
  setWindowTitle(QStringLiteral("필드고고학GIS — %1").arg(QFileInfo(gpkgPath).completeBaseName()));
  showMapWorkspace();
  updateNextActionStatus();
  KaCrashGuard::logLine(
      QStringLiteral("[open] 작업공간 복원 %1 ms — %2 · 레이어 %3")
          .arg(elapsedMs)
          .arg(sourceLabel)
          .arg(QgsProject::instance()->mapLayers().size()));
  rememberSurveyDir(gpkgPath);
  m_snapEnabled = LayerOps::readSnapSettings(QgsProject::instance()).enabled;
  applySnapConfig();
  markSurveySaved();
  // 방금 연 상태를 기준선으로 남긴다. 세션 도중 사라진 레이어는 이 줄과 비교해서 찾는다.
  logLayerCensus(QStringLiteral("열기직후"));
  m_lastLayerKeys.clear();
  auditLayerHealth();
#else
  Q_UNUSED(gpkgPath); Q_UNUSED(sourceLabel); Q_UNUSED(elapsedMs);
#endif
}

bool MainWindow::openSurveyGpkg(const QString& gpkgPath) {
  return openSurveyGpkg(gpkgPath, OpenSurveyMode::PreferWorkspace);
}

bool MainWindow::openSurveyGpkg(const QString& gpkgPath, OpenSurveyMode mode) {
  if (m_isOpeningSurvey || gpkgPath.isEmpty() || !QFile::exists(gpkgPath)) return false;
#if KA_HGIS_HAS_QGIS
  QString validationError;
  {
    QScopedValueRollback<bool> validating(m_isOpeningSurvey, true);
    if (!SurveyStorage::validateForOpen(gpkgPath, &validationError)) {
      notify(Notice::Warning, QStringLiteral("조사 열기 실패"), validationError, gpkgPath);
      return false;
    }
  }
#endif
  if (!confirmSaveBeforeOpeningSurvey()) return false;
  ++m_surveyGeneration;
  if (m_referenceDownload) m_referenceDownload->cancel();
  m_locator->cancel();
  if (m_searchProgress) { m_searchProgress->hide(); m_searchProgress->deleteLater(); m_searchProgress = nullptr; }
  m_locationSearchBusy = false;
  m_adminBoundary->cancel();
  if (m_boundaryProgress) { m_boundaryProgress->hide(); m_boundaryProgress->deleteLater(); m_boundaryProgress = nullptr; }
  QScopedValueRollback<bool> opening(m_isOpeningSurvey, true);
#if KA_HGIS_HAS_QGIS
  // 프로젝트 읽기가 이전 레이어를 해제하기 전에 도구와 편집 참조를 종료한다.
  stopAlignSession();
  stopCaptureTool();
  m_editLayer = nullptr;
  m_isSplittingPolygon = false;
  m_undoActions.clear();
#endif
  QElapsedTimer t;
  t.start();
  m_surveySessionReady = false;
  m_workspaceRestoreFailed = false;
  auto abortOpen = [&]() -> bool {
    // 읽기는 이미 프로젝트를 바꿨을 수 있다. 이전 경로로 자동 저장하지 않는다.
    m_surveyPath.clear();
    m_surveySessionReady = false;
    m_workspaceRestoreSuppressesAutosave = true;
    return false;
  };
#if KA_HGIS_HAS_QGIS
  // 동반되는 QGIS 프로젝트(.qgz/.qgs)가 있으면 외부 SHP, 라벨 5pt, 스타일 등 작업 레이어를 온전히 복원한다.
  const QString base = QFileInfo(gpkgPath).dir().filePath(QFileInfo(gpkgPath).completeBaseName());
  const QString qgz = base + QStringLiteral(".qgz");
  const QString qgs = base + QStringLiteral(".qgs");
  // 1순위: 조사 파일(.gpkg) 안에 들어 있는 작업공간. 바깥 경로에 기대지 않으므로
  // 파일을 옮기거나 복사해도 그대로 열린다. 부팅 복원은 이 경로를 건너뛴다
  // (위성 중복 AV). 실패해도 m_surveyPath를 미리 넣지 않는다 — persist가 빈
  // 홈 작업공간으로 원본을 덮는 것을 막는다.
  const bool hasEmbedded = SurveyStorage::hasEmbeddedProject(gpkgPath);
  if (mode == OpenSurveyMode::PreferWorkspace && hasEmbedded &&
      kaQgisProjectFileIsUnsafeToRead(gpkgPath)) {
    m_workspaceRestoreFailed = true;
    m_workspaceRestoreSuppressesAutosave = true;
  }
  if (mode == OpenSurveyMode::PreferWorkspace && hasEmbedded &&
      !kaQgisProjectFileIsUnsafeToRead(gpkgPath)) {
    bool embCrashed = false;
    QString embErr;
    if (SurveyStorage::readEmbedded(QgsProject::instance(), gpkgPath, &embCrashed, &embErr,
                                    /*loadLayouts=*/true)) {
      finishOpenedProject(gpkgPath, QStringLiteral("조사 파일 내장"), t.elapsed());
      return true;
    }
    m_workspaceRestoreFailed = true;
    m_workspaceRestoreSuppressesAutosave = true;
    KaCrashGuard::logLine(
        QStringLiteral("[open] 내장 작업공간 읽기 실패(%1) — 동반 .qgz로 넘어갑니다: %2")
            .arg(embCrashed ? QStringLiteral("예외") : QStringLiteral("실패"), embErr));
    if (embCrashed) {
      kaMarkQgisProjectUnsafeToRead(gpkgPath);
      QMessageBox::warning(
          this, QStringLiteral("조사 열기"),
          QStringLiteral("조사 파일 안의 작업공간을 읽는 중 오류가 났습니다.\n\n"
                         "프로그램을 닫았다가 다시 열어 주세요. 다시 열면 조사 데이터"
                         "(.gpkg)만으로 엽니다."));
      return abortOpen();
    }
  }
  const QString projectToRead = (mode == OpenSurveyMode::PreferWorkspace)
      ? (QFile::exists(qgz) ? qgz : (QFile::exists(qgs) ? qgs : QString()))
      : QString();
  if (!projectToRead.isEmpty()) {
    const bool alreadyUnsafe = kaQgisProjectFileIsUnsafeToRead(projectToRead);
    bool readCrashed = false;
    bool readOk = !alreadyUnsafe && kaSafeReadQgisProject(QgsProject::instance(),
                                                          projectToRead, &readCrashed, /*loadLayouts=*/true);
    if (!readOk && !alreadyUnsafe)
      kaMarkQgisProjectUnsafeToRead(projectToRead);
    // 원자적 저장이 남긴 직전 정상본. 현재 파일을 못 읽어도 한 세대 전으로 되살릴 수 있다.
    // 예외(AV)로 실패한 경우는 프로세스 상태를 믿을 수 없으므로 재시도하지 않는다.
    const QString bakPath = kaProjectBackupPath(projectToRead);
    if (!readOk && !readCrashed && QFile::exists(bakPath) &&
        !kaQgisProjectFileIsUnsafeToRead(bakPath)) {
      readOk = kaSafeReadQgisProject(QgsProject::instance(), bakPath, &readCrashed, /*loadLayouts=*/true);
      if (readOk)
        KaCrashGuard::logLine(
            QStringLiteral("[open] 직전 저장본으로 복구했습니다: %1").arg(bakPath));
      else if (!readCrashed)
        kaMarkQgisProjectUnsafeToRead(bakPath);
    }
    if (readCrashed) {
      // __except caught an access violation inside QgsProject::read. Every
      // QgsScopedRuntimeProfile still on the stack was skipped, so QgsRuntimeProfiler
      // now holds dangling parents and the next QgsVectorLayer ctor dies inside it
      // (crash-20260905-192205). Loading the .gpkg here is what actually killed the
      // app, so stop and let the user restart — the mark above is on disk now, so
      // the next launch skips this .qgz and opens the .gpkg normally.
      KaCrashGuard::logLine(
          QStringLiteral("[open] 동반 프로젝트 읽기 중 예외 — 이어서 열지 않음: %1")
              .arg(projectToRead));
      QMessageBox::warning(
          this, QStringLiteral("조사 열기"),
          QStringLiteral(
              "동반 프로젝트 파일이 손상되어 읽는 중 오류가 났습니다.\n\n%1\n\n"
              "프로그램을 닫았다가 다시 열면 이 파일을 건너뛰고 조사 데이터(.gpkg)로 "
              "정상적으로 열립니다. 손상된 파일을 지우고 다시 저장하면 원래대로 돌아갑니다.")
              .arg(QDir::toNativeSeparators(projectToRead)));
      return abortOpen();
    }
    if (readOk) {
      finishOpenedProject(gpkgPath, projectToRead, t.elapsed());
      return true;
    }
    // 여기까지 왔다는 것은 작업공간(.qgz)을 못 읽었다는 뜻이다. 이 파일에만 있는
    // 외부 SHP·스크린샷·스타일은 복원되지 않는다. 조용히 넘어가면 "열었더니 지적과
    // 위성만 있다"로 보이므로 무엇이 빠졌는지 반드시 알린다.
    m_workspaceRestoreFailed = true;
    m_workspaceRestoreSuppressesAutosave = true;
    KaCrashGuard::logLine(
        QStringLiteral("[open] 작업공간 복원 실패 — 조사 데이터만 엽니다: %1").arg(projectToRead));
  }
  // 이전 조사의 레이어를 남긴 채 다음 조사를 얹으면 범례가 섞인다. 열기는 언제나
  // 빈 프로젝트에서 시작한다(QGIS 「프로젝트 열기」와 같은 동작).
  if (m_canvas) m_canvas->freeze(true);
  const bool cleared = kaSafeClearQgisProject(QgsProject::instance());
  if (m_canvas) m_canvas->freeze(false);
  if (!cleared) return abortOpen();
#endif
  if (mode == OpenSurveyMode::LayersOnly)
    m_workspaceRestoreSuppressesAutosave = true;
  loadSurveyLayers(gpkgPath);
#if KA_HGIS_HAS_QGIS
  m_surveySessionReady = true;
  applyStartupMap();
  ensureDefaultBasemaps();
  QgsProject* proj = QgsProject::instance();
  for (QgsMapLayer* ml : proj->mapLayers()) {
    if (ml && ml->name() == QLatin1String("DEM") && ml->isValid()) {
      if (auto* rl = qobject_cast<QgsRasterLayer*>(ml)) {
        DemPresentation::restore(rl);
        DemPresentation::followCanvas(rl, m_canvas);
        if (LayerOps::isLayerVisible(proj, QStringLiteral("DEM"))) {
          LayerOps::ensureDemRelief(proj, rl);
        }
      }
      break;
    }
  }
#endif
  rememberSurvey(gpkgPath, QFileInfo(gpkgPath).completeBaseName());
  showMapWorkspace();
  KaCrashGuard::logLine(
      QStringLiteral("[open] 조사 열기 %1 ms — %2").arg(t.elapsed()).arg(gpkgPath));
#if KA_HGIS_HAS_QGIS
  rememberSurveyDir(gpkgPath);
  markSurveySaved();
  logLayerCensus(QStringLiteral("열기직후"));
  m_lastLayerKeys.clear();
  auditLayerHealth();
  if (m_workspaceRestoreFailed) {
    m_workspaceRestoreFailed = false;
    QMessageBox::warning(
        this, QStringLiteral("조사 열기"),
        QStringLiteral(
            "조사 데이터(.gpkg)는 열었지만 저장된 작업공간을 읽지 못했습니다.\n\n"
            "외부 파일과 일부 레이어 설정은 복원되지 않았을 수 있습니다. "
            "원래 작업공간은 자동으로 덮어쓰지 않습니다.\n\n"
            "「저장」을 누르면 현재 복원된 작업을 다른 이름의 조사 파일로 저장합니다."));
  }
#endif
  return true;
}

int MainWindow::domainLayerCount() const {
#if KA_HGIS_HAS_QGIS
  int n = 0;
  for (const QString& k : LayerOps::domainLayerKeys()) {
    if (LayerOps::findByLayerKey(QgsProject::instance(), k)) ++n;
  }
  return n;
#else
  return 0;
#endif
}

int MainWindow::seedDemoFieldData() {
#if !KA_HGIS_HAS_QGIS
  return 0;
#else
  int added = 0;
  auto commitLayer = [&](QgsVectorLayer* vl) -> bool {
    if (!vl || !vl->isValid()) return false;
    if (!vl->isEditable() && !vl->startEditing()) return false;
    if (!vl->commitChanges()) {
      vl->rollBack();
      return false;
    }
    return true;
  };
  auto ensure = [&](const char* key, const char* title) -> QgsVectorLayer* {
    QString err;
    return LayerOps::ensureDomainLayer(QgsProject::instance(), m_surveyPath,
                                       QString::fromUtf8(key), QString::fromUtf8(title), &err);
  };

  if (auto* sa = ensure("survey_area", "조사구역")) {
    if (sa->featureCount() == 0 && sa->startEditing()) {
      QgsFeature f(sa->fields());
      QgsPolylineXY ring;
      ring << QgsPointXY(198000, 451000) << QgsPointXY(202000, 451000)
           << QgsPointXY(202000, 454000) << QgsPointXY(198000, 454000)
           << QgsPointXY(198000, 451000);
      f.setGeometry(QgsGeometry::fromPolygonXY(QgsPolygonXY() << ring));
      const int isn = sa->fields().indexOf(QStringLiteral("survey_name"));
      if (isn >= 0) f.setAttribute(isn, QStringLiteral("demo_verify"));
      if (sa->addFeature(f) && commitLayer(sa)) ++added;
      else sa->rollBack();
    }
  }

  if (auto* fp = ensure("feature_poly", "유구면")) {
    if (fp->featureCount() == 0 && fp->startEditing()) {
      QgsFeature f(fp->fields());
      QgsPolylineXY ring;
      ring << QgsPointXY(199200, 452000) << QgsPointXY(200800, 452000)
           << QgsPointXY(200800, 453200) << QgsPointXY(199200, 453200)
           << QgsPointXY(199200, 452000);
      f.setGeometry(QgsGeometry::fromPolygonXY(QgsPolygonXY() << ring));
      const int ik = fp->fields().indexOf(QStringLiteral("kind"));
      const int ip = fp->fields().indexOf(QStringLiteral("period"));
      if (ik >= 0) f.setAttribute(ik, QStringLiteral("수혈주거지"));
      if (ip >= 0) f.setAttribute(ip, QStringLiteral("청동기"));
      if (fp->addFeature(f) && commitLayer(fp)) ++added;
      else fp->rollBack();
    }
  }

  if (auto* cp = ensure("control_points", "GPS기준점")) {
    LayerOps::ensureControlPointQualityFields(cp);
    if (cp->featureCount() < 2 && cp->startEditing()) {
      auto addPt = [&](const QString& id, double x, double y) {
        QgsFeature f(cp->fields());
        f.setGeometry(QgsGeometry::fromPointXY(QgsPointXY(x, y)));
        auto set = [&](const char* name, const QVariant& v) {
          const int i = cp->fields().indexOf(QString::fromUtf8(name));
          if (i >= 0) f.setAttribute(i, v);
        };
        set("point_id", id);
        set("x", x);
        set("y", y);
        set("datum", QStringLiteral("세계측지계"));
        set("ellipsoid", QStringLiteral("GRS80"));
        set("projection", QStringLiteral("TM/중부원점"));
        set("origin", QStringLiteral("중부"));
        set("accuracy", QStringLiteral("0.05m"));
        set("accuracy_m", 0.05);
        set("fix_type", QStringLiteral("RTK"));
        if (cp->addFeature(f)) ++added;
      };
      addPt(QStringLiteral("GCP1"), 198100, 451100);
      addPt(QStringLiteral("GCP2"), 201900, 453900);
      if (!commitLayer(cp)) cp->rollBack();
    }
  }

  if (m_canvas) {
    LayerOps::syncMapCanvas(QgsProject::instance(), m_canvas, false);
    if (auto* sa = layerByKey(QStringLiteral("survey_area")))
      LayerOps::zoomToLayerMax(m_canvas, sa);
    m_canvas->refresh();
  }
  statusBar()->showMessage(QStringLiteral("데모 시드: 피처 %1건 추가").arg(added), 8000);
  return added;
#endif
}

void MainWindow::rememberSurvey(const QString& path, const QString& name) {
  QSettings st = RecentSurveys::userSettings();
  RecentSurveys::remember(st, path, name);
  if (m_startPage)
    m_startPage->reload();
}

void MainWindow::showHomePage() {
#if KA_HGIS_HAS_QGIS
  if (m_viewTabs && m_startPage) {
    m_startPage->reload();
    m_viewTabs->setCurrentWidget(m_startPage);
  }
#endif
}

void MainWindow::showMapWorkspace() {
#if KA_HGIS_HAS_QGIS
  if (m_viewTabs && m_mapPage)
    m_viewTabs->setCurrentWidget(m_mapPage);
  QTimer::singleShot(0, this, [this]() { ensureStartupViewReady(); });
#endif
}

void MainWindow::openRecentSurvey(const QString& path) {
  if (m_isOpeningSurvey) return;
  if (path.isEmpty() || !QFile::exists(path)) {
    QMessageBox::warning(this, QStringLiteral("최근 조사"),
                         QStringLiteral("파일이 없습니다.\n%1").arg(path));
    QSettings st = RecentSurveys::userSettings();
    RecentSurveys::forget(st, path);
    if (m_startPage) m_startPage->reload();
    return;
  }
  const QString ext = QFileInfo(path).suffix().toLower();
  if (ext == QLatin1String("gpkg")) {
    if (openSurveyGpkg(path)) {
      rememberSurvey(path, QFileInfo(path).completeBaseName());
      setWindowTitle(QStringLiteral("필드고고학GIS — %1").arg(QFileInfo(path).completeBaseName()));
      showMapWorkspace();
    }
    return;
  }
#if KA_HGIS_HAS_QGIS
  const QString companionGpkg = QFileInfo(path).dir().filePath(QFileInfo(path).completeBaseName() + QStringLiteral(".gpkg"));
  if (QFile::exists(companionGpkg)) {
    if (openSurveyGpkg(companionGpkg)) {
      rememberSurvey(path, QFileInfo(path).completeBaseName());
      setWindowTitle(QStringLiteral("필드고고학GIS — %1").arg(QFileInfo(path).completeBaseName()));
      showMapWorkspace();
    }
    return;
  }

  if (!validateStandaloneProjectForOpen(path) || !confirmSaveBeforeOpeningSurvey()) return;
  ++m_surveyGeneration;
  if (m_referenceDownload) m_referenceDownload->cancel();
  m_locator->cancel();
  if (m_searchProgress) { m_searchProgress->hide(); m_searchProgress->deleteLater(); m_searchProgress = nullptr; }
  m_locationSearchBusy = false;
  m_adminBoundary->cancel();
  if (m_boundaryProgress) { m_boundaryProgress->hide(); m_boundaryProgress->deleteLater(); m_boundaryProgress = nullptr; }
  QScopedValueRollback<bool> opening(m_isOpeningSurvey, true);
  stopAlignSession();
  stopCaptureTool();
  m_editLayer = nullptr;
  m_isSplittingPolygon = false;
  m_undoActions.clear();
  m_surveySessionReady = false;
  m_surveyPath.clear();
  m_workspaceRestoreSuppressesAutosave = true;
  if (kaQgisProjectFileIsUnsafeToRead(path) ||
      !kaSafeReadQgisProject(QgsProject::instance(), path, nullptr, /*loadLayouts=*/true)) {
    kaMarkQgisProjectUnsafeToRead(path);
    QMessageBox::warning(this, QStringLiteral("오류"), QStringLiteral("프로젝트를 열 수 없습니다."));
    return;
  }
  LayerOps::pruneDuplicateSatelliteLayers(QgsProject::instance());
  LayerOps::restoreMissingLayerTreeNodes(QgsProject::instance());
  LayerOps::restoreThematicOverlayVisibility(QgsProject::instance());
  if (QFile::exists(companionGpkg)) {
    m_surveyPath = companionGpkg;
    LayerOps::addNonEmptySavedGpkgLayers(QgsProject::instance(), companionGpkg);
  }
  m_surveySessionReady = true;
  m_workspaceRestoreSuppressesAutosave = false;
  if (QgsProject::instance()->crs().isValid())
    m_workCrs = QgsProject::instance()->crs().authid();
  LayerOps::ensureOtfEnabled(QgsProject::instance(), m_canvas, m_workCrs);

  if (m_layerTree && m_layerTree->selectionModel())
    m_layerTree->selectionModel()->clear();
  refreshLayerEmptyState();

  LayerOps::syncMapCanvas(QgsProject::instance(), m_canvas, false);
  if (!LayerOps::zoomToProjectDataLayers(m_canvas, QgsProject::instance())) {
    LayerOps::zoomToKorea(m_canvas, m_workCrs, false);
  }
  m_startupViewApplied = true;
  if (m_canvas) m_canvas->refresh();
  ensureDefaultBasemaps();
  rememberSurvey(path, QFileInfo(path).completeBaseName());
  setWindowTitle(QStringLiteral("필드고고학GIS — %1").arg(QFileInfo(path).completeBaseName()));
  showMapWorkspace();
  updateNextActionStatus();
#endif
}

void MainWindow::onViewTabCloseRequested(int index) {
#if KA_HGIS_HAS_QGIS
  if (!m_viewTabs || index < 0)
    return;
  QWidget* w = m_viewTabs->widget(index);
  if (!w || (w != m_drawingStudio && w != m_sectionStudio && w != m_terrain3dStudio &&
             w != m_terrain3dLayoutStudio && w != m_topographicBrowser.data()))
    return;
  m_viewTabs->removeTab(index);
  w->hide();
  if (m_mapPage)
    m_viewTabs->setCurrentWidget(m_mapPage);
  else
    m_viewTabs->setCurrentIndex(0);
#else
  Q_UNUSED(index);
#endif
}

void MainWindow::newSurvey() {
  if (m_isOpeningSurvey) return;
  if (surveyHasUnsavedChanges()) {
    const auto answer = QMessageBox::question(
        this, QStringLiteral("새 조사"),
        QStringLiteral("현재 조사에 저장하지 않은 작업이 있습니다. 새 조사를 만들기 전에 저장할까요?"),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Save);
    if (answer == QMessageBox::Cancel ||
        (answer == QMessageBox::Save && !persistSurveyWork())) return;
  }
  QDialog dlg(this);
  dlg.setWindowTitle(QStringLiteral("새 조사"));
  dlg.setMinimumWidth(420);
  auto* form = new QFormLayout(&dlg);
  form->setSpacing(12);
  form->setContentsMargins(20, 20, 20, 16);
  auto* nameEdit = new QLineEdit(&dlg);
  nameEdit->setPlaceholderText(QStringLiteral("예: 병산동"));
  nameEdit->setMinimumHeight(36);
  auto* crsRow = new QHBoxLayout();
  auto* btn5186 = new QPushButton(QStringLiteral("5186  중부원점"), &dlg);
  auto* btn5187 = new QPushButton(QStringLiteral("5187  동부원점"), &dlg);
  for (auto* b : {btn5186, btn5187}) {
    b->setCheckable(true);
    b->setMinimumHeight(40);
    b->setCursor(Qt::PointingHandCursor);
  }
  const bool use5187 = m_workCrs.contains(QLatin1String("5187"));
  btn5186->setChecked(!use5187);
  btn5187->setChecked(use5187);
  connect(btn5186, &QPushButton::clicked, &dlg, [btn5186, btn5187]() {
    btn5186->setChecked(true);
    btn5187->setChecked(false);
  });
  connect(btn5187, &QPushButton::clicked, &dlg, [btn5186, btn5187]() {
    btn5187->setChecked(true);
    btn5186->setChecked(false);
  });
  crsRow->addWidget(btn5186, 1);
  crsRow->addWidget(btn5187, 1);
  auto* tip = new QLabel(QStringLiteral("나중에 「도면만들기」옆에서 업로드용으로 바꿀 수 있습니다."), &dlg);
  tip->setWordWrap(true);
  form->addRow(QStringLiteral("조사명"), nameEdit);
  form->addRow(QStringLiteral("작업 좌표계"), crsRow);
  form->addRow(tip);
  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
  buttons->button(QDialogButtonBox::Ok)->setText(QStringLiteral("다음"));
  buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("취소"));
  form->addRow(buttons);
  connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
  if (dlg.exec() != QDialog::Accepted) return;
  const QString name = nameEdit->text().trimmed();
  if (name.isEmpty()) {
    QMessageBox::information(this, QStringLiteral("새 조사"), QStringLiteral("조사명을 입력하세요."));
    return;
  }
  const QString selectedCrs = btn5187->isChecked() ? QStringLiteral("EPSG:5187")
                                                  : QStringLiteral("EPSG:5186");
  const QString dir =
      QFileDialog::getExistingDirectory(this, QStringLiteral("저장 폴더"), preferredSurveyDir());
  if (dir.isEmpty()) return;
  QScopedValueRollback<bool> creating(m_isOpeningSurvey, true);
  // 파일 생성 실패는 현재 조사와 편집 버퍼에 영향을 주지 않는다.
  QString err;
  const QString path = SurveyProjectFactory::createNewSurvey(dir, name, &err, selectedCrs);
  if (path.isEmpty()) {
    notify(Notice::Warning, QStringLiteral("새 조사를 만들지 못했습니다"), err);
    return;
  }
#if KA_HGIS_HAS_QGIS
  // 새 조사는 빈 프로젝트에서 시작한다. removeSurveyDomainLayers만 부르던 예전 코드는
  // 도면 레이어 7종만 지워서, 끌어다 넣은 SHP·스크린샷·클립 레이어가 새 조사 범례에
  // 그대로 남았고 자동 저장이 그것을 새 .qgz에 박아 넣었다. QGIS의 「새 프로젝트」와
  // 같이 전부 비운 뒤 지적·위성만 다시 올린다.
  hideSubTools();
  stopAlignSession();
  stopCaptureTool();
  m_editLayer = nullptr;
  m_undoActions.clear();
  ++m_surveyGeneration;
  if (m_referenceDownload) m_referenceDownload->cancel();
  m_locator->cancel();
  if (m_searchProgress) { m_searchProgress->hide(); m_searchProgress->deleteLater(); m_searchProgress = nullptr; }
  m_locationSearchBusy = false;
  m_adminBoundary->cancel();
  if (m_boundaryProgress) { m_boundaryProgress->hide(); m_boundaryProgress->deleteLater(); m_boundaryProgress = nullptr; }
  m_surveyPath.clear();  // 비우는 동안 자동 저장이 이전 조사에 덮어쓰지 않게 한다
  m_workspaceRestoreSuppressesAutosave = false;
  if (m_canvas) m_canvas->freeze(true);
  kaSafeClearQgisProject(QgsProject::instance());
  refreshLayerEmptyState();
#endif
  m_workCrs = selectedCrs;
  m_surveyPath = path;
  m_stubSurveyArea = 0; m_stubFeatures = 0; m_stubGcp = 0; m_stubHasMeta = false;
  loadSurveyLayers(path);
#if KA_HGIS_HAS_QGIS
  if (m_canvas) m_canvas->freeze(false);
  applyStartupMap();
  // applyStartupMap의 loadBootBasemaps는 m_isOpeningSurvey가 켜진 동안 취소된다.
  // 새 조사에서도 조사 열기와 같이 위성·지적을 지금 올린다.
  ensureDefaultBasemaps();
  // Factory에서 검증한 빈 작업공간을 이미 저장했다. 화면 준비 중 중복 저장하지 않는다.
#endif
  if (auto* b86 = findChild<QToolButton*>(QStringLiteral("btnCrs5186")))
    b86->setChecked(!m_workCrs.contains(QLatin1String("5187")));
  if (auto* b87 = findChild<QToolButton*>(QStringLiteral("btnCrs5187")))
    b87->setChecked(m_workCrs.contains(QLatin1String("5187")));
  setWindowTitle(QStringLiteral("필드고고학GIS — %1").arg(name));
  m_surveySessionReady = true;
  rememberSurvey(path, name);
  rememberSurveyDir(path);
  markSurveySaved();
  showMapWorkspace();
  updateNextActionStatus();
  refreshWorkPanel();
}

void MainWindow::refreshLayerEmptyState() {
#if KA_HGIS_HAS_QGIS
  const bool empty = !QgsProject::instance() || QgsProject::instance()->mapLayers().isEmpty();
  if (m_layerEmpty) m_layerEmpty->setVisible(empty);
  if (m_layerTree) m_layerTree->setVisible(!empty);
#else
  if (m_layerEmpty) m_layerEmpty->setVisible(true);
#endif
}

void MainWindow::loadSurveyLayers(const QString& gpkgOrStub) {
#if KA_HGIS_HAS_QGIS
  if (gpkgOrStub.endsWith(QLatin1String(".stub"))) return;
  QgsProject* proj = QgsProject::instance();
  stopAlignSession();
  m_editLayer = nullptr;
  m_undoActions.clear();
  LayerOps::removeSurveyDomainLayers(proj);
  proj->setCrs(QgsCoordinateReferenceSystem(m_workCrs));
  if (m_canvas) m_canvas->setDestinationCrs(QgsCoordinateReferenceSystem(m_workCrs));
  m_surveyPath = gpkgOrStub;

  // 이미 피처(도형)가 존재하는 도면 레이어만 한국어 명칭으로 불러온다.
  LayerOps::addNonEmptyDomainLayers(proj, gpkgOrStub);

  // 도면 5장을 미리 만들지 않는다 — 조사를 열 때마다 2.4초를 먹었고(실측),
  // 사용자가 도면을 안 볼 수도 있다. 검수·내보내기·도면 창에서 그때 만든다.
  LayerOps::pruneEmptyLegendGroups(proj);
  if (m_canvas) {
    m_canvas->freeze(true);
    LayerOps::ensureOtfEnabled(proj, m_canvas, m_workCrs);
    LayerOps::syncMapCanvas(proj, m_canvas, false);
    if (!LayerOps::zoomToProjectDataLayers(m_canvas, proj))
      LayerOps::zoomToKorea(m_canvas, m_workCrs, false);
    m_canvas->freeze(false);
    m_canvas->refresh();
  }
  refreshLayerEmptyState();
#else
  Q_UNUSED(gpkgOrStub);
#endif
}

#if KA_HGIS_HAS_QGIS
QgsVectorLayer* MainWindow::layerByKey(const QString& layerKey) const {
  return LayerOps::findByLayerKey(QgsProject::instance(), layerKey);
}

QgsVectorLayer* MainWindow::ensureDomainLayerForEdit(const QString& layerKey, const QString& titleKo) {
  QString err;
  auto* vl = LayerOps::ensureDomainLayer(QgsProject::instance(), m_surveyPath, layerKey, titleKo, &err);
  if (!vl) {
    const auto ans = QMessageBox::question(
        this, QStringLiteral("레이어"),
        QStringLiteral("%1\n\n지금 「새 조사」를 만들까요?")
            .arg(err.isEmpty() ? QStringLiteral("먼저 「새 조사」로 저장 경로를 만드세요.") : err),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
    if (ans == QMessageBox::Yes)
      newSurvey();
    vl = LayerOps::ensureDomainLayer(QgsProject::instance(), m_surveyPath, layerKey, titleKo, &err);
    if (!vl) return nullptr;
  }
  if (layerKey == QLatin1String("control_points"))
    LayerOps::ensureControlPointQualityFields(vl);
  LayerOps::applyDomainDrawStyle(vl, layerKey);
  if (m_canvas && !QgsProject::instance()->mapLayer(vl->id())) {
    LayerOps::syncMapCanvas(QgsProject::instance(), m_canvas, false);
  }
  if (m_layerTree)
    m_layerTree->setCurrentLayer(vl);
  statusBar()->showMessage(
      QStringLiteral("레이어 준비: %1 — 지도에서 그리세요").arg(vl->name()), 6000);
  return vl;
}
#endif

bool MainWindow::commitSurveyEdits(int* committedCount) {
  if (committedCount) *committedCount = 0;
#if KA_HGIS_HAS_QGIS
  if (QgsProject* proj = QgsProject::instance()) {
    QStringList committedLayers;
    for (QgsMapLayer* l : proj->mapLayers()) {
      auto* v = qobject_cast<QgsVectorLayer*>(l);
      if (!v || !v->isValid() || !v->isEditable() || !v->isModified())
        continue;
      if (!v->commitChanges(false)) {
        QString message = QStringLiteral("%1의 편집을 저장하지 못해 전체 저장을 마치지 못했습니다. "
                                         "미저장 편집은 유지됩니다. 창을 닫지 말고 원인을 확인한 뒤 다시 저장하세요.")
                              .arg(v->name());
        QString details = v->commitErrors().join(QLatin1Char('\n'));
        if (!committedLayers.isEmpty())
          message += QStringLiteral(" 이미 저장한 레이어: %1.").arg(committedLayers.join(QStringLiteral(", ")));
        const QString recoveryDirectory = QDir(m_surveyPath.isEmpty()
            ? preferredSurveyDir() : QFileInfo(m_surveyPath).absolutePath()).filePath(QStringLiteral("복구사본"));
        QString recoveryError;
        const QString recovery = SurveyStorage::writeRecoverySnapshot(proj, recoveryDirectory, &recoveryError);
        if (!recovery.isEmpty()) {
          message += QStringLiteral(" 현재 편집 도형의 복구 사본을 보관했습니다: %1").arg(QDir::toNativeSeparators(recovery));
          details += QStringLiteral("\n복구 사본은 벡터 피처와 레이어 구성을 보관합니다. "
                                    "조판은 포함하지 않으며 사진·래스터는 외부 원본 참조로 남습니다. "
                                    "원본 파일도 함께 보관하세요. 현재 조사 저장은 아직 완료되지 않았습니다.");
        } else {
          message += QStringLiteral(" 복구 사본도 만들지 못했습니다. 현재 창을 계속 열어 두세요.");
          details += QStringLiteral("\n복구 사본 실패: %1").arg(recoveryError);
        }
        KaCrashGuard::logLine(QStringLiteral("[save] %1 — %2").arg(message, details));
        notify(Notice::Warning, QStringLiteral("저장 실패"), message, details);
        return false;
      }
      committedLayers << v->name();
      if (committedCount) ++*committedCount;
    }
  }
#endif
  return true;
}

bool MainWindow::persistSurveyWork() {
#if KA_HGIS_HAS_QGIS
  if (m_isOpeningSurvey) return false;
  if (!m_surveySessionReady) return false;
  if (m_surveyPath.isEmpty() || m_workspaceRestoreSuppressesAutosave) {
    saveProjectAs();
    return !m_surveyPath.isEmpty() && !m_workspaceRestoreSuppressesAutosave && !surveyHasUnsavedChanges();
  }
  const auto updateTitle = qScopeGuard([this] { refreshWindowTitle(); });
  QScopedValueRollback<bool> saving(m_isOpeningSurvey, true);
  if (m_canvas) m_canvas->stopRendering();
  QgsProject* project = QgsProject::instance();
  bool saved = false;
  const auto preserveUnsaved = qScopeGuard([&] {
    if (!saved) project->setDirty(true);
  });
  try {
    SurveySession::PersistInput in;
    in.surveyPath = m_surveyPath;
    in.recoveryDirectory = SurveySession::recoveryDirectoryFor(m_surveyPath);
    const SurveySession::PersistResult result = SurveySession::persistWork(project, in);
    const auto& attempt = result.workspace;
    if (!result.saved) {
      QString message = attempt.error.isEmpty()
          ? QStringLiteral("저장을 마치지 못했습니다. 미저장 편집은 유지됩니다.")
          : attempt.error;
      QString details = attempt.failedLayers.join(QLatin1Char('\n'));
      if (!attempt.recoveryPath.isEmpty()) {
        message += QStringLiteral(" 복구 사본: %1")
                       .arg(QDir::toNativeSeparators(attempt.recoveryPath));
        details += QStringLiteral("\n원본 조사 파일은 덮어쓰지 않았습니다.");
      }
      KaCrashGuard::logLine(QStringLiteral("[save] %1 — %2").arg(message, details));
      notify(Notice::Warning, QStringLiteral("저장 실패"), message, details);
      return false;
    }
    const QFileInfo file(m_surveyPath);
    const bool companionSaved = result.companionSaved;
    if (!companionSaved) {
      KaCrashGuard::logLine(QStringLiteral("[save] 동반 .qgz 저장 실패 — %1").arg(result.companionError));
      notify(Notice::Warning, QStringLiteral("조사 저장 완료 · 보조 사본 확인 필요"),
             QStringLiteral("조사 데이터와 작업 구성은 GPKG에 저장했습니다. QGZ 사본은 갱신하지 "
                            "못했습니다. 해당 파일을 사용하는 프로그램을 닫고 다시 저장하세요."));
    }
    rememberSurvey(m_surveyPath, file.completeBaseName());
    rememberSurveyDir(m_surveyPath);
    markSurveySaved();
    saved = true;
    if (!attempt.skippedRaster.isEmpty())
      notify(Notice::Info, QStringLiteral("함께 보관할 파일"),
             QStringLiteral("사진·래스터 원본도 함께 보관하세요: %1")
                 .arg(attempt.skippedRaster.join(QStringLiteral(", "))));
    statusBar()->showMessage(companionSaved
        ? QStringLiteral("조사 데이터와 작업 구성을 저장했습니다: %1").arg(file.fileName())
        : QStringLiteral("GPKG 저장 완료. QGZ 사본은 다시 저장해야 합니다."), 8000);
    QTimer::singleShot(0, this, [this]() { auditLayerHealth(); });
  } catch (...) {
    KaCrashGuard::logLine(QStringLiteral("[save] 저장 예외로 중단 — 현재 작업 유지"));
    notify(Notice::Warning, QStringLiteral("저장을 마치지 못했습니다"),
           QStringLiteral("저장 중 오류가 발생했습니다. 창을 닫지 말고 여유 공간을 확인한 뒤 "
                          "다시 저장하거나 다른 이름으로 저장하세요."));
    return false;
  }
#endif
  return true;
}

void MainWindow::extractEmbeddedReferenceVectors() {
#if KA_HGIS_HAS_QGIS
  if (m_surveyPath.isEmpty() || !QFileInfo::exists(m_surveyPath)) {
    notify(Notice::Warning, QStringLiteral("조사 파일이 없습니다"),
           QStringLiteral("먼저 조사를 열거나 저장한 뒤에 참조 벡터를 밖으로 옮기세요."));
    return;
  }
  QgsProject* project = QgsProject::instance();
  const QStringList names = SurveyStorage::embeddedReferenceVectorNames(project, m_surveyPath);
  if (names.isEmpty()) {
    notify(Notice::Info, QStringLiteral("옮길 참조 벡터가 없습니다"),
           QStringLiteral("조사 파일 안에 들어 있는 참조 벡터가 없습니다. 이미 바깥 파일을 가리키는 "
                          "참조 레이어는 그대로 둡니다."));
    return;
  }
  const QString prompt = QStringLiteral(
      "조사 파일 안에 있는 참조 벡터 %1개를 바깥 파일로 옮기고, 조사 파일에서 해당 테이블을 뺍니다.\n\n"
      "%2\n\n"
      "조사 구역·유구·유물 등 조사 데이터는 그대로 둡니다. 원본 조사 파일은 검증된 다음 세대로 교체됩니다.")
      .arg(names.size())
      .arg(names.join(QLatin1Char('\n')));
  if (QMessageBox::question(this, QStringLiteral("참조 벡터를 조사 파일 밖으로"), prompt,
                            QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
    return;

  const QString outputDirectory = QDir(QFileInfo(m_surveyPath).absolutePath())
                                      .filePath(QStringLiteral("참조지도"));
  const auto attempt =
      SurveyStorage::extractEmbeddedReferenceVectors(project, m_surveyPath, outputDirectory);
  if (!attempt.extracted) {
    notify(Notice::Warning, QStringLiteral("참조 벡터를 옮기지 못했습니다"),
           attempt.error.isEmpty() ? QStringLiteral("조사 파일은 그대로 둡니다.") : attempt.error,
           attempt.failed.join(QLatin1Char('\n')));
    return;
  }
  const qint64 savedBytes = qMax<qint64>(0, attempt.bytesBefore - attempt.bytesAfter);
  notify(Notice::Success, QStringLiteral("참조 벡터를 밖으로 옮겼습니다"),
         QStringLiteral("%1개를 참조지도 폴더로 옮겼습니다. 조사 파일이 약 %2 KB 줄었습니다.")
             .arg(attempt.moved.size())
             .arg(savedBytes / 1024),
         attempt.moved.join(QLatin1Char('\n')));
  statusBar()->showMessage(QStringLiteral("참조 벡터를 조사 파일 밖으로 옮겼습니다"), 8000);
#else
  notify(Notice::Warning, QStringLiteral("QGIS 없음"),
         QStringLiteral("이 빌드에서는 참조 벡터를 밖으로 옮길 수 없습니다."));
#endif
}

void MainWindow::auditLayerHealth() {
#if KA_HGIS_HAS_QGIS
  QgsProject* proj = QgsProject::instance();
  if (!proj || m_isOpeningSurvey) return;

  QStringList revived;
  QStringList broken;
  const int back = LayerOps::reviveInvalidLayers(proj, &revived, &broken);
  if (back > 0)
    KaCrashGuard::logLine(QStringLiteral("[layers] 끊겼던 레이어 %1개 되살림 — %2")
                              .arg(back)
                              .arg(revived.join(QStringLiteral(", "))));
  if (!broken.isEmpty())
    KaCrashGuard::logLine(
        QStringLiteral("[layers] 원본을 못 여는 레이어 %1개 — %2")
            .arg(broken.size())
            .arg(broken.join(QStringLiteral(", "))));

  // 사라짐은 세 가지 모습으로 온다: 등록에서 빠짐 / 범례 노드가 빠짐 / 무효가 됨.
  // 셋 다 한 줄에 담아 두고, 직전 줄과 다를 때만 기록한다.
  QStringList keys;
  QgsLayerTree* root = proj->layerTreeRoot();
  for (QgsMapLayer* l : proj->mapLayers()) {
    if (!l) continue;
    const bool node = root && root->findLayer(l->id());
    keys << QStringLiteral("%1|%2|%3|%4").arg(l->id(), l->name()).arg(l->isValid()).arg(node);
  }
  keys.sort();
  if (!m_lastLayerKeys.isEmpty() && keys != m_lastLayerKeys) {
    QStringList gone;
    for (const QString& k : m_lastLayerKeys)
      if (!keys.contains(k)) gone << k.section(QLatin1Char('|'), 1);
    QStringList fresh;
    for (const QString& k : keys)
      if (!m_lastLayerKeys.contains(k)) fresh << k.section(QLatin1Char('|'), 1);
    KaCrashGuard::logLine(QStringLiteral("[layers] 변화 %1→%2 · 빠짐[%3] · 새로[%4]")
                              .arg(m_lastLayerKeys.size())
                              .arg(keys.size())
                              .arg(gone.join(QStringLiteral(", ")),
                                   fresh.join(QStringLiteral(", "))));
    logLayerCensus(QStringLiteral("변화후"));
  }
  m_lastLayerKeys = keys;
#endif
}

bool MainWindow::confirmSaveBeforeOpeningSurvey() {
  if (!surveyHasUnsavedChanges()) return true;
  QMessageBox::StandardButton answer;
  {
    // Block nested open requests while the question is active. Release this guard
    // before saving, since persistSurveyWork must run outside the opening state.
    QScopedValueRollback<bool> asking(m_isOpeningSurvey, true);
    answer = QMessageBox::question(
        this, QStringLiteral("조사 열기"),
        QStringLiteral("현재 조사에 저장하지 않은 작업이 있습니다. 선택한 조사를 열기 전에 저장할까요?"),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Save);
  }
  if (answer == QMessageBox::Discard) return true;
  return answer == QMessageBox::Save && persistSurveyWork();
}

bool MainWindow::validateStandaloneProjectForOpen(const QString& path) {
#if KA_HGIS_HAS_QGIS
  QScopedValueRollback<bool> validating(m_isOpeningSurvey, true);
  if (QFileInfo(path).isFile() && !kaQgisProjectFileIsUnsafeToRead(path)) {
    try {
      QgsProject probe;
      if (probe.read(path, Qgis::ProjectReadFlag::DontResolveLayers |
                               Qgis::ProjectReadFlag::DontLoadLayouts)) return true;
    } catch (...) {
      KaCrashGuard::logLine(QStringLiteral("[open] 작업공간 파일 사전 확인 중 예외 — 현재 작업 유지"));
    }
  }
  notify(Notice::Warning, QStringLiteral("조사 열기 실패"),
         QStringLiteral("선택한 작업공간 파일을 읽을 수 없습니다. 현재 작업은 유지됩니다. "
                        "정상적인 QGZ 또는 QGS 파일이나 조사 GPKG를 선택해 주세요."));
  return false;
#else
  Q_UNUSED(path);
  return true;
#endif
}

bool MainWindow::surveyHasUnsavedChanges() const {
#if KA_HGIS_HAS_QGIS
  if (m_isOpeningSurvey || !m_surveySessionReady)
    return false;
  QgsProject* proj = QgsProject::instance();
  if (!proj) return false;
  if (proj->isDirty()) return true;
  // 커밋 안 된 그리기 버퍼도 저장 안 된 작업이다.
  for (QgsMapLayer* l : proj->mapLayers()) {
    auto* v = qobject_cast<QgsVectorLayer*>(l);
    if (v && v->isEditable() && v->isModified()) return true;
  }
#endif
  return false;
}

void MainWindow::markSurveySaved() {
#if KA_HGIS_HAS_QGIS
  if (QgsProject* proj = QgsProject::instance())
    proj->setDirty(false);
  if (!m_surveyPath.isEmpty()) {
    clearRecoveryOffer(QDir(QFileInfo(m_surveyPath).absolutePath()).filePath(QStringLiteral("복구사본")));
  }
#endif
  refreshWindowTitle();
}

void MainWindow::clearRecoveryOffer(const QString& recoveryDirectory) {
  if (recoveryDirectory.isEmpty()) return;
  SurveyStorage::clearRecoveryPending(recoveryDirectory);
  QSettings st = RecentSurveys::userSettings();
  const QString noted = QDir::cleanPath(st.value(QStringLiteral("Survey/PendingRecoveryDir")).toString());
  if (!noted.isEmpty() && noted.compare(QDir::cleanPath(recoveryDirectory), Qt::CaseInsensitive) == 0)
    st.remove(QStringLiteral("Survey/PendingRecoveryDir"));
}

void MainWindow::captureRecoverySnapshot() {
#if KA_HGIS_HAS_QGIS
  if (m_recoverySnapshotBusy || m_isOpeningSurvey || m_closingWindow) return;
  if (!m_surveySessionReady || m_surveyPath.isEmpty() || !surveyHasUnsavedChanges()) return;
  QgsProject* project = QgsProject::instance();
  if (!project) return;
  QStringList ids;
  for (QgsMapLayer* layer : project->mapLayers()) {
    auto* vector = qobject_cast<QgsVectorLayer*>(layer);
    if (!vector) continue;
    const bool unsavedEdit = vector->isEditable() && vector->isModified();
    if (LayerOps::isReferenceLayer(vector) && !unsavedEdit) continue;
    ids << vector->id();
  }
  if (ids.isEmpty()) return;
  m_recoverySnapshotBusy = true;
  const auto busy = qScopeGuard([this] { m_recoverySnapshotBusy = false; });
  if (m_canvas) m_canvas->stopRendering();
  const QString recoveryDirectory =
      QDir(QFileInfo(m_surveyPath).absolutePath()).filePath(QStringLiteral("복구사본"));
  QString error;
  const QString path = SurveyStorage::writeRecoverySnapshot(project, recoveryDirectory, &error, ids);
  LayerOps::reloadSurveyGpkgReaders(project, m_surveyPath);
  if (path.isEmpty()) {
    KaCrashGuard::logLine(QStringLiteral("[recovery] 복구 사본 실패 — %1").arg(error));
    return;
  }
  if (!SurveyStorage::noteRecoveryPending(recoveryDirectory, path, &error)) {
    KaCrashGuard::logLine(QStringLiteral("[recovery] 표시 실패 — %1").arg(error));
    return;
  }
  SurveyStorage::pruneRecoverySnapshots(recoveryDirectory, 3, path);
  QSettings st = RecentSurveys::userSettings();
  st.setValue(QStringLiteral("Survey/PendingRecoveryDir"), QDir::cleanPath(recoveryDirectory));
  KaCrashGuard::logLine(QStringLiteral("[recovery] %1").arg(QDir::toNativeSeparators(path)));
  statusBar()->showMessage(QStringLiteral("복구 사본을 남겼습니다. 원본은 바꾸지 않았습니다."), 4000);
#else
  return;
#endif
}

void MainWindow::offerRecoverySnapshot() {
#if KA_HGIS_HAS_QGIS
  if (m_recoveryOfferDone || m_isOpeningSurvey || !m_surveyPath.isEmpty()) return;
  m_recoveryOfferDone = true;
  if (QCoreApplication::applicationName() != QLatin1String("ka-hgis")) return;
  const QStringList args = QCoreApplication::arguments();
  if (args.contains(QStringLiteral("--smoke-quit")) || args.contains(QStringLiteral("--qa-phase1"))) return;
  QSettings st = RecentSurveys::userSettings();
  QString recoveryDirectory = st.value(QStringLiteral("Survey/PendingRecoveryDir")).toString();
  if (recoveryDirectory.isEmpty()) {
    const QString last = RecentSurveys::lastPath(st);
    if (!last.isEmpty())
      recoveryDirectory = QDir(QFileInfo(last).absolutePath()).filePath(QStringLiteral("복구사본"));
  }
  const QString path = SurveyStorage::pendingRecoverySnapshot(recoveryDirectory);
  if (path.isEmpty()) return;
  QMessageBox box(this);
  box.setIcon(QMessageBox::Warning);
  box.setWindowTitle(QStringLiteral("복구 사본"));
  box.setText(QStringLiteral("저장하지 않은 작업의 복구 사본이 있습니다.\n"
                             "원본 조사 파일은 바꾸지 않습니다. 이 사본을 열까요?"));
  box.setInformativeText(QDir::toNativeSeparators(path));
  auto* openButton = box.addButton(QStringLiteral("복구 사본 열기"), QMessageBox::AcceptRole);
  box.addButton(QStringLiteral("나중에"), QMessageBox::RejectRole);
  box.exec();
  if (box.clickedButton() != openButton) return;
  clearRecoveryOffer(recoveryDirectory);
  openSurveyGpkg(path);
#else
  m_recoveryOfferDone = true;
#endif
}

void MainWindow::refreshWindowTitle() {
  QString base = windowTitle();
  if (base.endsWith(QLatin1String(" *"))) base.chop(2);
  if (base.isEmpty()) return;
  const QString wanted = base + (surveyHasUnsavedChanges() ? QStringLiteral(" *") : QString());
  if (windowTitle() != wanted)
    QMainWindow::setWindowTitle(wanted);
}

QString MainWindow::preferredSurveyDir() const {
  // 1) 마지막으로 조사를 저장한 폴더
  QSettings st = RecentSurveys::userSettings();
  const QString remembered = st.value(QStringLiteral("Survey/LastDir")).toString();
  if (!remembered.isEmpty() && QFileInfo(remembered).isDir())
    return remembered;
  // 2) 지금 열려 있는 조사의 폴더
  if (!m_surveyPath.isEmpty()) {
    const QString here = QFileInfo(m_surveyPath).absolutePath();
    if (QFileInfo(here).isDir()) return here;
  }
  // 3) 최근 조사 목록의 맨 위
  const QString last = RecentSurveys::lastPath(st);
  if (!last.isEmpty()) {
    const QString dir = QFileInfo(last).absolutePath();
    if (QFileInfo(dir).isDir()) return dir;
  }
  // 4) 그래도 없으면 바탕화면. 이 PC의 바탕화면은 OneDrive 폴더 안이라 마지막 수단이다.
  return resolvedDesktopPath();
}

void MainWindow::rememberSurveyDir(const QString& path) {
  if (path.isEmpty()) return;
  const QString dir = QFileInfo(path).absolutePath();
  if (dir.isEmpty() || !QFileInfo(dir).isDir()) return;
  QSettings st = RecentSurveys::userSettings();
  st.setValue(QStringLiteral("Survey/LastDir"), dir);
  updateTopographicDirectory(path);
}

void MainWindow::logCanvasPaintState() {
#if KA_HGIS_HAS_QGIS
  if (!m_canvas || m_isOpeningSurvey) return;
  QgsProject* proj = QgsProject::instance();
  if (!proj) return;

  QStringList onCanvas;
  for (QgsMapLayer* l : m_canvas->layers())
    if (l) onCanvas << l->name();

  // 범례에서 켜져 있는데 캔버스 목록에는 없는 레이어 = 화면에서 사라진 것.
  QStringList missing;
  if (QgsLayerTree* root = proj->layerTreeRoot()) {
    for (QgsMapLayer* l : proj->mapLayers()) {
      if (!l || !l->isValid()) continue;
      QgsLayerTreeLayer* node = root->findLayer(l->id());
      if (node && node->itemVisibilityChecked() && !m_canvas->layers().contains(l))
        missing << l->name();
    }
  }

  const QString state = QStringLiteral("그림[%1] 켰는데없음[%2]")
                            .arg(onCanvas.join(QStringLiteral(", ")),
                                 missing.join(QStringLiteral(", ")));
  if (state == m_lastCanvasPaintState) return;
  m_lastCanvasPaintState = state;
  KaCrashGuard::logLine(QStringLiteral("[canvas] 1:%1 · %2")
                            .arg(m_canvas->scale(), 0, 'f', 0)
                            .arg(state));
#endif
}

void MainWindow::logLayerCensus(const QString& tag) {
#if KA_HGIS_HAS_QGIS
  const QString census = LayerOps::layerCensus(QgsProject::instance());
  m_lastLayerCensus = census;
  KaCrashGuard::logLine(QStringLiteral("[layers/%1] %2").arg(tag, census));
#else
  Q_UNUSED(tag);
#endif
}

void MainWindow::restoreLastSurvey() {
  if (m_isOpeningSurvey || !m_restoreLastSurveyEnabled)
    return;
  if (!m_surveyPath.isEmpty())
    return;
  if (m_isLoadingBasemaps) {
    // 배경지도 생성 중 WMS 공급자의 QEventLoop 스핀으로 인한 재진입 방지
    QTimer::singleShot(100, this, &MainWindow::restoreLastSurvey);
    return;
  }
  QSettings st = RecentSurveys::userSettings();
  if (RecentSurveys::takeSkipAutoRestore(st)) {
    KaCrashGuard::logLine(QStringLiteral("[boot] 지난 실행이 조사 복원 중 끊겨 홈에 머뭅니다"));
    return;
  }
  const QString last = RecentSurveys::lastPath(st);
  if (last.isEmpty() || !QFile::exists(last))
    return;
  RecentSurveys::setSkipAutoRestore(st, true);
  const QString gpkg = QFileInfo(last).suffix().compare(QLatin1String("gpkg"), Qt::CaseInsensitive) == 0
      ? last
      : QFileInfo(last).dir().filePath(QFileInfo(last).completeBaseName() + QStringLiteral(".gpkg"));
  const QString toOpen = QFile::exists(gpkg) ? gpkg : last;
  if (QFileInfo(toOpen).suffix().compare(QLatin1String("gpkg"), Qt::CaseInsensitive) == 0) {
    // 아이콘 재실행은 내장 작업공간을 읽지 않는다. 저장된 위성·지적 WMS 를
    // 한꺼번에 그리면 crash-20260906-153833 / 153900
    // (provider_wms deleteLater AV) 이 난다. 작업공간은 「조사 열기」만.
    openSurveyGpkg(toOpen, OpenSurveyMode::LayersOnly);
  } else
    openRecentSurvey(toOpen);
}

void MainWindow::saveProject() {
#if KA_HGIS_HAS_QGIS
  if (m_isOpeningSurvey) return;
  if (m_surveyPath.isEmpty()) {
    saveProjectAs();
    return;
  }
  persistSurveyWork();
#endif
}

void MainWindow::saveProjectAs() {
#if KA_HGIS_HAS_QGIS
  if (m_isOpeningSurvey) return;
  const auto updateTitle = qScopeGuard([this] { refreshWindowTitle(); });
  QScopedValueRollback<bool> saving(m_isOpeningSurvey, true);

  QString defaultPath;
  if (!m_surveyPath.isEmpty()) {
    QFileInfo fi(m_surveyPath);
    defaultPath = fi.dir().filePath(fi.completeBaseName() + QStringLiteral("_복사본.gpkg"));
  } else {
    defaultPath = QDir(preferredSurveyDir()).filePath(QStringLiteral("새조사.gpkg"));
  }

  const QString selected = QFileDialog::getSaveFileName(
      this, QStringLiteral("다른 이름으로 저장"), defaultPath,
      QStringLiteral("고고학 조사 파일 (*.gpkg *.qgz);;GeoPackage (*.gpkg);;QGIS 프로젝트 (*.qgz)"));
  if (selected.isEmpty()) return;

  QFileInfo newFi(selected);
  QString targetGpkg = newFi.suffix().toLower() == QLatin1String("qgz")
      ? newFi.dir().filePath(newFi.completeBaseName() + QStringLiteral(".gpkg"))
      : selected;
  if (!targetGpkg.endsWith(QLatin1String(".gpkg"), Qt::CaseInsensitive))
    targetGpkg += QStringLiteral(".gpkg");

  const bool sameFile = !m_surveyPath.isEmpty() &&
      QFileInfo(m_surveyPath).absoluteFilePath().compare(
          QFileInfo(targetGpkg).absoluteFilePath(), Qt::CaseInsensitive) == 0;
  if (m_workspaceRestoreSuppressesAutosave && sameFile) {
    notify(Notice::Warning, QStringLiteral("다른 이름으로 저장"),
           QStringLiteral("복원하지 못한 원래 작업공간은 덮어쓸 수 없습니다. 다른 파일 이름을 선택해 주세요."));
    return;
  }
  if (!sameFile && (QFileInfo::exists(targetGpkg) ||
      QFileInfo::exists(QFileInfo(targetGpkg).dir().filePath(
          QFileInfo(targetGpkg).completeBaseName() + QStringLiteral(".qgz"))))) {
    notify(Notice::Warning, QStringLiteral("다른 이름을 선택해 주세요"),
           QStringLiteral("같은 이름의 조사 파일 또는 작업공간 사본이 있습니다. 기존 자료를 보존하기 위해 "
                          "덮어쓰지 않았습니다. 사용하지 않은 이름으로 저장하세요."));
    return;
  }
  try {
  if (!commitSurveyEdits()) return;

  struct OriginalSource {
    QPointer<QgsVectorLayer> layer;
    QString source;
    QString name;
    QString provider;
    QgsMapLayerStyle style;
    QgsFeatureList memoryFeatures;
    bool editable = false;
  };
  QList<OriginalSource> originalSources;
  QgsProject* project = QgsProject::instance();
  const QString originalProjectFile = project->fileName();
  const QString originalHome = project->presetHomePath();
  for (QgsMapLayer* item : project->mapLayers()) {
    auto* vector = qobject_cast<QgsVectorLayer*>(item);
    if (!vector || (vector->providerType() != QLatin1String("ogr") &&
                    vector->providerType() != QLatin1String("memory"))) continue;
    OriginalSource original;
    original.layer = vector;
    original.source = vector->source();
    original.name = vector->name();
    original.provider = vector->providerType();
    original.editable = vector->isEditable();
    if (original.provider == QLatin1String("memory")) {
      auto features = vector->getFeatures();
      QgsFeature feature;
      while (features.nextFeature(feature)) original.memoryFeatures.append(feature);
    }
    original.style.readFromLayer(vector);
    originalSources.append(original);
  }
  bool savedAs = false;
  const auto restoreSources = qScopeGuard([&] {
    if (savedAs) return;
    for (auto& original : originalSources) {
      if (!original.layer || original.layer->source() == original.source) continue;
      const QString storedSource = original.layer->source();
      original.layer->setDataSource(original.source, original.name, original.provider);
      if (original.provider == QLatin1String("memory") && original.layer->isValid() &&
          !original.layer->dataProvider()->addFeatures(original.memoryFeatures)) {
        original.layer->setDataSource(storedSource, original.name, QStringLiteral("ogr"));
        notify(Notice::Warning, QStringLiteral("임시 도형 복구 확인 필요"),
               QStringLiteral("%1을 메모리로 되돌리지 못해 새 사본에 보관한 도형을 유지합니다. "
                              "사본 파일을 삭제하지 말고 다시 저장하세요.").arg(original.name));
      }
      original.style.writeToLayer(original.layer);
      if (original.editable && !original.layer->isEditable()) original.layer->startEditing();
    }
    project->setFileName(originalProjectFile);
    project->setPresetHomePath(originalHome);
    project->setDirty(true);
  });

  // 1. 기존 조사가 있으면 새 GPKG로 복사하여 조사 데이터(유구, 구역 등) 보존
  if (!m_surveyPath.isEmpty() && QFile::exists(m_surveyPath) && !sameFile) {
    QString copyError;
    if (!SurveyStorage::copySurvey(m_surveyPath, targetGpkg, &copyError)) {
      QMessageBox::warning(this, QStringLiteral("저장 실패"),
                           QStringLiteral("파일을 생성할 수 없습니다:\n%1\n%2").arg(targetGpkg, copyError));
      return;
    }
    // 논리 키는 같은 구역도 실제 테이블은 survey_area_2 등으로 다를 수 있다.
    // 파일 경로만 바꾸고 각 레이어의 테이블·옵션·스타일은 보존한다.
    for (QgsMapLayer* l : QgsProject::instance()->mapLayers()) {
      auto* vl = qobject_cast<QgsVectorLayer*>(l);
      if (!vl || vl->providerType() != QLatin1String("ogr")) continue;
      const QString source = vl->source();
      const QString sourcePath = source.section(QLatin1Char('|'), 0, 0);
      if (QFileInfo(sourcePath).absoluteFilePath().compare(
              QFileInfo(m_surveyPath).absoluteFilePath(), Qt::CaseInsensitive) == 0) {
        const int options = source.indexOf(QLatin1Char('|'));
        vl->setDataSource(targetGpkg + (options < 0 ? QString() : source.mid(options)),
                          vl->name(), QStringLiteral("ogr"));
        if (!vl->isValid()) {
          notify(Notice::Warning, QStringLiteral("저장 실패"),
                 QStringLiteral("새 파일에서 %1을 읽지 못했습니다. 현재 작업은 유지됩니다. "
                                "다른 저장 폴더를 선택해 다시 저장하세요.").arg(vl->name()));
          return;
        }
      }
    }
  } else if (m_surveyPath.isEmpty() || !QFile::exists(m_surveyPath)) {
    QString err;
    const QFileInfo targetFile(targetGpkg);
    const QString created = SurveyProjectFactory::createNewSurvey(targetFile.dir().absolutePath(),
                                                                  targetFile.completeBaseName(),
                                                                  &err, m_workCrs);
    if (created.isEmpty()) {
      QMessageBox::warning(this, QStringLiteral("저장 실패"), err);
      return;
    }
    targetGpkg = created;
  }

  // 2. 외부 벡터를 새 조사 파일 안으로 들여온 뒤 작업공간을 그 안에 기록한다.
  //    이렇게 해야 새로 만든 .gpkg 하나만 건네도 상대가 그대로 열 수 있다.
  const SurveyStorage::AbsorbResult absorbed =
      SurveyStorage::absorbExternalVectors(QgsProject::instance(), targetGpkg);
  if (!absorbed.failed.isEmpty()) {
    notify(Notice::Warning, QStringLiteral("저장을 마치지 못했습니다"),
           QStringLiteral("%1을 보관하지 못했습니다. 현재 작업을 유지합니다. "
                          "저장 공간을 확인한 뒤 다시 저장하세요.")
               .arg(absorbed.failed.join(QStringLiteral(", "))));
    return;
  }
  QString serr;
  if (!SurveyStorage::writeEmbedded(QgsProject::instance(), targetGpkg, &serr)) {
    QMessageBox::warning(this, QStringLiteral("저장 실패"),
                         QStringLiteral("새 조사 파일에 작업공간을 저장하지 못했습니다:\n%1\n\n%2")
                             .arg(targetGpkg, serr));
    return;
  }
  m_surveyPath = targetGpkg;
  m_surveySessionReady = true;
  m_workspaceRestoreSuppressesAutosave = false;
  savedAs = true;
  kaClearQgisProjectUnsafeMark(targetGpkg);
  // 3. 동반 .qgz 사본. 파일 위치가 바뀌므로 상대경로가 새 폴더 기준으로 다시 계산된다.
  const QFileInfo targetInfo(targetGpkg);
  const QString targetQgz = targetInfo.dir().filePath(targetInfo.completeBaseName() + QStringLiteral(".qgz"));
  QString werr;
  const bool companionSaved = kaWriteQgisProjectAtomic(QgsProject::instance(), targetQgz, &werr);
  if (companionSaved)
    kaClearQgisProjectUnsafeMark(targetQgz);
  else {
    KaCrashGuard::logLine(QStringLiteral("[saveas] 동반 .qgz 사본 저장 실패 — %1").arg(werr));
    notify(Notice::Warning, QStringLiteral("GPKG 저장 완료 · 보조 사본 확인 필요"),
           QStringLiteral("QGZ 사본을 갱신하지 못했습니다. 해당 파일을 사용하는 프로그램을 "
                          "닫고 다시 저장하세요. 조사 내용은 새 GPKG에 보관했습니다."));
  }
  if (!absorbed.skippedRaster.isEmpty())
    notify(Notice::Info, QStringLiteral("다른 이름으로 저장"),
           QStringLiteral("사진·래스터는 바깥 파일을 함께 보관해 주세요: %1")
               .arg(absorbed.skippedRaster.join(QStringLiteral(", "))));

  // 3. 윈도우 타이틀 및 최근 조사 갱신
  setWindowTitle(QStringLiteral("필드고고학GIS — %1").arg(newFi.completeBaseName()));
  rememberSurvey(targetGpkg, newFi.completeBaseName());
  rememberSurveyDir(targetGpkg);
  markSurveySaved();

  const QString msg = QStringLiteral("조사 데이터와 레이어 구성을 새 파일로 저장했습니다:\n%1").arg(QDir::toNativeSeparators(targetGpkg));
  statusBar()->showMessage(companionSaved
      ? QStringLiteral("다른 이름으로 저장했습니다: %1").arg(newFi.fileName())
      : QStringLiteral("새 GPKG 저장 완료. QGZ 사본은 다시 저장해야 합니다."), 8000);
  if (companionSaved) notify(Notice::Success, QStringLiteral("다른 이름으로 저장"), msg);
  } catch (...) {
    QgsProject::instance()->setDirty(true);
    KaCrashGuard::logLine(QStringLiteral("[saveas] 저장 예외로 중단 — 현재 작업 유지"));
    notify(Notice::Warning, QStringLiteral("저장을 마치지 못했습니다"),
           QStringLiteral("저장 중 오류가 발생했습니다. 창을 닫지 말고 저장 공간을 확인한 뒤 "
                          "다른 이름으로 다시 저장하세요."));
  }
#else
  QMessageBox::information(this, QStringLiteral("스텁"), QStringLiteral("다른 이름으로 저장 시뮬레이션"));
#endif
}

void MainWindow::openProject() {
#if KA_HGIS_HAS_QGIS
  if (m_isOpeningSurvey) return;
  const QString path = QFileDialog::getOpenFileName(
      this, QStringLiteral("열기"), QString(),
      QStringLiteral("조사 (*.gpkg *.qgz *.qgs);;GeoPackage (*.gpkg);;QGIS (*.qgz *.qgs)"));
  if (path.isEmpty()) return;
  if (QFileInfo(path).suffix().compare(QLatin1String("gpkg"), Qt::CaseInsensitive) == 0) {
    if (openSurveyGpkg(path))
      setWindowTitle(QStringLiteral("필드고고학GIS — %1").arg(QFileInfo(path).completeBaseName()));
    return;
  }
  const QString companionGpkg =
      QFileInfo(path).dir().filePath(QFileInfo(path).completeBaseName() + QStringLiteral(".gpkg"));
  if (QFile::exists(companionGpkg)) {
    if (openSurveyGpkg(companionGpkg)) {
      setWindowTitle(QStringLiteral("필드고고학GIS — %1").arg(QFileInfo(path).completeBaseName()));
    }
    return;
  }
  if (!validateStandaloneProjectForOpen(path) || !confirmSaveBeforeOpeningSurvey()) return;
  ++m_surveyGeneration;
  if (m_referenceDownload) m_referenceDownload->cancel();
  m_locator->cancel();
  if (m_searchProgress) { m_searchProgress->hide(); m_searchProgress->deleteLater(); m_searchProgress = nullptr; }
  m_locationSearchBusy = false;
  m_adminBoundary->cancel();
  if (m_boundaryProgress) { m_boundaryProgress->hide(); m_boundaryProgress->deleteLater(); m_boundaryProgress = nullptr; }
  QScopedValueRollback<bool> opening(m_isOpeningSurvey, true);
  stopAlignSession();
  stopCaptureTool();
  m_editLayer = nullptr;
  m_isSplittingPolygon = false;
  m_undoActions.clear();
  m_surveySessionReady = false;
  m_surveyPath.clear();
  m_workspaceRestoreSuppressesAutosave = true;
  if (kaQgisProjectFileIsUnsafeToRead(path) ||
      !kaSafeReadQgisProject(QgsProject::instance(), path, nullptr, /*loadLayouts=*/true)) {
    kaMarkQgisProjectUnsafeToRead(path);
    QMessageBox::warning(this, QStringLiteral("오류"), QStringLiteral("프로젝트를 열 수 없습니다."));
    return;
  }
  LayerOps::pruneDuplicateSatelliteLayers(QgsProject::instance());
  LayerOps::restoreMissingLayerTreeNodes(QgsProject::instance());
  LayerOps::restoreThematicOverlayVisibility(QgsProject::instance());
  for (QgsMapLayer* ml : QgsProject::instance()->mapLayers()) {
    auto* vl = qobject_cast<QgsVectorLayer*>(ml);
    if (!vl || !vl->isValid()) continue;
    const QString src = vl->source();
    if (src.contains(QLatin1String(".gpkg"), Qt::CaseInsensitive)) {
      const QString gpkg = src.split(QLatin1Char('|')).first();
      if (QFile::exists(gpkg)) {
        m_surveyPath = gpkg;
        break;
      }
    }
  }
  if (m_surveyPath.isEmpty() && QFile::exists(companionGpkg))
    m_surveyPath = companionGpkg;
  if (!m_surveyPath.isEmpty())
    LayerOps::addNonEmptySavedGpkgLayers(QgsProject::instance(), m_surveyPath);
  m_surveySessionReady = true;
  m_workspaceRestoreSuppressesAutosave = false;
  if (auto* cp = layerByKey(QStringLiteral("control_points")))
    LayerOps::ensureControlPointQualityFields(cp);
  for (QgsMapLayer* ml : QgsProject::instance()->mapLayers()) {
    if (ml && ml->name() == QLatin1String("DEM") && ml->isValid()) {
      if (auto* rl = qobject_cast<QgsRasterLayer*>(ml)) {
        DemPresentation::restore(rl);
        DemPresentation::followCanvas(rl, m_canvas);
        if (LayerOps::isLayerVisible(QgsProject::instance(), QStringLiteral("DEM"))) {
          LayerOps::ensureDemRelief(QgsProject::instance(), rl);
        }
      }
      break;
    }
  }
  if (QgsProject::instance()->crs().isValid())
    m_workCrs = QgsProject::instance()->crs().authid();
  LayerOps::ensureOtfEnabled(QgsProject::instance(), m_canvas, m_workCrs);

  if (m_layerTree && m_layerTree->selectionModel())
    m_layerTree->selectionModel()->clear();
  refreshLayerEmptyState();

  LayerOps::syncMapCanvas(QgsProject::instance(), m_canvas, false);
  if (!LayerOps::zoomToProjectDataLayers(m_canvas, QgsProject::instance())) {
    LayerOps::zoomToKorea(m_canvas, m_workCrs, false);
  }
  m_startupViewApplied = true;
  if (m_canvas) m_canvas->refresh();
  ensureDefaultBasemaps();
  setWindowTitle(QStringLiteral("필드고고학GIS — %1").arg(QFileInfo(path).completeBaseName()));
  rememberSurvey(path, QFileInfo(path).completeBaseName());
  showMapWorkspace();
  updateNextActionStatus();
#else
  QMessageBox::information(this, QStringLiteral("스텁"), QStringLiteral("프로젝트 열기 시뮬레이션"));
#endif
}

void MainWindow::setupWorkPanel() {
  auto* dock = new QDockWidget(QStringLiteral("작업 제어"), this);
  dock->setObjectName(QStringLiteral("workDock"));
  dock->setAllowedAreas(Qt::RightDockWidgetArea | Qt::LeftDockWidgetArea);
  auto* box = new QWidget(dock);
  auto* lay = new QVBoxLayout(box);
  lay->setContentsMargins(10, 10, 10, 10);
  lay->setSpacing(8);
  auto* title = new QLabel(QStringLiteral("원하는 작업을 누르세요"), box);
  m_workHint = new QLabel(box);
  m_workHint->setObjectName(QStringLiteral("workHint"));
  m_workHint->setWordWrap(true);
  m_workList = new QListWidget(box);
  m_workList->setObjectName(QStringLiteral("workControlList"));
  m_workList->setSpacing(3);
  connect(m_workList, &QListWidget::itemClicked, this, &MainWindow::onWorkControlClicked);
  lay->addWidget(title);
  lay->addWidget(m_workHint);
  lay->addWidget(m_workList, 1);
  dock->setWidget(box);
  addDockWidget(Qt::RightDockWidgetArea, dock);
  dock->setMinimumWidth(220);
  dock->hide();
  refreshWorkPanel();
}

void MainWindow::refreshWorkPanel() {
  if (!m_workList) return;
  const QJsonObject st = buildProjectState();
#if KA_HGIS_HAS_QGIS
  const bool hasBg = LayerOps::hasVisibleReferenceLayer(QgsProject::instance());
#else
  const bool hasBg = false;
#endif
  const int errCount = lastChecklistErrorCount();
  const bool surveyReady = !m_surveyPath.isEmpty() ||
                           st.value(QStringLiteral("survey_area_count")).toInt() > 0;
  QJsonObject st2 = st;
  if (surveyReady && st2.value(QStringLiteral("survey_area_count")).toInt() == 0)
    st2.insert(QStringLiteral("layer_count"), 1);
  const auto steps = WorkflowGuide::evaluate(st2, hasBg, errCount, m_packageCreated);

  struct Extra {
    QString id;
    QString title;
    QString hint;
    bool done;
  };
  const QList<Extra> extras = {
      {QStringLiteral("action_edit_attrs"), QStringLiteral("속성 고치기"),
       QStringLiteral("그린 도형을 클릭해 종류·시대를 넣습니다."),
       st.value(QStringLiteral("has_kind_period")).toBool() &&
           st.value(QStringLiteral("feature_poly_count")).toInt() > 0},
      {QStringLiteral("action_section"), QStringLiteral("단면선 그리기"),
       QStringLiteral("층위·단면 기준선을 그립니다."), false},
      {QStringLiteral("action_import_csv"), QStringLiteral("CSV 기준점"),
       QStringLiteral("GPS CSV를 가져와 기준점을 채웁니다."),
       st.value(QStringLiteral("control_points_count")).toInt() >= 2},
      {QStringLiteral("action_drawing_studio"), QStringLiteral("도면 만들기"),
       QStringLiteral("조사구역도 등 5종 PDF를 미리보고 저장합니다."), false},
  };

  const QString cur = m_workList->currentItem()
                          ? m_workList->currentItem()->data(Qt::UserRole).toString()
                          : QString();
  m_workList->clear();
  auto addItem = [&](const QString& id, const QString& title, const QString& hint, bool done) {
    auto* it = new QListWidgetItem(
        QStringLiteral("%1  %2\n    %3")
            .arg(done ? QStringLiteral("완료") : QStringLiteral("실행"), title, hint),
        m_workList);
    it->setData(Qt::UserRole, id);
    it->setToolTip(hint);
  };
  for (const auto& s : steps)
    addItem(s.actionId, s.title, s.completionHint, s.complete);
  for (const auto& e : extras)
    addItem(e.id, e.title, e.hint, e.done);

  if (!cur.isEmpty()) {
    for (int i = 0; i < m_workList->count(); ++i) {
      if (m_workList->item(i)->data(Qt::UserRole).toString() == cur) {
        m_workList->setCurrentRow(i);
        break;
      }
    }
  }
  if (m_workHint) {
    QString next = QStringLiteral("아무 항목이나 눌러 바로 실행합니다.");
    for (const auto& s : steps) {
      if (!s.complete) {
        next = QStringLiteral("다음: %1 — %2").arg(s.title, s.completionHint);
        break;
      }
    }
    m_workHint->setText(next);
  }
  updateUndoRedoActions();
}

void MainWindow::onWorkControlClicked(QListWidgetItem* item) {
  if (!item) return;
  const QString id = item->data(Qt::UserRole).toString();
  if (id == QLatin1String("action_new_survey"))
    newSurvey();
  else if (id == QLatin1String("action_add_basemap"))
    showSubToolsBasemap();
  else if (id == QLatin1String("action_digitize_area"))
    startEditSurveyArea();
  else if (id == QLatin1String("action_digitize_feature"))
    startEditFeaturePoly();
  else if (id == QLatin1String("action_add_control_point"))
    addControlPoint();
  else if (id == QLatin1String("action_run_checklist"))
    runChecklist();
  else if (id == QLatin1String("action_export_package"))
    exportShpPackage();
  else if (id == QLatin1String("action_edit_attrs"))
    startAttributeEditTool();
  else if (id == QLatin1String("action_section"))
    startEditSectionLine();
  else if (id == QLatin1String("action_import_csv"))
    importControlCsv();
  else if (id == QLatin1String("action_drawing_studio"))
    openLayoutDesigner();
  refreshWorkPanel();
}

void MainWindow::setWorkCrs(const QString& authId) {
  m_workCrs = authId;
  if (m_status) m_status->setWorkCrs(authId);
#if KA_HGIS_HAS_QGIS
  QString err;
  LayerOps::ensureOtfEnabled(QgsProject::instance(), m_canvas, authId);
  if (!LayerOps::setWorkCrs(QgsProject::instance(), m_canvas, authId, &err, false)) {
    QMessageBox::warning(this, QStringLiteral("CRS"), err);
    return;
  }
  LayerOps::applyKoreaMapLimits(QgsProject::instance(), m_canvas);
  LayerOps::syncMapCanvas(QgsProject::instance(), m_canvas, false);
  LayerOps::clampCanvasToKorea(m_canvas);
  LayerOps::refreshXyzBasemapTiles(m_canvas);
  if (authId.contains(QLatin1String("5187")))
    statusBar()->showMessage(QStringLiteral("동부원점으로 맞춰 두었습니다. 이제 구역을 그리면 됩니다."), 8000);
  else
    statusBar()->showMessage(QStringLiteral("중부원점으로 맞춰 두었습니다. 이제 구역을 그리면 됩니다."), 8000);
#endif
}

void MainWindow::setWorkCrs5186() {
  setWorkCrs(QStringLiteral("EPSG:5186"));
  if (auto* b86 = findChild<QToolButton*>(QStringLiteral("btnCrs5186"))) b86->setChecked(true);
  if (auto* b87 = findChild<QToolButton*>(QStringLiteral("btnCrs5187"))) b87->setChecked(false);
}

void MainWindow::setWorkCrs5187() {
  setWorkCrs(QStringLiteral("EPSG:5187"));
  if (auto* b86 = findChild<QToolButton*>(QStringLiteral("btnCrs5186"))) b86->setChecked(false);
  if (auto* b87 = findChild<QToolButton*>(QStringLiteral("btnCrs5187"))) b87->setChecked(true);
}

QString MainWindow::vworldApiKeyOrPrompt() {
  QString key = VworldSettings::loadApiKey();
  if (!key.isEmpty()) return key;
  QMessageBox::information(this, QStringLiteral("VWorld API 키 필요"),
      QStringLiteral("VWorld 배경지도를 쓰려면 API 키가 필요합니다.\n도움말 → VWorld API 키 설정"));
  return {};
}

void MainWindow::configureVworldKey() {
  bool ok = false;
  const QString cur = VworldSettings::loadApiKey();
  const QString key = QInputDialog::getText(
      this, QStringLiteral("VWorld API 키"),
      QStringLiteral("vworld.kr 인증키 (SSOT: VWorld/ApiKey)\n배경지도·검색 공통"),
      QLineEdit::Normal, cur, &ok);
  if (!ok) return;
  VworldSettings::saveApiKey(key);
  if (!key.trimmed().isEmpty()) {
    ensureDefaultBasemaps();
    if (m_canvas)
      LayerOps::syncMapCanvas(QgsProject::instance(), m_canvas, false);
  }
  statusBar()->showMessage(key.isEmpty()
      ? QStringLiteral("VWorld 키 삭제됨")
      : QStringLiteral("VWorld API 키 저장됨 — 위성과 지적을 올립니다"), 6000);
}

QString MainWindow::rulesPath() const {
  const QStringList cands = {
    QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("../data/rules/drawing_checklist.v1.json")),
    QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("data/rules/drawing_checklist.v1.json")),
    QDir::current().filePath(QStringLiteral("data/rules/drawing_checklist.v1.json"))
  };
  for (const QString& c : cands) if (QFile::exists(c)) return c;
  return cands.last();
}

void MainWindow::zoomMapToFullMax() {
#if KA_HGIS_HAS_QGIS
  LayerOps::zoomToFullMax(m_canvas);
  LayerOps::clampCanvasToKorea(m_canvas);
  LayerOps::refreshXyzBasemapTiles(m_canvas);
  statusBar()->showMessage(QStringLiteral("한국 전체 범위"), 4000);
#endif
}

