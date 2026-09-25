#include "MainWindow.h"
#include "KaHgisVersion.h"
#include <QDateTime>
#include "KaStartupSplash.h"
#include "KaLayerInformation.h"
#include "KaWindowGeometry.h"
#include "core/DemPresentation.h"
#include "KaTheme.h"
#include "KaUserError.h"
#include "KaIcons.h"
#include "KaCaptureMapTool.h"
#include "KaAttributeMapTool.h"
#include "KaAlignMapTool.h"
#include "KaImageView.h"
#include "KaDrawingStudio.h"
#include "KaSectionDrawingStudio.h"
#include "KaTerrain3dStudio.h"
#include "KaTerrain3dLayoutStudio.h"
#include "KaStartPage.h"
#include "KaCoordPointMapTool.h"
#include "KaMeasureMapTool.h"
#include "core/DemAnalyzer.h"
#include "core/TilePackService.h"
#include "core/TrenchGridGenerator.h"
#include "KaAboveLabelsOverlay.h"
#include "KaCanvasGridOverlay.h"
#include "KaTrenchMoveTool.h"
#include "KaFeatureSelectTool.h"
#include "KaFoundLocationMark.h"
#include "KaStatusBar.h"
#include "KaBeginnerRibbon.h"
#include "KaSnapSettingsWidget.h"
#include "KaFeatureFormDialog.h"
#include "KaFileBrowserPanel.h"
#include "KaLayerOpacityRail.h"
#include "KaCrashGuard.h"
#include "KaReferenceDownloadJob.h"
#include <QProgressDialog>
#include <QElapsedTimer>
#include <QScopeGuard>
#include <exception>
#include "KaTrenchDialog.h"
#include "KaDemClassDialog.h"
#include "KaTopographicBrowser.h"
#include "KaTopographicImportDialog.h"
#include "KaSurveyAreaDialog.h"
#include "core/RecentSurveys.h"
#include "core/KaSafeQgis.h"
#include "core/SurveyStorage.h"
#include "core/SurveySession.h"
#include "core/GeorefService.h"
#include "core/BufferAnalysis.h"
#include "core/ChecklistEngine.h"
#include "core/SurveyProjectFactory.h"
#include "core/Terrain3dLayoutService.h"
#include "core/LayerOps.h"
#include "KaHeritageBrowser.h"
#include "KaHeritageSetupDialogs.h"
#include "core/HeritageImport.h"
#include "core/HeritageIntranetSettings.h"
#include "core/HeritageRegionResolver.h"
#include "core/HeritageStyle.h"
#include "core/SoilMapService.h"
#include "core/PaleoLandformService.h"
#include "core/GeologyMapService.h"
#include "core/RiverMapService.h"
#include "core/VworldSettings.h"
#include "core/LocationSearch.h"
#include "core/CadastralImport.h"
#include "core/KoreaRegionCatalog.h"
#include "core/AdminBoundaryService.h"
#include "KaRegionLocator.h"
#include "KaAppBar.h"
#include "KaMapControls.h"
#include "core/WorkflowGuide.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <QApplication>
#include <QScreen>
#include <QShowEvent>
#include <QWindow>
#include <QTimer>
#include <QPointer>
#include <QCursor>
#include <QAction>
#include <QDockWidget>
#include <QScrollArea>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QTabBar>
#include <QTabWidget>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMenuBar>
#include <QMessageBox>
#include <QStatusBar>
#include <QToolBar>

#include <QVBoxLayout>
#include <QWidget>
#include <QDialog>
#include <QDialogButtonBox>
#include <QLineEdit>
#include <QComboBox>
#include <QEventLoop>
#include <QCoreApplication>
#include <QDir>
#include <QTextStream>
#include <QFile>
#include <QShortcut>
#include <QKeySequence>
#include <QKeySequence>
#include <QAbstractItemView>
#include <QMenu>
#include <QPixmap>
#include <QImage>
#include <QAction>
#include <QSize>
#include <QListWidgetItem>
#include <QToolBar>
#include <QAction>
#include <QToolButton>
#include <QKeyEvent>
#include <QEvent>
#include <QMouseEvent>
#include <QModelIndex>
#include <QItemSelectionModel>
#include <QCompleter>
#include <QStringListModel>
#include <QInputDialog>
#include <QVector>
#include <QPair>
#include <QTextEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QCloseEvent>
#include <QSettings>
#include <QScopedValueRollback>
#include <QListWidget>
#include <QListWidgetItem>
#include <QSplitter>
#include <QFrame>
#include <QStandardPaths>
#include <QColor>
#include <QPalette>
#include <QUrl>
#include <QDesktopServices>
#include <QSizePolicy>
#include <QMimeData>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QHBoxLayout>
#include <QStorageInfo>
#include <QHash>
#include <QMetaType>
#include <QColorDialog>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QAbstractSpinBox>
#include <QCheckBox>
#include <QLocale>
#include <QLayout>

#if KA_HGIS_HAS_QGIS
#include <qgsmapcanvas.h>
#include <qgsmapmouseevent.h>
#include <qgsmessagebar.h>
#include <qgsproject.h>
#include <qgsmaplayerstyle.h>
#include <qgsmaplayer.h>
#include <qgsvectorlayer.h>
#include <qgsrasterlayer.h>
#include <qgssinglebandpseudocolorrenderer.h>
#include <qgsrasterrenderer.h>
#include <qgsrastertransparency.h>
#include <qgslayertree.h>
#include <qgslayertreegroup.h>
#include <qgslayertreenode.h>
#include <qgslayertreelayer.h>
#include <qgslayertreeview.h>
#include <qgslayertreemodel.h>
#include <qgslayertreemapcanvasbridge.h>
#include <qgscoordinatereferencesystem.h>
#include <qgsmaptool.h>
#include <qgsmaptoolemitpoint.h>
#include <qgspointxy.h>
#include <qgsmaptoolpan.h>
#include <qgsmaptoolselect.h>
#include <qgssnappingconfig.h>
#include <qgssnappingutils.h>
#include <qgsrubberband.h>
#include <qgsapplication.h>
#include <qgsmessagelog.h>
#include <qgsfeature.h>
#include <qgsfeatureiterator.h>
#include <qgsgeometry.h>
#include <qgspointxy.h>
#include <qgsvectorfilewriter.h>
#include <qgscoordinatetransform.h>
#include <qgscoordinatetransformcontext.h>
#include <qgsrectangle.h>
#include <qgsvertexmarker.h>
#include <qgsmaptopixel.h>
#include <qgspoint.h>
#include <qgspointlocator.h>
#include <qgsexception.h>
#include <qgsproviderregistry.h>
#include <qgsnetworkaccessmanager.h>
#include <qgsvectordataprovider.h>
#include <qgsprovidersublayerdetails.h>
#endif
#include <QGraphicsDropShadowEffect>

static void applyWidgetShadow(QWidget* w, int blur = 14, int yOffset = 3, int alpha = 35) {
  if (!w) return;
  auto* shadow = new QGraphicsDropShadowEffect(w);
  shadow->setBlurRadius(blur);
  shadow->setOffset(0, yOffset);
  shadow->setColor(QColor(0, 0, 0, alpha));
  w->setGraphicsEffect(shadow);
}

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
  setWindowTitle(QStringLiteral("Strata"));
  setWindowIcon(KaIcons::appIcon());
  resize(1280, 900);
  m_status = new KaStatusBar(this);
  setStatusBar(m_status);
  m_checklist = new ChecklistEngine(this);
  m_checklist->loadRules(rulesPath());
  m_locator = new LocationSearch(this);
  connect(m_locator, &LocationSearch::finished, this, [this](const QVector<LocationHit>& hits) {
    onLocationResults(hits);
  });
  connect(m_locator, &LocationSearch::failed, this, [this](const QString& msg) {
    onLocationFailed(msg);
  });
  m_adminBoundary = new AdminBoundaryService(this);
  connect(m_adminBoundary, &AdminBoundaryService::fetched, this, &MainWindow::onAdminBoundaryFetched);
  connect(m_adminBoundary, &AdminBoundaryService::failed, this, &MainWindow::onAdminBoundaryFailed);
  buildMenus();
  buildUi();
  refreshWorkPanel();
  updateNextActionStatus();
  auto* delAct = new QAction(QStringLiteral("선택 도형 삭제"), this);
  delAct->setShortcut(QKeySequence::Delete);
  delAct->setShortcutContext(Qt::WindowShortcut);
  connect(delAct, &QAction::triggered, this, [this]() {
    if (routeEditKeyToActiveStudio(true)) return;
    QWidget* focus = QApplication::focusWidget();
    if (m_alignPointList && focus && (focus == m_alignPointList || m_alignPointList->isAncestorOf(focus)))
      deleteSelectedAlignPoint();
    else if (m_layerTree && focus && (focus == m_layerTree || m_layerTree->isAncestorOf(focus)))
      removeSelectedLayers();
    else
      deleteFeaturesOrSelectedReferenceLayers();
  });
  addAction(delAct);
  auto* fullAct = new QAction(QStringLiteral("전체 화면"), this);
  fullAct->setShortcut(Qt::Key_F11);
  fullAct->setShortcutContext(Qt::WindowShortcut);
  connect(fullAct, &QAction::triggered, this, [this]() {
    if (isFullScreen())
      showMaximized();
    else
      showFullScreen();
  });
  addAction(fullAct);
  {
    QSettings st = RecentSurveys::userSettings();
    const QByteArray geo = st.value(QStringLiteral("MainWindow/geometry")).toByteArray();
    if (!geo.isEmpty())
      restoreGeometry(geo);
    const QByteArray dockState = st.value(QStringLiteral("MainWindow/state")).toByteArray();
    if (!dockState.isEmpty())
      restoreState(dockState);
    // restoreState brings back whether the drawing-tools row was showing when the window was
    // last closed. That row belongs to an active tool, so it must not reappear as an empty strip.
    if (m_subToolbar) m_subToolbar->hide();
    const QByteArray split = st.value(QStringLiteral("MainWindow/mainSplit")).toByteArray();
    if (m_mainSplit && !split.isEmpty()) {
      m_mainSplit->restoreState(split);
      const int totalW = m_mainSplit->width();
      if (totalW > 300) {
        const QList<int> sz = m_mainSplit->sizes();
        if (!sz.isEmpty() && sz.at(0) > totalW * 0.35) {
          const int leftW = qBound(160, int(totalW * 0.22), 360);
          m_mainSplit->setSizes({leftW, totalW - leftW});
        }
      }
    }
    const QByteArray leftState = st.value(QStringLiteral("MainWindow/leftSplit")).toByteArray();
    if (m_leftSplit && !leftState.isEmpty())
      m_leftSplit->restoreState(leftState);
    if (m_leftSplit) {
      QTimer::singleShot(0, this, [this]() {
        KaLayerInformationView::protectSidebarList(
            m_leftSplit, m_layerTree, findChild<QToolButton*>(QStringLiteral("sidebarFilesToggle")),
            findChild<QWidget*>(QStringLiteral("sidebarFilesScroll")),
            findChild<KaLayerInformationPanel*>(QStringLiteral("layerInformationPanel")));
      });
    }
  }
}

MainWindow::~MainWindow() {
  delete m_topographicBrowser.data();
  delete m_topographicImport.data();
  if (m_referenceDownload) m_referenceDownload->cancel();
  // QgsMapToolIdentify must release its canvas-dependent state before the
  // central widget destroys the map canvas.
  delete m_attributeTool;
  m_attributeTool = nullptr;
}

void MainWindow::closeEvent(QCloseEvent* event) {
  if (m_isOpeningSurvey) {
    statusBar()->showMessage(QStringLiteral("조사를 열거나 저장하는 중입니다. 작업이 끝난 뒤 닫아 주세요."), 5000);
    event->ignore();
    return;
  }
  // 예전에는 여기서 말없이 저장했다. 이제 저장은 사용자가 시키는 것이므로,
  // 저장 안 한 작업이 있으면 묻고 답에 따른다. 묻지 않고 버리면 조용히 잃는다.
  if (surveyHasUnsavedChanges()) {
    const auto answer = QMessageBox::question(
        this, QStringLiteral("조사 닫기"),
        QStringLiteral("저장하지 않은 작업이 있습니다.\n\n%1\n\n저장할까요?")
            .arg(QFileInfo(m_surveyPath).fileName()),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Save);
    if (answer == QMessageBox::Cancel) {
      event->ignore();
      return;
    }
    if (answer == QMessageBox::Save && !persistSurveyWork()) {
      event->ignore();
      return;
    }
  }
  m_closingWindow = true;
  // Cancel catalog jobs before app.exec() can wait on their event-loop locks.
  delete m_topographicImport.data();
  delete m_topographicBrowser.data();
  if (m_referenceDownload) m_referenceDownload->cancel();
  m_locator->cancel();
  if (m_searchProgress) { m_searchProgress->hide(); m_searchProgress->deleteLater(); m_searchProgress = nullptr; }
  m_locationSearchBusy = false;
  m_adminBoundary->cancel();
  if (m_boundaryProgress) { m_boundaryProgress->hide(); m_boundaryProgress->deleteLater(); m_boundaryProgress = nullptr; }
  QSettings st = RecentSurveys::userSettings();
  RecentSurveys::setSkipAutoRestore(st, false);
  st.setValue(QStringLiteral("MainWindow/geometry"), saveGeometry());
  st.setValue(QStringLiteral("MainWindow/state"), saveState());
  if (m_mainSplit)
    st.setValue(QStringLiteral("MainWindow/mainSplit"), m_mainSplit->saveState());
  if (m_leftSplit)
    st.setValue(QStringLiteral("MainWindow/leftSplit"), m_leftSplit->saveState());
  QMainWindow::closeEvent(event);
}

// 작업공간을 읽은 뒤 화면·범례·창 제목을 한 번에 맞춘다. 내장(.gpkg)과 동반(.qgz)
// 두 경로가 같은 마무리를 쓰도록 한곳에 모았다.

void MainWindow::buildMenus() {
  menuBar()->setNativeMenuBar(false);
  menuBar()->hide();

  auto* mainTb = addToolBar(QStringLiteral("주요"));
  mainTb->setObjectName(QStringLiteral("mainToolbar"));
  mainTb->setIconSize(QSize(20, 20));
  mainTb->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
  mainTb->setMovable(false);
  mainTb->setFloatable(false);
  mainTb->setAttribute(Qt::WA_InputMethodEnabled, true);

  // Place search joins the ribbon row at its right end (added after the ribbon below).
  // As a bar of its own it cost a whole row and ended up sharing the drawing tools' row.
  m_appBar = new KaAppBar(mainTb);
  connect(m_appBar, &KaAppBar::searchRequested, this,
          [this](const QString& q, bool lot) { searchLocation(q, lot); });
  auto* actFind = new QAction(QStringLiteral("주소·지번 찾기"), this);
  actFind->setShortcut(QKeySequence::Find);
  actFind->setShortcutContext(Qt::WindowShortcut);
  connect(actFind, &QAction::triggered, m_appBar, &KaAppBar::focusSearch);
  addAction(actFind);

  auto* ribbon = new KaBeginnerRibbon(mainTb);
  m_ribbon = ribbon;
  ribbon->addGroup(QStringLiteral("survey"), QStringLiteral("조사파일"));
  ribbon->addGroup(QStringLiteral("record"), QStringLiteral("기록"));
  ribbon->addGroup(QStringLiteral("basemap"), QStringLiteral("배경 지도"));
  ribbon->addGroup(QStringLiteral("align"), QStringLiteral("좌표 정합"));
  ribbon->addGroup(QStringLiteral("out"), QStringLiteral("내보내기"));
  ribbon->addGroup(QStringLiteral("more"), QStringLiteral("기타"));

  auto addIcon = [this, ribbon](const QString& group, const QString& iconId, const QString& text,
                                const QString& tip, auto slot) -> QPair<QAction*, QToolButton*> {
    auto* a = new QAction(KaIcons::icon(iconId), text, this);
    a->setToolTip(tip);
    connect(a, &QAction::triggered, this, slot);
    addAction(a);
    QToolButton* b = ribbon->addAction(group, a);
    return {a, b};
  };
  auto paintPrimary = [](QToolButton* b, const QString& iconId) {
    if (!b) return;
    b->setObjectName(QStringLiteral("btnPrimary"));
    b->setIcon(KaIcons::icon(iconId));
  };
  auto [actNew, btnNew] = addIcon(QStringLiteral("survey"), QStringLiteral("new"),
                                  QStringLiteral("신규"),
                                  QStringLiteral("현장 조사 프로젝트를 새로 만듭니다"),
                                  &MainWindow::newSurvey);
  auto [actOpen, btnOpen] = addIcon(QStringLiteral("survey"), QStringLiteral("open"),
                                    QStringLiteral("열기"),
                                    QStringLiteral("저장한 조사를 엽니다"), &MainWindow::openProject);
  auto [actSave, btnSave] = addIcon(QStringLiteral("survey"), QStringLiteral("save"),
                                    QStringLiteral("저장"),
                                    QStringLiteral("현재 조사를 저장합니다 (Ctrl+S)"), &MainWindow::saveProject);
  actNew->setShortcut(QKeySequence::New);
  actNew->setToolTip(QStringLiteral("새 조사 — 현장 조사 프로젝트를 새로 만듭니다 (Ctrl+N)"));
  actOpen->setShortcut(QKeySequence::Open);
  actOpen->setToolTip(QStringLiteral("저장한 조사를 엽니다 (Ctrl+O)"));
  actSave->setShortcut(QKeySequence::Save);
  auto [actSaveAs, btnSaveAs] = addIcon(QStringLiteral("survey"), QStringLiteral("save_as"),
                                        QStringLiteral("다른이름"),
                                        QStringLiteral("작업 중인 모든 레이어를 다른 이름으로 저장합니다 (Ctrl+Shift+S)"),
                                        &MainWindow::saveProjectAs);
  actSaveAs->setShortcut(QKeySequence::SaveAs);
  Q_UNUSED(actNew);
  Q_UNUSED(actOpen);
  Q_UNUSED(actSave);
  Q_UNUSED(actSaveAs);
  paintPrimary(btnNew, QStringLiteral("new"));
  paintPrimary(btnOpen, QStringLiteral("open"));
  paintPrimary(btnSave, QStringLiteral("save"));
  paintPrimary(btnSaveAs, QStringLiteral("save_as"));
  btnNew->setObjectName(QStringLiteral("ribbonNew"));
  btnOpen->setObjectName(QStringLiteral("ribbonOpen"));
  btnSave->setObjectName(QStringLiteral("ribbonSave"));
  btnSaveAs->setObjectName(QStringLiteral("ribbonSaveAs"));

  auto [actSelect, btnSelect] = addIcon(
      QStringLiteral("record"), QStringLiteral("select"), QStringLiteral("선택"),
      QStringLiteral("그린 도형을 선택합니다. 다시 누르면 이동으로 돌아갑니다"),
      &MainWindow::startSelectTool);
  m_actSelect = actSelect;
  Q_UNUSED(btnSelect);
  m_actSelect->setCheckable(true);
  m_actSelect->setShortcut(QKeySequence(QStringLiteral("Ctrl+1")));
  m_actSelect->setToolTip(QStringLiteral("그린 도형을 선택합니다. 다시 누르면 이동으로 돌아갑니다 (Ctrl+1)"));
  auto* actUndo = new QAction(KaIcons::icon(QStringLiteral("undo")), QStringLiteral("되돌리기"), this);
  actUndo->setToolTip(QStringLiteral("마지막 그리기·정점·삭제를 되돌립니다 (Ctrl+Z)"));
  actUndo->setShortcut(QKeySequence::Undo);
  actUndo->setShortcutContext(Qt::WindowShortcut);
  connect(actUndo, &QAction::triggered, this, [this]() {
    if (routeEditKeyToActiveStudio(false)) return;
    undoLastAction();
  });
  addAction(actUndo);
  m_actUndo = actUndo;
  auto* actRedo = new QAction(KaIcons::icon(QStringLiteral("redo")), QStringLiteral("다시 실행"), this);
  actRedo->setToolTip(QStringLiteral("되돌린 편집을 다시 적용합니다 (Ctrl+Y)"));
  actRedo->setShortcut(QKeySequence::Redo);
  actRedo->setShortcutContext(Qt::WindowShortcut);
  connect(actRedo, &QAction::triggered, this, [this]() {
    redoLastAction();
  });
  addAction(actRedo);
  m_actRedo = actRedo;
  auto [actMeasure, btnMeasure] = addIcon(
      QStringLiteral("record"), QStringLiteral("measure"), QStringLiteral("측거"),
      QStringLiteral("지도에서 거리와 면적을 측정합니다. 다시 누르면 종료합니다"),
      &MainWindow::startMeasureTool);
  m_actMeasure = actMeasure;
  Q_UNUSED(btnMeasure);
  if (m_actMeasure)
    m_actMeasure->setCheckable(true);

  m_btnDraw = new QToolButton(ribbon);
  m_btnDraw->setObjectName(QStringLiteral("btnDraw"));
  m_btnDraw->setIcon(KaIcons::icon(QStringLiteral("draw_poly")));
  m_btnDraw->setText(QStringLiteral("그리기"));
  m_btnDraw->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
  m_btnDraw->setCheckable(true);
  m_btnDraw->setToolTip(QStringLiteral("조사구역·유구 면과 선을 그립니다. 다시 누르면 도구를 닫고 이동합니다"));
  connect(m_btnDraw, &QToolButton::clicked, this, [this](bool on) {
    if (on) showSubToolsDraw();
    else hideSubTools();
    if (m_btnDraw)
      m_btnDraw->setChecked(m_subToolbar && m_subToolbar->isVisible() &&
                            m_subToolsMode == QLatin1String("draw"));
  });
  ribbon->addWidget(QStringLiteral("record"), m_btnDraw);
  auto* actDraw = new QAction(QStringLiteral("그리기"), this);
  actDraw->setShortcut(QKeySequence(QStringLiteral("Ctrl+D")));
  actDraw->setShortcutContext(Qt::WindowShortcut);
  actDraw->setToolTip(QStringLiteral("조사구역·유구 면과 선을 그립니다 (Ctrl+D)"));
  connect(actDraw, &QAction::triggered, this, [this]() {
    if (m_btnDraw) m_btnDraw->click();
  });
  addAction(actDraw);

  addIcon(QStringLiteral("record"), QStringLiteral("trench_grid"), QStringLiteral("시굴격자"),
          QStringLiteral("조사구역이 있으면 바로 깔고, 없으면 맵을 찍어 놓습니다. 깐 뒤에는 끌어 옮깁니다"),
          &MainWindow::startTrenchGrid);

  // Reference maps stay one click away on the ribbon. A narrow window folds the whole
  // group into 「더 많은 작업」 (KaBeginnerRibbon::updateOverflow) instead of hiding it.
  m_btnTerrain = new QToolButton(ribbon);
  m_btnTerrain->setObjectName(QStringLiteral("btnTerrain"));
  m_btnTerrain->setIcon(KaIcons::icon(QStringLiteral("contour")));
  m_btnTerrain->setText(QStringLiteral("지형"));
  m_btnTerrain->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
  m_btnTerrain->setCheckable(true);
  m_btnTerrain->setToolTip(
      QStringLiteral("등고·음영 지형도를 켭니다. 다시 누르면 숨깁니다"));
  connect(m_btnTerrain, &QToolButton::clicked, this, &MainWindow::toggleTerrainMap);
  connect(m_btnTerrain, &QToolButton::clicked, this, [this]() {
    QTimer::singleShot(0, this, [this]() { syncThematicButtons(); });
  });
  ribbon->addWidget(QStringLiteral("basemap"), m_btnTerrain);
  auto* topographic = new QToolButton(ribbon);
  topographic->setObjectName(QStringLiteral("btnTopographic"));
  topographic->setIcon(KaIcons::icon(QStringLiteral("contour")));
  topographic->setText(QStringLiteral("수치"));
  topographic->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
  topographic->setToolTip(QStringLiteral(
      "HGIS 안에서 국토지리정보원에 로그인하고 수치지형도를 받습니다. 우클릭: 받아 둔 폴더 불러오기"));
  topographic->setPopupMode(QToolButton::MenuButtonPopup);
  auto* topographicMenu = new QMenu(topographic);
  topographicMenu->addAction(QStringLiteral("국토지리정보원에서 내려받기…"), this, &MainWindow::openTopographicDownload);
  topographicMenu->addAction(QStringLiteral("받아 둔 수치지형도 폴더…"), this, &MainWindow::importTopographicFolder);
  topographic->setMenu(topographicMenu);
  // The ribbon theme hides menu arrows, so the sub-menu opens on right-click like DEM and 토양.
  topographic->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(topographic, &QToolButton::customContextMenuRequested, this,
          [topographic, topographicMenu](const QPoint& pos) { topographicMenu->exec(topographic->mapToGlobal(pos)); });
  connect(topographic, &QToolButton::clicked, this, &MainWindow::openTopographicDownload);
  ribbon->addWidget(QStringLiteral("basemap"), topographic);
  m_btnDem = new QToolButton(ribbon);
  m_btnDem->setObjectName(QStringLiteral("btnDem"));
  m_btnDem->setIcon(KaIcons::icon(QStringLiteral("dem")));
  m_btnDem->setText(QStringLiteral("DEM"));
  m_btnDem->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
  m_btnDem->setCheckable(true);
  m_btnDem->setToolTip(
      QStringLiteral("고도를 색으로 보여 줍니다. 다시 누르면 숨깁니다. 우클릭: 상세 메뉴"));
  auto* demMenu = new QMenu(m_btnDem);
  demMenu->addAction(KaIcons::icon(QStringLiteral("dem")),
                      QStringLiteral("국토지리원 DEM 불러오기(.img)…"), this,
                      &MainWindow::importDemElevationRaster);
  demMenu->addAction(KaIcons::icon(QStringLiteral("dem")), QStringLiteral("DEM 파일로 음영 만들기…"),
                     this, &MainWindow::runDemHillshade);
  demMenu->addAction(KaIcons::icon(QStringLiteral("dem")), QStringLiteral("DEM 표현…"), this,
                     &MainWindow::editDemElevationClasses);
  m_btnDem->setMenu(demMenu);
  m_btnDem->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(m_btnDem, &QToolButton::customContextMenuRequested, this,
          [this, demMenu](const QPoint& pos) { demMenu->exec(m_btnDem->mapToGlobal(pos)); });
  connect(m_btnDem, &QToolButton::clicked, this, &MainWindow::toggleDemMap);
  connect(m_btnDem, &QToolButton::clicked, this, [this]() {
    QTimer::singleShot(0, this, [this]() { syncThematicButtons(); });
  });
  ribbon->addWidget(QStringLiteral("basemap"), m_btnDem);
  m_btnSoil = new QToolButton(ribbon);
  m_btnSoil->setObjectName(QStringLiteral("btnSoil"));
  m_btnSoil->setIcon(KaIcons::icon(QStringLiteral("soil")));
  m_btnSoil->setText(QStringLiteral("토양"));
  m_btnSoil->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
  m_btnSoil->setCheckable(true);
  m_btnSoil->setPopupMode(QToolButton::MenuButtonPopup);
  m_btnSoil->setToolTip(
      QStringLiteral("흙토람 토양도를 겹칩니다. 다시 누르면 숨깁니다. 우클릭: 상세 메뉴"));
  auto* soilMenu = new QMenu(m_btnSoil);
  soilMenu->addAction(KaIcons::icon(QStringLiteral("layer")),
                      QStringLiteral("분포지형 내려받기 — 현재 화면 범위"), this,
                      &MainWindow::downloadSoilTerrain);
  soilMenu->addAction(KaIcons::icon(QStringLiteral("open")),
                      QStringLiteral("토양도 파일 불러오기(SHP·GPKG)"), this,
                      &MainWindow::importSoilShapefile);
  m_btnSoil->setMenu(soilMenu);
  m_btnSoil->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(m_btnSoil, &QToolButton::customContextMenuRequested, this,
          [this, soilMenu](const QPoint& pos) { soilMenu->exec(m_btnSoil->mapToGlobal(pos)); });
  connect(m_btnSoil, &QToolButton::clicked, this, &MainWindow::downloadSoilTerrain);
  connect(m_btnSoil, &QToolButton::clicked, this, [this]() {
    QTimer::singleShot(0, this, [this]() { syncThematicButtons(); });
  });
  ribbon->addWidget(QStringLiteral("basemap"), m_btnSoil);
  m_btnPaleo = new QToolButton(ribbon);
  m_btnPaleo->setObjectName(QStringLiteral("btnPaleo"));
  m_btnPaleo->setIcon(KaIcons::icon(QStringLiteral("paleo")));
  m_btnPaleo->setText(QStringLiteral("고지형"));
  m_btnPaleo->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
  m_btnPaleo->setToolTip(
      QStringLiteral("흙토람 입지 후보를 강조하고 구하도·자연제방 등 가설을 그립니다. 확정이 아닙니다"));
  connect(m_btnPaleo, &QToolButton::clicked, this, &MainWindow::startPaleoLandform);
  ribbon->addWidget(QStringLiteral("basemap"), m_btnPaleo);
  auto [actCadastral, btnCadastral] = addIcon(
      QStringLiteral("basemap"), QStringLiteral("cadastral"), QStringLiteral("지적"),
      QStringLiteral("조사구역 주변 5km 지적도를 받아 경계선과 지번을 표시합니다. 우클릭: 계정·선 색 설정"),
      &MainWindow::downloadCadastral);
  actCadastral->setObjectName(QStringLiteral("actionCadastralDownload"));
  auto* cadastralMenu = new QMenu(btnCadastral);
  cadastralMenu->addAction(QStringLiteral("VWorld 계정 설정"), this, &MainWindow::configureCadastralAccount);
  cadastralMenu->addAction(QStringLiteral("선 색·지번 표시"), this, &MainWindow::configureCadastralStyle);
  btnCadastral->setMenu(cadastralMenu);
  btnCadastral->setPopupMode(QToolButton::MenuButtonPopup);
  btnCadastral->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(btnCadastral, &QToolButton::customContextMenuRequested, this,
          [btnCadastral, cadastralMenu](const QPoint& pos) { cadastralMenu->exec(btnCadastral->mapToGlobal(pos)); });
  m_btnDaedong = new QToolButton(ribbon);
  m_btnDaedong->setObjectName(QStringLiteral("btnDaedongyeojido"));
  m_btnDaedong->setIcon(KaIcons::icon(QStringLiteral("map")));
  m_btnDaedong->setText(QStringLiteral("대동여지"));
  m_btnDaedong->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
  m_btnDaedong->setToolTip(
      QStringLiteral("대동여지도를 참조 지도로 올립니다. API 키가 필요 없습니다"));
  connect(m_btnDaedong, &QToolButton::clicked, this, &MainWindow::addDaedongyeojidoMap);
  ribbon->addWidget(QStringLiteral("basemap"), m_btnDaedong);
  m_btnMap1919 = new QToolButton(ribbon);
  m_btnMap1919->setObjectName(QStringLiteral("btnMap1919"));
  m_btnMap1919->setIcon(KaIcons::icon(QStringLiteral("contour")));
  m_btnMap1919->setText(QStringLiteral("1919지형"));
  m_btnMap1919->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
  connect(m_btnMap1919, &QToolButton::clicked, this, &MainWindow::addHistoryGisMap1919);
  ribbon->addWidget(QStringLiteral("basemap"), m_btnMap1919);
  auto [actGeology, btnGeology] = addIcon(
      QStringLiteral("basemap"), QStringLiteral("geology"), QStringLiteral("지질"),
      QStringLiteral("KIGAM 1:5만 지질 색 위에 지형 음영을 겹칩니다. 다시 누르면 숨깁니다"),
      &MainWindow::downloadGeologyMap);
  m_actGeology = actGeology;
  Q_UNUSED(btnGeology);
  if (m_actGeology) m_actGeology->setCheckable(true);
  auto [actRiver, btnRiver] = addIcon(
      QStringLiteral("basemap"), QStringLiteral("river"), QStringLiteral("수계"),
      QStringLiteral("하천망을 겹칩니다. 다시 누르면 숨깁니다"),
      &MainWindow::downloadRiverMap);
  m_actRiver = actRiver;
  Q_UNUSED(btnRiver);
  if (m_actRiver) m_actRiver->setCheckable(true);
  for (QAction* thematic : {m_actGeology, m_actRiver}) {
    if (!thematic) continue;
    connect(thematic, &QAction::triggered, this, [this]() {
      QTimer::singleShot(0, this, [this]() { syncThematicButtons(); });
    });
  }
  addIcon(QStringLiteral("align"), QStringLiteral("georef"), QStringLiteral("정합"),
          QStringLiteral("좌표없는 사진·CAD를 도면에 합치기"), &MainWindow::georefAssistant);

  auto* btnBuffer = new QToolButton(ribbon);
  btnBuffer->setObjectName(QStringLiteral("btnBuffer"));
  btnBuffer->setIcon(KaIcons::icon(QStringLiteral("buffer")));
  btnBuffer->setText(QStringLiteral("버퍼"));
  btnBuffer->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
  btnBuffer->setCheckable(true);
  btnBuffer->setToolTip(QStringLiteral("주변유적 500M·1000M 표시하기 — 선택한 면 둘레에 거리 경계를 그립니다"));
  connect(btnBuffer, &QToolButton::clicked, this, [this, btnBuffer](bool on) {
    if (on) showSubToolsBuffer();
    else hideSubTools();
    btnBuffer->setChecked(m_subToolbar && m_subToolbar->isVisible()
                          && m_subToolsMode == QLatin1String("buffer"));
  });
  ribbon->addWidget(QStringLiteral("align"), btnBuffer);

  // 위 버튼은 이미 올라온 레이어에 버퍼를 그린다. 이 버튼은 자료를 받아 온다. 역할이 다르다.
  auto* btnHeritage = new QToolButton(ribbon);
  btnHeritage->setObjectName(QStringLiteral("btnHeritageFetch"));
  btnHeritage->setIcon(KaIcons::icon(QStringLiteral("buffer")));
  btnHeritage->setText(QStringLiteral("유산"));
  btnHeritage->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
  btnHeritage->setToolTip(QStringLiteral(
      "조사구역이 속한 시·군의 국가유산 자료를 인트라넷에서 받아 참조 지도로 올립니다.\n"
      "받은 자료는 조사폴더 안에만 두며 포터블·제출물에 실리지 않습니다"));
  connect(btnHeritage, &QToolButton::clicked, this, &MainWindow::fetchNearbyHeritage);
  ribbon->addWidget(QStringLiteral("align"), btnHeritage);

  auto [actLayout, btnLayout] = addIcon(
      QStringLiteral("out"), QStringLiteral("pdf"), QStringLiteral("도면"),
      QStringLiteral("도면 만들기 — 종이에 지도를 올려 도면을 만듭니다 (Ctrl+L)"), &MainWindow::openLayoutDesigner);
  actLayout->setShortcut(QKeySequence(QStringLiteral("Ctrl+L")));
  Q_UNUSED(btnLayout);
  // 인쇄는 사람들이 먼저 찾는 곳(첫 화면 리본)에도 둔다. 도면 화면 안의 「인쇄」와 같은 창을 연다.
  auto [actPrint, btnPrint] = addIcon(
      QStringLiteral("out"), QStringLiteral("print"), QStringLiteral("인쇄"),
      QStringLiteral("도면을 프린터로 찍거나, 큰 도면을 A3·A4 여러 장으로 나눠 찍습니다 (Ctrl+P)"),
      &MainWindow::printDrawing);
  actPrint->setShortcut(QKeySequence::Print);
  btnPrint->setObjectName(QStringLiteral("btnRibbonPrint"));
  addIcon(QStringLiteral("out"), QStringLiteral("section"), QStringLiteral("단면"),
          QStringLiteral("단면 GeoTIFF로 표고·거리 눈금 도면을 만듭니다"),
          &MainWindow::openSectionDesigner);
  auto [actMapGeoTiff, btnMapGeoTiff] = addIcon(
      QStringLiteral("out"), QStringLiteral("map"), QStringLiteral("GeoTIFF"),
      QStringLiteral("현재 지도에 보이는 범위와 레이어를 지도 좌표계 그대로 GeoTIFF로 저장합니다"),
      &MainWindow::exportMapGeoTiff);
  m_actMapGeoTiff = actMapGeoTiff;
  m_actMapGeoTiff->setObjectName(QStringLiteral("actionMapGeoTiff"));
  m_actMapGeoTiff->setEnabled(false);
  btnMapGeoTiff->setObjectName(QStringLiteral("btnMapGeoTiff"));
  auto [actExport, btnExport] = addIcon(
      QStringLiteral("out"), QStringLiteral("transform"), QStringLiteral("제출 변환"),
      QStringLiteral("인트라넷 제출. 선택한 레이어를 EPSG:5179 SHP 파일로만 저장합니다 (Ctrl+E)."),
      &MainWindow::convertSelectedTo5179);
  actExport->setShortcut(QKeySequence(QStringLiteral("Ctrl+E")));
  Q_UNUSED(btnExport);
  ribbon->applyTabOrder();

  auto* region = new KaRegionLocator(m_appBar);
  region->setObjectName(QStringLiteral("regionLocator"));
  connect(region, &KaRegionLocator::searchRequested, this,
          [this](const QString& q) { searchLocation(q); });
  connect(region, &KaRegionLocator::parcelSearchRequested, this,
          [this](const QString& q) { searchLocation(q, true); });
  connect(region, &KaRegionLocator::regionSelected, this, [this](const QString& sido) {
    const auto bounds = KoreaRegionCatalog::overviewBounds(sido);
    if (!bounds || m_isOpeningSurvey) return;
    // Province chips navigate directly. An older address reply must not move
    // the map back after this newer user selection.
    if (m_locator) m_locator->cancel();
    m_locationSearchBusy = false;
    if (m_searchProgress) {
      m_searchProgress->hide();
      m_searchProgress->deleteLater();
      m_searchProgress = nullptr;
    }
    LocationHit hit;
    hit.title = sido;
    hit.west = bounds->west; hit.south = bounds->south;
    hit.east = bounds->east; hit.north = bounds->north;
    hit.lon = (hit.west + hit.east) * 0.5;
    hit.lat = (hit.south + hit.north) * 0.5;
    hit.hasBbox = true;
    zoomToLocation(hit);
  });
  m_appBar->setRegionWidget(region);

  auto* webBtn = new QToolButton(ribbon);
  webBtn->setObjectName(QStringLiteral("btnWeb"));
  webBtn->setIcon(KaIcons::icon(QStringLiteral("map")));
  webBtn->setText(QStringLiteral("웹"));
  webBtn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
  webBtn->setToolTip(QStringLiteral("인트라넷·토양도·지적도·지형도 웹 자료를 엽니다"));
  webBtn->setPopupMode(QToolButton::InstantPopup);
  auto* webMenu = new QMenu(webBtn);
  auto addWeb = [this, webMenu](const QString& iconId, const QString& text, const QString& url) {
    QAction* a = webMenu->addAction(KaIcons::icon(iconId), text);
    QObject::connect(a, &QAction::triggered, this, [url]() {
      QDesktopServices::openUrl(QUrl(url));
    });
  };
  addWeb(QStringLiteral("upload"), QStringLiteral("문화재 GIS 인트라넷"),
         QStringLiteral("https://intranet.gis-heritage.go.kr/"));
  addWeb(QStringLiteral("layer"), QStringLiteral("농진청 토양도"),
         QStringLiteral("http://soil.rda.go.kr/geoweb/soilmain.do"));
  addWeb(QStringLiteral("cadastral"), QStringLiteral("VWorld 지적도 자료"),
         QStringLiteral("https://www.vworld.kr/dtmk/dtmk_ntads_s002.do?dsId=30563"));
  addWeb(QStringLiteral("contour"), QStringLiteral("국토정보맵 지형도"),
         QStringLiteral("https://map.ngii.go.kr/ms/map/NlipMap.do"));
  webMenu->addSeparator();
  webMenu->addAction(KaIcons::icon(QStringLiteral("layer")),
                     QStringLiteral("토양도 SHP 불러오기(흙토람 다운로드)…"), this,
                     &MainWindow::importSoilShapefile);
  webBtn->setMenu(webMenu);

  auto* more = new QToolButton(ribbon);
  more->setIcon(KaIcons::icon(QStringLiteral("more")));
  more->setText(QStringLiteral("더보기"));
  more->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
  more->setToolTip(QStringLiteral("가끔 쓰는 기능"));
  auto* moreMenu = new QMenu(more);
  moreMenu->addAction(KaIcons::icon(QStringLiteral("open")), QStringLiteral("벡터 불러오기"),
                      this, &MainWindow::openVectorLayer);
  moreMenu->addAction(KaIcons::icon(QStringLiteral("layer")),
                      QStringLiteral("참조 벡터를 조사 파일 밖으로…"), this,
                      &MainWindow::extractEmbeddedReferenceVectors);
  moreMenu->addAction(KaIcons::icon(QStringLiteral("layer")),
                      QStringLiteral("토양도 SHP 불러오기"), this,
                      &MainWindow::importSoilShapefile);
  moreMenu->addAction(KaIcons::icon(QStringLiteral("import")), QStringLiteral("CSV 기준점"),
                      this, &MainWindow::importControlCsv);
  moreMenu->addAction(KaIcons::icon(QStringLiteral("georef")), QStringLiteral("맞추기"),
                      this, &MainWindow::georefAssistant);
  moreMenu->addAction(KaIcons::icon(QStringLiteral("map")), QStringLiteral("OSM 배경"),
                      this, &MainWindow::addBasemapOsm);
  moreMenu->addAction(KaIcons::icon(QStringLiteral("satellite")), QStringLiteral("Google 위성"),
                      this, &MainWindow::addBasemapGoogle);
  moreMenu->addSeparator();
  moreMenu->addAction(QStringLiteral("파일함 보이기/숨기기"), this, [this]() {
    if (auto* toggle = findChild<QToolButton*>(QStringLiteral("sidebarFilesToggle")))
      toggle->toggle();
  });
  moreMenu->addAction(QStringLiteral("작업 목록"), this, [this]() {
    if (auto* d = findChild<QDockWidget*>(QStringLiteral("workDock"))) {
      d->setVisible(!d->isVisible());
      if (d->isVisible()) d->raise();
    }
  });
  moreMenu->addAction(QStringLiteral("API 키 입력"), this, &MainWindow::configureVworldKey);
  moreMenu->addAction(QStringLiteral("VWorld 지적도 아이디·비밀번호"), this, &MainWindow::configureCadastralAccount);
  moreMenu->addAction(QStringLiteral("수치지형도 아이디·비밀번호"), this, &MainWindow::configureTopographicAccount);
  moreMenu->addAction(QStringLiteral("국가유산 인트라넷 아이디·비밀번호"), this, &MainWindow::configureHeritageAccount);
  moreMenu->addAction(QStringLiteral("정보"), this, &MainWindow::showAbout);
  more->setMenu(moreMenu);
  more->setPopupMode(QToolButton::InstantPopup);
  ribbon->addWidget(QStringLiteral("more"), webBtn);
  ribbon->addWidget(QStringLiteral("more"), more);
  mainTb->addWidget(ribbon);
  // The ribbon expands and keeps its buttons packed left, so place search ends up on the right.
  mainTb->addWidget(m_appBar);
  updateHistoricalMapButtons();

  m_subToolbar = addToolBar(QStringLiteral("세부도구"));
  m_subToolbar->setObjectName(QStringLiteral("subToolbar"));
  m_subToolbar->setIconSize(QSize(20, 20));
  m_subToolbar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  m_subToolbar->setMovable(false);
  m_subToolbar->setVisible(false);
  applyWidgetShadow(m_subToolbar, 10, 2, 25);
  insertToolBarBreak(m_subToolbar);
}

void MainWindow::clearSubToolbar() {
#if KA_HGIS_HAS_QGIS
  if (!m_subToolbar) return;
  m_subToolbar->clear();
#endif
}

// 어떤 도구가 켜져 있는지는 실제 지도 도구와 편집 대상 레이어가 말해 준다.
// 버튼마다 따로 플래그를 두면 어긋나므로 여기서 한 곳에서만 판단한다.
// QSS 의 QToolBar#subToolbar QToolButton:checked 가 파란 밑줄을 그린다.
void MainWindow::updateSubToolbarChecks() {
#if KA_HGIS_HAS_QGIS
  if (!m_subToolbar || !m_canvas) return;
  QString active;
  if (m_featureSelectTool && m_canvas->mapTool() == m_featureSelectTool) {
    active = QStringLiteral("select");
  } else if (m_captureTool && m_canvas->mapTool() == m_captureTool && m_editLayer) {
    const QString key = LayerOps::layerKeyOf(m_editLayer);
    if (m_editLayer->name() == QLatin1String("쉽게그리기"))
      active = QStringLiteral("easy");
    else if (key == QLatin1String("survey_area"))
      active = QStringLiteral("area");
    else if (key == QLatin1String("feature_poly"))
      active = QStringLiteral("poly");
    else if (key == QLatin1String("feature_line"))
      active = QStringLiteral("line");
    else if (key.startsWith(QLatin1String("artifact")))
      active = QStringLiteral("artifact");
  }
  for (QAction* a : m_subToolbar->actions()) {
    if (!a) continue;
    const QString id = a->property("kaSubTool").toString();
    if (id.isEmpty()) continue;
    a->setCheckable(true);
    const QSignalBlocker block(a);
    a->setChecked(id == active);
  }
#endif
}

void MainWindow::hideSubTools() {
#if KA_HGIS_HAS_QGIS
  if (m_subToolsMode == QLatin1String("align"))
    stopAlignSession();
  if (m_subToolsMode == QLatin1String("draw"))
    stopCaptureTool();
  clearSubToolbar();
  if (m_subToolbar) m_subToolbar->setVisible(false);
  m_subToolsMode.clear();
  if (auto* b = findChild<QToolButton*>(QStringLiteral("btnDraw"))) b->setChecked(false);
  if (auto* b = findChild<QToolButton*>(QStringLiteral("btnBasemap"))) b->setChecked(false);
  if (auto* b = findChild<QToolButton*>(QStringLiteral("btnSubmit"))) b->setChecked(false);
  if (auto* b = findChild<QToolButton*>(QStringLiteral("btnBuffer"))) b->setChecked(false);
#endif
}

void MainWindow::showSubToolsBuffer() {
#if KA_HGIS_HAS_QGIS
  if (!m_subToolbar) return;
  if (m_subToolsMode == QLatin1String("align")) stopAlignSession();
  if (m_subToolsMode == QLatin1String("buffer") && m_subToolbar->isVisible()) {
    hideSubTools();
    return;
  }
  clearSubToolbar();
  m_subToolsMode = QStringLiteral("buffer");
  auto* lab = new QLabel(QStringLiteral("  주변유적경계 › "));
  lab->setObjectName(QStringLiteral("subToolbarCaption"));
  m_subToolbar->addWidget(lab);
  m_subToolbar->addAction(KaIcons::icon(QStringLiteral("buffer")), QStringLiteral("500m"),
                          this, &MainWindow::runSiteBuffer500);
  m_subToolbar->addAction(KaIcons::icon(QStringLiteral("buffer")), QStringLiteral("1000m"),
                          this, &MainWindow::runSiteBuffer1000);
  auto* closeAct = m_subToolbar->addAction(QStringLiteral("닫기"));
  connect(closeAct, &QAction::triggered, this, &MainWindow::hideSubTools);
  m_subToolbar->setVisible(true);
  statusBar()->showMessage(
      QStringLiteral("면 레이어를 고른 뒤 500m 또는 1000m를 누르세요. 점선은 도형 색에서 바꿉니다."),
      8000);
#endif
}

void MainWindow::runSiteBuffer500() {
#if KA_HGIS_HAS_QGIS
  QgsVectorLayer* layer = m_layerTree ? qobject_cast<QgsVectorLayer*>(m_layerTree->currentLayer())
                                      : nullptr;
  QString err;
  if (!BufferAnalysis::addDistanceRing(QgsProject::instance(), m_canvas, layer, 500.0, &err)) {
    notify(Notice::Warning, QStringLiteral("주변유적경계"),
           QStringLiteral("500m 경계를 그리지 못했습니다."), err);
    return;
  }
  statusBar()->showMessage(QStringLiteral("주변 500m 경계를 그렸습니다"), 6000);
#endif
}

void MainWindow::runSiteBuffer1000() {
#if KA_HGIS_HAS_QGIS
  QgsVectorLayer* layer = m_layerTree ? qobject_cast<QgsVectorLayer*>(m_layerTree->currentLayer())
                                      : nullptr;
  QString err;
  if (!BufferAnalysis::addDistanceRing(QgsProject::instance(), m_canvas, layer, 1000.0, &err)) {
    notify(Notice::Warning, QStringLiteral("주변유적경계"),
           QStringLiteral("1000m 경계를 그리지 못했습니다."), err);
    return;
  }
  statusBar()->showMessage(QStringLiteral("주변 1000m 경계를 그렸습니다"), 6000);
#endif
}

void MainWindow::showSubToolsDraw() {
#if KA_HGIS_HAS_QGIS
  if (!m_subToolbar) return;
  if (m_subToolsMode == QLatin1String("align")) stopAlignSession();
  if (m_subToolsMode == QLatin1String("draw") && m_subToolbar->isVisible()) {
    hideSubTools();
    return;
  }
  if (m_viewTabs && m_mapPage && m_viewTabs->currentWidget() != m_mapPage)
    showMapWorkspace();
  clearSubToolbar();
  m_subToolsMode = QStringLiteral("draw");
  auto* lab = new QLabel(QStringLiteral("  그리기 › "));
  lab->setObjectName(QStringLiteral("subToolbarCaption"));
  m_subToolbar->addWidget(lab);
  auto* selAct = m_subToolbar->addAction(KaIcons::icon(QStringLiteral("select")),
                                        QStringLiteral("도형선택"),
                                        this, &MainWindow::startSelectTool);
  selAct->setProperty("kaSubTool", QStringLiteral("select"));
  selAct->setToolTip(QStringLiteral(
      "도형을 클릭하면 수정점이 바로 나옵니다.\n"
      "점을 끌면 그 점만 옮겨지고, 선 위에서 우클릭하면 점추가·점삭제입니다.\n"
      "Shift+클릭으로 여러 도형을 골라 폴리곤 묶기·나누기에 씁니다."));
  auto* snap = new KaSnapSettingsWidget(m_subToolbar);
  snap->syncFromProject();
  connect(snap, &KaSnapSettingsWidget::settingsChanged, this, [this, snap]() {
    m_snapEnabled = LayerOps::readSnapSettings(QgsProject::instance()).enabled;
    applySnapConfig();
    snap->syncFromProject();
    statusBar()->showMessage(m_snapEnabled
                                 ? QStringLiteral("자석 켜짐 — 조사 도형·지적 선에 붙습니다. 위성 그림은 제외")
                                 : QStringLiteral("자석 꺼짐"),
                             4000);
  });
  m_subToolbar->addWidget(snap);
  auto* easyAct = m_subToolbar->addAction(KaIcons::icon(QStringLiteral("easy_draw")),
                                          QStringLiteral("쉽게그리기"),
                                          this, &MainWindow::startEasyDraw);
  easyAct->setProperty("kaSubTool", QStringLiteral("easy"));
  easyAct->setToolTip(QStringLiteral(
      "쉽게그리기 — 이미 찍은 점 가까이에 마우스를 가져가면 그 점에 딱 붙어 자동으로 찍힙니다.\n"
      "잘못 찍었으면 Ctrl+Z를 누르세요. 바로 앞 점으로 되돌아갑니다."));
  auto* areaAct = m_subToolbar->addAction(KaIcons::icon(QStringLiteral("draw_area")),
                                          QStringLiteral("조사구역 그리기"),
                                          this, &MainWindow::startEditSurveyArea);
  areaAct->setProperty("kaSubTool", QStringLiteral("area"));
  areaAct->setToolTip(QStringLiteral(
      "조사구역(발굴 범위) 면을 그립니다. 점을 차례로 찍고 Enter로 닫습니다.\n"
      "Ctrl+Z는 마지막 점을 되돌립니다."));
  auto* polyAct = m_subToolbar->addAction(KaIcons::icon(QStringLiteral("draw_poly")),
                                          QStringLiteral("유구 그리기"),
                                          this, &MainWindow::startEditFeaturePoly);
  polyAct->setProperty("kaSubTool", QStringLiteral("poly"));
  polyAct->setToolTip(QStringLiteral(
      "유구의 면(수혈·주거지 등)을 그립니다. 점을 차례로 찍고 Enter로 닫습니다.\n"
      "Ctrl+Z는 마지막 점을 되돌립니다."));
  auto* lineAct = m_subToolbar->addAction(KaIcons::icon(QStringLiteral("draw_line")),
                                          QStringLiteral("유구 선"),
                                          this, &MainWindow::startEditFeatureLine);
  lineAct->setProperty("kaSubTool", QStringLiteral("line"));
  lineAct->setToolTip(QStringLiteral(
      "유구의 선(석렬·구 등)을 그립니다. 점을 차례로 찍고 Enter로 끝냅니다."));
  auto* artiAct = m_subToolbar->addAction(KaIcons::icon(QStringLiteral("artifact")),
                                          QStringLiteral("유물위치표시"),
                                          this, &MainWindow::startEditArtifact);
  artiAct->setProperty("kaSubTool", QStringLiteral("artifact"));
  artiAct->setToolTip(QStringLiteral(
      "유물이 나온 자리를 점으로 찍어 표시합니다. 한 번 클릭에 한 점입니다."));

  // 폴리곤 묶기·나누기는 「그리는 도구」가 아니라 「이미 그린 면을 고치는 도구」다.
  // 예전에는 늘어나는 빈칸으로 오른쪽 끝까지 밀어 두었는데, 넓은 화면에서는 그리기
  // 버튼과 너무 멀어져 한눈에 들어오지 않았다. 빈칸을 빼고 구분선만 두어
  // 그리기 버튼 바로 옆(가운데 쪽)에 묶기·나누기·구간 분리·닫기를 같이 보이게 한다.
  m_subToolbar->addSeparator();
  auto* mergeAct = m_subToolbar->addAction(QStringLiteral("폴리곤 묶기"),
                                           this, &MainWindow::mergeFeaturePolygons);
  mergeAct->setToolTip(QStringLiteral(
      "여러 면을 하나로 묶습니다.\n"
      "쓰는 법: [도형선택]을 누르고 Shift를 누른 채 지도에서 면을 두 개 이상 고른 뒤\n"
      "[폴리곤 묶기]를 누르면 고른 면들이 하나로 합쳐집니다."));
  auto* splitAct = m_subToolbar->addAction(QStringLiteral("폴리곤 나누기"),
                                           this, &MainWindow::startSplitPolygonTool);
  splitAct->setToolTip(QStringLiteral(
      "[폴리곤 묶기]로 하나가 된 면을 다시 나눕니다.\n"
      "쓰는 법: 면을 가로지르는 선을 클릭해 그리고 우클릭하면 그 선을 따라 갈라집니다.\n"
      "(Shift로 도형 두 개를 고른 뒤 눌러도 겹친 구간에서 나뉩니다.)"));
  auto* clipAct = m_subToolbar->addAction(QStringLiteral("구간 분리"),
                                          this, &MainWindow::clipOverlappingLayers);
  clipAct->setToolTip(QStringLiteral(
      "겹쳐 있는 두 레이어에서 겹치는 구간만 새 레이어로 떼어 냅니다. 원본은 그대로 남습니다."));
  auto* closeAct = m_subToolbar->addAction(QStringLiteral("닫기"));
  closeAct->setToolTip(QStringLiteral("그리기 도구 모음을 닫고 지도 이동으로 돌아갑니다"));
  connect(closeAct, &QAction::triggered, this, &MainWindow::hideSubTools);
  m_subToolbar->setVisible(true);
  updateSubToolbarChecks();
  statusBar()->showMessage(
      QStringLiteral("유구 면을 그리는 중 — 점을 찍고 Enter로 닫기. 작업 좌표계 → 제출 5179."), 8000);
#endif
}

void MainWindow::showSubToolsBasemap() {
#if KA_HGIS_HAS_QGIS
  hideSubTools();
  ensureDefaultBasemaps();
  showMapWorkspace();
  statusBar()->showMessage(QStringLiteral("위성과 지적도는 시작할 때 자동으로 올라옵니다."), 6000);
#endif
}

void MainWindow::updateNextActionStatus() {
  QString msg;
  if (m_surveyPath.isEmpty()) {
    msg = QStringLiteral("먼저 「새 조사」로 오늘 현장을 만드세요.");
  } else {
#if KA_HGIS_HAS_QGIS
    const bool hasBg = LayerOps::hasVisibleReferenceLayer(QgsProject::instance());
    const bool hasDraw = domainLayerCount() > 0;
#else
    const bool hasBg = false;
    const bool hasDraw = (m_stubSurveyArea + m_stubFeatures) > 0;
#endif
    if (!hasBg)
      msg = QStringLiteral("위성·지적이 없습니다. 더보기 → VWorld API 키를 확인하세요.");
    else if (!hasDraw)
      msg = QStringLiteral("「그리기」로 구역을 그리세요.");
    else
      msg = QStringLiteral("다 그렸으면 「도면 만들기」로 종이에 옮기세요.");
  }
  statusBar()->showMessage(msg);
}

void MainWindow::notify(Notice level, const QString& title, const QString& text,
                        const QString& details) {
#if KA_HGIS_HAS_QGIS
  if (m_messageBar) {
    Qgis::MessageLevel lv = Qgis::MessageLevel::Info;
    switch (level) {
      case Notice::Success:
        lv = Qgis::MessageLevel::Success;
        break;
      case Notice::Warning:
        lv = Qgis::MessageLevel::Warning;
        break;
      case Notice::Critical:
        lv = Qgis::MessageLevel::Critical;
        break;
      case Notice::Info:
        break;
    }
    if (details.isEmpty())
      m_messageBar->pushMessage(title, text, lv);
    else
      m_messageBar->pushMessage(title, text, details, lv);
    return;
  }
#endif
  const QString body = details.isEmpty() ? text : text + QLatin1Char('\n') + details;
  if (level == Notice::Warning || level == Notice::Critical)
    QMessageBox::warning(this, title, body);
  else
    QMessageBox::information(this, title, body);
}

#if KA_HGIS_HAS_QGIS
// 위성·배경 타일이 일부 실패해 반만 그려진 채 캐시에 굳는 문제의 자동 복구.
// 실패가 감지된 타일 레이어만 1.8초 뒤 다시 그린다(같은 화면에서 레이어당 최대 3회).
void MainWindow::healTileLayer(QgsRasterLayer* layer) {
  if (!layer || layer->providerType() != QLatin1String("wms")) return;
  const QString id = layer->id();
  if (m_tileHealCount.value(id) >= 3 || m_tileHealPending.contains(id)) return;
  const int attempt = ++m_tileHealCount[id];
  m_tileHealPending.insert(id);
  QPointer<QgsRasterLayer> guard(layer);
  QTimer::singleShot(1800, this, [this, guard, id, attempt]() {
    m_tileHealPending.remove(id);
    if (!guard) return;
    if (m_canvas && m_canvas->isDrawing()) return;
    KaCrashGuard::logLine(QStringLiteral("[render] '%1' 빠진 타일 자동 재시도 %2/3")
                              .arg(guard->name())
                              .arg(attempt));
    guard->triggerRepaint();
  });
}

void MainWindow::notifyBasemapFailure(bool timedOut, const QString& reason) {
  if (m_closingWindow) return;
  // 서버가 답을 준 정상 응답(범례 미지원·국토 밖 타일)은 실패가 아니다.
  if (LayerOps::mapServerMessageIsBenign(reason)) return;
  // 원인은 매번 로그에 남기고, 화면 안내는 세션에 한 번만 띄운다. URL에는 인증키가
  // 들어 있으므로 괄호 앞까지만 남긴다.
  KaCrashGuard::logLine(QStringLiteral("[basemap] 배경지도 실패 — %1")
                            .arg(reason.section(QLatin1Char('('), 0, 0).trimmed()));
  if (m_basemapNoticeShown) return;
  m_basemapNoticeShown = true;
  // 렌더러와 네트워크 콜백 안에서는 UI를 바꾸지 않는다.
  QTimer::singleShot(0, this, [this, timedOut] {
    if (m_closingWindow) return;
    notify(Notice::Warning, QStringLiteral("배경지도를 불러오지 못했습니다"),
           (timedOut ? QStringLiteral("지도 서버의 응답이 늦어 요청을 중단했습니다. ")
                     : QStringLiteral("지도 서버에 연결하지 못했거나 서버가 요청을 거절했습니다. ")) +
               QStringLiteral("인터넷 연결과 배경지도 설정을 확인한 뒤 다시 켜 주세요. "
                              "조사 도형을 그리거나 저장하는 작업은 계속할 수 있습니다. "
                              "같은 안내는 다시 띄우지 않습니다."));
  });
}
#endif

void MainWindow::buildUi() {
  auto* central = new QWidget(this);
  central->setObjectName(QStringLiteral("centralRoot"));
  central->setAttribute(Qt::WA_StyledBackground, true);
  auto* root = new QHBoxLayout(central);
  root->setContentsMargins(8, 8, 8, 8);
  root->setSpacing(8);

#if KA_HGIS_HAS_QGIS
  m_canvas = new QgsMapCanvas(central);
  connect(m_canvas, &QgsMapCanvas::messageEmitted, this,
          [this](const QString& title, const QString& message, Qgis::MessageLevel) {
    statusBar()->showMessage(title + QStringLiteral(": ") + message, 8000);
  });
  m_canvas->setObjectName(QStringLiteral("mapCanvas"));
  LayerOps::applyWheelZoomFactor(m_canvas);
  KaTheme::excludeMapSurface(m_canvas);
  m_canvas->setCanvasColor(KaTheme::tokens().canvasNeutral);
  m_canvas->enableAntiAliasing(true);
  m_canvas->setCachingEnabled(true);
  // 병렬 렌더는 꺼 둔다. ParallelJob이 provider_wms의 중첩 이벤트 루프와 겹치면
  // deleteLater가 ACCESS_VIOLATION을 낸다(현장 덤프 2026-08-31). KaApplication의
  // qgis/parallel_rendering=false와 같은 값이어야 서로 싸우지 않는다.
  m_canvas->setParallelRenderingEnabled(false);
  // provider_wms may enter another event loop from partial tile output while
  // the previous reply is still active. Keep the completed image until the next
  // render completes, avoiding that reentrant response-lifetime path.
  m_canvas->setMapSettingsFlags(m_canvas->mapSettings().flags() &
                               ~Qgis::MapSettingsFlags(Qgis::MapSettingsFlag::RenderPartialOutput));
  // 미리보기 작업은 켠다 — 이게 꺼져 있으면 화면을 끄는 동안 캔버스가 비어
  // 흰 화면이 보인다. 스레드 풀이 2개 이상이라야 실제로 겹쳐서 돈다.
  m_canvas->setPreviewJobsEnabled(true);
  // Completed renders and the existing cache still refresh normally.
  m_canvas->setMapUpdateInterval(80);
  m_canvas->setAcceptDrops(true);
  m_canvas->setSegmentationTolerance(2.0);
  const QgsCoordinateReferenceSystem crs(m_workCrs);
  m_canvas->setDestinationCrs(crs);
  QgsProject::instance()->setCrs(crs);
  m_panTool = new QgsMapToolPan(m_canvas);
  m_canvas->setMapTool(m_panTool);
  m_mapGrid = new KaCanvasGridOverlay(m_canvas);
  m_mapGrid->setEnabled(false);
  // 지도를 다 그린 뒤 위 레이어를 라벨 위에 한 번 더 그린다.
  m_aboveLabels = new KaAboveLabelsOverlay(m_canvas);
  // 팬·줌에서 지도가 하얗게 비는 원인을 좁히기 위한 계측.
  // KA_HGIS_TRACE_RENDER=1 로 켠다. 렌더가 몇 번 시작·취소되는지, 레이어 목록이
  // 팬 도중에 갈리는지가 로그에 남는다.
  if (!qgetenv("KA_HGIS_TRACE_RENDER").isEmpty()) {
    auto* seq = new int(0);
    connect(m_canvas, &QgsMapCanvas::extentsChanged, this, [this, seq]() {
      const QgsRectangle e = m_canvas->extent();
      KaCrashGuard::logLine(
          QStringLiteral("[trace %1] extentsChanged scale=%2 draw=%3 size=%4x%5 dpr=%6 dpi=%7")
              .arg(++*seq)
              .arg(m_canvas->scale(), 0, 'f', 0)
              .arg(m_canvas->isDrawing() ? 1 : 0)
              .arg(m_canvas->width())
              .arg(m_canvas->height())
              .arg(m_canvas->mapSettings().devicePixelRatio(), 0, 'f', 2)
              .arg(m_canvas->mapSettings().outputDpi(), 0, 'f', 1));
    });
    connect(m_canvas, &QgsMapCanvas::renderStarting, this, [this, seq]() {
      KaCrashGuard::logLine(QStringLiteral("[trace %1] renderStarting").arg(++*seq));
    });
    connect(m_canvas, &QgsMapCanvas::mapCanvasRefreshed, this, [this, seq]() {
      KaCrashGuard::logLine(QStringLiteral("[trace %1] refreshed").arg(++*seq));
    });
    connect(m_canvas, &QgsMapCanvas::layersChanged, this, [this, seq]() {
      KaCrashGuard::logLine(QStringLiteral("[trace %1] layersChanged  n=%2")
                                .arg(++*seq)
                                .arg(m_canvas->layers().size()));
    });
  }
  // 타일 일부 실패(요청 제한·순간 네트워크 오류)로 위성지도가 반만 보이면
  // 해당 레이어만 자동으로 다시 그린다. 화면을 움직이면 재시도 카운터 초기화.
  connect(m_canvas, &QgsMapCanvas::renderErrorOccurred, this,
          [this](const QString& err, QgsMapLayer* layer) {
            auto* raster = qobject_cast<QgsRasterLayer*>(layer);
            if (raster && raster->providerType() == QLatin1String("wms")) {
              notifyBasemapFailure(err.contains(QLatin1String("timeout"), Qt::CaseInsensitive), err);
              healTileLayer(raster);
            } else {
              const QString name = layer ? layer->name() : QStringLiteral("선택한 지도");
              QTimer::singleShot(0, this, [this, name] {
                if (!m_closingWindow)
                  notify(Notice::Warning, QStringLiteral("지도를 표시하지 못했습니다"),
                         QStringLiteral("%1의 원본 파일과 좌표계를 확인한 뒤 다시 켜 주세요.").arg(name));
              });
            }
          });
  connect(QgsApplication::messageLog(),
          &QgsMessageLog::messageReceivedWithFormat,
          this, [this](const QString& message, const QString& tag, Qgis::MessageLevel level,
                       Qgis::StringFormat) {
            if (level != Qgis::MessageLevel::Warning && level != Qgis::MessageLevel::Critical)
              return;
            const bool tileIssue = tag.contains(QLatin1String("WMS"), Qt::CaseInsensitive) ||
                                   message.contains(QLatin1String("tile"), Qt::CaseInsensitive) ||
                                   message.contains(QStringLiteral("타일"));
            if (!tileIssue || !m_canvas) return;
            notifyBasemapFailure(message.contains(QLatin1String("timeout"), Qt::CaseInsensitive),
                                 message);
          });
  connect(QgsNetworkAccessManager::instance(),
          qOverload<QgsNetworkRequestParameters>(&QgsNetworkAccessManager::requestTimedOut),
          this, [this](const QgsNetworkRequestParameters& request) {
            const QString host = request.request().url().host();
            if (host.endsWith(QLatin1String("vworld.kr"), Qt::CaseInsensitive) ||
                host.endsWith(QLatin1String("kigam.re.kr"), Qt::CaseInsensitive))
              notifyBasemapFailure(true, QStringLiteral("요청 시간 초과 · %1").arg(host));
          });
  connect(m_canvas, &QgsMapCanvas::extentsChanged, this, [this]() {
    m_tileHealCount.clear();
    LayerOps::applyCanvasScreenDpi(m_canvas);
    if (m_mapGrid && m_mapGrid->isEnabled()) {
      m_mapGrid->updatePosition();
      m_mapGrid->update();
    }
  });
  connect(m_canvas, &QgsMapCanvas::scaleChanged, this, [this](double) {
    if (m_mapGrid && m_mapGrid->isEnabled()) {
      m_mapGrid->updatePosition();
      m_mapGrid->update();
    }
  });
  connect(m_canvas, &QgsMapCanvas::mapToolSet, this, [this](QgsMapTool* tool, QgsMapTool*) {
    if (m_actMeasure)
      m_actMeasure->setChecked(m_measureTool && tool == m_measureTool);
  });

  auto* layerTreeRoot = QgsProject::instance()->layerTreeRoot();
  auto* model = new KaLayerInformationModel(QgsProject::instance(), false, this);
  model->setFlag(QgsLayerTreeModel::AllowNodeReorder, true);
  model->setFlag(QgsLayerTreeModel::AllowNodeChangeVisibility, true);
  model->setFlag(QgsLayerTreeModel::AllowNodeRename, true);
  m_layerTree = new KaLayerInformationView(central);
  m_layerTree->setObjectName(QStringLiteral("layerTree"));
  m_layerTree->setModel(model);
  KaLayerInformationModel::configureView(m_layerTree);
  model->setScale(m_canvas->scale());
  connect(m_canvas, &QgsMapCanvas::scaleChanged, model, &KaLayerInformationModel::setScale);
  connect(model, &KaLayerInformationModel::labelsEdited, this, [this] {
    applyLabelStackOrder();
    LayerOps::refreshCanvasIfIdle(m_canvas);
    if (m_drawingStudio) m_drawingStudio->refreshMapFromProject();
  });
  m_layerTree->setFocusPolicy(Qt::StrongFocus);
  m_layerTree->setSelectionMode(QAbstractItemView::ExtendedSelection);
  m_layerTree->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);
  m_layerTree->setDragEnabled(true);
  m_layerTree->setAcceptDrops(true);
  m_layerTree->setDropIndicatorShown(true);
  m_layerTree->setDefaultDropAction(Qt::MoveAction);
  m_layerTree->setDragDropMode(QAbstractItemView::DragDrop);
  connect(model, &QAbstractItemModel::rowsMoved, this, [this](const QModelIndex&, int, int, const QModelIndex&, int) {
    onLayerTreeRowsMoved();
  });
  m_layerTree->installEventFilter(this);
  if (m_layerTree->viewport())
    m_layerTree->viewport()->installEventFilter(this);
  m_layerTree->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(m_layerTree, &QWidget::customContextMenuRequested, this, &MainWindow::onLayerTreeContextMenu);
  connect(m_layerTree, &QTreeView::doubleClicked, this, [this](const QModelIndex& index) {
    if (index.column() == 0) onLayerTreeDoubleClicked(index);
  });
  m_bridge = new QgsLayerTreeMapCanvasBridge(layerTreeRoot, m_canvas, this);
  m_bridge->setAutoSetupOnFirstLayer(false);

  m_canvas->setContextMenuPolicy(Qt::DefaultContextMenu);
  m_canvas->installEventFilter(this);
  if (m_canvas->viewport())
    m_canvas->viewport()->installEventFilter(this);
  connect(m_canvas, &QWidget::customContextMenuRequested, this, &MainWindow::onMapContextMenu);
  connect(m_canvas, &QgsMapCanvas::contextMenuAboutToShow, this,
          [this](QMenu* menu, QgsMapMouseEvent* event) {
            if (event) populateMapContextMenu(menu, event->pos());
          });
  connect(m_canvas, &QgsMapCanvas::zoomLastStatusChanged, this,
          [this](bool available) { m_canZoomPrevious = available; });
  connect(m_canvas, &QgsMapCanvas::zoomNextStatusChanged, this,
          [this](bool available) { m_canZoomNext = available; });
  connect(m_canvas, &QgsMapCanvas::scaleChanged, this, &MainWindow::onCanvasScaleChanged);
  connect(m_canvas, &QgsMapCanvas::xyCoordinates, this, [this](const QgsPointXY& p) {
    if (m_status) m_status->setCoordinate(p.x(), p.y());
  });
  m_status->setWorkCrs(m_workCrs);
  // 새 조사와 작업공간 복원도 실제 지도 좌표계 변경을 통해 표시를 갱신한다.
  connect(m_canvas, &QgsMapCanvas::destinationCrsChanged, this, [this]() {
    const auto crs = m_canvas->mapSettings().destinationCrs();
    if (crs.isValid()) m_status->setWorkCrs(crs.authid());
  });
  connect(m_status, &KaStatusBar::crsClicked, this, [this]() {
    QMenu menu(this);
    QAction* a86 = menu.addAction(QStringLiteral("중부원점 (EPSG:5186)"));
    QAction* a87 = menu.addAction(QStringLiteral("동부원점 (EPSG:5187)"));
    a86->setCheckable(true);
    a87->setCheckable(true);
    a86->setChecked(m_workCrs == QLatin1String("EPSG:5186"));
    a87->setChecked(m_workCrs == QLatin1String("EPSG:5187"));
    menu.addSeparator();
    QAction* note = menu.addAction(QStringLiteral("제출용 파일은 항상 EPSG:5179로 나갑니다"));
    note->setEnabled(false);
    const QAction* picked = menu.exec(QCursor::pos());
    if (picked == a86)
      setWorkCrs5186();
    else if (picked == a87)
      setWorkCrs5187();
  });
  connect(m_status, &KaStatusBar::renderingToggled, this, [this](bool on) {
    if (!m_canvas) return;
    m_canvas->setRenderFlag(on);
    if (on) m_canvas->refresh();
  });
  connect(m_canvas, &QgsMapCanvas::mapToolSet, this, [this](QgsMapTool* newTool, QgsMapTool*) {
    if (m_actSelect)
      m_actSelect->setChecked(newTool && m_selectTool && newTool == m_selectTool);
    if (m_actMeasure)
      m_actMeasure->setChecked(newTool && m_measureTool && newTool == m_measureTool);
    if (m_btnDraw) {
      const bool drawing = newTool && m_captureTool && newTool == m_captureTool;
      const bool subOpen = m_subToolbar && m_subToolbar->isVisible() &&
                           m_subToolsMode == QLatin1String("draw");
      m_btnDraw->setChecked(drawing || subOpen);
    }
  });
  connect(m_canvas, &QgsMapCanvas::extentsChanged, this, [this]() {
    if (m_subToolsMode == QLatin1String("align"))
      updateAlignOverlay();
    if (m_extentClampGuard || !m_canvas) return;
    // 예전에는 그리는 중이면 여기서 그냥 돌아갔다. 그런데 줌아웃은 렌더를 띄우므로
    // extentsChanged 가 올 때 캔버스는 대개 그리는 중이고, 그래서 클램프가 거의
    // 매번 건너뛰어졌다. 한국 밖(1:800만 등)까지 줌아웃되면 VWorld 타일이 없어
    // 위성이 통째로 사라진다. clampCanvasToKorea 가 알아서 끝난 뒤로 미룬다.
    m_extentClampGuard = true;
    LayerOps::clampCanvasToKorea(m_canvas);
    m_extentClampGuard = false;
  });
  connect(m_canvas, &QgsMapCanvas::scaleChanged, this, [this](double) {
    if (m_subToolsMode == QLatin1String("align"))
      updateAlignOverlay();
  });
  // 도구를 바꾸면 파란 밑줄도 그 자리로 옮겨간다.
  connect(m_canvas, &QgsMapCanvas::mapToolSet, this,
          [this](QgsMapTool*, QgsMapTool*) { updateSubToolbarChecks(); });
  connect(m_canvas, &QgsMapCanvas::renderComplete, this, [this](QPainter*) {
    if (m_subToolsMode == QLatin1String("align"))
      updateAlignOverlay();
    logCanvasPaintState();
  });
  connect(QgsProject::instance(), &QgsProject::layersAdded, this, [this](const QList<QgsMapLayer*>& layers) {
    for (auto* layer : layers) {
      if (auto* vector = qobject_cast<QgsVectorLayer*>(layer)) watchUndoFeatureIds(vector);
    }
    if (m_isOpeningSurvey) return;
    // AdvancedConfiguration 개별 설정은 추가 당시 레이어만 가진다. 새 조사 레이어를 다시 넣는다.
    applySnapConfig();
    LayerOps::restoreThematicOverlayVisibility(QgsProject::instance());
    LayerOps::ensureSatelliteAtBottom(QgsProject::instance());
    QTimer::singleShot(0, this, [this]() { refreshMapCanvasNow(); syncThematicButtons(); });
  });
  connect(QgsProject::instance(), &QgsProject::layersRemoved, this, [this](const QStringList&) {
    if (m_isOpeningSurvey) return;
    LayerOps::ensureSatelliteAtBottom(QgsProject::instance());
    QTimer::singleShot(0, this, [this]() { refreshMapCanvasNow(); syncThematicButtons(); });
  });
  // 레이어의 체크를 끄고 켜는 것도 아이콘에 그대로 따라와야 한다.
  if (QgsLayerTree* legendRoot = QgsProject::instance()->layerTreeRoot()) {
    connect(legendRoot, &QgsLayerTreeNode::visibilityChanged, this,
            [this](QgsLayerTreeNode*) { syncThematicButtons(); });
  }
  LayerOps::applyKoreaMapLimits(QgsProject::instance(), m_canvas);

  // 1. 레이어 패널 (m_layersCard) - 상단 배치
  auto* layersCard = new QFrame(central);
  layersCard->setObjectName(QStringLiteral("layersCard"));
  m_layersCard = layersCard;
  applyWidgetShadow(layersCard, 14, 3, 30);
  auto* layersLay = new QVBoxLayout(layersCard);
  layersLay->setContentsMargins(6, 6, 6, 6);
  layersLay->setSpacing(6);

  auto* capLayers = new QLabel(QStringLiteral("레이어"), layersCard);
  capLayers->setObjectName(QStringLiteral("cardCaption"));

  auto* layersInner = new QFrame(layersCard);
  layersInner->setObjectName(QStringLiteral("layersInner"));
  auto* layersInnerLay = new QVBoxLayout(layersInner);
  layersInnerLay->setContentsMargins(4, 4, 4, 4);
  layersInnerLay->addWidget(m_layerTree, 1);
  layersInnerLay->addWidget(new KaLayerInformationPanel(model, m_layerTree, layersInner));
  m_layerEmpty = new QLabel(
      QStringLiteral("레이어가 없습니다.\n파일함에서 SHP·DXF·DWG를 끌어 넣거나\n위성·지적 배경을 올리세요."),
      layersInner);
  m_layerEmpty->setObjectName(QStringLiteral("emptyState"));
  m_layerEmpty->setAlignment(Qt::AlignCenter);
  m_layerEmpty->setWordWrap(true);
  layersInnerLay->addWidget(m_layerEmpty, 1);
#if KA_HGIS_HAS_QGIS
  connect(QgsProject::instance(), &QgsProject::layersAdded, this, [this](const QList<QgsMapLayer*>&) {
    refreshLayerEmptyState();
  });
  connect(QgsProject::instance(), &QgsProject::layersRemoved, this, [this](const QStringList&) {
    refreshLayerEmptyState();
  });
#endif
  refreshLayerEmptyState();

  // 도면/글자 체크와 선택 도면 설정은 목록에 둔다. 가져오기/순서/삭제는
  // 기존 파일함·드래그·Delete 흐름을 유지하며 전체 도면 체크는 제목 옆에 둔다.
  auto* capRow = new QHBoxLayout();
  capRow->setContentsMargins(0, 0, 0, 0);
  capRow->setSpacing(6);
  capRow->addWidget(capLayers);
  capRow->addStretch(1);
  auto* filesToggle = new QToolButton(layersCard);
  filesToggle->setObjectName(QStringLiteral("sidebarFilesToggle"));
  filesToggle->setText(QStringLiteral("파일함"));
  filesToggle->setCheckable(true);
  filesToggle->setChecked(true);
  capRow->addWidget(filesToggle);
  m_layerCheckAllBtn = new QToolButton(layersCard);
  m_layerCheckAllBtn->setObjectName(QStringLiteral("layerCheckAllBtn"));
  m_layerCheckAllBtn->setFocusPolicy(Qt::NoFocus);
  m_layerCheckAllBtn->setToolTip(QStringLiteral(
      "레이어 체크를 한 번에 모두 끄거나 켭니다. 하나라도 켜져 있으면 전부 끕니다."));
  connect(m_layerCheckAllBtn, &QToolButton::clicked, this, &MainWindow::toggleAllLayersChecked);
  capRow->addWidget(m_layerCheckAllBtn);
  layersLay->addLayout(capRow);
#if KA_HGIS_HAS_QGIS
  // 체크를 하나씩 손으로 바꿔도 단추 글씨가 따라가야 한다.
  // visibilityChanged 는 트리 안 어느 노드가 바뀌어도 뿌리까지 올라온다.
  if (QgsLayerTree* visibilityRoot = QgsProject::instance()->layerTreeRoot()) {
    connect(visibilityRoot, &QgsLayerTreeNode::visibilityChanged, this,
            [this](QgsLayerTreeNode* node) {
              LayerOps::revealCheckedLegendNode(node);
              refreshLayerCheckAllButton();
              // 체크 하나마다 전 레이어 setLabeling 을 하면 유적 글자가
              // 보였다가 사라진다. 한 틱에 한 번만 다시 쌓는다.
              if (m_labelOrderQueued) return;
              m_labelOrderQueued = true;
              QTimer::singleShot(0, this, [this]() {
                m_labelOrderQueued = false;
                applyLabelStackOrder();
              });
            });
  }
  connect(QgsProject::instance(), &QgsProject::layersAdded, this,
          [this](const QList<QgsMapLayer*>&) { refreshLayerCheckAllButton(); });
  connect(QgsProject::instance(), &QgsProject::layersRemoved, this,
          [this](const QStringList&) { refreshLayerCheckAllButton(); });
  // 레이어가 밑에 있으면 글자도 밑으로. 순서가 바뀔 때마다 다시 건다.
  // 끌어서 순서를 바꾸면 트리에서 노드가 빠졌다 들어오므로 두 신호를 다 듣는다.
  if (QgsLayerTree* orderRoot = QgsProject::instance()->layerTreeRoot()) {
    connect(orderRoot, &QgsLayerTreeNode::addedChildren, this,
            [this](QgsLayerTreeNode*, int, int) { applyLabelStackOrder(); });
    connect(orderRoot, &QgsLayerTreeNode::removedChildren, this,
            [this](QgsLayerTreeNode*, int, int) { applyLabelStackOrder(); });
  }
  connect(QgsProject::instance(), &QgsProject::layersAdded, this,
          [this](const QList<QgsMapLayer*>&) { applyLabelStackOrder(); });
#endif
  refreshLayerCheckAllButton();
  layersLay->addWidget(layersInner, 1);

#if KA_HGIS_HAS_QGIS
  connect(m_layerTree, &QgsLayerTreeView::currentLayerChanged, this, &MainWindow::updateLayerOpacityControl);
  if (m_layerTree->selectionModel()) {
    connect(m_layerTree->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, [this](const QItemSelection&, const QItemSelection&) {
              updateLayerOpacityControl();
            });
  }
  connect(QgsProject::instance(), &QgsProject::layersAdded, this, [this](const QList<QgsMapLayer*>&) {
    updateLayerOpacityControl();
  });
  connect(QgsProject::instance(), &QgsProject::layersRemoved, this, [this](const QStringList&) {
    updateLayerOpacityControl();
  });
#endif

  // 2. 파일함 패널 (m_filesPanel) - 하단 배치
  auto* filesPanel = new KaFileBrowserPanel(central);
  m_filesPanel = filesPanel;
  m_filesCard = filesPanel;
  m_fileBrowser = filesPanel->listView();
  applyWidgetShadow(filesPanel, 14, 3, 30);
  connect(filesPanel, &KaFileBrowserPanel::fileActivated, this, [this](const QString& path) {
    const bool raster = GeorefService::isImagePath(path);
    if (raster ? !addRasterFromPath(path) : !addVectorFromPath(path)) {
      KaUserError::warn(this, {
          QStringLiteral("파일"),
          QStringLiteral("선택한 파일을 지도 레이어로 열지 못했습니다."),
          QStringLiteral("SHP/DXF/DWG/GPKG/GeoTIFF/JPG만 지도에 올릴 수 있습니다.\n%1")
              .arg(QDir::toNativeSeparators(path)),
          QStringLiteral("지원 형식인지 확인한 뒤 다시 열어 주세요. DWG는 DXF로 저장해 보세요."),
      });
    }
  });
  connect(filesPanel, &KaFileBrowserPanel::statusMessage, this, [this](const QString& msg) {
    statusBar()->showMessage(msg, 6000);
  });

  // 3. 좌측 패널 수직 분할: 레이어(위) + 파일함(아래) 동시 노출
  auto* leftSplit = new QSplitter(Qt::Vertical, central);
  leftSplit->setObjectName(QStringLiteral("leftSplit"));
  m_leftSplit = leftSplit;
  leftSplit->setHandleWidth(8);
  leftSplit->setChildrenCollapsible(false);
  leftSplit->setCollapsible(1, true);
  leftSplit->installEventFilter(this);
  auto* filesScroll = new QScrollArea(leftSplit);
  filesScroll->setObjectName(QStringLiteral("sidebarFilesScroll"));
  filesScroll->setWidgetResizable(true);
  filesScroll->setFrameShape(QFrame::NoFrame);
  filesScroll->setMinimumHeight(0);
  filesPanel->setMinimumHeight(0);
  filesScroll->setWidget(filesPanel);
  m_filesCard = filesScroll;
  connect(filesToggle, &QToolButton::toggled, filesScroll, &QWidget::setVisible);
  leftSplit->addWidget(layersCard);
  leftSplit->addWidget(filesScroll);
  leftSplit->setStretchFactor(0, 3);
  leftSplit->setStretchFactor(1, 2);
  leftSplit->setSizes({380, 260});
  leftSplit->setMinimumWidth(160);
  leftSplit->setMaximumWidth(720);
  leftSplit->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);

  auto* mapCard = new QFrame(central);
  mapCard->setObjectName(QStringLiteral("mapCard"));
  applyWidgetShadow(mapCard, 16, 4, 30);
  auto* mapLay = new QVBoxLayout(mapCard);
  mapLay->setContentsMargins(4, 4, 4, 4);
  mapLay->setSpacing(4);
  m_messageBar = new QgsMessageBar(mapCard);
  m_messageBar->setObjectName(QStringLiteral("mapMessageBar"));
  mapLay->addWidget(m_messageBar, 0);
  mapLay->addWidget(m_canvas, 1);
  {
    QWidget* railHost = m_canvas->viewport() ? m_canvas->viewport() : static_cast<QWidget*>(m_canvas);
    m_layerOpacityRail = new KaLayerOpacityRail(railHost);
    // Zoom buttons and a scale bar float on the map like the opacity card.
    auto* controls = new KaMapControls(m_canvas, railHost);
    controls->setFitHandler([this]() {
      for (QgsVectorLayer* area : LayerOps::findAllByLayerKey(QgsProject::instance(), QStringLiteral("survey_area")))
        if (area && area->isValid() && area->featureCount() > 0 && LayerOps::zoomToLayerMax(m_canvas, area))
          return;
      if (m_layerTree && m_layerTree->currentLayer())
        LayerOps::zoomToLayerMax(m_canvas, m_layerTree->currentLayer());
    });
    new KaMapScaleBar(m_canvas, railHost);
  }
  connect(m_layerOpacityRail, &KaLayerOpacityRail::brightnessChanged, this, [this](int value) {
    if (!m_layerTree) return;
    QgsMapLayer* cur = m_layerTree->currentLayer();
    if (!LayerOps::canAdjustBrightness(cur)) return;
    LayerOps::setMapLayerBrightness(cur, value, m_canvas);
    if (m_drawingStudio) {
      m_drawingStudio->updateLayerOpacityControl();
      m_drawingStudio->repaintMapLayers();
    }
  });
  connect(m_layerOpacityRail, &KaLayerOpacityRail::percentChanged, this, [this](int value) {
    if (!m_layerTree) return;
    QgsMapLayer* cur = m_layerTree->currentLayer();
    if (!cur || !LayerOps::isReferenceOrBasemapLayer(cur)) return;
    LayerOps::setMapLayerOpacity(cur, value / 100.0, m_canvas);
    if (m_drawingStudio) {
      m_drawingStudio->updateLayerOpacityControl();
      m_drawingStudio->repaintMapLayers();
    }
  });
  updateLayerOpacityControl();

  auto* scaleBar = new QHBoxLayout();
  scaleBar->setSpacing(4);
  m_scaleEdit = m_status->scaleEdit();
  m_scaleCombo = m_status->scaleCombo();
  connect(m_scaleEdit, &QLineEdit::returnPressed, this, &MainWindow::applyMapScaleFromUi);
  // 프리셋 선택과 직접 입력이 완전히 같은 경로를 타야 결과가 같다.
  connect(m_scaleCombo, QOverload<int>::of(&QComboBox::activated), this, [this](int idx) {
    if (!m_scaleCombo || !m_scaleEdit) return;
    const int s = m_scaleCombo->itemData(idx).toInt();
    if (s > 0) {
      m_scaleEdit->setText(QStringLiteral("1:%1").arg(s));
      applyMapScaleFromUi();
    }
  });
  m_mapGridCheck = new QCheckBox(QStringLiteral("좌표격자"), mapCard);
  m_mapGridCheck->setObjectName(QStringLiteral("mapGridCheck"));
  m_mapGridCheck->setToolTip(QStringLiteral("맵에 좌표 격자를 켭니다. Shift를 누른 채 켜면 경위도입니다."));
  connect(m_mapGridCheck, &QCheckBox::toggled, this, &MainWindow::toggleMapGrid);
  m_mapGridStep = new QDoubleSpinBox(mapCard);
  m_mapGridStep->setObjectName(QStringLiteral("mapGridStep"));
  m_mapGridStep->setRange(1.0, 10000.0);
  m_mapGridStep->setDecimals(0);
  m_mapGridStep->setSuffix(QStringLiteral(" m"));
  m_mapGridStep->setValue(20);
  m_mapGridStep->setMaximumWidth(80);
  m_mapGridStep->setToolTip(QStringLiteral("격자 간격(미터). 시굴격자 이동 때도 이 간격에 붙습니다."));
  connect(m_mapGridStep, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double) {
    if (m_mapGridCheck && m_mapGridCheck->isChecked())
      toggleMapGrid();
  });
  m_mapGridRot = new QDoubleSpinBox(mapCard);
  m_mapGridRot->setRange(0.0, 360.0);
  m_mapGridRot->setDecimals(1);
  m_mapGridRot->setSuffix(QStringLiteral(" °"));
  m_mapGridRot->setValue(0);
  m_mapGridRot->setMaximumWidth(70);
  m_mapGridRot->setToolTip(QStringLiteral("격자 회전 (동쪽 기준 시계 방향)"));
  connect(m_mapGridRot, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double) {
    if (m_mapGridCheck && m_mapGridCheck->isChecked())
      toggleMapGrid();
  });
  m_mapGridWidth = new QDoubleSpinBox(mapCard);
  m_mapGridWidth->setRange(0.5, 5.0);
  m_mapGridWidth->setDecimals(1);
  m_mapGridWidth->setSingleStep(0.1);
  m_mapGridWidth->setValue(1.2);
  m_mapGridWidth->setMaximumWidth(60);
  m_mapGridWidth->setToolTip(QStringLiteral("격자 선 굵기"));
  connect(m_mapGridWidth, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double) {
    if (m_mapGridCheck && m_mapGridCheck->isChecked())
      toggleMapGrid();
  });
  m_mapGridDash = new QComboBox(mapCard);
  m_mapGridDash->addItem(QStringLiteral("실선"), static_cast<int>(Qt::SolidLine));
  m_mapGridDash->addItem(QStringLiteral("점선"), static_cast<int>(Qt::DashLine));
  m_mapGridDash->addItem(QStringLiteral("점"), static_cast<int>(Qt::DotLine));
  m_mapGridDash->setCurrentIndex(1);
  m_mapGridDash->setMaximumWidth(70);
  m_mapGridDash->setToolTip(QStringLiteral("격자 선 모양"));
  connect(m_mapGridDash, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
    if (m_mapGridCheck && m_mapGridCheck->isChecked())
      toggleMapGrid();
  });
  auto* gridDetail = new QWidget(mapCard);
  gridDetail->setObjectName(QStringLiteral("gridDetail"));
  auto* gridDetailLay = new QHBoxLayout(gridDetail);
  gridDetailLay->setContentsMargins(0, 0, 0, 0);
  gridDetailLay->setSpacing(3);
  gridDetailLay->addWidget(m_mapGridStep);
  gridDetailLay->addWidget(new QLabel(QStringLiteral("회전"), gridDetail));
  gridDetailLay->addWidget(m_mapGridRot);
  gridDetailLay->addWidget(new QLabel(QStringLiteral("굵기"), gridDetail));
  gridDetailLay->addWidget(m_mapGridWidth);
  gridDetailLay->addWidget(m_mapGridDash);

  // 격자 선 색: 눌러 보면 바로 아는 색칠 단추 셋(빨강·파랑·검정)과 「그 밖의 색」.
  gridDetailLay->addWidget(new QLabel(QStringLiteral("색"), gridDetail));
  const QVector<QPair<QColor, QString>> gridSwatches = {
      {QColor(0xD9, 0x2B, 0x2B), QStringLiteral("빨간색 격자")},
      {QColor(0x1D, 0x4E, 0xD8), QStringLiteral("파란색 격자")},
      {QColor(0x1F, 0x29, 0x37), QStringLiteral("검정색 격자")},
  };
  for (const auto& sw : gridSwatches) {
    auto* b = new QToolButton(gridDetail);
    b->setObjectName(QStringLiteral("gridColorSwatch"));
    b->setFixedSize(20, 20);
    b->setCheckable(true);
    b->setAutoRaise(false);
    b->setStyleSheet(KaTheme::colorSwatchStyle(sw.first));
    b->setToolTip(sw.second);
    const QColor c = sw.first;
    connect(b, &QToolButton::clicked, this, [this, c]() { setMapGridColor(c); });
    m_mapGridColorBtns.append(b);
    gridDetailLay->addWidget(b);
  }
  auto* gridColorMore = new QToolButton(gridDetail);
  gridColorMore->setObjectName(QStringLiteral("gridColorPick"));
  gridColorMore->setText(QStringLiteral("…"));
  gridColorMore->setFixedSize(22, 20);
  gridColorMore->setToolTip(QStringLiteral("그 밖의 색을 직접 고릅니다"));
  connect(gridColorMore, &QToolButton::clicked, this, [this]() {
    const QColor picked = QColorDialog::getColor(m_mapGridColor, this,
                                                 QStringLiteral("격자 선 색"),
                                                 QColorDialog::ShowAlphaChannel);
    if (picked.isValid())
      setMapGridColor(picked);
  });
  gridDetailLay->addWidget(gridColorMore);
  syncMapGridColorButtons();
  gridDetail->setVisible(false);
  connect(m_mapGridCheck, &QCheckBox::toggled, gridDetail, &QWidget::setVisible);
  scaleBar->addWidget(m_mapGridCheck);
  scaleBar->addWidget(gridDetail);
  scaleBar->addStretch(1);
  mapLay->addLayout(scaleBar);

  // 왼쪽 패널 ↔ 지도 사이를 끌어서 나눌 수 있게 한다. 나눈 폭은 창 상태와
  // 같이 저장돼 다음에 열 때 그대로 온다(MainWindow/mainSplit).
  m_mainSplit = new QSplitter(Qt::Horizontal, central);
  m_mainSplit->setObjectName(QStringLiteral("mainSplit"));
  m_mainSplit->setHandleWidth(10);
  m_mainSplit->setChildrenCollapsible(false);
  m_mainSplit->addWidget(leftSplit);
  m_mainSplit->addWidget(mapCard);
  m_mainSplit->setStretchFactor(0, 0);
  m_mainSplit->setStretchFactor(1, 1);
  m_mainSplit->setSizes({348, 932});
  root->addWidget(m_mainSplit, 1);
#else
  root->addWidget(new QLabel(QStringLiteral("QGIS SDK 스텁 모드"), central), 1);
  setCentralWidget(central);
#endif
#if KA_HGIS_HAS_QGIS
  m_viewTabs = new QTabWidget(this);
  m_viewTabs->setObjectName(QStringLiteral("viewTabs"));
  m_viewTabs->setDocumentMode(true);
  m_viewTabs->setTabsClosable(true);
  m_viewTabs->setMovable(false);
  m_startPage = new KaStartPage(m_viewTabs);
  connect(m_startPage, &KaStartPage::newSurveyRequested, this, &MainWindow::newSurvey);
  connect(m_startPage, &KaStartPage::openRequested, this, &MainWindow::openProject);
  connect(m_startPage, &KaStartPage::recentOpened, this, &MainWindow::openRecentSurvey);
  connect(m_startPage, &KaStartPage::forgetRequested, this, [this](const QString& path) {
    QSettings st = RecentSurveys::userSettings();
    RecentSurveys::forget(st, path);
    if (m_startPage) m_startPage->reload();
  });
  m_mapPage = central;
  const int homeIdx = m_viewTabs->addTab(m_startPage, KaIcons::icon(QStringLiteral("new")),
                                         QStringLiteral("홈"));
  const int mapIdx = m_viewTabs->addTab(central, KaIcons::icon(QStringLiteral("map")),
                                        QStringLiteral("지도"));
  if (QTabBar* bar = m_viewTabs->tabBar()) {
    bar->setTabButton(homeIdx, QTabBar::RightSide, nullptr);
    bar->setTabButton(mapIdx, QTabBar::RightSide, nullptr);
  }
  connect(m_viewTabs, &QTabWidget::tabCloseRequested, this, &MainWindow::onViewTabCloseRequested);
  connect(m_viewTabs, &QTabWidget::currentChanged, this, [this](int i) {
    if (!m_viewTabs) return;
    QWidget* page = m_viewTabs->widget(i);
    if (m_actMapGeoTiff) m_actMapGeoTiff->setEnabled(page == m_mapPage);
    if (m_startPage && page == m_startPage)
      m_startPage->reload();
    if (m_status) m_status->setMapInstrumentsVisible(page != m_startPage);
    if (m_mapPage && page == m_mapPage) {
      QTimer::singleShot(0, this, [this]() { ensureStartupViewReady(); });
    } else {
      hideSubTools();
    }
    if (m_drawingStudio && page == m_drawingStudio)
      QTimer::singleShot(0, this, [this]() {
        if (!m_drawingStudio) return;
        m_drawingStudio->refreshMapFromProject();
      });
    if (m_sectionStudio && page == m_sectionStudio)
      QTimer::singleShot(0, this, [this]() {
        if (m_sectionStudio) m_sectionStudio->refreshLayers();
      });
    if (m_canvas) onCanvasScaleChanged(m_canvas->scale());
  });
  m_viewTabs->setCurrentWidget(m_startPage);
  if (m_status) m_status->setMapInstrumentsVisible(false);
  setCentralWidget(m_viewTabs);
  // 자동 저장 없음. 저장은 사용자가 「저장」(Ctrl+S)을 누를 때만 일어난다.
  // 저장 안 한 작업은 창 제목의 * 로 보이고, 닫을 때 한 번 물어본다.
  if (QgsProject* proj = QgsProject::instance()) {
    connect(proj, &QgsProject::isDirtyChanged, this, [this](bool) { refreshWindowTitle(); });
  }
  // 레이어가 세션 도중 사라지는 일을 잡기 위한 감시. 레이어 상태를 세어 직전과 다르면
  // 무엇이 어떻게 달라졌는지 세션 로그에 남긴다. 사용자가 "사라졌다"고 말한 시각과
  // 로그를 맞춰 볼 수 있는 유일한 근거다. 파일에 쓰지 않으므로 저장과 무관하다.
  m_layerWatchTimer = new QTimer(this);
  m_layerWatchTimer->setObjectName(QStringLiteral("layerWatchTimer"));
  m_layerWatchTimer->setInterval(30000);
  connect(m_layerWatchTimer, &QTimer::timeout, this, &MainWindow::auditLayerHealth);
  m_layerWatchTimer->start();
  // 원본 조사 파일에는 쓰지 않는다. 간격은 60초보다 길게 두어, 없앤 20초 자동 저장과
  // 같은 타이머로 잡히지 않게 한다.
  m_recoverySnapshotTimer = new QTimer(this);
  m_recoverySnapshotTimer->setObjectName(QStringLiteral("recoverySnapshotTimer"));
  m_recoverySnapshotTimer->setInterval(120000);
  connect(m_recoverySnapshotTimer, &QTimer::timeout, this, &MainWindow::captureRecoverySnapshot);
  m_recoverySnapshotTimer->start();
  // 마지막 조사는 첫 showEvent 뒤에 복원한다. show() 안쪽 nested event 처리 중
  // 프로젝트를 읽으면 WMS/캔버스 객체 정리가 겹친다.
#endif

}

void MainWindow::openTerrain3dStudio() {
#if KA_HGIS_HAS_QGIS
  if (!m_viewTabs)
    return;
  if (m_terrain3dStudio && m_viewTabs->indexOf(m_terrain3dStudio) >= 0) {
    m_viewTabs->setCurrentWidget(m_terrain3dStudio);
    hideSubTools();
    return;
  }
  if (!m_terrain3dStudio) {
    m_terrain3dStudio = new KaTerrain3dStudio(QgsProject::instance(), m_canvas, this);
    m_terrain3dStudio->setAttribute(Qt::WA_DeleteOnClose, false);
    connect(m_terrain3dStudio, &KaTerrain3dStudio::requestDrawingStudio, this,
            &MainWindow::placeTerrain3dOnSheet);
  }
  m_terrain3dStudio->setParent(m_viewTabs, Qt::Widget);
  if (m_viewTabs->indexOf(m_terrain3dStudio) < 0)
    m_viewTabs->addTab(m_terrain3dStudio, KaIcons::icon(QStringLiteral("terrain_3d")),
                       QStringLiteral("입체지형"));
  m_viewTabs->setCurrentWidget(m_terrain3dStudio);
  hideSubTools();
  statusBar()->showMessage(QStringLiteral("지금 지도 화면을 고해상 입체로 만듭니다."), 6000);
#endif
}

// 레이어가 수십 개가 되면 하나씩 체크를 푸는 게 일이다. 한 번에 끄고 켠다.
// 라벨은 「지금 상태」가 아니라 「누르면 일어날 일」을 보여 준다.
// 라벨은 QGIS 가 맨 마지막에 한꺼번에 얹는다. 그대로 두면 아래 레이어의 지번이
// 위 레이어의 선 위로 올라온다. 레이어 순서를 글자에도 그대로 먹인다.
// Delete·Ctrl+Z 는 이 창의 단축키(WindowShortcut)라 같은 창 안 탭인 조판 화면보다
// 먼저 잡힌다. 그래서 조판에서는 레이어도 범례도 지워지지 않았다.
// 조판이 열려 있으면 그 화면에 넘긴다. 글자 칸에 커서가 있으면 아무것도 하지 않는다.
bool MainWindow::routeEditKeyToActiveStudio(bool isDelete) {
#if KA_HGIS_HAS_QGIS
  if (m_topographicBrowser && (QApplication::activeWindow() == m_topographicBrowser.data() ||
      m_topographicBrowser->isAncestorOf(QApplication::focusWidget())))
    return true;
  QWidget* focus = QApplication::focusWidget();
  if (qobject_cast<QLineEdit*>(focus) || qobject_cast<QAbstractSpinBox*>(focus))
    return true;  // 글자를 고치는 중이다. 도형·레이어를 지우면 안 된다.
  if (m_viewTabs && m_terrain3dLayoutStudio && m_viewTabs->currentWidget() == m_terrain3dLayoutStudio) {
    if (isDelete) m_terrain3dLayoutStudio->deleteSelectedItems();
    else m_terrain3dLayoutStudio->undoLastChange();
    return true;
  }
  if (!m_viewTabs || !m_drawingStudio) return false;
  if (m_viewTabs->currentWidget() != m_drawingStudio) return false;
  if (isDelete)
    m_drawingStudio->handleDeleteKey();
  else
    m_drawingStudio->handleUndoKey();
  return true;
#else
  Q_UNUSED(isDelete);
  return false;
#endif
}

// 덧그림에 올릴 레이어를 다시 고른다. 레이어 순서·표시가 바뀔 때마다 부른다.
void MainWindow::refreshAboveLabelsOverlay() {
#if KA_HGIS_HAS_QGIS
  if (!m_aboveLabels) return;
  const QList<QgsMapLayer*> above = LayerOps::layersDrawnAboveLabels(QgsProject::instance());
  m_aboveLabels->setLayers(above);
  // 몇 개가 뽑혔는지 보여 준다. 0이면 규칙이 대상을 못 고른 것이라 바로 알 수 있다.
  if (m_aboveLabelsCount != above.size()) {
    m_aboveLabelsCount = above.size();
    QStringList names;
    for (QgsMapLayer* l : above) {
      if (l) names << l->name();
    }
    statusBar()->showMessage(
        above.isEmpty()
            ? QStringLiteral("글자 위로 올릴 레이어 없음")
            : QStringLiteral("글자 위로 올릴 레이어 %1개: %2")
                  .arg(above.size())
                  .arg(names.join(QStringLiteral(", "))),
        6000);
  }
#endif
}

void MainWindow::applyLabelStackOrder() {
#if KA_HGIS_HAS_QGIS
  LayerOps::applyLayerOrderToLabels(QgsProject::instance(), m_canvas);
  refreshAboveLabelsOverlay();
  if (m_drawingStudio)
    m_drawingStudio->repaintMapLayers();
#endif
}

void MainWindow::toggleAllLayersChecked() {
#if KA_HGIS_HAS_QGIS
  QgsProject* proj = QgsProject::instance();
  QgsLayerTree* root = proj ? proj->layerTreeRoot() : nullptr;
  if (!root) return;
  const QList<QgsLayerTreeLayer*> layers = root->findLayers();
  if (layers.isEmpty()) return;
  bool anyOn = false;
  for (QgsLayerTreeLayer* n : layers) {
    if (n && n->isVisible()) {
      anyOn = true;
      break;
    }
  }
  // 하나라도 켜져 있으면 전부 끈다. 전부 꺼져 있을 때만 전부 켠다.
  const bool turnOn = !anyOn;
  const auto children = root->children();
  for (QgsLayerTreeNode* n : children) {
    if (n) n->setItemVisibilityCheckedRecursive(turnOn);
  }
  refreshLayerCheckAllButton();
  LayerOps::refreshCanvasIfIdle(m_canvas);
  statusBar()->showMessage(turnOn ? QStringLiteral("레이어 %1개를 모두 켰습니다").arg(layers.size())
                                  : QStringLiteral("레이어 %1개를 모두 껐습니다").arg(layers.size()),
                           4000);
#endif
}

void MainWindow::refreshLayerCheckAllButton() {
#if KA_HGIS_HAS_QGIS
  if (!m_layerCheckAllBtn) return;
  QgsProject* proj = QgsProject::instance();
  QgsLayerTree* root = proj ? proj->layerTreeRoot() : nullptr;
  const QList<QgsLayerTreeLayer*> layers = root ? root->findLayers() : QList<QgsLayerTreeLayer*>();
  m_layerCheckAllBtn->setEnabled(!layers.isEmpty());
  bool anyOn = false;
  for (QgsLayerTreeLayer* n : layers) {
    if (n && n->isVisible()) {
      anyOn = true;
      break;
    }
  }
  m_layerCheckAllBtn->setText(anyOn ? QStringLiteral("전체 끄기") : QStringLiteral("전체 켜기"));
#endif
}

void MainWindow::updateLayerOpacityControl() {
#if KA_HGIS_HAS_QGIS
  if (!m_layerOpacityRail || !m_layerTree) return;
  QgsMapLayer* cur = m_layerTree->currentLayer();
  if (cur && LayerOps::isReferenceOrBasemapLayer(cur)) {
    const int val = qBound(0, qRound(LayerOps::mapLayerOpacity(cur) * 100.0), 100);
    m_layerOpacityRail->setPercent(val, true);
  } else {
    m_layerOpacityRail->setPercent(100, false);
  }
  // 밝기는 그림(래스터)에만 있다. 항공사진·위성·지형맵이 대상이다.
  const bool bright = LayerOps::canAdjustBrightness(cur);
  m_layerOpacityRail->setBrightness(bright ? LayerOps::mapLayerBrightness(cur) : 0, bright);
  if (m_drawingStudio)
    m_drawingStudio->updateLayerOpacityControl();
#endif
}

void MainWindow::ensureDefaultBasemaps() {
#if KA_HGIS_HAS_QGIS
  QgsProject* proj = QgsProject::instance();
  if (!proj) return;
  const bool dirtyBefore = proj->isDirty();
  // 저장된 조사의 위성·지적 주소에는 그때 쓰던 인증키가 박혀 있다. 키를 새로 받아도
  // 예전 조사를 열면 만료된 키로 타일을 받아 배경지도가 소리 없이 백지가 됐다.
  {
    QStringList swapped;
    const int n = LayerOps::refreshVworldApiKeyInLayers(proj, VworldSettings::loadApiKey(), &swapped);
    if (n > 0)
      KaCrashGuard::logLine(QStringLiteral("[basemap] 저장된 VWorld 인증키를 현재 키로 교체 %1개 — %2")
                                .arg(n)
                                .arg(swapped.join(QStringLiteral(", "))));
  }
  LayerOps::pruneDuplicateSatelliteLayers(proj);
  bool hasSat = false;
  for (QgsMapLayer* l : proj->mapLayers()) {
    if (!l) continue;
    // 이름만 보고 "이미 있다"고 판단하면, 원본이 깨진 배경지도가 이름만 남아 영영
    // 다시 만들어지지 않는다(지적 설정 파일이 지워진 경우가 그랬다). 살아 있는
    // 레이어만 있다고 친다.
    if (!l->isValid()) continue;
    if (l->name().contains(QStringLiteral("위성")))
      hasSat = true;
  }
  bool hasCad = false;
  for (QgsMapLayer* l : proj->mapLayers()) {
    if (l && l->isValid() && LayerOps::isVworldCadastralPicture(l))
      hasCad = true;
  }
  const QString key = VworldSettings::loadApiKey();
  QString satErr;
  QString cadErr;
  // Add without canvas so LayerOps does not rewrite the current extent/scale.
  if (!hasSat)
    hasSat = LayerOps::addVworldSatelliteMap(proj, nullptr, key, &satErr);
  if (!hasCad && !key.isEmpty() && !LayerOps::userRemovedCadastral(proj))
    hasCad = LayerOps::addVworldCadastralMap(proj, nullptr, key, &cadErr);
  LayerOps::ensureSatelliteAtBottom(proj);
  // 예전에는 실패해도 사라지는 상태바 메시지뿐이라 현장 로그에 아무 흔적이 없었다.
  // 무엇이 왜 안 올라왔는지 반드시 남긴다. 인증키 값 자체는 절대 남기지 않는다.
  KaCrashGuard::logLine(
      QStringLiteral("[basemap] 위성 %1 · 지적 %2 · 키 %3%4%5")
          .arg(hasSat ? QStringLiteral("있음") : QStringLiteral("없음"),
               hasCad ? QStringLiteral("있음") : QStringLiteral("없음"),
               key.isEmpty() ? QStringLiteral("없음") : QStringLiteral("있음"),
               satErr.isEmpty() ? QString()
                                : QStringLiteral(" · 위성오류=%1").arg(satErr.left(200)),
               cadErr.isEmpty() ? QString()
                                : QStringLiteral(" · 지적오류=%1").arg(cadErr.left(200))));
  if (hasSat && hasCad)
    statusBar()->showMessage(QStringLiteral("위성과 지적도를 올려 두었습니다."), 5000);
  else if (hasSat && !hasCad)
    statusBar()->showMessage(
        key.isEmpty()
            ? QStringLiteral("위성은 올렸습니다. 지적도는 VWorld API 키가 필요합니다.")
            : (cadErr.isEmpty() ? QStringLiteral("지적도를 올리지 못했습니다.") : cadErr),
        8000);
  else if (!hasSat)
    statusBar()->showMessage(satErr.isEmpty() ? QStringLiteral("위성을 올리지 못했습니다.") : satErr,
                             8000);
  if (!dirtyBefore)
    proj->setDirty(false);
  updateHistoricalMapButtons();
#endif
}

void MainWindow::applyStartupMap() {
#if KA_HGIS_HAS_QGIS
  LayerOps::ensureOtfEnabled(QgsProject::instance(), m_canvas, m_workCrs);
  LayerOps::setWorkCrs(QgsProject::instance(), m_canvas, m_workCrs, nullptr, false);
  if (m_canvas) m_canvas->freeze(true);
  LayerOps::applyKoreaMapLimits(QgsProject::instance(), m_canvas);
  updateNextActionStatus();
  m_startupViewApplied = false;
  // 지적 WMS는 GetCapabilities 왕복을 동기로 기다린다(현장 측정 약 2.9초).
  // 창이 뜨기 전에 그 왕복을 붙들고 있으면 시작이 그만큼 늦다. 이벤트 루프로
  // 미뤄 창을 먼저 띄우고 배경지도는 뒤따라 올린다.
  if (!m_basemapBootPending) {
    m_basemapBootPending = true;
    QTimer::singleShot(0, this, &MainWindow::loadBootBasemaps);
  }
  LayerOps::pruneEmptyLegendGroups(QgsProject::instance());
  ensureStartupViewReady();
#endif
}

void MainWindow::syncThematicButtons() {
#if KA_HGIS_HAS_QGIS
  QgsProject* proj = QgsProject::instance();
  if (!proj) return;
  // 범례가 진실이다. 레이어를 지우거나 레이어의 체크를 끄면 아이콘도 꺼진다.
  // isLayerVisible은 레이어가 없으면 false라 삭제·체크해제를 한 번에 덮는다.
  const auto sync = [proj](QAction* act, QToolButton* btn, const QString& title) {
    const bool on = LayerOps::isLayerVisible(proj, title);
    if (act && act->isCheckable() && act->isChecked() != on) {
      const QSignalBlocker block(act);
      act->setChecked(on);
    }
    if (btn && btn->isCheckable() && btn->isChecked() != on) {
      const QSignalBlocker block(btn);
      btn->setChecked(on);
    }
  };
  sync(nullptr, m_btnTerrain, QStringLiteral("지형맵"));
  sync(nullptr, m_btnDem, QStringLiteral("DEM"));
  sync(nullptr, m_btnSoil, QStringLiteral("토양도(흙토람)"));
  sync(m_actGeology, nullptr, QStringLiteral("지질도(KIGAM 1:5만)"));
  sync(m_actRiver, nullptr, QStringLiteral("수계도(하천망)"));
  updateHistoricalMapButtons();
#endif
}

void MainWindow::updateHistoricalMapButtons() {
  const bool hasHistoryKey =
      LayerOps::historyGisApiKeyUsable(VworldSettings::loadHistoryGisApiKey());
  if (m_btnMap1919) {
    m_btnMap1919->setEnabled(hasHistoryKey);
    m_btnMap1919->setToolTip(
        hasHistoryKey
            ? QStringLiteral("1919년 조선지형도 1:5만 (국사편찬위원회 WMTS, EPSG:5179)")
            : QStringLiteral(
                  "더보기 → API 키 입력에 역사지리정보DB 키를 저장하면 켤 수 있습니다"));
  }
}

void MainWindow::loadBootBasemaps() {
#if KA_HGIS_HAS_QGIS
  if (!m_basemapBootPending) return;
  if (m_isOpeningSurvey) {
    // 조사를 여는 중이면 지금 올리지 않는다. 그런데 예전에는 여기서 대기 표시를
    // 꺼 버리고 끝냈다. 시작하자마자 작업공간을 복원하는 포터블에서는 이 경합에
    // 늘 져서 위성·지적이 영영 올라오지 않았다(현장 로그에 [boot] 배경지도 줄이
    // 아예 없다). 표시를 유지하고 열기가 끝난 뒤 다시 시도한다.
    if (m_basemapBootRetries < kBasemapBootRetryMax) {
      ++m_basemapBootRetries;
      QTimer::singleShot(300, this, &MainWindow::loadBootBasemaps);
    } else {
      m_basemapBootPending = false;
      KaCrashGuard::logLine(
          QStringLiteral("[boot] 배경지도 건너뜀 — 조사 열기가 %1초 넘게 끝나지 않았다")
              .arg(kBasemapBootRetryMax * 0.3, 0, 'f', 1));
    }
    return;
  }
  m_basemapBootPending = false;
  m_basemapBootRetries = 0;
  m_isLoadingBasemaps = true;
  QElapsedTimer bm;
  bm.start();
  ensureDefaultBasemaps();
  LayerOps::pruneEmptyLegendGroups(QgsProject::instance());
  ensureStartupViewReady();
  m_isLoadingBasemaps = false;
  KaCrashGuard::logLine(
      QStringLiteral("[boot] 배경지도 %1 ms · 미리보기 = %2 · 병렬렌더 = %3")
          .arg(bm.elapsed())
          .arg(m_canvas && m_canvas->previewJobsEnabled() ? QStringLiteral("켬")
                                                          : QStringLiteral("끔"))
          .arg(QStringLiteral("끔")));
#endif
}

void MainWindow::ensureStartupViewReady() {
#if KA_HGIS_HAS_QGIS
  if (m_isOpeningSurvey || !m_canvas || m_startupViewApplied) return;
  if (m_canvas->width() < 40 || m_canvas->height() < 40) return;
  m_canvas->freeze(true);
  LayerOps::applyCanvasScreenDpi(m_canvas);
  LayerOps::ensureOtfEnabled(QgsProject::instance(), m_canvas, m_workCrs);
  LayerOps::syncMapCanvas(QgsProject::instance(), m_canvas, false);
  m_canvas->freeze(true);
  LayerOps::zoomToKorea(m_canvas, m_workCrs, false);
  LayerOps::clampCanvasToKorea(m_canvas);
  m_canvas->freeze(false);
  LayerOps::refreshXyzBasemapTiles(m_canvas);
  m_startupViewApplied = true;
#endif
}

void MainWindow::scheduleMapDisplayRefresh() {
#if KA_HGIS_HAS_QGIS
  if (!m_canvas) return;
  if (!m_displayRefresh) {
    m_displayRefresh = new QTimer(this);
    m_displayRefresh->setObjectName(QStringLiteral("mapDisplayRefresh"));
    m_displayRefresh->setSingleShot(true);
    connect(m_displayRefresh, &QTimer::timeout, this, [this]() {
      if (!m_canvas) return;
      if (m_canvas->isDrawing()) {
        if (m_displayRefreshWaits < 30) {
          ++m_displayRefreshWaits;
          m_displayRefresh->start(200);
        }
        return;
      }
      m_displayRefreshWaits = 0;
      const QSize before = m_canvas->mapSettings().outputSize();
      const float dprBefore = m_canvas->mapSettings().devicePixelRatio();
      const double dpiBefore = m_canvas->mapSettings().outputDpi();
      LayerOps::applyCanvasScreenDpi(m_canvas);
      const bool changed = m_canvas->mapSettings().outputSize() != before ||
                           !qFuzzyCompare(m_canvas->mapSettings().devicePixelRatio(), dprBefore) ||
                           !qFuzzyCompare(m_canvas->mapSettings().outputDpi(), dpiBefore);
      if (changed)
        LayerOps::refreshXyzBasemapTiles(m_canvas);
    });
  }
  // QGIS resizeEvent는 500ms 뒤에 refresh 한다. 그 전에 격자를 바꾸지 않는다.
  m_displayRefreshWaits = 0;
  m_displayRefresh->start(700);
#endif
}

void MainWindow::changeEvent(QEvent* event) {
  QMainWindow::changeEvent(event);
#if KA_HGIS_HAS_QGIS
  if (event && event->type() == QEvent::WindowStateChange)
    scheduleMapDisplayRefresh();
#endif
}

void MainWindow::bindMapDisplayScreen() {
#if KA_HGIS_HAS_QGIS
  QWindow* wh = windowHandle();
  if (!wh || m_mapScreenBound) return;
  m_mapScreenBound = true;
  connect(wh, &QWindow::screenChanged, this, [this](QScreen*) {
    KaWindowGeometry::fit(this);
    if (m_canvas) LayerOps::applyCanvasScreenDpi(m_canvas);
    scheduleMapDisplayRefresh();
    QTimer::singleShot(100, this, [this]() {
      if (m_mainSplit) {
        const int tw = m_mainSplit->width();
        if (tw > 300) {
          const QList<int> sz = m_mainSplit->sizes();
          if (!sz.isEmpty() && sz.at(0) > tw * 0.35) {
            const int lw = qBound(200, int(tw * 0.22), 360);
            m_mainSplit->setSizes({lw, tw - lw});
          }
        }
      }
      updateGeometry();
      if (centralWidget()) centralWidget()->updateGeometry();
    });
  });
#endif
}

void MainWindow::showEvent(QShowEvent* event) {
  QMainWindow::showEvent(event);
  KaWindowGeometry::fit(this);
#if KA_HGIS_HAS_QGIS
  bindMapDisplayScreen();
  if (m_canvas) {
    LayerOps::applyCanvasScreenDpi(m_canvas);
    scheduleMapDisplayRefresh();
  }
  if (m_mainSplit) {
    const int tw = m_mainSplit->width();
    if (tw > 300) {
      const QList<int> sz = m_mainSplit->sizes();
      if (!sz.isEmpty() && sz.at(0) > tw * 0.35) {
        const int lw = qBound(200, int(tw * 0.22), 360);
        m_mainSplit->setSizes({lw, tw - lw});
      }
    }
  }
  if (!m_recoveryOfferDone)
    QTimer::singleShot(0, this, &MainWindow::offerRecoverySnapshot);
#endif
}

void MainWindow::zoomSelectedLayerMax() {
#if KA_HGIS_HAS_QGIS
  if (!m_canvas) return;
  QgsMapLayer* layer = nullptr;
  if (m_layerTree) {
    layer = m_layerTree->currentLayer();
    if (!layer) {
      const QList<QgsMapLayer*> sel = m_layerTree->selectedLayers();
      if (!sel.isEmpty())
        layer = sel.first();
    }
  }
  if (!layer) {
    zoomMapToFullMax();
    return;
  }
  if (!LayerOps::zoomToLayerMax(m_canvas, layer)) {
    statusBar()->showMessage(
        QStringLiteral("이 레이어에 도형이 없습니다. 먼저 그린 뒤 다시 누르세요: %1").arg(layer->name()),
        8000);
    return;
  }
  // 옮긴 범위로 위성·지적 타일을 다시 받아 온다. 예전에는 이동 직후 배경이 비어
  // 있다가 사용자가 줌인·줌아웃하거나 점을 찍고 지워야 채워졌다.
  LayerOps::refreshXyzBasemapTiles(m_canvas);
  statusBar()->showMessage(QStringLiteral("이 레이어로 이동: %1").arg(layer->name()), 5000);
#endif
}

#if KA_HGIS_HAS_QGIS
static bool isProjectSurveyDomainLayer(const QgsVectorLayer* vl, const QString& surveyGpkgPath) {
  if (!vl || !vl->isValid() || surveyGpkgPath.isEmpty()) return false;
  const QString key = LayerOps::layerKeyOf(vl);
  if (key.isEmpty() || key.startsWith(QLatin1String("user:"))) return false;
  const QString src = vl->source().split(QLatin1Char('|')).first();
  if (src.endsWith(QLatin1String(".shp"), Qt::CaseInsensitive)) return false;
  QFileInfo fiSrc(src);
  QFileInfo fiGpkg(surveyGpkgPath);
  if (fiSrc.absoluteFilePath().compare(fiGpkg.absoluteFilePath(), Qt::CaseInsensitive) != 0)
    return false;
  const QStringList domainKeys = {
    QStringLiteral("survey_area"), QStringLiteral("feature_poly"),
    QStringLiteral("feature_line"), QStringLiteral("section_line"),
    QStringLiteral("control_points"), QStringLiteral("trial_trench"),
    QStringLiteral("artifact_point")
  };
  return domainKeys.contains(key);
}
#endif

void MainWindow::renameSelectedLayer(QgsMapLayer* targetLayer) {
#if KA_HGIS_HAS_QGIS
  QString currentName;
  QgsMapLayer* mapLayer = targetLayer;
  if (!mapLayer && m_layerTree) {
    mapLayer = m_layerTree->currentLayer();
  }
  QgsLayerTreeNode* node = m_layerTree ? m_layerTree->currentNode() : nullptr;

  if (mapLayer) {
    currentName = mapLayer->name();
  } else if (node && QgsLayerTree::isGroup(node)) {
    currentName = node->name();
  } else {
    QMessageBox::information(this, QStringLiteral("이름 바꾸기"),
                             QStringLiteral("이름을 바꿀 레이어 또는 그룹을 선택하세요."));
    return;
  }

  bool ok = false;
  const QString name = QInputDialog::getText(
      this, QStringLiteral("이름 바꾸기"), QStringLiteral("새 이름:"),
      QLineEdit::Normal, currentName, &ok);
  if (!ok) return;
  const QString trimmed = name.trimmed();
  if (trimmed.isEmpty()) {
    QMessageBox::warning(this, QStringLiteral("이름 바꾸기"),
                         QStringLiteral("이름은 비울 수 없습니다."));
    return;
  }
  if (trimmed == currentName) return;

  if (mapLayer)
    mapLayer->setName(trimmed);
  else if (node)
    node->setName(trimmed);

  if (m_canvas) m_canvas->refresh();
  if (m_drawingStudio) m_drawingStudio->refreshMapFromProject();
  statusBar()->showMessage(QStringLiteral("이름 변경: %1 → %2").arg(currentName, trimmed), 5000);
#endif
}

void MainWindow::onLayerTreeDoubleClicked(const QModelIndex& index) {
#if KA_HGIS_HAS_QGIS
  if (!m_layerTree || !index.isValid()) return;
  m_layerTree->setCurrentIndex(index);
  m_layerTree->edit(index);
#else
  Q_UNUSED(index);
#endif
}

void MainWindow::importDemElevationRaster() {
#if KA_HGIS_HAS_QGIS
  if (!m_canvas) return;
  const QString path = QFileDialog::getOpenFileName(
      this, QStringLiteral("국토지리원 DEM"), QString(),
      QStringLiteral("국토지리원 DEM (*.img *.IMG *.tif *.tiff *.TIF *.TIFF)"));
  if (path.isEmpty()) return;
  QString err;
  if (!LayerOps::addDemElevationRaster(QgsProject::instance(), m_canvas, path, &err)) {
    notify(Notice::Warning, QStringLiteral("DEM"),
           err.isEmpty() ? QStringLiteral("DEM 파일을 열지 못했습니다.") : err);
    if (m_btnDem) m_btnDem->setChecked(false);
    return;
  }
  if (m_btnDem) m_btnDem->setChecked(true);
  statusBar()->showMessage(QStringLiteral("국토지리원 DEM을 올렸습니다. 범례에 높이(m)가 표시됩니다."), 6000);
#else
  QMessageBox::information(this, QStringLiteral("스텁"), QStringLiteral("DEM 파일 불러오기"));
#endif
}

void MainWindow::runDemHillshade() {
#if KA_HGIS_HAS_QGIS
  const QString dem = QFileDialog::getOpenFileName(
      this, QStringLiteral("DEM GeoTIFF"), QString(),
      QStringLiteral("GeoTIFF (*.tif *.tiff *.TIF *.TIFF)"));
  if (dem.isEmpty())
    return;
  QDialog dlg(this);
  dlg.setWindowTitle(QStringLiteral("지형분석"));
  auto* form = new QFormLayout(&dlg);
  auto* mode = new QComboBox(&dlg);
  mode->addItem(QStringLiteral("다중광원 음영 (유구·분묘)"), static_cast<int>(DemAnalyzer::HillshadeMode::Multi));
  mode->addItem(QStringLiteral("단방향 음영"), static_cast<int>(DemAnalyzer::HillshadeMode::Single));
  auto* az = new QDoubleSpinBox(&dlg);
  az->setRange(0, 360);
  az->setValue(315);
  az->setSuffix(QStringLiteral(" °"));
  auto* alt = new QDoubleSpinBox(&dlg);
  alt->setRange(1, 90);
  alt->setValue(45);
  alt->setSuffix(QStringLiteral(" °"));
  auto* zf = new QDoubleSpinBox(&dlg);
  zf->setRange(0.01, 50);
  zf->setValue(1.0);
  form->addRow(QStringLiteral("방식"), mode);
  form->addRow(QStringLiteral("방위각"), az);
  form->addRow(QStringLiteral("고도각"), alt);
  form->addRow(QStringLiteral("Z계수"), zf);
  auto* box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
  form->addRow(box);
  connect(box, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
  connect(box, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
  if (dlg.exec() != QDialog::Accepted)
    return;
  DemAnalyzer::Options opt;
  opt.hillshade = static_cast<DemAnalyzer::HillshadeMode>(mode->currentData().toInt());
  opt.azimuthDeg = az->value();
  opt.altitudeDeg = alt->value();
  opt.zFactor = zf->value();
  const QString out = QFileInfo(dem).completeBaseName().isEmpty()
                          ? (dem + QStringLiteral("_hillshade.tif"))
                          : (QFileInfo(dem).absolutePath() + QLatin1Char('/') +
                             QFileInfo(dem).completeBaseName() + QStringLiteral("_hillshade.tif"));
  QString err;
  if (!DemAnalyzer::runHillshadeFile(dem, out, opt, &err)) {
    notify(Notice::Warning, QStringLiteral("지형분석"),
           QStringLiteral("음영기복을 만들지 못했습니다."), err);
    return;
  }
  if (!addRasterFromPath(out)) {
    notify(Notice::Warning, QStringLiteral("지형분석"),
           QStringLiteral("음영 래스터를 맵에 올리지 못했습니다."), out);
    return;
  }
  if (auto* rl = qobject_cast<QgsRasterLayer*>(QgsProject::instance()->mapLayersByName(
          QFileInfo(out).completeBaseName()).value(0, nullptr))) {
    LayerOps::markReferenceLayer(rl);
    rl->setName(QStringLiteral("음영기복"));
    LayerOps::placeInLegendGroup(QgsProject::instance(), rl, QStringLiteral("참조 지도"));
  }
  LayerOps::syncMapCanvas(QgsProject::instance(), m_canvas, false);
  statusBar()->showMessage(QStringLiteral("음영기복을 참조 지도에 올렸습니다."), 6000);
#endif
}

namespace {
TrenchGridGenerator::Spec g_pendingTrench;

#if KA_HGIS_HAS_QGIS
TrenchGridGenerator::PickedArea trenchFillFromSurveyLayer(QgsVectorLayer* areaVl, bool useAll = false) {
  TrenchGridGenerator::PickedArea empty;
  if (!areaVl)
    return empty;
  std::vector<TrenchGridGenerator::SurveyPoly> feats;
  QgsFeature f;
  QgsFeatureIterator it = areaVl->getFeatures();
  while (it.nextFeature(f)) {
    if (!f.hasGeometry() || f.geometry().isEmpty())
      continue;
    feats.push_back({f.geometry().asWkb(), f.id()});
  }
  std::vector<qint64> selected;
  const QgsFeatureIds ids = areaVl->selectedFeatureIds();
  selected.reserve(static_cast<size_t>(ids.size()));
  for (QgsFeatureId id : ids)
    selected.push_back(id);
  return TrenchGridGenerator::pickAutoFillArea(feats, selected, useAll);
}

QString leftoverSurveyAreaHint(const TrenchGridGenerator::PickedArea& pick) {
  if (pick.totalCount <= pick.usedCount || pick.usedCount <= 0)
    return {};
  if (pick.usedSelection) {
    return QStringLiteral("조사구역 %1곳 중 선택한 %2곳에만 시굴격자를 깝니다.")
        .arg(pick.totalCount)
        .arg(pick.usedCount);
  }
  return QStringLiteral(
             "조사구역 %1곳이 남아 있어 마지막에 그린 구역에만 깝니다. "
             "예전 구역을 쓰려면 그 구역을 선택한 뒤 다시 깔으세요.")
      .arg(pick.totalCount);
}
#endif
}

void MainWindow::startTrenchGrid() {
#if KA_HGIS_HAS_QGIS
  if (!m_canvas)
    return;
  if (m_surveyPath.isEmpty()) {
    notify(Notice::Info, QStringLiteral("시굴격자"),
           QStringLiteral("먼저 「새 조사」로 GPKG를 만드세요."));
    return;
  }
  // 시굴조사 도메인: 선택한(없으면 마지막) 조사구역만 규칙 배치로 덮고,
  // 총 굴착 면적이 그 구역 면적의 규정 비율(시굴 10%, 표본 2%)인지 확인한다.
  // 남은 옛 조사구역을 union 하면 격자가 그 큰 구역에 깔린다.
  QByteArray areaWkb;
  double areaM2 = 0.0;
  QString areaCrs;
  if (auto* areaVl = LayerOps::findByLayerKey(QgsProject::instance(), QStringLiteral("survey_area"))) {
    const TrenchGridGenerator::PickedArea pick = trenchFillFromSurveyLayer(areaVl);
    areaWkb = pick.wkb;
    areaM2 = pick.areaM2;
    areaCrs = areaVl->crs().authid();
    const QString leftover = leftoverSurveyAreaHint(pick);
    if (!leftover.isEmpty())
      statusBar()->showMessage(leftover, 8000);
  }

  // 세밀 설정(회전 포함)은 하나의 속성 창에서: 모덜리스라 맵을 보면서 조정한다.
  if (!m_trenchDlg) {
    m_trenchDlg = new KaTrenchDialog(this);
    connect(m_trenchDlg, &KaTrenchDialog::applyRequested, this,
            &MainWindow::applyTrenchFromDialog);
    connect(m_trenchDlg, &KaTrenchDialog::manualPlaceRequested, this,
            &MainWindow::beginTrenchOriginPick);
    connect(m_trenchDlg, &KaTrenchDialog::editSingleRequested, this,
            &MainWindow::startTrenchGridEdit);
    connect(m_trenchDlg, &KaTrenchDialog::moveRequested, this,
            &MainWindow::startTrenchGridMove);
  }
  m_trenchDlg->setArea(areaWkb, areaM2);
  m_trenchDlg->setTerrainAspect(terrainAspectForArea(areaWkb, areaCrs));
  m_trenchDlg->show();
  m_trenchDlg->raise();
  if (m_trenchDlg->autoFill()) {
    applyTrenchFromDialog();
  } else {
    beginTrenchOriginPick();
  }
#endif
}

// 조사구역 안에서 DEM 표고를 격자로 뽑아 오르막 방위를 낸다.
// DEM이 없거나 평지면 valid=false — 그때는 방위 칸 값을 그대로 쓴다.
TrenchGridGenerator::SlopeAspect MainWindow::terrainAspectForArea(const QByteArray& areaWkb,
                                                                 const QString& areaCrs) {
#if KA_HGIS_HAS_QGIS
  TrenchGridGenerator::SlopeAspect none;
  if (areaWkb.isEmpty() || !m_canvas) return none;
  QgsProject* proj = QgsProject::instance();
  if (!proj) return none;

  QgsRasterLayer* dem = nullptr;
  for (QgsMapLayer* l : proj->mapLayers()) {
    if (auto* rl = qobject_cast<QgsRasterLayer*>(l)) {
      if (rl->name() == QLatin1String("DEM")) { dem = rl; break; }
    }
  }
  if (!dem || !dem->dataProvider()) return none;

  QgsGeometry area;
  area.fromWkb(areaWkb);
  if (area.isNull() || area.isEmpty()) return none;
  const QgsRectangle env = area.boundingBox();
  if (env.isEmpty()) return none;

  const QgsCoordinateReferenceSystem workCrs(areaCrs.isEmpty() ? m_workCrs : areaCrs);
  QgsCoordinateTransform toDem(workCrs, dem->crs(), proj);
  std::vector<TrenchGridGenerator::ElevSample> samples;
  const int kSteps = 14;  // 14×14 표본이면 사면 방향은 충분히 안정적이다.
  for (int i = 0; i <= kSteps; ++i) {
    for (int j = 0; j <= kSteps; ++j) {
      const double x = env.xMinimum() + env.width() * i / kSteps;
      const double y = env.yMinimum() + env.height() * j / kSteps;
      if (!area.contains(x, y)) continue;  // 구역 밖 표고는 안 쓴다
      QgsPointXY inDem(x, y);
      try {
        inDem = toDem.transform(QgsPointXY(x, y));
      } catch (const QgsException&) {
        return none;
      }
      bool ok = false;
      const double z = dem->dataProvider()->sample(inDem, 1, &ok);
      if (!ok || std::isnan(z)) continue;
      samples.push_back({x, y, z});
    }
  }
  return TrenchGridGenerator::upslopeAspect(samples);
#else
  Q_UNUSED(areaWkb);
  Q_UNUSED(areaCrs);
  return {};
#endif
}

bool MainWindow::applyTrenchFromDialog() {
#if KA_HGIS_HAS_QGIS
  if (!m_trenchDlg)
    return false;
  const TrenchGridGenerator::Spec sp = m_trenchDlg->spec();
  if (m_trenchDlg->autoFill()) {
    QString areaCrs;
    if (auto* areaVl = LayerOps::findByLayerKey(QgsProject::instance(), QStringLiteral("survey_area"))) {
      const TrenchGridGenerator::PickedArea pick = trenchFillFromSurveyLayer(areaVl);
      m_trenchDlg->setArea(pick.wkb, pick.areaM2);
      areaCrs = areaVl->crs().authid();
      const QString leftover = leftoverSurveyAreaHint(pick);
      if (!leftover.isEmpty())
        statusBar()->showMessage(leftover, 8000);
    }
    // 시굴 10% · 표본 2%는 길이·둑을 프로그램이 맞춘다. 「직접 지정」만 사용자 규격.
    const double target = m_trenchDlg->targetPct();
    std::vector<TrenchGridGenerator::Cell> cells;
    if (target > 0.0) {
      const auto plan = TrenchGridGenerator::buildForTargetRatio(
          m_trenchDlg->areaWkb(), target, 2.0, sp.azimuthDeg);
      if (plan.cells.empty()) {
        notify(Notice::Warning, QStringLiteral("시굴격자"), plan.error);
        return false;
      }
      cells = plan.cells;
    } else {
      cells = TrenchGridGenerator::buildInArea(sp, m_trenchDlg->areaWkb());
    }
    if (cells.empty()) {
      notify(Notice::Warning, QStringLiteral("시굴격자"),
             QStringLiteral("현재 규격과 방향으로 구역 안에 격자를 배치하지 못했습니다. 회전이나 규격을 바꿔 다시 적용하세요. 기존 격자는 유지됩니다."));
      return false;
    }
    if (!applyTrenchCells(cells, m_trenchDlg->areaM2(), target, areaCrs))
      return false;
    // 깔자마자 마우스로 하나씩 옮길 수 있어야 한다(회전·재배치 뒤도 같다).
    activateTrenchTool(true);
    return true;
  }
  // 수동 모드: 격자가 이미 있으면 중심을 고정한 채 회전·간격만 바꿔 재배치한다.
  auto* vl = LayerOps::findByLayerKey(QgsProject::instance(), QStringLiteral("trial_trench"));
  if (vl && vl->featureCount() > 0) {
    TrenchGridGenerator::Spec centered = sp;
    centered.originX = 0.0;
    centered.originY = 0.0;
    auto cells = TrenchGridGenerator::build(centered);
    if (cells.empty()) {
      notify(Notice::Warning, QStringLiteral("시굴격자"),
             QStringLiteral("격자를 계산하지 못했습니다. 간격과 행·열을 확인하세요."));
      return false;
    }
    double minx = 1e300, miny = 1e300, maxx = -1e300, maxy = -1e300;
    for (const auto& c : cells) {
      for (const auto& pt : c.ring) {
        minx = std::min(minx, pt.first);
        maxx = std::max(maxx, pt.first);
        miny = std::min(miny, pt.second);
        maxy = std::max(maxy, pt.second);
      }
    }
    const QgsPointXY keep = vl->extent().center();
    const double dx = keep.x() - (minx + maxx) * 0.5;
    const double dy = keep.y() - (miny + maxy) * 0.5;
    for (auto& c : cells) {
      for (auto& pt : c.ring) {
        pt.first += dx;
        pt.second += dy;
      }
    }
    return applyTrenchCells(cells, m_trenchDlg->areaM2(), 0.0, vl->crs().authid());
  }
  beginTrenchOriginPick();
  return false;
#else
  return false;
#endif
}

void MainWindow::beginTrenchOriginPick() {
#if KA_HGIS_HAS_QGIS
  if (!m_canvas || !m_trenchDlg)
    return;
  g_pendingTrench = m_trenchDlg->spec();
  if (!m_trenchOriginTool) {
    m_trenchOriginTool = new QgsMapToolEmitPoint(m_canvas);
    m_trenchOriginTool->setParent(this);
    connect(m_trenchOriginTool, &QgsMapToolEmitPoint::canvasClicked, this,
            [this](const QgsPointXY& pt, Qt::MouseButton btn) {
              if (btn != Qt::LeftButton)
                return;
              placeTrenchGridAt(pt);
            });
  }
  m_canvas->setMapTool(m_trenchOriginTool);
  statusBar()->showMessage(QStringLiteral("시굴격자: 원점을 맵에서 클릭하세요."), 0);
#endif
}

void MainWindow::placeTrenchGridAt(const QgsPointXY& origin) {
#if KA_HGIS_HAS_QGIS
  g_pendingTrench.originX = origin.x();
  g_pendingTrench.originY = origin.y();
  const auto cells = TrenchGridGenerator::build(g_pendingTrench);
  if (cells.empty()) {
    notify(Notice::Warning, QStringLiteral("시굴격자"),
           QStringLiteral("격자를 계산하지 못했습니다. 간격과 범위를 확인하세요."));
    return;
  }
  applyTrenchCells(cells, m_trenchDlg ? m_trenchDlg->areaM2() : 0.0);
#endif
}

void MainWindow::applyTrenchByRatio(double targetPct) {
#if KA_HGIS_HAS_QGIS
  if (m_surveyPath.isEmpty()) {
    notify(Notice::Info, QStringLiteral("시굴격자"),
           QStringLiteral("먼저 「새 조사」로 GPKG를 만드세요."));
    return;
  }
  QgsVectorLayer* areaVl = LayerOps::findByLayerKey(QgsProject::instance(),
                                                    QStringLiteral("survey_area"));
  if (m_layerTree) {
    if (auto* cur = qobject_cast<QgsVectorLayer*>(m_layerTree->currentLayer())) {
      if (LayerOps::layerKeyOf(cur) == QLatin1String("survey_area"))
        areaVl = cur;
    }
  }
  if (!areaVl || areaVl->featureCount() <= 0) {
    notify(Notice::Info, QStringLiteral("시굴격자"),
           QStringLiteral("먼저 조사구역을 그린 뒤 그 레이어에서 우클릭하세요."));
    return;
  }
  TrenchGridGenerator::PickedArea pick = trenchFillFromSurveyLayer(areaVl);
  // 조사구역을 여러 조각으로 그렸으면, 예전에는 마지막 조각에만 깔렸다. 그래서
  // 사용자가 시굴격자를 쓰려고 굳이 「폴리곤 묶기」를 먼저 해야 했다. 이제 묻는다.
  // 기본값을 전체로 바꾸지는 않는다 — 지난 조사의 구역이 남아 있으면 격자가
  // 수백 칸으로 불어나기 때문이다(tests/test_dem_trench.cpp 에 그 이유가 있다).
  if (!pick.usedSelection && pick.totalCount > 1) {
    QMessageBox box(this);
    box.setWindowTitle(QStringLiteral("시굴격자"));
    box.setText(QStringLiteral("조사구역이 %1곳입니다. 어디에 깔까요?").arg(pick.totalCount));
    box.setInformativeText(
        QStringLiteral("「전체」를 고르면 %1곳을 합친 면적으로 비율을 계산해 모든 구역에 깝니다.\n"
                       "예전 조사의 구역이 남아 있다면 「마지막 구역만」을 고르세요.")
            .arg(pick.totalCount));
    QPushButton* all = box.addButton(QStringLiteral("전체 %1곳").arg(pick.totalCount),
                                     QMessageBox::AcceptRole);
    QPushButton* last = box.addButton(QStringLiteral("마지막 구역만"), QMessageBox::RejectRole);
    box.addButton(QStringLiteral("취소"), QMessageBox::DestructiveRole);
    box.setDefaultButton(all);
    box.exec();
    if (box.clickedButton() == all)
      pick = trenchFillFromSurveyLayer(areaVl, true);
    else if (box.clickedButton() != last)
      return;  // 취소
  }
  const QString leftover = leftoverSurveyAreaHint(pick);
  if (!leftover.isEmpty())
    statusBar()->showMessage(leftover, 8000);
  if (pick.wkb.isEmpty() || pick.areaM2 <= 0.0) {
    notify(Notice::Warning, QStringLiteral("시굴격자"),
           QStringLiteral("조사구역 면을 찾지 못했습니다."));
    return;
  }
  const TrenchGridGenerator::RatioFill plan =
      TrenchGridGenerator::buildForTargetRatio(pick.wkb, targetPct, 2.0);
  if (plan.cells.empty()) {
    notify(Notice::Warning, QStringLiteral("시굴격자"), plan.error);
    return;
  }
  applyTrenchCells(plan.cells, pick.areaM2, targetPct, areaVl->crs().authid());
#else
  Q_UNUSED(targetPct);
#endif
}

bool MainWindow::applyTrenchCells(const std::vector<TrenchGridGenerator::Cell>& cells,
                                  double areaM2, double targetPct, const QString& sourceCrs) {
#if KA_HGIS_HAS_QGIS
  QString err;
  if (cells.empty() || (targetPct > 0.0 &&
      (!(areaM2 > 0.0) || !std::isfinite(areaM2) ||
       std::abs(TrenchGridGenerator::totalArea(cells) - areaM2 * targetPct / 100.0) >
           std::max(1e-6, areaM2 * 1e-8)))) {
    notify(Notice::Warning, QStringLiteral("시굴격자"),
           QStringLiteral("목표 면적에 맞는 격자를 계산하지 못했습니다. 방향과 규격을 확인해 다시 적용하세요. 기존 격자는 유지됩니다."));
    return false;
  }
  if (auto* existing = LayerOps::findByLayerKey(QgsProject::instance(), QStringLiteral("trial_trench"));
      existing && existing->isModified()) {
    notify(Notice::Warning, QStringLiteral("시굴격자"),
           QStringLiteral("시굴격자에 저장하지 않은 편집이 있습니다. 먼저 저장한 뒤 다시 만드세요. 현재 편집은 그대로 유지됩니다."));
    return false;
  }
  // Auto-fill cells are in the survey layer CRS, even if the canvas has since
  // changed work CRS. Preserve that identity; QGIS transforms them for display.
  const QString auth = !sourceCrs.isEmpty() ? sourceCrs
                       : QgsProject::instance() && QgsProject::instance()->crs().isValid()
                           ? QgsProject::instance()->crs().authid()
                           : QStringLiteral("EPSG:5186");
  if (!TrenchGridGenerator::writeGpkg(m_surveyPath, QStringLiteral("trial_trench"), cells, auth, &err)) {
    notify(Notice::Warning, QStringLiteral("시굴격자"),
           QStringLiteral("격자를 조사 파일에 저장하지 못했습니다. 저장 위치와 파일 사용 상태를 확인하고 다시 적용하세요."), err);
    return false;
  }
  auto* vl = ensureDomainLayerForEdit(QStringLiteral("trial_trench"), QStringLiteral("시굴격자"));
  if (!vl) {
    notify(Notice::Warning, QStringLiteral("시굴격자"),
           QStringLiteral("격자는 파일에 저장했지만 지도에 불러오지 못했습니다. 조사를 다시 열어 확인하세요."));
    return false;
  }
  vl->dataProvider()->reloadData();
  vl->updateExtents();
  vl->triggerRepaint();
  LayerOps::syncMapCanvas(QgsProject::instance(), m_canvas, false);
  const double t = TrenchGridGenerator::totalArea(cells);
  QString msg = QStringLiteral("시굴격자 %1개 · 총 %2㎡")
                    .arg(cells.size())
                    .arg(QLocale().toString(t, 'f', 2));
  if (areaM2 > 0.0) {
    const double pct = t / areaM2 * 100.0;
    const QString kind = (targetPct > 0.0 && targetPct < 5.0)
                             ? QStringLiteral("표본 기준 2%")
                             : QStringLiteral("시굴 기준 10%");
    msg += QStringLiteral(" · 조사구역의 %1% (%2)")
               .arg(QLocale().toString(pct, 'f', 1), kind);
  }
  msg += QStringLiteral(" — 격자를 끌어 옮기세요. 우클릭 = 개별 삭제");
  statusBar()->showMessage(msg, 0);
  notify(Notice::Success, QStringLiteral("시굴격자"), msg);
  startTrenchGridMove();
  return true;
#else
  Q_UNUSED(cells);
  Q_UNUSED(areaM2);
  Q_UNUSED(targetPct);
  Q_UNUSED(sourceCrs);
  return false;
#endif
}

void MainWindow::startTrenchGridMove() {
  activateTrenchTool(false);
}

void MainWindow::startTrenchGridEdit() {
  activateTrenchTool(true);
}

void MainWindow::activateTrenchTool(bool single) {
#if KA_HGIS_HAS_QGIS
  auto* vl = LayerOps::findByLayerKey(QgsProject::instance(), QStringLiteral("trial_trench"));
  if (!vl || vl->featureCount() <= 0) {
    notify(Notice::Info, QStringLiteral("시굴격자"), QStringLiteral("먼저 시굴격자를 만드세요."));
    return;
  }
  if (!m_trenchMoveTool) {
    m_trenchMoveTool = new KaTrenchMoveTool(m_canvas);
    m_trenchMoveTool->setParent(this);
    connect(m_trenchMoveTool, &KaTrenchMoveTool::statusMessage, this, [this](const QString& t) {
      statusBar()->showMessage(t, 6000);
    });
  }
  m_trenchMoveTool->setLayer(vl);
  double snapM = 0.0;
  if (m_mapGrid && m_mapGrid->isEnabled())
    snapM = m_mapGrid->stepMeters();
  else if (m_mapGridStep)
    snapM = m_mapGridStep->value();
  m_trenchMoveTool->setSnapMeters(snapM);
  m_trenchMoveTool->setGridOverlay(m_mapGrid);
  m_trenchMoveTool->setMode(single ? KaTrenchMoveTool::Mode::Single
                                   : KaTrenchMoveTool::Mode::Whole);
  m_canvas->setMapTool(m_trenchMoveTool);
  m_canvas->setFocus(Qt::OtherFocusReason);
#else
  Q_UNUSED(single);
#endif
}

void MainWindow::toggleMapGrid() {
#if KA_HGIS_HAS_QGIS
  if (!m_canvas)
    return;
  if (!m_mapGrid)
    m_mapGrid = new KaCanvasGridOverlay(m_canvas);
  KaCanvasGridOverlay::Config cfg = m_mapGrid->config();
  cfg.enabled = m_mapGridCheck && m_mapGridCheck->isChecked();
  cfg.stepMeters = m_mapGridStep ? m_mapGridStep->value() : 20.0;
  cfg.rotationDeg = m_mapGridRot ? m_mapGridRot->value() : 0.0;
  cfg.lineWidth = m_mapGridWidth ? m_mapGridWidth->value() : 1.2;
  cfg.penStyle = m_mapGridDash
                     ? static_cast<Qt::PenStyle>(m_mapGridDash->currentData().toInt())
                     : Qt::DashLine;
  if (m_mapGridColor.isValid())
    cfg.color = m_mapGridColor;
  cfg.type = (QApplication::keyboardModifiers() & Qt::ShiftModifier)
                 ? KaCanvasGridOverlay::Type::GeographicDms
                 : KaCanvasGridOverlay::Type::ProjectedMeters;
  m_mapGrid->setConfig(cfg);
  m_mapGrid->setEnabled(cfg.enabled);
  m_canvas->refresh();
  statusBar()->showMessage(
      cfg.enabled
          ? (cfg.type == KaCanvasGridOverlay::Type::GeographicDms
                 ? QStringLiteral("경위도 격자를 켰습니다.")
                 : QStringLiteral("미터 좌표 격자를 켰습니다."))
          : QStringLiteral("좌표 격자를 껐습니다."),
      4000);
#endif
}

void MainWindow::setMapGridColor(const QColor& color) {
  if (!color.isValid())
    return;
  m_mapGridColor = color;
  syncMapGridColorButtons();
  // 색만 바꿔도 바로 보이게 한다. 격자가 꺼져 있으면 켜지지 않고 값만 남는다.
  if (m_mapGridCheck && m_mapGridCheck->isChecked())
    toggleMapGrid();
}

void MainWindow::syncMapGridColorButtons() {
  for (QToolButton* b : m_mapGridColorBtns) {
    if (!b) continue;
    const QSignalBlocker block(b);
    // 툴팁 색과 현재 색이 같은 단추만 눌린 모양으로 둔다.
    b->setChecked(b->toolTip().startsWith(QStringLiteral("빨간색")) ? m_mapGridColor == QColor(0xD9, 0x2B, 0x2B)
                  : b->toolTip().startsWith(QStringLiteral("파란색")) ? m_mapGridColor == QColor(0x1D, 0x4E, 0xD8)
                  : m_mapGridColor == QColor(0x1F, 0x29, 0x37));
  }
}

bool MainWindow::eventFilter(QObject* watched, QEvent* event) {
#if KA_HGIS_HAS_QGIS
  if (!event) return QMainWindow::eventFilter(watched, event);
  if (watched == m_leftSplit && event->type() == QEvent::Resize) {
    KaLayerInformationView::protectSidebarList(
        m_leftSplit, m_layerTree, findChild<QToolButton*>(QStringLiteral("sidebarFilesToggle")),
        findChild<QWidget*>(QStringLiteral("sidebarFilesScroll")),
        findChild<KaLayerInformationPanel*>(QStringLiteral("layerInformationPanel")));
  }
  if (m_mapSplitter && watched == m_mapSplitter && event->type() == QEvent::Resize)
    refreshAlignUi();
  if (m_subToolsMode == QLatin1String("align") &&
      (event->type() == QEvent::Wheel || event->type() == QEvent::Resize)) {
    const bool onAlignView =
        (m_alignImage && (watched == m_alignImage || watched == m_alignImage->viewport())) ||
        (m_alignLeftCanvas &&
         (watched == m_alignLeftCanvas || watched == m_alignLeftCanvas->viewport())) ||
        (m_canvas && (watched == m_canvas || watched == m_canvas->viewport()));
    if (onAlignView)
      QTimer::singleShot(0, this, [this]() { updateAlignOverlay(); });
  }
  if (m_subToolsMode == QLatin1String("align") && event->type() == QEvent::MouseMove) {
    if (auto* me = static_cast<QMouseEvent*>(event)) {
      QWidget* w = qobject_cast<QWidget*>(watched);
      if (w) trackAlignPointer(w->mapToGlobal(me->pos()));
    }
  }

  const bool onCanvas = m_canvas &&
      (watched == m_canvas || watched == m_canvas->viewport());
  if (onCanvas) {
    const QEvent::Type t = event->type();
    if (t == QEvent::Resize || t == QEvent::Show
#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
        || t == QEvent::DevicePixelRatioChange
#endif
    ) {
      // 그리는 중에 outputSize·DPR을 바꾸면, 끝난 그림이 바뀐 격자에 안 맞아
      // 전체 화면에서 지도가 빈다. QGIS가 리사이즈 500ms 뒤에 스스로 다시 그린다.
      // 여기서는 그 그림이 끝난 뒤에, 크기가 아직 어긋날 때만 한 번 더 맞춘다.
      scheduleMapDisplayRefresh();
      if (m_subToolsMode == QLatin1String("align"))
        QTimer::singleShot(0, this, [this]() { updateAlignOverlay(); });
    }
    if (t == QEvent::Resize && !m_startupViewApplied)
      QTimer::singleShot(0, this, [this]() { ensureStartupViewReady(); });
  }
  if (onCanvas && event->type() == QEvent::KeyPress) {
    auto* ke = static_cast<QKeyEvent*>(event);
    if (ke && (ke->matches(QKeySequence::Undo) ||
               ((ke->modifiers() & Qt::ControlModifier) && ke->key() == Qt::Key_Z))) {
      undoLastAction();
      return true;
    }
    if (ke && (ke->matches(QKeySequence::Redo) ||
               ((ke->modifiers() & Qt::ControlModifier) && ke->key() == Qt::Key_Y))) {
      redoLastAction();
      return true;
    }
  }
  if (onCanvas && event->type() == QEvent::MouseButtonDblClick) {
    const bool capturing = m_captureTool && m_canvas->mapTool() == m_captureTool;
    const bool measuring = m_measureTool && m_canvas->mapTool() == m_measureTool;
    if (!capturing && !measuring) {
      auto* me = static_cast<QMouseEvent*>(event);
      if (me && me->button() == Qt::LeftButton) {
        ensureAttributeTool();
        if (m_attributeTool) {
          QgsVectorLayer* layer = nullptr;
          QgsFeature feat;
          if (m_attributeTool->pickAtScreen(me->pos(), &layer, &feat) && layer &&
              !LayerOps::isReferenceLayer(layer) && !LayerOps::isCadastralLayer(layer)) {
            if (m_layerTree) m_layerTree->setCurrentLayer(layer);
            editCurrentLayerStyle();
            return true;
          }
        }
      }
    }
  }

  const bool fromLayerTree = m_layerTree &&
      (event->type() == QEvent::DragEnter || event->type() == QEvent::DragMove ||
       event->type() == QEvent::Drop) &&
      (static_cast<QDropEvent*>(event)->source() == m_layerTree ||
       static_cast<QDropEvent*>(event)->source() == m_layerTree->viewport());
  const bool onLayerDrop = !fromLayerTree &&
      ((m_layerTree &&
        (watched == m_layerTree || watched == m_layerTree->viewport())) ||
       (m_canvas && (watched == m_canvas || watched == m_canvas->viewport())));
  if (onLayerDrop) {
    if (event->type() == QEvent::DragEnter || event->type() == QEvent::DragMove) {
      auto* de = static_cast<QDragEnterEvent*>(event);
      const bool fromBrowser = de->source() == m_fileBrowser ||
                               (m_fileBrowser && de->source() == m_fileBrowser->viewport());
      if ((de->mimeData() && de->mimeData()->hasUrls()) || fromBrowser) {
        de->acceptProposedAction();
        return true;
      }
    }
    if (event->type() == QEvent::Drop) {
      auto* de = static_cast<QDropEvent*>(event);
      bool ok = false;
      if (de->mimeData() && de->mimeData()->hasUrls())
        ok = tryAddDroppedUrls(de->mimeData()->urls());
      if (!ok)
        ok = tryAddDroppedPaths(selectedBrowserFiles());
      if (ok) {
        de->acceptProposedAction();
        return true;
      }
    }
    if (event->type() == QEvent::KeyPress) {
      auto* ke = static_cast<QKeyEvent*>(event);
      const bool measuring = m_measureTool && m_canvas && m_canvas->mapTool() == m_measureTool;
      if (!measuring && ke->key() == Qt::Key_Delete) {
        if (onCanvas) {
          if (m_captureTool && m_canvas->mapTool() == m_captureTool) return false;
          deleteFeaturesOrSelectedReferenceLayers();
        } else removeSelectedLayers();
        return true;
      }
      if (!onCanvas && ke->key() == Qt::Key_F2) {
        renameSelectedLayer();
        return true;
      }
    }
  }
#else
  Q_UNUSED(watched);
  Q_UNUSED(event);
#endif
  return QMainWindow::eventFilter(watched, event);
}

// 「그린 도형 모두 지우기」 — 레이어는 그대로 두고 GPKG의 도형만 비운다.
// 조사구역을 새로 그리려는 현장 흐름의 최단 경로다.
// 원격 XYZ 레이어의 주소 틀을 꺼낸다. QGIS 소스는
// "type=xyz&url=<퍼센트인코딩된 주소>&zmax=..." 꼴이다.
static QString kaXyzUrlTemplate(const QgsRasterLayer* rl) {
  if (!rl) return QString();
  const QString src = rl->source();
  if (!src.contains(QLatin1String("type=xyz"))) return QString();
  for (const QString& part : src.split(QLatin1Char('&'))) {
    if (!part.startsWith(QLatin1String("url="))) continue;
    return QUrl::fromPercentEncoding(part.mid(4).toUtf8());
  }
  return QString();
}

void MainWindow::saveOfflineTilePack() {
#if KA_HGIS_HAS_QGIS
  auto* rl = m_layerTree ? qobject_cast<QgsRasterLayer*>(m_layerTree->currentLayer()) : nullptr;
  const QString tmpl = kaXyzUrlTemplate(rl);
  if (tmpl.isEmpty()) {
    notify(Notice::Info, QStringLiteral("오프라인 저장"),
           QStringLiteral("위성처럼 인터넷에서 받아 오는 배경 레이어를 먼저 고르세요."));
    return;
  }
  if (!m_canvas) return;

  // 화면 범위를 웹메르카토르로 옮긴다. 타일은 3857로만 잘려 있다.
  QgsRectangle ext = m_canvas->extent();
  const QgsCoordinateReferenceSystem web(QStringLiteral("EPSG:3857"));
  const QgsCoordinateReferenceSystem cur = m_canvas->mapSettings().destinationCrs();
  if (cur.isValid() && cur != web) {
    try {
      QgsCoordinateTransform tr(cur, web, QgsProject::instance());
      tr.setBallparkTransformsAreAppropriate(true);
      ext = tr.transformBoundingBox(ext);
    } catch (const QgsException&) {
      notify(Notice::Critical, QStringLiteral("오프라인 저장"),
             QStringLiteral("화면 범위를 좌표 변환하지 못했습니다."));
      return;
    }
  }

  TilePackService::Options opt;
  opt.urlTemplate = tmpl;
  opt.jpeg = tmpl.contains(QLatin1String(".jpeg")) || tmpl.contains(QLatin1String(".jpg"));
  opt.referer = QStringLiteral("https://localhost");
  // 지금 화면의 해상도에서 한 단계 더 자세한 데까지 받는다.
  const double mupp = qMax(m_canvas->mapUnitsPerPixel(), 1e-6);
  int z = 0;
  while (z < 19 && TilePackService::resolutionAtZoom(z) > mupp) ++z;
  opt.maxZoom = qBound(10, z + 1, 19);
  opt.minZoom = qMax(8, opt.maxZoom - 6);

  const qint64 tiles = TilePackService::tileCount(ext.xMinimum(), ext.yMinimum(), ext.xMaximum(),
                                                  ext.yMaximum(), opt.minZoom, opt.maxZoom);
  if (tiles <= 0) {
    notify(Notice::Warning, QStringLiteral("오프라인 저장"),
           QStringLiteral("범위가 비었습니다. 조사지역으로 확대한 뒤 다시 하세요."));
    return;
  }
  if (tiles > 20000) {
    notify(Notice::Warning, QStringLiteral("오프라인 저장"),
           QStringLiteral("타일 %1장은 너무 많습니다. 조사지역으로 더 확대한 뒤 하세요.")
               .arg(QLocale().toString(tiles)));
    return;
  }
  if (QMessageBox::question(
          this, QStringLiteral("오프라인 저장"),
          QStringLiteral("지금 화면 범위를 타일 %1장(줌 %2~%3)으로 받아 둡니다.\n"
                         "받는 동안 지도 작업을 계속할 수 있습니다. 계속할까요?")
              .arg(QLocale().toString(tiles))
              .arg(opt.minZoom)
              .arg(opt.maxZoom),
          QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes) != QMessageBox::Yes)
    return;

  const QString dir = m_surveyPath.isEmpty() ? QDir::tempPath()
                                             : QFileInfo(m_surveyPath).absolutePath();
  const QString safe = QString(rl->name()).replace(QRegularExpression(QStringLiteral("[^\\w가-힣]")),
                                                   QStringLiteral("_"));
  const QString out = QDir(dir).filePath(QStringLiteral("%1_오프라인.mbtiles").arg(safe));

  const QString packName = QStringLiteral("%1 (오프라인)").arg(rl->name());
  const QPointer<QgsRasterLayer> online(rl);
  startFileDownload(QStringLiteral("오프라인 지도"), [opt, ext, out](QgsFeedback* feedback, const std::function<bool()>& cancelled) {
    PreparedReferenceMap result;
    if (TilePackService::build(opt, ext.xMinimum(), ext.yMinimum(), ext.xMaximum(), ext.yMaximum(), out, &result.error, feedback, cancelled)) {
      result.status = PreparedReferenceMap::Status::Ready;
      result.outputCommitted = true;
      result.rasterUri = out;
    } else if (cancelled()) result.status = PreparedReferenceMap::Status::Cancelled;
    return result;
  }, [this, online, packName](const PreparedReferenceMap& result) {
    QString error;
    if (!LayerOps::addTilePackBasemap(QgsProject::instance(), m_canvas, result.rasterUri, packName, &error)) {
      notify(Notice::Warning, QStringLiteral("오프라인 지도"), QStringLiteral("파일을 저장했지만 지도에 표시하지 못했습니다. 파일함에서 다시 열어 주세요.\n%1\n%2").arg(result.rasterUri, error));
      return;
    }
    if (online) {
      if (auto* node = QgsProject::instance()->layerTreeRoot()->findLayer(online->id())) node->setItemVisibilityChecked(false);
    }
    QgsProject::instance()->setDirty(true);
    notify(Notice::Success, QStringLiteral("오프라인 지도"), QStringLiteral("내려받기를 마쳤습니다. 인터넷 없이 이 범위의 지도를 볼 수 있습니다.\n%1").arg(QDir::toNativeSeparators(result.rasterUri)));
  });
#endif
}

#if KA_HGIS_HAS_QGIS
static void afterBasemapAdded(MainWindow* self, QgsMapCanvas* canvas, const QString& workCrs,
                              const QString& label) {
  if (!self || !canvas) return;
  LayerOps::ensureOtfEnabled(QgsProject::instance(), canvas, workCrs);
  LayerOps::syncMapCanvas(QgsProject::instance(), canvas, false);
  if (canvas->scale() > 80000.0 || canvas->scale() < 100.0)
    canvas->zoomScale(25000.0, true);
  LayerOps::clampCanvasToKorea(canvas);
  LayerOps::refreshXyzBasemapTiles(canvas);
  QString next = QStringLiteral("%1을 올렸습니다. 「그리기」로 구역을 그리세요.").arg(label);
  if (label.contains(QStringLiteral("지적")))
    next = QStringLiteral("지적을 올렸습니다. 가까이 보면 번지가 보입니다.");
  else if (label.contains(QStringLiteral("위성")))
    next = QStringLiteral("위성을 올렸습니다. 「그리기」로 구역을 그리세요.");
  self->statusBar()->showMessage(next, 8000);
}
#endif

void MainWindow::onCanvasScaleChanged(double scale) {
#if KA_HGIS_HAS_QGIS
  if (m_scaleUiGuard || !m_scaleEdit) return;
  if (m_drawingStudio && m_viewTabs && m_viewTabs->currentWidget() == m_drawingStudio)
    scale = m_drawingStudio->drawingScale();
  if (!std::isfinite(scale) || scale <= 0.) return;
  m_scaleUiGuard = true;
  // 축척 칸은 콤보의 입력줄 하나뿐이다. 여기에 현재 축척을 그대로 보여 준다.
  // 예전에는 가까운 프리셋으로 setCurrentIndex 까지 했는데, 그러면 1:1873 을
  // 1:2000 으로 바꿔 적어 화면과 글자가 어긋났다.
  m_scaleEdit->setText(QStringLiteral("1:%1").arg(scale, 0, 'f', 0));
  m_scaleUiGuard = false;
#else
  Q_UNUSED(scale);
#endif
}

// 축척 칸에 적힌 값을 분모로 읽는다. 콤보 프리셋은 "1:2000", 직접 입력은 "2000" 이라
// 두 모양을 다 받아야 한다. 예전에는 "1:2000" 이 숫자로 안 읽혀 조용히 무시됐다.
double MainWindow::scaleDenominatorFromUi(const QString& raw) {
  QString t = raw.trimmed();
  t.remove(QLatin1Char(','));
  t.remove(QLatin1Char(' '));
  if (t.startsWith(QLatin1String("1:")))
    t = t.mid(2);
  else if (t.contains(QLatin1Char(':')))
    t = t.section(QLatin1Char(':'), -1);
  bool ok = false;
  const double v = t.toDouble(&ok);
  return (ok && v > 0.0) ? v : 0.0;
}

void MainWindow::applyMapScaleFromUi() {
#if KA_HGIS_HAS_QGIS
  if (!m_canvas || !m_scaleEdit) return;
  const double s = scaleDenominatorFromUi(m_scaleEdit->text());
  if (s <= 0.0) {
    statusBar()->showMessage(QStringLiteral("축척 숫자를 입력하세요 (예: 1000 → 1:1000)"), 5000);
    return;
  }
  if (m_drawingStudio && m_viewTabs && m_viewTabs->currentWidget() == m_drawingStudio) {
    m_drawingStudio->setDrawingScale(s);
    onCanvasScaleChanged(m_drawingStudio->drawingScale());
    statusBar()->showMessage(QStringLiteral("도면 축척 적용 1:%1")
                                .arg(m_drawingStudio->drawingScale(), 0, 'f', 0), 4000);
    return;
  }
  m_scaleUiGuard = true;
  m_canvas->zoomScale(s, true);
  // 입력을 "1:2000" 한 가지 모양으로 되돌려 놓는다. 프리셋으로 고르든 직접 치든
  // 칸에 남는 글자가 같아야 다음 Enter 가 같은 결과를 낸다.
  m_scaleEdit->setText(QStringLiteral("1:%1").arg(s, 0, 'f', 0));
  LayerOps::refreshCanvasIfIdle(m_canvas);
  m_scaleUiGuard = false;
  statusBar()->showMessage(QStringLiteral("축척 적용 1:%1").arg(s, 0, 'f', 0), 4000);
#endif
}

void MainWindow::refreshMapCanvasNow() {
#if KA_HGIS_HAS_QGIS
  if (m_isOpeningSurvey || !m_canvas) return;
  if (m_canvas->isDrawing()) {
    if (!m_canvasSyncQueued) {
      m_canvasSyncQueued = true;
      QTimer::singleShot(80, this, [this]() {
        m_canvasSyncQueued = false;
        refreshMapCanvasNow();
      });
    }
    return;
  }
  // 이미 열려 있던 조사는 레이어를 넣고 빼는 일이 없어 순서 규칙이 한 번도
  // 안 돌 수 있다. 화면을 새로 그릴 때마다 맞춰 둔다.
  LayerOps::applyLayerOrderToLabels(QgsProject::instance(), nullptr);
  refreshAboveLabelsOverlay();
  LayerOps::syncMapCanvas(QgsProject::instance(), m_canvas, false);
  updateNextActionStatus();
#endif
}

void MainWindow::addBasemapVworld() {
#if KA_HGIS_HAS_QGIS
  const QString key = vworldApiKeyOrPrompt();
  if (key.isEmpty()) return;
  QString err;
  if (!LayerOps::addVworldBaseMap(QgsProject::instance(), m_canvas, key, &err))
    notify(Notice::Warning, QStringLiteral("배경"),
           QStringLiteral("배경지도를 올리지 못했습니다."), err);
  else
    afterBasemapAdded(this, m_canvas, m_workCrs, QStringLiteral("배경"));
#endif
}

void MainWindow::addBasemapVworldSat() {
#if KA_HGIS_HAS_QGIS
  const QString key = VworldSettings::loadApiKey();
  QString err;
  if (!LayerOps::addVworldSatelliteMap(QgsProject::instance(), m_canvas, key, &err))
    notify(Notice::Warning, QStringLiteral("위성"),
           QStringLiteral("위성영상을 올리지 못했습니다."), err);
  else
    afterBasemapAdded(this, m_canvas, m_workCrs, QStringLiteral("위성"));
#endif
}

void MainWindow::addBasemapVworldCadastral() {
#if KA_HGIS_HAS_QGIS
  const QString key = vworldApiKeyOrPrompt();
  if (key.isEmpty()) return;
  LayerOps::clearUserRemovedCadastral(QgsProject::instance());
  QString err;
  if (!LayerOps::addVworldCadastralMap(QgsProject::instance(), m_canvas, key, &err))
    notify(Notice::Warning, QStringLiteral("지적도"),
           QStringLiteral("지적도를 올리지 못했습니다."), err);
  else {
    afterBasemapAdded(this, m_canvas, m_workCrs, QStringLiteral("지적"));
    if (m_canvas && m_canvas->scale() > 8000.0)
      m_canvas->zoomScale(5000.0, true);
  }
#endif
}

void MainWindow::addDaedongyeojidoMap() {
#if KA_HGIS_HAS_QGIS
  QString err;
  if (!LayerOps::addDaedongyeojidoMap(QgsProject::instance(), m_canvas, &err))
    notify(Notice::Warning, QStringLiteral("대동여지도"),
           QStringLiteral("대동여지도를 올리지 못했습니다."), err);
  else
    afterBasemapAdded(this, m_canvas, m_workCrs, QStringLiteral("대동여지도"));
#endif
}

void MainWindow::addHistoryGisMap1919() {
#if KA_HGIS_HAS_QGIS
  const QString key = VworldSettings::loadHistoryGisApiKey();
  QString err;
  if (!LayerOps::addHistoryGisMap1919(QgsProject::instance(), m_canvas, key, &err))
    notify(Notice::Warning, QStringLiteral("1919 조선지형도"),
           QStringLiteral("1919 조선지형도를 올리지 못했습니다."), err);
  else
    afterBasemapAdded(this, m_canvas, m_workCrs, QStringLiteral("1919 조선지형도"));
#endif
}

void MainWindow::addBasemapOsm() {
#if KA_HGIS_HAS_QGIS
  QString err;
  if (!LayerOps::addOsmBasemap(QgsProject::instance(), m_canvas, &err))
    notify(Notice::Warning, QStringLiteral("배경"),
           QStringLiteral("OSM 배경지도를 올리지 못했습니다."), err);
  else
    afterBasemapAdded(this, m_canvas, m_workCrs, QStringLiteral("OSM"));
#endif
}

void MainWindow::addBasemapGoogle() {
#if KA_HGIS_HAS_QGIS
  QString err;
  if (!LayerOps::addKoreaBasemap(QgsProject::instance(), m_canvas, LayerOps::KoreaBasemap::GoogleSatellite, &err))
    notify(Notice::Warning, QStringLiteral("배경"),
           QStringLiteral("위성 배경지도를 올리지 못했습니다."), err);
  else
    afterBasemapAdded(this, m_canvas, m_workCrs, QStringLiteral("위성"));
#endif
}

void MainWindow::onLayerTreeRowsMoved() {
  if (m_isOpeningSurvey || !m_canvas) return;
  LayerOps::ensureSatelliteAtBottom(QgsProject::instance());
  LayerOps::syncMapCanvas(QgsProject::instance(), m_canvas, false);
  LayerOps::refreshCanvasIfIdle(m_canvas);
}

void MainWindow::moveSelectedLayer(int dir) {
#if KA_HGIS_HAS_QGIS
  if (!m_layerTree || !QgsProject::instance()) return;
  QgsLayerTreeNode* node = m_layerTree->currentNode();
  if (!node || node->nodeType() != QgsLayerTreeNode::NodeLayer) return;
  auto* parent = qobject_cast<QgsLayerTreeGroup*>(node->parent());
  if (!parent) parent = QgsProject::instance()->layerTreeRoot();
  if (!parent) return;
  const int idx = parent->children().indexOf(node);
  if (idx < 0) return;
  const int dest = idx + dir;
  if (dest < 0 || dest >= parent->children().size()) return;
  QPointer<QgsMapLayer> layer = QgsLayerTree::toLayer(node)->layer();
  if (LayerOps::moveLegendLayer(QgsLayerTree::toLayer(node), dest)) {
    onLayerTreeRowsMoved();
    if (layer) m_layerTree->setCurrentLayer(layer);
  }
#else
  Q_UNUSED(dir);
#endif
}

namespace {

class FileListView : public QListWidget {
public:
  explicit FileListView(QWidget* parent = nullptr) : QListWidget(parent) {
    setDragEnabled(true);
    setDragDropMode(QAbstractItemView::DragOnly);
    setDefaultDropAction(Qt::CopyAction);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setUniformItemSizes(true);
    setIconSize(QSize(0, 0));
  }

protected:
  void startDrag(Qt::DropActions) override {
    QList<QUrl> urls;
    const auto items = selectedItems();
    for (QListWidgetItem* it : items) {
      if (!it || it->data(Qt::UserRole + 1).toBool()) continue;
      const QString p = it->data(Qt::UserRole).toString();
      if (!p.isEmpty()) urls.append(QUrl::fromLocalFile(p));
    }
    if (urls.isEmpty()) return;
    auto* md = new QMimeData;
    md->setUrls(urls);
    QDrag drag(this);
    drag.setMimeData(md);
    drag.exec(Qt::CopyAction);
  }
};

void kaPaintColorButton(QPushButton* b, const QColor& c, const QString& suffix) {
  if (!b) return;
  b->setStyleSheet(KaTheme::colorSwatchStyle(c));
  b->setText(c.name(QColor::HexRgb).toUpper() + QStringLiteral("  ·  ") + suffix);
  b->setProperty("kaColor", c);
  b->setCursor(Qt::PointingHandCursor);
}

QPushButton* kaMakeColorButton(QWidget* parent, const QColor& c, const QString& suffix,
                               const QString& pickerTitle, const std::function<void()>& onChanged = {}) {
  auto* b = new QPushButton(parent);
  kaPaintColorButton(b, c.isValid() && c.alpha() > 0 ? c : QColor(22, 163, 74, 160), suffix);
  QObject::connect(b, &QPushButton::clicked, b, [b, suffix, pickerTitle, onChanged]() {
    QColorDialog picker(b->property("kaColor").value<QColor>(), b->window());
    picker.setOption(QColorDialog::DontUseNativeDialog, true);
    picker.setOption(QColorDialog::ShowAlphaChannel, true);
    picker.setWindowTitle(pickerTitle);
    if (picker.exec() != QDialog::Accepted) return;
    const QColor picked = picker.selectedColor();
    if (!picked.isValid()) return;
    kaPaintColorButton(b, picked, suffix);
    if (onChanged) onChanged();
  });
  return b;
}

QWidget* kaWrapLabeled(QWidget* parent, const QString& caption, QWidget* inner) {
  auto* box = new QWidget(parent);
  auto* v = new QVBoxLayout(box);
  v->setContentsMargins(0, 0, 0, 0);
  v->setSpacing(4);
  auto* lab = new QLabel(caption, box);
  v->addWidget(lab);
  v->addWidget(inner);
  return box;
}

QDoubleSpinBox* kaMakeArrowSpin(QWidget* parent, QWidget** rowOut, double minV, double maxV,
                                double step, int decimals, double value) {
  auto* row = new QWidget(parent);
  auto* h = new QHBoxLayout(row);
  h->setContentsMargins(0, 0, 0, 0);
  h->setSpacing(0);
  auto* spin = new QDoubleSpinBox(row);
  spin->setButtonSymbols(QAbstractSpinBox::UpDownArrows);
  spin->setRange(minV, maxV);
  spin->setSingleStep(step);
  spin->setDecimals(decimals);
  spin->setSuffix(QStringLiteral(" mm"));
  spin->setValue(value);
  spin->setMinimumHeight(29);
  spin->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
  h->addWidget(spin, 1);
  if (rowOut) *rowOut = row;
  return spin;
}

}  // namespace

void MainWindow::addUserLayer() {
#if KA_HGIS_HAS_QGIS
  QDialog dlg(this);
  dlg.setObjectName(QStringLiteral("kaStyleDlg"));
  dlg.setWindowTitle(QStringLiteral("레이어 추가"));
  dlg.setWindowFlag(Qt::MSWindowsFixedSizeDialogHint, true);
  auto* root = new QVBoxLayout(&dlg);
  root->setSpacing(8);
  root->setContentsMargins(16, 14, 16, 12);
  root->setSizeConstraint(QLayout::SetFixedSize);
  auto* nameEdit = new QLineEdit(&dlg);
  nameEdit->setPlaceholderText(QStringLiteral("예: 조사구역, 1호 주거지"));
  nameEdit->setMinimumHeight(36);
  root->addWidget(kaWrapLabeled(&dlg, QStringLiteral("이름"), nameEdit));
  auto* crsHost = new QWidget(&dlg);
  auto* crsRow = new QHBoxLayout(crsHost);
  crsRow->setContentsMargins(0, 0, 0, 0);
  crsRow->setSpacing(8);
  auto* btn5186 = new QPushButton(QStringLiteral("5186  중부"), crsHost);
  auto* btn5187 = new QPushButton(QStringLiteral("5187  동부"), crsHost);
  btn5186->setCheckable(true);
  btn5187->setCheckable(true);
  btn5186->setMinimumHeight(40);
  btn5187->setMinimumHeight(40);
  btn5186->setCursor(Qt::PointingHandCursor);
  btn5187->setCursor(Qt::PointingHandCursor);
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
  root->addWidget(kaWrapLabeled(&dlg, QStringLiteral("좌표계"), crsHost));
  auto* noFill = new QCheckBox(QStringLiteral("채우기 없음 (외곽선만)"), &dlg);
  root->addWidget(noFill);
  auto* fillBtn = kaMakeColorButton(&dlg, QColor(34, 197, 94, 140),
                                    QStringLiteral("클릭해서 색 고르기"), QStringLiteral("면 색"));
  auto* fillBox = kaWrapLabeled(&dlg, QStringLiteral("면 색"), fillBtn);
  root->addWidget(fillBox);
  auto* strokeBtn = kaMakeColorButton(&dlg, QColor(21, 128, 61),
                                      QStringLiteral("클릭해서 색 고르기"), QStringLiteral("외곽선 색"));
  root->addWidget(kaWrapLabeled(&dlg, QStringLiteral("외곽선 색"), strokeBtn));
  connect(noFill, &QCheckBox::toggled, &dlg, [&dlg, fillBox](bool on) {
    fillBox->setVisible(!on);
    dlg.adjustSize();
  });
  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
  buttons->button(QDialogButtonBox::Ok)->setText(QStringLiteral("추가"));
  buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("취소"));
  root->addWidget(buttons);
  connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
  if (dlg.exec() != QDialog::Accepted) return;
  const QString title = nameEdit->text().trimmed();
  if (title.isEmpty()) {
    QMessageBox::information(this, QStringLiteral("레이어 추가"), QStringLiteral("이름을 입력하세요."));
    return;
  }
  if (m_surveyPath.isEmpty()) {
    const auto ans = QMessageBox::question(this, QStringLiteral("레이어"),
                                           QStringLiteral("먼저 새 조사가 필요합니다. 지금 만들까요?"));
    if (ans != QMessageBox::Yes) return;
    newSurvey();
    if (m_surveyPath.isEmpty()) return;
  }
  QString err;
  const QString crsId = btn5187->isChecked() ? QStringLiteral("EPSG:5187")
                                             : QStringLiteral("EPSG:5186");
  auto* vl = LayerOps::createUserPolygonLayer(QgsProject::instance(), m_surveyPath, title,
                                              crsId, &err);
  if (!vl) {
    notify(Notice::Warning, QStringLiteral("레이어 추가"),
           QStringLiteral("레이어를 만들지 못했습니다."), err);
    return;
  }
  LayerOps::applySimpleVectorStyle(vl, fillBtn->property("kaColor").value<QColor>(),
                                   strokeBtn->property("kaColor").value<QColor>(),
                                   1.2, 3.5, noFill->isChecked(), false);
  LayerOps::applyAreaM2Labels(vl);
  if (m_layerTree) m_layerTree->setCurrentLayer(vl);
  if (m_canvas) {
    LayerOps::syncMapCanvas(QgsProject::instance(), m_canvas, false);
    m_canvas->refresh();
  }
  statusBar()->showMessage(QStringLiteral("레이어 추가: %1").arg(title), 6000);
#else
  QMessageBox::information(this, QStringLiteral("레이어"), QStringLiteral("QGIS 빌드 필요"));
#endif
}

void MainWindow::importControlCsv() {
  const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("CSV 기준점"), QString(),
                                                    QStringLiteral("CSV (*.csv)"));
  if (path.isEmpty()) return;
#if KA_HGIS_HAS_QGIS
  auto* layer = ensureDomainLayerForEdit(QStringLiteral("control_points"), QStringLiteral("GPS기준점"));
  if (!layer) return;
  QString err;
  const LayerOps::ControlCsvPreview preview =
      LayerOps::previewControlPointsCsv(layer, path, QgsProject::instance(), &err);
  if (!preview.ok) {
    KaUserError::warn(this, {
        QStringLiteral("CSV"),
        QStringLiteral("기준점 CSV를 읽지 못했습니다."),
        err.isEmpty() ? QStringLiteral("파일 형식이나 좌표 칸을 확인하지 못했습니다.") : err,
        QStringLiteral("CSV에 X·Y(또는 경도·위도) 칸이 있는지 확인한 뒤 다시 가져오세요."),
    });
    return;
  }
  QMessageBox box(this);
  box.setIcon(preview.swapSuggested ? QMessageBox::Warning : QMessageBox::Information);
  box.setWindowTitle(QStringLiteral("기준점 미리보기"));
  box.setText(preview.summary);
  auto* keep = qobject_cast<QPushButton*>(box.addButton(QStringLiteral("이대로 가져오기"), QMessageBox::AcceptRole));
  auto* swap = qobject_cast<QPushButton*>(box.addButton(QStringLiteral("X·Y 교환"), QMessageBox::ActionRole));
  box.addButton(QStringLiteral("취소"), QMessageBox::RejectRole);
  box.setDefaultButton(preview.swapSuggested ? swap : keep);
  box.exec();
  if (box.clickedButton() != keep && box.clickedButton() != swap) return;
  const int n = LayerOps::importControlPointsCsv(layer, path, &err, box.clickedButton() == swap);
  if (n < 0) {
    QMessageBox::warning(this, QStringLiteral("CSV"), err);
    return;
  }
  m_stubGcp = int(layer->featureCount());
  m_stubHasMeta = true;
  if (m_canvas) m_canvas->refresh();
  statusBar()->showMessage(QStringLiteral("CSV 기준점 %1개 저장 (합 %2)").arg(n).arg(m_stubGcp), 6000);
  refreshWorkPanel();
#else
  Q_UNUSED(path);
  statusBar()->showMessage(QStringLiteral("스텁: CSV"), 3000);
#endif
}

#if KA_HGIS_HAS_QGIS
static bool layerSitsOnWorkMap(QgsMapLayer* layer, const QString& workCrs) {
  if (!layer) return false;
  const QgsRectangle e = layer->extent();
  if (e.isEmpty() || !e.isFinite()) return false;
  const QgsRectangle kr = LayerOps::koreaExtentForCrs(workCrs);
  if (kr.isEmpty()) return true;
  return kr.intersects(e);
}
#endif

void MainWindow::ensureAlignSplit() {
#if KA_HGIS_HAS_QGIS
  if (m_mapSplitter) return;
  auto* mapCard = findChild<QFrame*>(QStringLiteral("mapCard"));
  if (!mapCard || !m_canvas) return;
  auto* mapLay = qobject_cast<QVBoxLayout*>(mapCard->layout());
  if (!mapLay) return;

  m_mapSplitter = new QSplitter(Qt::Horizontal, mapCard);
  m_mapSplitter->setObjectName(QStringLiteral("alignSplitter"));
  m_mapSplitter->setChildrenCollapsible(false);

  m_alignLeftPane = new QWidget(m_mapSplitter);
  auto* ll = new QVBoxLayout(m_alignLeftPane);
  ll->setContentsMargins(0, 0, 0, 0);
  ll->setSpacing(2);
  m_alignLeftLabel = new QLabel(QStringLiteral("왼쪽 · 맞출 도면"), m_alignLeftPane);
  m_alignLeftLabel->setObjectName(QStringLiteral("subToolbarCaption"));
  ll->addWidget(m_alignLeftLabel);

  m_alignImage = new KaImageView(m_alignLeftPane);
  m_alignImage->setCursor(Qt::CrossCursor);
  connect(m_alignImage, &KaImageView::pixelClicked, this, [this](double x, double y) {
    if (!m_alignTool) return;
    m_alignTool->setSourcePoint(x, y);
    if (m_canvas) m_canvas->setMapTool(m_alignTool);
  });
  connect(m_alignImage, &KaImageView::viewChanged, this, [this]() {
    if (m_subToolsMode == QLatin1String("align"))
      updateAlignOverlay();
  });
  ll->addWidget(m_alignImage, 1);

  m_alignLeftCanvas = new QgsMapCanvas(m_alignLeftPane);
  LayerOps::applyWheelZoomFactor(m_alignLeftCanvas);
  KaTheme::excludeMapSurface(m_alignLeftCanvas);
  m_alignLeftCanvas->setCanvasColor(KaTheme::tokens().canvasNeutral);
  m_alignLeftCanvas->enableAntiAliasing(true);
  m_alignPickTool = new KaAlignPickTool(m_alignLeftCanvas);
  m_alignPickTool->setParent(this);
  connect(m_alignPickTool, &KaAlignPickTool::picked, this, [this](const QgsPointXY& pt) {
    if (!m_alignTool) return;
    m_alignTool->setSourcePoint(pt.x(), pt.y());
    if (m_canvas) m_canvas->setMapTool(m_alignTool);
  });
  m_alignLeftCanvas->setMapTool(m_alignPickTool);
  connect(m_alignLeftCanvas, &QgsMapCanvas::extentsChanged, this, [this]() {
    if (m_subToolsMode == QLatin1String("align"))
      updateAlignOverlay();
  });
  connect(m_alignLeftCanvas, &QgsMapCanvas::scaleChanged, this, [this](double) {
    if (m_subToolsMode == QLatin1String("align"))
      updateAlignOverlay();
  });
  ll->addWidget(m_alignLeftCanvas, 1);
  m_alignLeftCanvas->hide();

  m_alignPointList = new QListWidget(m_alignLeftPane);
  m_alignPointList->setObjectName(QStringLiteral("alignPointList"));
  m_alignPointList->setMaximumHeight(130);
  m_alignPointList->setToolTip(QStringLiteral("찍은 점. Delete로 지웁니다"));
  ll->addWidget(m_alignPointList);

  mapLay->removeWidget(m_canvas);
  m_mapSplitter->addWidget(m_alignLeftPane);
  m_mapSplitter->addWidget(m_canvas);
  m_mapSplitter->setStretchFactor(0, 1);
  m_mapSplitter->setStretchFactor(1, 1);
  mapLay->insertWidget(0, m_mapSplitter, 1);

  m_alignOverlay = new KaAlignLinkOverlay(m_mapSplitter);
  m_alignOverlay->setMouseTracking(true);
  m_alignOverlay->raise();
  m_mapSplitter->setMouseTracking(true);
  m_mapSplitter->installEventFilter(this);
  m_alignOverlay->installEventFilter(this);
  m_alignLeftPane->setMouseTracking(true);
  m_alignLeftPane->installEventFilter(this);
  if (m_alignImage) {
    m_alignImage->setMouseTracking(true);
    m_alignImage->installEventFilter(this);
    if (m_alignImage->viewport()) {
      m_alignImage->viewport()->setMouseTracking(true);
      m_alignImage->viewport()->installEventFilter(this);
    }
  }
  if (m_canvas) {
    m_canvas->setMouseTracking(true);
    m_canvas->installEventFilter(this);
    if (m_canvas->viewport()) {
      m_canvas->viewport()->setMouseTracking(true);
      m_canvas->viewport()->installEventFilter(this);
    }
  }
  if (m_alignLeftCanvas) {
    m_alignLeftCanvas->setMouseTracking(true);
    m_alignLeftCanvas->installEventFilter(this);
    if (m_alignLeftCanvas->viewport()) {
      m_alignLeftCanvas->viewport()->setMouseTracking(true);
      m_alignLeftCanvas->viewport()->installEventFilter(this);
    }
  }
#endif
}

// Qt 의 그림 읽기는 한 장을 통째로 메모리에 편다(Qt6 기본 상한 256MB).
// 항공사진 원판은 1억 화소가 예사라 여기서 막히고, 예전에는 그대로 빈 칸이 됐다.
// 지도 쪽에서는 같은 파일이 잘 보이는데, 그건 GDAL 이 필요한 만큼만 솎아 읽기
// 때문이다. 그래서 왼쪽 칸도 GDAL 축소본으로 채운다. 클릭 좌표는 KaImageView 가
// 원본 픽셀로 되돌려 주므로 정합 계산은 원본 기준 그대로다.
bool MainWindow::loadAlignPreviewFromRaster() {
#if KA_HGIS_HAS_QGIS
  if (!m_alignImage || !m_alignTool) return false;
  auto* rl = qobject_cast<QgsRasterLayer*>(m_alignTool->targetLayer());
  const QString why = m_alignImage->lastError();
  if (!rl || !rl->isValid() || rl->width() < 2 || rl->height() < 2) {
    notify(Notice::Warning, QStringLiteral("사진·CAD 정합"),
           QStringLiteral("왼쪽에 그림을 띄우지 못했습니다."), why);
    return false;
  }
  const int nw = rl->width();
  const int nh = rl->height();
  const int maxSide = 4000;  // 점 찍기에 충분하고 메모리는 60MB 아래로 유지된다
  const int longSide = qMax(nw, nh);
  const double f = longSide > maxSide ? double(maxSide) / double(longSide) : 1.0;
  const QSize want(qMax(1, int(std::lround(nw * f))), qMax(1, int(std::lround(nh * f))));

  statusBar()->showMessage(
      QStringLiteral("큰 그림이라 축소본을 만드는 중입니다 (%1 x %2 화소)...").arg(nw).arg(nh));
  QApplication::setOverrideCursor(Qt::WaitCursor);
  const QImage img = rl->previewAsImage(want);
  QApplication::restoreOverrideCursor();

  if (img.isNull() || !m_alignImage->setPreview(QPixmap::fromImage(img), nw, nh)) {
    notify(Notice::Warning, QStringLiteral("사진·CAD 정합"),
           QStringLiteral("왼쪽에 그림을 띄우지 못했습니다: %1").arg(rl->name()), why);
    return false;
  }
  statusBar()->showMessage(
      QStringLiteral("원본 %1 x %2 화소는 Qt 로 한 번에 못 엽니다. 축소본(%3 x %4)으로 "
                     "찍으세요 — 좌표는 원본 기준으로 계산됩니다.")
          .arg(nw).arg(nh).arg(img.width()).arg(img.height()),
      12000);
  return true;
#else
  return false;
#endif
}

void MainWindow::showAlignSplit() {
#if KA_HGIS_HAS_QGIS
  ensureAlignSplit();
  if (!m_mapSplitter || !m_alignLeftPane || !m_alignTool) return;
  m_alignLeftPane->show();
  if (m_alignTool->isRasterSession()) {
    if (m_alignImage) {
      m_alignImage->show();
      m_alignImage->clearMarks();
      // 예전에는 결과를 보지 않고 넘어가서, 못 연 그림은 아무 말 없이 빈 칸이 됐다.
      if (!m_alignImage->loadPath(m_alignTool->rasterSourcePath()))
        loadAlignPreviewFromRaster();
    }
    if (m_alignLeftCanvas) m_alignLeftCanvas->hide();
    if (m_alignLeftLabel)
      m_alignLeftLabel->setText(QStringLiteral("왼쪽 · 그림 — 여기를 먼저 찍기"));
  } else {
    if (m_alignImage) m_alignImage->hide();
    if (m_alignLeftCanvas) {
      m_alignLeftCanvas->show();
      LayerOps::applyCanvasScreenDpi(m_alignLeftCanvas);
      if (QgsMapLayer* src = m_alignTool->sourceDisplayLayer()) {
        if (src->crs().isValid())
          m_alignLeftCanvas->setDestinationCrs(src->crs());
        m_alignLeftCanvas->setLayers(QList<QgsMapLayer*>() << src);
        const QgsRectangle ext = src->extent();
        if (!ext.isEmpty() && ext.isFinite())
          m_alignLeftCanvas->setExtent(ext);
        m_alignLeftCanvas->refresh();
      }
    }
    if (m_alignLeftLabel)
      m_alignLeftLabel->setText(QStringLiteral("왼쪽 · CAD — 여기를 먼저 찍기"));
  }
  m_mapSplitter->setSizes({1000, 1000});
#endif
}

void MainWindow::hideAlignSplit() {
#if KA_HGIS_HAS_QGIS
  if (m_alignLeftPane) m_alignLeftPane->hide();
  if (m_mapSplitter) m_mapSplitter->setSizes({0, 1});
  if (m_alignOverlay) m_alignOverlay->hide();
  if (m_alignCursorTimer) m_alignCursorTimer->stop();
  for (auto* m : m_alignLeftMarks) delete m;
  m_alignLeftMarks.clear();
#endif
}

void MainWindow::refreshAlignUi() {
#if KA_HGIS_HAS_QGIS
  if (!m_alignTool) return;
  QVector<QPointF> pts;
  for (const GeorefService::Pair& p : m_alignTool->pairs())
    pts.append(QPointF(p.srcX, p.srcY));
  QPointF pending;
  const QPointF* pend = nullptr;
  if (m_alignTool->hasPendingSource()) {
    pending = QPointF(m_alignTool->pendingSrcX(), m_alignTool->pendingSrcY());
    pend = &pending;
  }
  if (m_alignImage && m_alignImage->isVisible())
    m_alignImage->setMarks(pts, pend);

  for (auto* m : m_alignLeftMarks) delete m;
  m_alignLeftMarks.clear();
  if (m_alignLeftCanvas && m_alignLeftCanvas->isVisible()) {
    auto addMk = [&](const QgsPointXY& pt, const QColor& col) {
      auto* mk = new QgsVertexMarker(m_alignLeftCanvas);
      mk->setIconType(QgsVertexMarker::ICON_CIRCLE);
      mk->setIconSize(14);
      mk->setPenWidth(2);
      mk->setColor(col);
      mk->setFillColor(QColor(255, 255, 255, 230));
      mk->setCenter(pt);
      mk->show();
      m_alignLeftMarks.append(mk);
    };
    for (const QPointF& p : pts)
      addMk(QgsPointXY(p.x(), p.y()), QColor(220, 38, 38));
    if (pend)
      addMk(QgsPointXY(pend->x(), pend->y()), QColor(234, 179, 8));
  }

  if (m_alignPointList) {
    m_alignPointList->clear();
    const auto& pairs = m_alignTool->pairs();
    for (int i = 0; i < pairs.size(); ++i) {
      m_alignPointList->addItem(
          QStringLiteral("%1번  왼쪽 → 오른쪽").arg(i + 1));
    }
    if (m_alignTool->hasPendingSource()) {
      m_alignPointList->addItem(
          QStringLiteral("%1번  왼쪽만 — 오른쪽 모서리를 찍으세요").arg(pairs.size() + 1));
    }
  }

  updateAlignOverlay();
  if (m_alignTool->hasPendingSource() && !m_alignApplied) {
    if (!m_alignCursorTimer) {
      m_alignCursorTimer = new QTimer(this);
      m_alignCursorTimer->setInterval(16);
      connect(m_alignCursorTimer, &QTimer::timeout, this, [this]() {
        if (m_alignTool && m_alignTool->hasPendingSource() && !m_alignApplied)
          trackAlignPointer(QCursor::pos());
        else if (m_alignCursorTimer)
          m_alignCursorTimer->stop();
      });
    }
    if (!m_alignCursorTimer->isActive()) m_alignCursorTimer->start();
    trackAlignPointer(QCursor::pos());
  } else if (m_alignCursorTimer) {
    m_alignCursorTimer->stop();
  }
#endif
}

void MainWindow::trackAlignPointer(const QPoint& globalPos) {
#if KA_HGIS_HAS_QGIS
  if (!m_alignTool || !m_alignTool->hasPendingSource() || m_alignApplied) return;
  if (!m_alignOverlay) return;

  m_alignLiveScreen = m_alignOverlay->mapFromGlobal(globalPos);
  m_alignLiveScreenValid = true;
  m_alignCursorValid = false;

  if (m_canvas) {
    QWidget* vp = m_canvas->viewport() ? static_cast<QWidget*>(m_canvas->viewport())
                                       : static_cast<QWidget*>(m_canvas);
    const QPoint inRight = vp->mapFromGlobal(globalPos);
    if (vp->rect().contains(inRight)) {
      const QgsMapToPixel& m2p = m_canvas->mapSettings().mapToPixel();
      QgsPointXY mapPt = m2p.toMapCoordinates(inRight.x(), inRight.y());
      const QgsPointXY cursorMap = mapPt;
      if (m_canvas->snappingUtils()) {
        const QgsPointLocator::Match hit = m_canvas->snappingUtils()->snapToMap(inRight);
        if (hit.isValid()) {
          mapPt = hit.point();
          const double mupp = m_canvas->mapUnitsPerPixel();
          if (mupp > 1e-12) {
            m_alignLiveScreen += QPoint(
                int(std::lround((mapPt.x() - cursorMap.x()) / mupp)),
                int(std::lround((cursorMap.y() - mapPt.y()) / mupp)));
          }
        }
      }
      m_alignCursorX = mapPt.x();
      m_alignCursorY = mapPt.y();
      m_alignCursorValid = true;
      if (m_alignTool) m_alignTool->setMapHint(mapPt.x(), mapPt.y(), true);
    } else if (m_alignTool) {
      m_alignTool->setMapHint(0, 0, false);
    }
  }
  updateAlignOverlay();
#endif
}

void MainWindow::updateAlignOverlay() {
#if KA_HGIS_HAS_QGIS
  if (!m_alignOverlay || !m_mapSplitter || !m_alignTool || !m_alignLeftPane
      || !m_alignLeftPane->isVisible() || m_alignApplied) {
    if (m_alignOverlay) m_alignOverlay->hide();
    return;
  }
  m_alignOverlay->setGeometry(m_mapSplitter->rect());
  m_alignOverlay->show();
  m_alignOverlay->raise();

  // 오버레이는 스플리터의 자식이라 캔버스·그림뷰의 조상이 아니라 형제다.
  // QWidget::mapTo는 대상이 조상일 때만 유효하고, 형제를 주면 최상위 창 좌표를
  // 돌려줘 화살표가 스플리터 원점만큼 통째로 밀린다. 라이브 점선이 쓰는
  // mapFromGlobal과 같은 기준으로 맞춘다(trackAlignPointer).
  auto toOverlay = [this](QWidget* from, const QPoint& inFrom) -> QPoint {
    if (!from || !m_alignOverlay) return {};
    return m_alignOverlay->mapFromGlobal(from->mapToGlobal(inFrom));
  };
  auto mapToOverlay = [&](QgsMapCanvas* c, double mx, double my) -> QPoint {
    if (!c || !c->viewport()) return {};
    // QgsMapMouseEvent::mapToPixelCoordinates: transform() == 뷰포트 좌표.
    const QgsPointXY xy = c->mapSettings().mapToPixel().transform(QgsPointXY(mx, my));
    return toOverlay(c->viewport(),
                     QPoint(int(std::lround(xy.x())), int(std::lround(xy.y()))));
  };
  auto srcToOverlay = [&](double sx, double sy) -> QPoint {
    // viewPosForPixel이 돌려주는 값은 뷰 위젯이 아니라 뷰포트 좌표다.
    if (m_alignImage && m_alignImage->isVisible() && m_alignImage->viewport())
      return toOverlay(m_alignImage->viewport(), m_alignImage->viewPosForPixel(sx, sy));
    if (m_alignLeftCanvas && m_alignLeftCanvas->isVisible())
      return mapToOverlay(m_alignLeftCanvas, sx, sy);
    return {};
  };

  const auto& pairs = m_alignTool->pairs();
  QVector<QLine> done;
  for (int i = 0; i < pairs.size(); ++i) {
    const QPoint a = srcToOverlay(pairs[i].srcX, pairs[i].srcY);
    const QPoint b = mapToOverlay(m_canvas, pairs[i].mapX, pairs[i].mapY);
    if (!a.isNull() && !b.isNull())
      done.append(QLine(a, b));
  }
  QLine live;
  bool hasLive = false;
  if (m_alignTool->hasPendingSource()) {
    const QPoint a = srcToOverlay(m_alignTool->pendingSrcX(), m_alignTool->pendingSrcY());
    QPoint b;
    if (m_alignCursorValid)
      b = mapToOverlay(m_canvas, m_alignCursorX, m_alignCursorY);
    else if (m_alignLiveScreenValid)
      b = m_alignLiveScreen;
    if (!a.isNull() && !b.isNull()) {
      live = QLine(a, b);
      hasLive = true;
    }
  }
  m_alignOverlay->setLinks(done, live, hasLive);
#endif
}

void MainWindow::deleteSelectedAlignPoint() {
#if KA_HGIS_HAS_QGIS
  if (!m_alignTool || !m_alignPointList) return;
  const int row = m_alignPointList->currentRow();
  if (row < 0) {
    m_alignTool->removeLastPair();
    return;
  }
  if (row >= m_alignTool->pairCount()) {
    m_alignTool->removeLastPair();
    return;
  }
  m_alignTool->removePairAt(row);
#endif
}

void MainWindow::applyAlignMove() {
#if KA_HGIS_HAS_QGIS
  if (!m_alignTool) return;
  QString err;
  if (!m_alignTool->applyMove(&err)) {
    QMessageBox::warning(this, QStringLiteral("이동"), err);
    return;
  }
  m_alignApplied = true;
  if (m_alignOverlay) m_alignOverlay->hide();
  hideAlignSplit();
  applySnapConfig();
  ensureDefaultBasemaps();
  QgsMapLayer* aligned = m_alignTool->targetLayer();
  if (aligned) {
    LayerOps::setAlignPending(aligned, false);
    if (QgsProject::instance() && QgsProject::instance()->layerTreeRoot()) {
      if (QgsLayerTreeLayer* n = QgsProject::instance()->layerTreeRoot()->findLayer(aligned->id()))
        n->setItemVisibilityChecked(true);
    }
  }
  LayerOps::syncMapCanvas(QgsProject::instance(), m_canvas, false);
  if (aligned && m_canvas) {
    QList<QgsMapLayer*> stacked = m_canvas->layers();
    if (stacked.isEmpty())
      stacked = LayerOps::visibleLayersPaintOrder(QgsProject::instance());
    stacked.removeAll(aligned);
    int insertAt = stacked.size();
    for (int i = 0; i < stacked.size(); ++i) {
      if (stacked[i] && (LayerOps::isBasemapLayer(stacked[i]) ||
                         stacked[i]->name().contains(QStringLiteral("위성")) ||
                         stacked[i]->name().contains(QStringLiteral("지적")))) {
        insertAt = i;
        break;
      }
    }
    stacked.insert(insertAt, aligned);
    m_canvas->setLayers(stacked);

    QgsRectangle ext = aligned->extent();
    if (!ext.isEmpty() && ext.isFinite() && ext.xMinimum() > 1000.0) {
      m_canvas->setExtent(ext);
      m_canvas->zoomToFeatureExtent(ext);
      m_canvas->zoomScale(m_canvas->scale() * 1.25, true);
    }
  }
  refreshAlignUi();
  const auto kickAlignedPaint = [this]() {
    if (!m_canvas) return;
    if (m_alignTool) {
      if (QgsMapLayer* l = m_alignTool->targetLayer())
        l->triggerRepaint();
    }
    LayerOps::refreshCanvasIfIdle(m_canvas);
  };
  kickAlignedPaint();
  QTimer::singleShot(0, this, kickAlignedPaint);
  QTimer::singleShot(350, this, kickAlignedPaint);
  statusBar()->showMessage(
      QStringLiteral("맞춘 도면을 지금 보는 지적 위에 올렸습니다. 흰 종이만 빼고 먹선은 진하게 보이게 했습니다."),
      10000);
#endif
}

void MainWindow::stopAlignSession() {
#if KA_HGIS_HAS_QGIS
  hideAlignSplit();
  if (!m_alignTool) return;
  if (m_canvas && m_canvas->mapTool() == m_alignTool)
    m_canvas->unsetMapTool(m_alignTool);
  m_alignTool->endSession();
  if (m_panTool && m_canvas) m_canvas->setMapTool(m_panTool);
#endif
}

void MainWindow::showSubToolsAlign() {
#if KA_HGIS_HAS_QGIS
  if (!m_subToolbar) return;
  clearSubToolbar();
  m_subToolsMode = QStringLiteral("align");
  auto* lab = new QLabel(QStringLiteral("  맞추기 › "));
  lab->setObjectName(QStringLiteral("subToolbarCaption"));
  m_subToolbar->addWidget(lab);
  m_subToolbar->addAction(KaIcons::icon(QStringLiteral("transform")), QStringLiteral("이동"),
                          this, &MainWindow::applyAlignMove);
  m_subToolbar->addAction(QStringLiteral("점 지우기"), this, [this]() {
    deleteSelectedAlignPoint();
    if (m_alignTool) statusBar()->showMessage(m_alignTool->statusText(), 4000);
  });
  m_subToolbar->addAction(QStringLiteral("되돌리기"), this, [this]() {
    if (m_alignTool) m_alignTool->restoreOriginals();
    m_alignApplied = false;
    refreshAlignUi();
    statusBar()->showMessage(QStringLiteral("맞추기를 처음 상태로 되돌렸습니다"), 4000);
  });
  m_subToolbar->addAction(KaIcons::icon(QStringLiteral("save")), QStringLiteral("맞추기 저장"),
                          this, [this]() {
                            if (!m_alignTool) return;
                            if (m_alignTool->pairCount() < 2 && !m_alignApplied) {
                              notify(Notice::Warning, QStringLiteral("맞추기"),
                                     QStringLiteral("점을 2곳 이상 찍은 뒤 저장하세요."));
                              return;
                            }
                            if (!m_alignApplied) {
                              applyAlignMove();
                              if (!m_alignApplied) return;
                            }
                            QString path, err;
                            if (!m_alignTool->saveAligned(&path, &err)) {
                              notify(Notice::Warning, QStringLiteral("맞추기"),
                                     QStringLiteral("맞춘 결과를 저장하지 못했습니다."), err);
                              return;
                            }
                            hideAlignSplit();
                            if (QgsMapLayer* aligned = m_alignTool->targetLayer()) {
                              LayerOps::setAlignPending(aligned, false);
                              if (QgsProject::instance() && QgsProject::instance()->layerTreeRoot()) {
                                if (QgsLayerTreeLayer* n = QgsProject::instance()->layerTreeRoot()->findLayer(aligned->id()))
                                  n->setItemVisibilityChecked(true);
                              }
                              if (m_canvas) {
                                QList<QgsMapLayer*> stacked = m_canvas->layers();
                                stacked.removeAll(aligned);
                                int insertAt = stacked.size();
                                for (int i = 0; i < stacked.size(); ++i) {
                                  if (stacked[i] && (LayerOps::isBasemapLayer(stacked[i]) ||
                                                     stacked[i]->name().contains(QStringLiteral("위성")) ||
                                                     stacked[i]->name().contains(QStringLiteral("지적")))) {
                                    insertAt = i;
                                    break;
                                  }
                                }
                                stacked.insert(insertAt, aligned);
                                m_canvas->setLayers(stacked);
                                aligned->triggerRepaint();
                                LayerOps::refreshCanvasIfIdle(m_canvas);
                              }
                            }
                            notify(Notice::Success, QStringLiteral("맞추기"),
                                   QStringLiteral("맞춰 두었습니다. 이 도면은 참고용이니 제출할 구역은 그리기로 직접 그리세요."),
                                   QDir::toNativeSeparators(path));
                            statusBar()->showMessage(QStringLiteral("맞춤 저장: %1").arg(path), 8000);
                          });
  auto* closeAct = m_subToolbar->addAction(QStringLiteral("닫기"));
  connect(closeAct, &QAction::triggered, this, &MainWindow::hideSubTools);
  m_subToolbar->setVisible(true);
#endif
}

void MainWindow::startAlignSession(QgsMapLayer* layer) {
#if KA_HGIS_HAS_QGIS
  if (!m_canvas || !layer) return;
  m_alignApplied = false;
  m_alignCursorValid = false;
  if (!m_alignTool) {
    m_alignTool = new KaAlignMapTool(m_canvas);
    m_alignTool->setParent(this);
    connect(m_alignTool, &KaAlignMapTool::statusChanged, this, [this](const QString& t) {
      statusBar()->showMessage(t, 8000);
    });
    connect(m_alignTool, &KaAlignMapTool::pairsChanged, this, &MainWindow::refreshAlignUi);
    connect(m_alignTool, &KaAlignMapTool::cursorMoved, this, [this](const QgsPointXY& pt) {
      m_alignCursorX = pt.x();
      m_alignCursorY = pt.y();
      m_alignCursorValid = true;
      updateAlignOverlay();
    });
  }
  stopCaptureTool();
  QString err;
  const QgsCoordinateReferenceSystem crs =
      QgsProject::instance() && QgsProject::instance()->crs().isValid()
          ? QgsProject::instance()->crs()
          : QgsCoordinateReferenceSystem(m_workCrs);
  if (!m_alignTool->beginLayer(layer, crs, &err)) {
    QMessageBox::warning(this, QStringLiteral("맞추기"), err);
    return;
  }
  if (m_layerTree) m_layerTree->setCurrentLayer(m_alignTool->targetLayer());
  showAlignSplit();
  showSubToolsAlign();
  applySnapConfig();
  m_canvas->setMapTool(m_alignTool);
  LayerOps::ensureOtfEnabled(QgsProject::instance(), m_canvas, m_workCrs);
  LayerOps::syncMapCanvas(QgsProject::instance(), m_canvas, false);
  refreshAlignUi();
  statusBar()->showMessage(m_alignTool->statusText(), 10000);
#else
  Q_UNUSED(layer);
#endif
}

void MainWindow::georefAssistant() {
#if KA_HGIS_HAS_QGIS
  QgsMapLayer* layer = m_layerTree ? m_layerTree->currentLayer() : nullptr;
  if (!GeorefService::isAlignableLayer(layer)) {
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("맞출 도면"), QString(),
        QStringLiteral("도면 (*.png *.jpg *.jpeg *.tif *.tiff *.dxf *.dwg)"));
    if (path.isEmpty()) return;
    const bool ok = GeorefService::isImagePath(path) ? addRasterFromPath(path)
                                                     : addVectorFromPath(path);
    if (!ok) {
      KaUserError::warn(this, {
          QStringLiteral("맞추기"),
          QStringLiteral("맞출 도면 파일을 열지 못했습니다."),
          QStringLiteral("지원하지 않는 형식이거나 DWG 드라이버가 이 버전을 읽지 못합니다."),
          QStringLiteral("DWG면 AutoCAD에서 DXF로 저장한 뒤 다시 시도하세요."),
      });
      return;
    }
    layer = m_layerTree ? m_layerTree->currentLayer() : nullptr;
    if (!layer) {
      const auto layers = QgsProject::instance()->mapLayers();
      for (auto it = layers.constBegin(); it != layers.constEnd(); ++it) {
        if (GeorefService::isAlignableLayer(it.value())) layer = it.value();
      }
    }
  }
  if (!GeorefService::isAlignableLayer(layer)) {
    QMessageBox::information(this, QStringLiteral("맞추기"),
                             QStringLiteral("JPG·PNG·DXF 도면을 고르거나, 목록에서 도면을 선택한 뒤 다시 누르세요."));
    return;
  }
  startAlignSession(layer);
#else
  QMessageBox::warning(this, QStringLiteral("맞추기"), QStringLiteral("QGIS 빌드 필요"));
#endif
}

void MainWindow::openVectorLayer() {
#if KA_HGIS_HAS_QGIS
  const QString path = QFileDialog::getOpenFileName(
      this, QStringLiteral("SHP/벡터 추가"), QString(),
      QStringLiteral("Vector (*.shp *.gpkg *.geojson)"));
  if (path.isEmpty()) return;
  LayerOps::prepareShapefileEncoding(path);
  const QString title = QFileInfo(path).completeBaseName();
  auto* layer = new QgsVectorLayer(path, title, QStringLiteral("ogr"));
  if (!layer->isValid()) {
    QMessageBox::warning(this, QStringLiteral("오류"),
                         QStringLiteral("열 수 없음: %1").arg(layer->error().message()));
    delete layer;
    return;
  }
  for (const QgsField& f : layer->fields()) {
    if (f.name().contains(QChar(0xFFFD))) {
      LayerOps::setShapefileEncoding(layer, QStringLiteral("CP949"));
      break;
    }
  }
  LayerOps::markSurveyLayer(layer, QStringLiteral("user:%1").arg(title));
  LayerOps::applySimpleVectorStyle(layer, QColor(0, 0, 0, 0), QColor(0, 0, 0), 0.2, 3.5, true,
                                   false);
  const QString nameField = LayerOps::detectNameField(layer);
  if (!nameField.isEmpty()) {
    LayerOps::applyNameAttributeLabels(layer, nameField, 5.0, false);
  }
  LayerOps::ensureOtfEnabled(QgsProject::instance(), m_canvas, m_workCrs);
  if (!layer->crs().isValid() && QgsProject::instance() &&
      QgsProject::instance()->crs().isValid()) {
    layer->setCrs(QgsProject::instance()->crs());
    statusBar()->showMessage(
        QStringLiteral("벡터에 좌표계가 없어 작업 좌표계(%1)를 붙였습니다.").arg(m_workCrs),
        8000);
  }
  LayerOps::applyLegendCrsLabel(layer);
  QgsProject::instance()->addMapLayer(layer, true);
  LayerOps::pruneEmptyLegendGroups(QgsProject::instance());
  if (m_canvas) {
    LayerOps::syncMapCanvas(QgsProject::instance(), m_canvas, false);
    LayerOps::zoomToLayerMax(m_canvas, layer);
  }
  statusBar()->showMessage(QStringLiteral("지도를 올렸습니다: %1").arg(title), 6000);
#else
  QMessageBox::information(this, QStringLiteral("스텁"), QStringLiteral("벡터 로드 시뮬레이션"));
#endif
}

// 흙토람(soil.rda.go.kr) 토양도 신청으로 받은 SHP를 참조 지도로 불러온다.
// VWorld WMS 토양 레이어는 서버 오류가 잦고 분포지형이 없어 파일 방식을 쓴다.
void MainWindow::importSoilShapefile() {
#if KA_HGIS_HAS_QGIS
  const QString path = QFileDialog::getOpenFileName(
      this, QStringLiteral("토양도 SHP 불러오기"), QString(),
      QStringLiteral("Shapefile (*.shp)"));
  if (path.isEmpty()) return;

  // 필드 목록과 파일에 기록된 좌표계를 미리 읽어 선택지를 만든다.
  QStringList fieldNames;
  QString fileCrsAuth;
  {
    QgsVectorLayer probe(path, QStringLiteral("probe"), QStringLiteral("ogr"));
    if (!probe.isValid()) {
      notify(Notice::Critical, QStringLiteral("토양도"),
             QStringLiteral("SHP를 열 수 없습니다: %1").arg(probe.error().message()));
      return;
    }
    const QgsFields flds = probe.fields();
    for (int i = 0; i < flds.count(); ++i) {
      const QgsField f = flds.at(i);
      const auto t = static_cast<QMetaType::Type>(f.type());
      if (t == QMetaType::QString || t == QMetaType::Int || t == QMetaType::LongLong)
        fieldNames.append(f.name());
    }
    if (probe.crs().isValid()) fileCrsAuth = probe.crs().authid();
  }

  QDialog dlg(this);
  dlg.setWindowTitle(QStringLiteral("토양도 불러오기 설정"));
  auto* form = new QFormLayout(&dlg);

  auto* crsBox = new QComboBox(&dlg);
  if (!fileCrsAuth.isEmpty())
    crsBox->addItem(QStringLiteral("파일에 기록된 좌표계 사용 — %1").arg(fileCrsAuth), QString());
  crsBox->addItem(QStringLiteral("EPSG:2097 — 중부원점(Bessel) · 흙토람 고시 좌표계"),
                  QStringLiteral("EPSG:2097"));
  crsBox->addItem(QStringLiteral("EPSG:5174 — 중부원점(Bessel, 10.405″ 보정)"),
                  QStringLiteral("EPSG:5174"));
  crsBox->addItem(QStringLiteral("EPSG:5186 — 중부원점(GRS80)"), QStringLiteral("EPSG:5186"));
  crsBox->addItem(QStringLiteral("EPSG:5187 — 동부원점(GRS80)"), QStringLiteral("EPSG:5187"));
  crsBox->addItem(QStringLiteral("EPSG:5179 — UTM-K"), QStringLiteral("EPSG:5179"));
  crsBox->addItem(QStringLiteral("EPSG:4326 — 경위도(WGS84)"), QStringLiteral("EPSG:4326"));
  crsBox->setCurrentIndex(0);
  form->addRow(QStringLiteral("좌표계"), crsBox);

  auto* fieldBox = new QComboBox(&dlg);
  fieldBox->addItem(QStringLiteral("(단색 — 구분 없음)"), QString());
  for (const QString& n : fieldNames)
    fieldBox->addItem(n, n);
  // 분포지형·토양부호 계열 필드가 있으면 미리 고른다.
  static const QRegularExpression kSoilFieldRx(
      QStringLiteral("(분포|지형|토양|tpgrp|topo|dist|soil|sltp|sym)"),
      QRegularExpression::CaseInsensitiveOption);
  for (int i = 1; i < fieldBox->count(); ++i) {
    if (fieldBox->itemText(i).contains(kSoilFieldRx)) {
      fieldBox->setCurrentIndex(i);
      break;
    }
  }
  form->addRow(QStringLiteral("색 구분 필드"), fieldBox);

  auto* note = new QLabel(
      QStringLiteral("흙토람 → 토양도 신청에서 무료로 받은 SHP를 그대로 불러옵니다.\n"
                     "지도가 엉뚱한 위치에 뜨면 좌표계를 EPSG:2097 ↔ 5174로 바꿔 다시 불러오세요.\n"
                     "불러온 뒤에는 작업 좌표계(%1)로 자동 재투영되어 지적·위성과 겹쳐 보입니다.")
          .arg(m_workCrs),
      &dlg);
  note->setWordWrap(true);
  form->addRow(note);

  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
  buttons->button(QDialogButtonBox::Ok)->setText(QStringLiteral("불러오기"));
  buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("취소"));
  connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
  form->addRow(buttons);
  if (dlg.exec() != QDialog::Accepted) return;

  QString err;
  QgsVectorLayer* layer = LayerOps::addSoilShapefile(
      QgsProject::instance(), m_canvas, path, crsBox->currentData().toString(),
      fieldBox->currentData().toString(), &err);
  if (!layer) {
    notify(Notice::Critical, QStringLiteral("토양도"),
           err.isEmpty() ? QStringLiteral("토양도를 불러오지 못했습니다.") : err);
    return;
  }
  if (m_layerTree) m_layerTree->setCurrentLayer(layer);
  const QString fieldTxt = fieldBox->currentData().toString().isEmpty()
                               ? QStringLiteral("단색")
                               : fieldBox->currentData().toString();
  const QString msg = QStringLiteral("%1 · %2 · 색 구분: %3 — 참조 지도 그룹")
                          .arg(layer->name(), layer->crs().authid(), fieldTxt);
  statusBar()->showMessage(QStringLiteral("토양도를 올렸습니다: %1").arg(msg), 8000);
  notify(Notice::Success, QStringLiteral("토양도"), msg);
#else
  QMessageBox::information(this, QStringLiteral("스텁"), QStringLiteral("토양도 로드 시뮬레이션"));
#endif
}

#if KA_HGIS_HAS_QGIS
namespace {
// QGIS 범례 체크 / ArcGIS 레이어 on-off: 이미 있으면 보이기↔숨기기만 한다.
bool toggleExistingOverlay(QgsProject* project, QgsMapCanvas* canvas, const QString& title,
                           QAction* act, QStatusBar* bar) {
  if (!project) return false;
  if (LayerOps::isLayerVisible(project, title)) {
    LayerOps::toggleLayerVisibility(project, canvas, title, false);
    if (act) act->setChecked(false);
    if (bar) bar->showMessage(title + QStringLiteral("를 껐습니다."), 4000);
    return true;
  }
  if (LayerOps::toggleLayerVisibility(project, canvas, title, true)) {
    if (act) act->setChecked(true);
    if (bar) bar->showMessage(title + QStringLiteral("를 다시 켰습니다."), 4000);
    return true;
  }
  return false;
}
}  // namespace
#endif

void MainWindow::toggleTerrainMap() {
#if KA_HGIS_HAS_QGIS
  if (!m_canvas)
    return;
  if (toggleExistingOverlay(QgsProject::instance(), m_canvas, QStringLiteral("지형맵"), nullptr,
                            statusBar())) {
    if (m_btnTerrain)
      m_btnTerrain->setChecked(
          LayerOps::isLayerVisible(QgsProject::instance(), QStringLiteral("지형맵")));
    return;
  }
  QString err;
  if (!LayerOps::addElevationHillshadeMap(QgsProject::instance(), m_canvas,
                                          VworldSettings::loadApiKey(), &err)) {
    notify(Notice::Warning, QStringLiteral("지형맵"),
           err.isEmpty() ? QStringLiteral("지형맵을 올리지 못했습니다.") : err);
    if (m_btnTerrain)
      m_btnTerrain->setChecked(false);
    return;
  }
  if (!m_canvas->isDrawing())
    LayerOps::syncMapCanvas(QgsProject::instance(), m_canvas, false);
  if (m_btnTerrain)
    m_btnTerrain->setChecked(true);
  statusBar()->showMessage(QStringLiteral("지형맵을 올렸습니다. 다시 누르면 숨깁니다."), 6000);
#else
  QMessageBox::information(this, QStringLiteral("스텁"), QStringLiteral("지형맵 시뮬레이션"));
#endif
}

void MainWindow::editDemElevationClasses() {
#if KA_HGIS_HAS_QGIS
  QgsRasterLayer* dem = nullptr;
  if (QgsProject* proj = QgsProject::instance()) {
    const QList<QgsMapLayer*> found = proj->mapLayersByName(QStringLiteral("DEM"));
    for (QgsMapLayer* l : found) {
      auto* rl = qobject_cast<QgsRasterLayer*>(l);
      if (rl && rl->isValid() &&
          dynamic_cast<QgsSingleBandPseudoColorRenderer*>(rl->renderer())) {
        dem = rl;
        break;
      }
    }
  }
  if (!dem) {
    QMessageBox::information(
        this, QStringLiteral("DEM 표현"),
        QStringLiteral("먼저 DEM을 켜 주세요. 표고 자료를 불러오면 색 표현과 음영을 조절할 수 있습니다."));
    return;
  }
  auto* dlg = new KaDemClassDialog(dem, this, m_canvas);
  dlg->setAttribute(Qt::WA_DeleteOnClose);
  dlg->show();
#else
  QMessageBox::information(this, QStringLiteral("스텁"), QStringLiteral("DEM 높이 구간"));
#endif
}

void MainWindow::toggleDemMap() {
#if KA_HGIS_HAS_QGIS
  if (!m_canvas)
    return;
  QgsProject* proj = QgsProject::instance();
  QgsRasterLayer* dem = nullptr;
  for (QgsMapLayer* ml : proj->mapLayers()) {
    if (ml && ml->name() == QLatin1String("DEM")) {
      dem = qobject_cast<QgsRasterLayer*>(ml);
      if (dem) break;
    }
  }

  if (dem) {
    const bool wasVisible = LayerOps::isLayerVisible(proj, QStringLiteral("DEM"));
    const bool makeVisible = !wasVisible;
    // 켜는 참인데 지금 화면이 기존 DEM 밖으로 나가 있으면, 화면 전체를 덮도록 새로 받는다.
    if (makeVisible && !LayerOps::demCoversCanvas(proj, m_canvas)) {
      startDemDownload();
      return;
    }
    LayerOps::toggleLayerVisibility(proj, m_canvas, QStringLiteral("DEM"), makeVisible);
    if (m_btnDem)
      m_btnDem->setChecked(makeVisible);

    if (makeVisible) {
      LayerOps::ensureDemRelief(proj, dem);
      DemPresentation::followCanvas(dem, m_canvas);
    }
    if (!makeVisible && !LayerOps::isLayerVisible(proj, QStringLiteral("지질도"))) {
      LayerOps::toggleLayerVisibility(proj, m_canvas, QStringLiteral("지형 음영"), false);
    }
    if (!m_canvas->isDrawing()) {
      LayerOps::syncMapCanvas(proj, m_canvas, false);
      m_canvas->refresh();
    }
    statusBar()->showMessage(
        makeVisible ? QStringLiteral("DEM을 켰습니다. 「DEM 표현」에서 색과 음영을 조절하세요.")
                    : QStringLiteral("DEM 지형을 숨겼습니다."),
        4000);
    return;
  }

  startDemDownload();
#else
  QMessageBox::information(this, QStringLiteral("스텁"), QStringLiteral("DEM 시뮬레이션"));
#endif
}

void MainWindow::startPaleoLandform() {
#if KA_HGIS_HAS_QGIS
  if (m_isOpeningSurvey || m_closingWindow) return;
  if (m_surveyPath.isEmpty()) {
    QMessageBox::information(this, QStringLiteral("고지형"),
                             QStringLiteral("먼저 「새 조사」로 저장 위치를 만드세요.\n"
                                            "그다음 조사지역으로 확대한 뒤 다시 「고지형」을 누르면 "
                                            "흙토람 분포지형이 깔립니다."));
    return;
  }
  if (!m_canvas) return;
  QgsProject* proj = QgsProject::instance();
  LayerOps::clampCanvasToThematicScale(m_canvas);

  QgsVectorLayer* soil = PaleoLandformService::findSoilTerrainLayer(proj);
  if (!soil) {
    QgsRectangle ext = m_canvas->extent();
    const QgsCoordinateReferenceSystem crs5186(QStringLiteral("EPSG:5186"));
    const QgsCoordinateReferenceSystem canvasCrs = m_canvas->mapSettings().destinationCrs();
    if (canvasCrs.isValid() && canvasCrs != crs5186) {
      try {
        const QgsCoordinateTransform tr(canvasCrs, crs5186, QgsProject::instance());
        ext = tr.transformBoundingBox(ext);
      } catch (const QgsException&) {
        QMessageBox::warning(this, QStringLiteral("고지형"),
                             QStringLiteral("화면 범위를 좌표 변환하지 못했습니다."));
        return;
      }
    }
    if (ext.width() > SoilMapService::maxSpanMeters() ||
        ext.height() > SoilMapService::maxSpanMeters()) {
      QMessageBox::information(
          this, QStringLiteral("고지형"),
          QStringLiteral("지금 화면이 너무 넓습니다. 조사지역(한 변 %1km 이하)으로 확대한 뒤 "
                         "다시 「고지형」을 누르세요.\n"
                         "전국·시도 화면에는 분포지형을 깔지 않습니다.")
              .arg(SoilMapService::maxSpanMeters() / 1000.0, 0, 'f', 0));
      return;
    }
    const QString dir = QFileInfo(m_surveyPath).absolutePath();
    const QString outGpkg = QDir(dir).filePath(QStringLiteral("토양도_흙토람.gpkg"));
    startReferenceDownload(ReferenceMapKind::PaleoSoil, ext, outGpkg);
    return; // 같은 조사의 토양도가 준비된 뒤 판독 작업을 이어 간다.
  }
  PaleoLandformService::applyCandidateEmphasis(soil);
  if (QgsLayerTreeLayer* n = proj->layerTreeRoot()->findLayer(soil->id()))
    n->setItemVisibilityChecked(true);
  if (m_layerTree) m_layerTree->setCurrentLayer(soil);

  QString err;
  QgsVectorLayer* layer =
      PaleoLandformService::ensureInterpretationLayer(proj, m_surveyPath, &err);
  if (!layer) {
    notify(Notice::Warning, QStringLiteral("고지형"),
           err.isEmpty() ? QStringLiteral("판독 레이어를 만들지 못했습니다.") : err);
    return;
  }
  const PaleoLandformService::SeedResult seeded =
      PaleoLandformService::seedInterpretationFromSoil(soil, layer, &err);
  beginEdit(layer);
  if (seeded.added > 0) {
    notify(Notice::Success, QStringLiteral("고지형"),
           QStringLiteral("흙토람 분포지형에서 가설 %1개를 자동으로 깔았습니다. 확정이 아닙니다.")
               .arg(seeded.added));
    QMessageBox::information(
        this, QStringLiteral("고지형"),
        QStringLiteral("흙토람 분포지형에서 가설 면을 자동으로 깔았습니다.\n\n"
                       "선상지 · 해성평탄 · 하안단구는 토양 구분을 옮긴 것입니다.\n"
                       "하성평탄은 안쪽을 구하도, 가장자리를 자연제방 가설로 나눕니다.\n\n"
                       "옛 지형이 자동으로 복원된 것은 아닙니다. "
                       "틀린 면은 지우고 고치세요. 확정이 아닙니다."));
    statusBar()->showMessage(
        QStringLiteral("고지형 가설 %1개 — 확정 아님. 좌클릭으로 추가, 우클릭으로 완료")
            .arg(seeded.added),
        0);
  } else {
    notify(Notice::Success, QStringLiteral("고지형"),
           QStringLiteral("분포지형을 올렸습니다. 진한 색이 입지 후보"
                          "(곡간·선상·해성·하성평탄·홍적대지)입니다."));
    QMessageBox::information(
        this, QStringLiteral("고지형"),
        QStringLiteral("지도에 색 면이 생겼으면 그게 고지형입니다.\n\n"
                       "진한 색 = 유적 입지 후보 (곡간·선상·해성평탄·하성평탄·홍적대지)\n"
                       "연한 색 = 산지·구릉 등\n\n"
                       "옛 지형이 자동으로 복원된 것은 아닙니다. "
                       "이제 그 위에 구하도·자연제방 가설을 그리세요. 확정이 아닙니다."));
    statusBar()->showMessage(
        QStringLiteral("고지형 가설 — 진한 색이 입지 후보. 좌클릭으로 그리고 우클릭으로 완료"), 0);
  }
#else
  QMessageBox::information(this, QStringLiteral("스텁"), QStringLiteral("고지형 시뮬레이션"));
#endif
}

// 흙토람 공개 지도서버(국립농업과학원 GeoServer)에서 현재 화면 범위의
// 정밀토양도를 내려받아 분포지형 공식 색으로 겹친다. 신청·키가 필요 없다.
void MainWindow::downloadSoilTerrain() {
#if KA_HGIS_HAS_QGIS
  if (!m_canvas) return;
  // 본체 클릭만 토글. 화살표 메뉴의 「내려받기」는 다시 받는다.
  if (!qobject_cast<QAction*>(sender())) {
    if (toggleExistingOverlay(QgsProject::instance(), m_canvas, QStringLiteral("토양도(흙토람)"),
                              nullptr, statusBar())) {
      if (m_btnSoil)
        m_btnSoil->setChecked(
            LayerOps::isLayerVisible(QgsProject::instance(), QStringLiteral("토양도(흙토람)")));
      return;
    }
  }
  if (LayerOps::clampCanvasToThematicScale(m_canvas))
    statusBar()->showMessage(QStringLiteral("축척을 1:100000으로 맞춘 뒤 토양도를 받습니다."), 4000);

  QgsRectangle ext = m_canvas->extent();
  const QgsCoordinateReferenceSystem crs5186(QStringLiteral("EPSG:5186"));
  const QgsCoordinateReferenceSystem canvasCrs = m_canvas->mapSettings().destinationCrs();
  if (canvasCrs.isValid() && canvasCrs != crs5186) {
    try {
      const QgsCoordinateTransform tr(canvasCrs, crs5186, QgsProject::instance());
      ext = tr.transformBoundingBox(ext);
    } catch (const QgsException&) {
      notify(Notice::Critical, QStringLiteral("토양도"),
             QStringLiteral("화면 범위를 좌표 변환하지 못했습니다."));
      return;
    }
  }
  if (ext.width() > SoilMapService::maxSpanMeters() ||
      ext.height() > SoilMapService::maxSpanMeters()) {
    notify(Notice::Warning, QStringLiteral("토양도"),
           QStringLiteral("범위가 너무 넓습니다. 지도를 조사지역(한 변 %1km 이하)으로 "
                          "확대한 뒤 다시 내려받으세요.")
               .arg(SoilMapService::maxSpanMeters() / 1000.0, 0, 'f', 0));
    return;
  }

  // 조사 GPKG 옆에 저장해 다음에도(오프라인 포함) 다시 쓸 수 있게 한다.
  const QString dir = m_surveyPath.isEmpty() ? QDir::tempPath()
                                             : QFileInfo(m_surveyPath).absolutePath();
  const QString outGpkg = QDir(dir).filePath(QStringLiteral("토양도_흙토람.gpkg"));

  startReferenceDownload(ReferenceMapKind::Soil, ext, outGpkg);
#else
  QMessageBox::information(this, QStringLiteral("스텁"),
                           QStringLiteral("토양도 내려받기 시뮬레이션"));
#endif
}

// KIGAM 공개 지도서버에서 현재 화면 범위의 1:5만 지질도(암상)를 내려받아
// 지질시대별 ICS 표준색 + 암상 기호 라벨로 겹친다. 신청·키가 필요 없다.
void MainWindow::downloadGeologyMap() {
#if KA_HGIS_HAS_QGIS
  if (!m_canvas) return;
  if (toggleExistingOverlay(QgsProject::instance(), m_canvas,
                            QStringLiteral("지질도(KIGAM 1:5만)"), m_actGeology, statusBar())) {
    QgsProject* proj = QgsProject::instance();
    const bool on = LayerOps::isLayerVisible(proj, QStringLiteral("지질도(KIGAM 1:5만)"));
    if (on) {
      if (QgsMapLayer* geo = GeologyMapService::existingGeologyLayer(proj))
        GeologyMapService::ensureReliefUnderlay(proj, m_canvas, geo, nullptr);
      if (m_canvas && !m_canvas->isDrawing())
        LayerOps::syncMapCanvas(proj, m_canvas, false);
    }
    LayerOps::toggleLayerVisibility(proj, m_canvas,
                                    GeologyMapService::reliefLayerTitle(), on);
    return;
  }
  if (LayerOps::clampCanvasToThematicScale(m_canvas))
    statusBar()->showMessage(QStringLiteral("축척을 1:100000으로 맞춘 뒤 지질도를 받습니다."), 4000);

  QgsRectangle ext = m_canvas->extent();
  const QgsCoordinateReferenceSystem crs5186(QStringLiteral("EPSG:5186"));
  const QgsCoordinateReferenceSystem canvasCrs = m_canvas->mapSettings().destinationCrs();
  if (canvasCrs.isValid() && canvasCrs != crs5186) {
    try {
      const QgsCoordinateTransform tr(canvasCrs, crs5186, QgsProject::instance());
      ext = tr.transformBoundingBox(ext);
    } catch (const QgsException&) {
      notify(Notice::Critical, QStringLiteral("지질도"),
             QStringLiteral("화면 범위를 좌표 변환하지 못했습니다."));
      return;
    }
  }
  if (ext.width() > GeologyMapService::maxSpanMeters() ||
      ext.height() > GeologyMapService::maxSpanMeters()) {
    notify(Notice::Warning, QStringLiteral("지질도"),
           QStringLiteral("범위가 너무 넓습니다. 지도를 조사지역(한 변 %1km 이하)으로 "
                          "확대한 뒤 다시 내려받으세요.")
               .arg(GeologyMapService::maxSpanMeters() / 1000.0, 0, 'f', 0));
    return;
  }

  const QString dir = m_surveyPath.isEmpty() ? QDir::tempPath()
                                             : QFileInfo(m_surveyPath).absolutePath();
  const QString outGpkg = QDir(dir).filePath(QStringLiteral("지질도_KIGAM.gpkg"));

  startReferenceDownload(ReferenceMapKind::Geology, ext, outGpkg);
#else
  QMessageBox::information(this, QStringLiteral("스텁"),
                           QStringLiteral("지질도 내려받기 시뮬레이션"));
#endif
}

// VWorld 공개 WFS에서 현재 화면 범위의 하천망(국가·지방하천)을 내려받아
// 등급별 물색 + 하천명 라벨로 겹친다. 배경지도와 같은 VWorld 키를 쓴다.
void MainWindow::downloadRiverMap() {
#if KA_HGIS_HAS_QGIS
  if (!m_canvas) return;
  if (toggleExistingOverlay(QgsProject::instance(), m_canvas, QStringLiteral("수계도(하천망)"),
                            m_actRiver, statusBar()))
    return;
  if (LayerOps::clampCanvasToThematicScale(m_canvas))
    statusBar()->showMessage(QStringLiteral("축척을 1:100000으로 맞춘 뒤 수계도를 받습니다."), 4000);

  const QString key = VworldSettings::loadApiKey();
  if (key.trimmed().isEmpty()) {
    notify(Notice::Warning, QStringLiteral("수계도"),
           QStringLiteral("VWorld 인증키가 없습니다. 배경지도 설정에서 키를 먼저 "
                          "등록하세요."));
    return;
  }

  QgsRectangle ext = m_canvas->extent();
  const QgsCoordinateReferenceSystem crs5186(QStringLiteral("EPSG:5186"));
  const QgsCoordinateReferenceSystem canvasCrs = m_canvas->mapSettings().destinationCrs();
  if (canvasCrs.isValid() && canvasCrs != crs5186) {
    try {
      const QgsCoordinateTransform tr(canvasCrs, crs5186, QgsProject::instance());
      ext = tr.transformBoundingBox(ext);
    } catch (const QgsException&) {
      notify(Notice::Critical, QStringLiteral("수계도"),
             QStringLiteral("화면 범위를 좌표 변환하지 못했습니다."));
      return;
    }
  }
  if (ext.width() > RiverMapService::maxSpanMeters() ||
      ext.height() > RiverMapService::maxSpanMeters()) {
    notify(Notice::Warning, QStringLiteral("수계도"),
           QStringLiteral("범위가 너무 넓습니다. 지도를 조사지역(한 변 %1km 이하)으로 "
                          "확대한 뒤 다시 내려받으세요.")
               .arg(RiverMapService::maxSpanMeters() / 1000.0, 0, 'f', 0));
    return;
  }

  const QString dir = m_surveyPath.isEmpty() ? QDir::tempPath()
                                             : QFileInfo(m_surveyPath).absolutePath();
  const QString outGpkg = QDir(dir).filePath(QStringLiteral("수계도_VWorld.gpkg"));

  startReferenceDownload(ReferenceMapKind::River, ext, outGpkg, key);
#else
  QMessageBox::information(this, QStringLiteral("스텁"),
                           QStringLiteral("수계도 내려받기 시뮬레이션"));
#endif
}

void MainWindow::startReferenceDownload(ReferenceMapKind kind, const QgsRectangle& extent,
                                        const QString& targetPath, const QString& apiKey) {
  if (m_isOpeningSurvey || m_closingWindow) return;
  if (m_referenceDownload) {
    statusBar()->showMessage(QStringLiteral("지도 자료를 내려받고 있습니다. 완료를 기다리거나 취소하세요."), 5000);
    return;
  }
  const QString title = (kind == ReferenceMapKind::Soil || kind == ReferenceMapKind::PaleoSoil) ? QStringLiteral("토양도")
      : kind == ReferenceMapKind::Geology ? QStringLiteral("지질도") : QStringLiteral("수계도");
  auto* progress = createDownloadProgress(title);
  const auto context = QgsProject::instance()->transformContext();
  const quint64 generation = m_surveyGeneration;
  const QPointer<MainWindow> window(this);
  const QPointer<QProgressDialog> dialog(progress);
  auto prepare = [kind, extent, targetPath, apiKey, context](QgsFeedback* feedback) {
    switch (kind) {
      case ReferenceMapKind::Soil:
      case ReferenceMapKind::PaleoSoil:
        return SoilMapService::prepare(extent, targetPath, context, feedback);
      case ReferenceMapKind::Geology:
        return GeologyMapService::prepare(extent, targetPath, context, feedback);
      case ReferenceMapKind::River:
        return RiverMapService::prepare(extent, apiKey, targetPath, context, feedback);
    }
    return PreparedReferenceMap{};
  };
  auto complete = [window, dialog, generation, kind, title](const PreparedReferenceMap& result) {
    if (dialog) { dialog->hide(); dialog->deleteLater(); }
    if (!window) return;
    window->m_referenceDownload = nullptr;
    if (window->m_closingWindow || generation != window->m_surveyGeneration) return;
    if (result.status == PreparedReferenceMap::Status::Cancelled) {
      window->statusBar()->showMessage(title + QStringLiteral(" 내려받기를 취소했습니다. 기존 지도는 유지됩니다."), 6000);
      return;
    }
    if (!result.isReady()) {
      window->notify(Notice::Warning, title + QStringLiteral(" 내려받기 실패"), result.error.isEmpty()
          ? QStringLiteral("지도 자료를 받지 못했습니다. 인터넷 연결을 확인한 뒤 다시 시도하세요.") : result.error);
      return;
    }
    try {
      QString error;
      QgsMapLayer* layer = nullptr;
      switch (kind) {
        case ReferenceMapKind::Soil:
        case ReferenceMapKind::PaleoSoil:
          layer = SoilMapService::addPrepared(QgsProject::instance(), window->m_canvas, result, &error); break;
        case ReferenceMapKind::Geology:
          layer = GeologyMapService::addPrepared(QgsProject::instance(), window->m_canvas, result, &error); break;
        case ReferenceMapKind::River:
          layer = RiverMapService::addPrepared(QgsProject::instance(), window->m_canvas, result, &error); break;
      }
      if (!window || window->m_closingWindow || generation != window->m_surveyGeneration) return;
      if (!layer) {
        window->notify(Notice::Warning, title + QStringLiteral(" 표시 실패"), error.isEmpty()
            ? QStringLiteral("받은 지도를 열지 못했습니다. 기존 지도는 유지됩니다. 다시 내려받으세요.") : error);
        return;
      }
      if (window->m_layerTree) window->m_layerTree->setCurrentLayer(layer);
      if ((kind == ReferenceMapKind::Soil || kind == ReferenceMapKind::PaleoSoil) && window->m_btnSoil)
        window->m_btnSoil->setChecked(true);
      if (kind == ReferenceMapKind::Geology && window->m_actGeology) window->m_actGeology->setChecked(true);
      if (kind == ReferenceMapKind::River && window->m_actRiver) window->m_actRiver->setChecked(true);
      QgsProject::instance()->setDirty(true);
      window->statusBar()->showMessage(title + QStringLiteral("를 추가했습니다. 조사 저장으로 지도 구성을 보관하세요."), 10000);
      if (!result.warnings.isEmpty())
        window->notify(Notice::Warning, title + QStringLiteral(" 확인 사항"), result.warnings.join(QLatin1Char('\n')));
      if (kind == ReferenceMapKind::PaleoSoil) {
        QTimer::singleShot(0, window, [window, generation] {
          if (window && !window->m_closingWindow && generation == window->m_surveyGeneration)
            window->startPaleoLandform();
        });
      }
    } catch (...) {
      QgsProject::instance()->setDirty(true);
      window->refreshWindowTitle();
      KaCrashGuard::logLine(QStringLiteral("[reference] 받은 지도 표시 중 예외 — 현재 작업 유지"));
      window->notify(Notice::Warning, title + QStringLiteral(" 표시 실패"),
                     QStringLiteral("받은 지도를 표시하는 중 오류가 발생했습니다. 현재 작업을 저장하고 "
                                    "레이어 목록을 확인한 뒤 다시 내려받으세요."));
    }
  };
  auto* job = new KaReferenceDownloadJob(title, std::move(prepare), std::move(complete));
  m_referenceDownload = job;
  connect(progress, &QProgressDialog::canceled, job, &KaReferenceDownloadJob::cancel);
  progress->show();
  QgsApplication::taskManager()->addTask(job);
}

// 레이어 점호. 상태가 직전과 다르면 무엇이 어떻게 달라졌는지 로그에 남기고,
// 원본이 잠깐 끊겨 무효가 된 것은 다시 연다.

// 제목 뒤 " *" 하나로 "아직 저장 안 됨"을 보여 준다. 제목을 세우는 곳이 아홉 군데라
// 별도 필드를 두지 않고 현재 제목에서 표식만 떼었다 붙인다.

// 캔버스가 "지금 그리는 중"이라고 잡고 있는 레이어와 축척을 남긴다. 헤드리스 렌더로는
// 재현이 안 되는(레이어·좌표계·확대한계 모두 정상인) 화면 전용 현상을 추적하기 위한 것.
// 목록이 직전과 같으면 아무것도 쓰지 않는다 — 팬·줌마다 로그가 폭주하지 않도록.

void MainWindow::searchLocation(const QString& query, bool parcel) {
  if (!m_locator) return;
  const QString q = query.trimmed();
  if (q.isEmpty()) {
    statusBar()->showMessage(QStringLiteral("주소·지번·지역·상호를 입력하세요"), 4000);
    return;
  }
  // 검색 칸이 없어졌으므로 중복 실행은 플래그로 막는다.
  if (m_locationSearchBusy) return;
  m_locationSearchBusy = true;
  statusBar()->showMessage(QStringLiteral("위치 검색 중… %1").arg(q), 0);
  if (m_searchProgress) { m_searchProgress->hide(); m_searchProgress->deleteLater(); }
  m_searchProgress = createDownloadProgress(QStringLiteral("위치 자료"));
  m_searchProgress->setWindowTitle(QStringLiteral("위치 검색"));
  connect(m_searchProgress, &QProgressDialog::canceled, this, [this]() {
    m_locator->cancel();
    m_locationSearchBusy = false;
    if (m_searchProgress) { m_searchProgress->deleteLater(); m_searchProgress = nullptr; }
    statusBar()->showMessage(QStringLiteral("위치 검색을 취소했습니다."), 4000);
  });
  m_searchProgress->show();
  if (parcel) m_locator->searchParcel(q);
  else m_locator->search(q);
}

void MainWindow::applySurfaceSurveyFieldMap(const QString& sido, const QString& city,
                                            const QString& dong) {
#if KA_HGIS_HAS_QGIS
  if (!m_adminBoundary) return;
  statusBar()->showMessage(
      QStringLiteral("읍면동 경계를 받는 중… %1 %2 %3").arg(sido, city, dong), 0);
  if (m_boundaryProgress) { m_boundaryProgress->hide(); m_boundaryProgress->deleteLater(); }
  m_boundaryProgress = createDownloadProgress(QStringLiteral("읍면동 경계"));
  connect(m_boundaryProgress, &QProgressDialog::canceled, this, [this]() {
    m_adminBoundary->cancel();
    if (m_boundaryProgress) { m_boundaryProgress->deleteLater(); m_boundaryProgress = nullptr; }
    statusBar()->showMessage(QStringLiteral("읍면동 경계 내려받기를 취소했습니다."), 4000);
  });
  m_boundaryProgress->show();
  m_adminBoundary->fetchEmd(sido, city, dong);
#else
  Q_UNUSED(sido);
  Q_UNUSED(city);
  Q_UNUSED(dong);
#endif
}

void MainWindow::onAdminBoundaryFailed(const QString& message) {
  if (m_boundaryProgress) { m_boundaryProgress->hide(); m_boundaryProgress->deleteLater(); m_boundaryProgress = nullptr; }
  statusBar()->showMessage(message, 8000);
  QMessageBox::warning(this, QStringLiteral("현장 지도"), message);
}

void MainWindow::onAdminBoundaryFetched(const AdminBoundaryParse& parsed) {
  if (m_boundaryProgress) { m_boundaryProgress->hide(); m_boundaryProgress->deleteLater(); m_boundaryProgress = nullptr; }
#if KA_HGIS_HAS_QGIS
  if (!parsed.ok) {
    onAdminBoundaryFailed(parsed.error);
    return;
  }
  const QgsGeometry geom = QgsGeometry::fromWkt(parsed.wkt);
  if (geom.isEmpty()) {
    onAdminBoundaryFailed(QStringLiteral("읍면동 경계를 읽지 못했습니다"));
    return;
  }
  QgsProject* proj = QgsProject::instance();
  QString satErr;
  const bool satAdded = LayerOps::addVworldSatelliteMap(proj, m_canvas, VworldSettings::loadApiKey(),
                                                        &satErr);
  bool hasSat = satAdded;
  if (!hasSat) {
    for (QgsMapLayer* l : proj->mapLayers()) {
      if (l && l->name().contains(QStringLiteral("위성"))) {
        hasSat = true;
        break;
      }
    }
  }
  if (!hasSat) {
    onAdminBoundaryFailed(satErr.isEmpty() ? QStringLiteral("위성을 올리지 못했습니다") : satErr);
    return;
  }
  QgsVectorLayer* mask = LayerOps::upsertAdminEmdMask(
      QgsProject::instance(), geom, QgsCoordinateReferenceSystem(parsed.crsAuthId), m_workCrs,
      parsed.title);
  if (!mask) {
    onAdminBoundaryFailed(QStringLiteral("읍면동 마스크를 만들지 못했습니다"));
    return;
  }
  QgsVectorLayer* site = LayerOps::findImportedSiteLayer(QgsProject::instance());
  if (!site) {
    QMessageBox::information(
        this, QStringLiteral("현장 지도"),
        QStringLiteral("유적 SHP를 고르세요. 번호·이름 필드가 있으면 라벨로 씁니다."));
    openVectorLayer();
    site = LayerOps::findImportedSiteLayer(QgsProject::instance());
  }
  LayerOps::isolateSurfaceSurveyView(QgsProject::instance(), m_canvas, site);
  LayerOps::ensureSatelliteAtBottom(QgsProject::instance());
  if (m_canvas) {
    LayerOps::zoomToLayerMax(m_canvas, mask);
    LayerOps::syncMapCanvas(QgsProject::instance(), m_canvas, false);
    LayerOps::refreshCanvasIfIdle(m_canvas);
  }
  if (m_drawingStudio)
    m_drawingStudio->repaintMapLayers();
  statusBar()->showMessage(
      QStringLiteral("현장 지도: %1 — 위성은 이 읍면동만, 위는 유적").arg(parsed.title), 8000);
#else
  Q_UNUSED(parsed);
#endif
}

void MainWindow::onLocationFailed(const QString& message) {
  if (m_searchProgress) { m_searchProgress->hide(); m_searchProgress->deleteLater(); m_searchProgress = nullptr; }
  m_locationSearchBusy = false;
  statusBar()->showMessage(message, 8000);
  QMessageBox::information(this, QStringLiteral("위치 검색"), message);
}

void MainWindow::onLocationResults(const QVector<LocationHit>& hits) {
  if (m_searchProgress) { m_searchProgress->hide(); m_searchProgress->deleteLater(); m_searchProgress = nullptr; }
  m_locationSearchBusy = false;
  if (hits.isEmpty()) {
    onLocationFailed(QStringLiteral("검색 결과 없음"));
    return;
  }
  if (hits.size() == 1) {
    zoomToLocation(hits.first());
    return;
  }
  QStringList labels;
  for (const LocationHit& h : hits) {
    QString line = h.title;
    if (!h.detail.isEmpty()) line += QStringLiteral("  —  ") + h.detail;
    labels << line;
  }
  bool ok = false;
  const QString pick = QInputDialog::getItem(
      this, QStringLiteral("위치 선택"),
      QStringLiteral("검색 결과 %1건 — 이동할 위치를 선택하세요").arg(hits.size()),
      labels, 0, false, &ok);
  if (!ok) return;
  const int idx = labels.indexOf(pick);
  if (idx >= 0 && idx < hits.size())
    zoomToLocation(hits.at(idx));
}

// 찾은 자리를 십자 표식으로 남긴다. 다음 검색이 오면 그 자리로 옮긴다.
// 조사 도메인이 아니라 화면 표시일 뿐이라 레이어로 만들지 않는다.
void MainWindow::markFoundLocation(const QgsPointXY& mapPt, const QString& title) {
#if KA_HGIS_HAS_QGIS
  if (!m_canvas) return;
  if (!m_locationMark)
    m_locationMark = new KaFoundLocationMark(m_canvas);
  m_locationMark->setLocation(mapPt, title);
  m_locationMark->show();
  m_locationMarkTitle = title;
#else
  Q_UNUSED(mapPt);
  Q_UNUSED(title);
#endif
}

void MainWindow::clearFoundLocationMark() {
#if KA_HGIS_HAS_QGIS
  if (m_locationMark) {
    delete m_locationMark;
    m_locationMark = nullptr;
  }
  m_locationMarkTitle.clear();
#endif
}

void MainWindow::zoomToLocation(const LocationHit& hit) {
#if KA_HGIS_HAS_QGIS
  if (!m_canvas) return;
  double lon = hit.lon;
  double lat = hit.lat;
  if (qAbs(lon) <= 90.0 && qAbs(lat) > 90.0)
    std::swap(lon, lat);
  if (lon < 120.0 || lon > 135.0 || lat < 30.0 || lat > 45.0) {
    statusBar()->showMessage(
        QStringLiteral("위치 좌표가 한국 범위 밖입니다 (lon=%1 lat=%2)").arg(lon).arg(lat), 8000);
  }

  const QgsCoordinateReferenceSystem wgs(QStringLiteral("EPSG:4326"));
  const QgsCoordinateReferenceSystem dest =
      m_workCrs.isEmpty() ? QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186"))
                          : QgsCoordinateReferenceSystem(m_workCrs);
  try {
    QgsCoordinateTransform xf(wgs, dest, QgsProject::instance()
                                             ? QgsProject::instance()->transformContext()
                                             : QgsCoordinateTransformContext());
    xf.setBallparkTransformsAreAppropriate(true);
    LayerOps::ensureOtfEnabled(QgsProject::instance(), m_canvas, dest.authid());

    QgsPointXY p = xf.transform(QgsPointXY(lon, lat));
    m_startupViewApplied = true; // Explicit navigation wins over queued initial framing.
    showMapWorkspace();
    LayerOps::syncMapCanvas(QgsProject::instance(), m_canvas, false);
    const bool localParcel = !hit.pnu.isEmpty()
        && CadastralImport::focusParcel(QgsProject::instance(), m_canvas, hit.pnu, &p);
    const bool useBounds = hit.hasBbox && hit.west != 0.0 && hit.east != 0.0;
    if (localParcel) {
      // focusParcel has already framed the actual cadastral geometry.
    } else if (useBounds) {
      QgsRectangle r(hit.west, hit.south, hit.east, hit.north);
      r = xf.transformBoundingBox(r);
      r.scale(1.2);
      m_canvas->setExtent(r);
    } else {
      const double pad = dest.authid().contains(QLatin1String("4326")) ? 0.004 : 400.0;
      m_canvas->setExtent(QgsRectangle(p.x() - pad, p.y() - pad, p.x() + pad, p.y() + pad));
    }
    if (!localParcel && !useBounds && m_canvas->scale() > 8000.0)
      m_canvas->zoomScale(3000.0, true);
    LayerOps::clampCanvasToKorea(m_canvas);
    markFoundLocation(p, hit.title);
    LayerOps::refreshCanvasIfIdle(m_canvas);
    statusBar()->showMessage(
        QStringLiteral("이동: %1  (lon %2, lat %3 → %4)")
            .arg(hit.title)
            .arg(lon, 0, 'f', 5)
            .arg(lat, 0, 'f', 5)
            .arg(dest.authid()),
        10000);
  } catch (const QgsCsException& e) {
    QMessageBox::warning(this, QStringLiteral("좌표 변환"), e.what());
  } catch (...) {
    KaCrashGuard::logLine(QStringLiteral("[except] app/MainWindow.cpp:8404"));
    QMessageBox::warning(this, QStringLiteral("위치"), QStringLiteral("좌표 변환 실패"));
  }
#else
  Q_UNUSED(hit);
#endif
}

void MainWindow::setupFileBrowser() {
  auto* view = new FileListView(this);
  m_fileBrowser = view;
  m_fileBrowser->setObjectName(QStringLiteral("fileBrowser"));
  connect(m_fileBrowser, &QListWidget::itemDoubleClicked, this, &MainWindow::onFileBrowserActivated);
  goFileBrowserRoot(QString());
}

QString MainWindow::resolvedDesktopPath() {
  static QString cached;
  if (!cached.isEmpty() && QFileInfo(cached).isDir())
    return cached;
  const QStringList candidates = {
      QStandardPaths::writableLocation(QStandardPaths::DesktopLocation),
      QDir::homePath() + QStringLiteral("/Desktop"),
      QDir::homePath() + QStringLiteral("/OneDrive/Desktop"),
      QDir::homePath() + QStringLiteral("/OneDrive/바탕 화면"),
  };
  for (const QString& c : candidates) {
    if (c.isEmpty()) continue;
    const QFileInfo fi(c);
    if (fi.exists() && fi.isDir()) {
      cached = QDir::cleanPath(fi.absoluteFilePath());
      return cached;
    }
  }
  cached = QDir::homePath();
  return cached;
}

void MainWindow::goFileBrowserRoot(const QString& path) {
  if (!m_fileBrowser) return;
  m_fileBrowser->clear();

  QString p = QDir::fromNativeSeparators(path.trimmed());
  if (p.length() == 2 && p[1] == QLatin1Char(':'))
    p += QLatin1Char('/');

  auto addRow = [this](const QString& label, const QString& full, bool isDir) {
    auto* it = new QListWidgetItem(label);
    it->setData(Qt::UserRole, full);
    it->setData(Qt::UserRole + 1, isDir);
    it->setToolTip(QDir::toNativeSeparators(full));
    m_fileBrowser->addItem(it);
  };

  if (p.isEmpty()) {
    m_browserPath.clear();
    const QFileInfoList drives = QDir::drives();
    for (const QFileInfo& d : drives)
      addRow(QDir::toNativeSeparators(d.absoluteFilePath()),
             QDir::fromNativeSeparators(d.absoluteFilePath()), true);
    statusBar()->showMessage(QStringLiteral("드라이브 목록 — 폴더를 더블클릭하세요"), 5000);
    return;
  }

  p = QDir::cleanPath(p);
  const QFileInfo fi(p);
  if (!fi.exists() || !fi.isDir()) {
    statusBar()->showMessage(QStringLiteral("폴더 없음 → 드라이브 목록"), 5000);
    goFileBrowserRoot(QString());
    return;
  }
  m_browserPath = QDir::cleanPath(fi.absoluteFilePath());

  QDir dir(m_browserPath);
  dir.setFilter(QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks);
  dir.setSorting(QDir::Name | QDir::IgnoreCase);
  const QStringList folders = dir.entryList();
  int n = 0;
  for (const QString& name : folders) {
    if (n >= 250) break;
    if (name.compare(QLatin1String("$Recycle.Bin"), Qt::CaseInsensitive) == 0 ||
        name.compare(QLatin1String("System Volume Information"), Qt::CaseInsensitive) == 0)
      continue;
    addRow(QStringLiteral("[폴더] ") + name, dir.absoluteFilePath(name), true);
    ++n;
  }
  dir.setFilter(QDir::Files | QDir::NoSymLinks);
  dir.setNameFilters({QStringLiteral("*.shp"), QStringLiteral("*.dxf"), QStringLiteral("*.dwg"),
                      QStringLiteral("*.gpkg"), QStringLiteral("*.geojson"), QStringLiteral("*.json"),
                      QStringLiteral("*.tif"), QStringLiteral("*.tiff"), QStringLiteral("*.gtiff"),
                      QStringLiteral("*.jpg"), QStringLiteral("*.jpeg"), QStringLiteral("*.png")});
  const QStringList files = dir.entryList();
  for (const QString& name : files) {
    if (n >= 400) break;
    addRow(name, dir.absoluteFilePath(name), false);
    ++n;
  }
  statusBar()->showMessage(
      QStringLiteral("경로: %1").arg(QDir::toNativeSeparators(m_browserPath)), 6000);
}

void MainWindow::browseDataFolder() {
  const QString dir = QFileDialog::getExistingDirectory(
      this, QStringLiteral("조사 데이터 폴더 선택"),
      QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation));
  if (!dir.isEmpty())
    goFileBrowserRoot(dir);
}

void MainWindow::onFileBrowserActivated(QListWidgetItem* item) {
  if (!item) return;
  const QString path = item->data(Qt::UserRole).toString();
  const bool isDir = item->data(Qt::UserRole + 1).toBool();
  if (path.isEmpty()) return;
  if (isDir) {
    goFileBrowserRoot(path);
    return;
  }
  const QString low = path.toLower();
  const bool raster = GeorefService::isImagePath(path);
  if (raster ? !addRasterFromPath(path) : !addVectorFromPath(path)) {
    KaUserError::warn(this, {
        QStringLiteral("파일"),
        QStringLiteral("선택한 파일을 지도 레이어로 열지 못했습니다."),
        QStringLiteral("SHP/DXF/DWG/GPKG/GeoTIFF/JPG만 지도에 올릴 수 있습니다.\n%1")
            .arg(QDir::toNativeSeparators(path)),
        QStringLiteral("지원 형식인지 확인한 뒤 다시 열어 주세요. DWG는 DXF로 저장해 보세요."),
    });
  } else {
    if (m_layersCard && !m_layersCard->isVisible())
      m_layersCard->setVisible(true);
  }
}

QStringList MainWindow::selectedBrowserFiles() const {
  if (m_filesPanel)
    return m_filesPanel->selectedFiles();
  QStringList out;
  if (!m_fileBrowser) return out;
  const auto items = m_fileBrowser->selectedItems();
  for (QListWidgetItem* it : items) {
    if (!it || it->data(Qt::UserRole + 1).toBool()) continue;
    const QString p = it->data(Qt::UserRole).toString();
    if (!p.isEmpty()) out.append(p);
  }
  return out;
}

bool MainWindow::tryAddDroppedUrls(const QList<QUrl>& urls) {
  QStringList paths;
  for (const QUrl& u : urls) {
    if (u.isLocalFile()) paths.append(u.toLocalFile());
  }
  return tryAddDroppedPaths(paths);
}

bool MainWindow::tryAddDroppedPaths(const QStringList& paths) {
  int n = 0;
  for (const QString& path : paths) {
    const QString low = path.toLower();
    if (!(low.endsWith(QLatin1String(".shp")) || low.endsWith(QLatin1String(".dxf")) ||
          low.endsWith(QLatin1String(".dwg")) || low.endsWith(QLatin1String(".gpkg")) ||
          low.endsWith(QLatin1String(".geojson")) || low.endsWith(QLatin1String(".json")) ||
          GeorefService::isImagePath(path)))
      continue;
    const bool raster = GeorefService::isImagePath(path);
    if (raster ? addRasterFromPath(path) : addVectorFromPath(path))
      ++n;
  }
  if (n > 0) {
    if (m_layersCard && !m_layersCard->isVisible())
      m_layersCard->setVisible(true);
    statusBar()->showMessage(QStringLiteral("레이어 %1개 추가됨 (파일→지도)").arg(n), 5000);
  }
  return n > 0;
}

bool MainWindow::addSectionGeoTiffFromPath(const QString& path, const QString& crsAuthId) {
#if KA_HGIS_HAS_QGIS
  const QString title = QFileInfo(path).completeBaseName();
  auto* rl = new QgsRasterLayer(path, title, QStringLiteral("gdal"));
  if (!rl || !rl->isValid()) {
    delete rl;
    return false;
  }
  const QString crsLabel = crsAuthId.isEmpty()
      ? QStringLiteral("EPSG:5187") : crsAuthId;
  rl->setCustomProperty(QStringLiteral("ka_hgis/section_raster"), true);
  rl->setCustomProperty(QStringLiteral("ka_hgis/section_crs_label"), crsLabel);
  // 범례·캔버스에 넣지 않음. 단면도 탭 전용 (위성·지적과 섞지 않음).
  QgsProject::instance()->addMapLayer(rl, false);
  statusBar()->showMessage(
      QStringLiteral("단면 GeoTIFF 추가: %1 · %2").arg(title, crsLabel), 8000);
  return true;
#else
  Q_UNUSED(path);
  Q_UNUSED(crsAuthId);
  return false;
#endif
}

bool MainWindow::addRasterFromPath(const QString& path) {
#if KA_HGIS_HAS_QGIS
  const QString title = QFileInfo(path).completeBaseName();
  auto* rl = new QgsRasterLayer(path, title, QStringLiteral("gdal"));
  if (!rl || !rl->isValid()) {
    delete rl;
    return false;
  }
  const bool unrefImage = GeorefService::isImagePath(path)
                          && GeorefService::looksUnreferencedRaster(rl);
  if (unrefImage)
    LayerOps::setAlignPending(rl, true);
  else if (!rl->crs().isValid() && QgsProject::instance() && QgsProject::instance()->crs().isValid()) {
    rl->setCrs(QgsProject::instance()->crs());
    statusBar()->showMessage(
        QStringLiteral("GeoTIFF에 좌표계가 없어 작업 좌표계(%1)로 올렸습니다.").arg(m_workCrs), 8000);
  }
  const bool geotiff = path.toLower().endsWith(QLatin1String(".tif"))
                       || path.toLower().endsWith(QLatin1String(".tiff"))
                       || path.toLower().endsWith(QLatin1String(".gtiff"));
  LayerOps::markReferenceLayer(rl);
  rl->setCustomProperty(QStringLiteral("ka_hgis/imported_reference"), true);
  LayerOps::applyLegendCrsLabel(rl);
  QgsProject::instance()->addMapLayer(rl, true);
  if (geotiff)
    LayerOps::knockOutRasterPaper(rl);
  if (m_layerTree) m_layerTree->setCurrentLayer(rl);
  if (unrefImage) {
    if (QgsLayerTreeLayer* n = QgsProject::instance()->layerTreeRoot()->findLayer(rl->id()))
      n->setItemVisibilityChecked(false);
  }
  LayerOps::ensureOtfEnabled(QgsProject::instance(), m_canvas, m_workCrs);
  LayerOps::pruneEmptyLegendGroups(QgsProject::instance());
  refreshLayerEmptyState();
  if (m_canvas)
    LayerOps::syncMapCanvas(QgsProject::instance(), m_canvas, false);
  if (unrefImage) {
    statusBar()->showMessage(
        QStringLiteral("좌표 없는 그림입니다. 더보기 → 맞추기로 지적 위에 올리세요."), 10000);
    return true;
  }
  if (m_canvas && geotiff && layerSitsOnWorkMap(rl, m_workCrs)) {
    if (!LayerOps::zoomToLayerMax(m_canvas, rl))
      statusBar()->showMessage(QStringLiteral("그림은 올렸지만 범위를 잡지 못했습니다."), 8000);
  }
  const QString crs = rl->crs().isValid() ? rl->crs().authid() : m_workCrs;
  statusBar()->showMessage(
      QStringLiteral("그림 추가: %1 · %2 · 화면 %3").arg(title, crs, m_workCrs), 10000);
  return true;
#else
  Q_UNUSED(path);
  return false;
#endif
}

bool MainWindow::addVectorFromPath(const QString& path) {
#if KA_HGIS_HAS_QGIS
  LayerOps::prepareShapefileEncoding(path);
  const QString baseTitle = QFileInfo(path).completeBaseName();
  QList<QgsVectorLayer*> added;
  const QList<QgsProviderSublayerDetails> subs =
      QgsProviderRegistry::instance()->querySublayers(path);
  auto takeLayer = [&](QgsVectorLayer* layer, const QString& title) {
    if (!layer || !layer->isValid()) {
      delete layer;
      return;
    }
    // 한글 필드명 깨짐(\uFFFD) 자동 감지 및 CP949 복구
    bool hasGarbled = false;
    for (const QgsField& f : layer->fields()) {
      if (f.name().contains(QChar(0xFFFD))) {
        hasGarbled = true;
        break;
      }
    }
    if (hasGarbled) {
      LayerOps::setShapefileEncoding(layer, QStringLiteral("CP949"));
    }
    layer->setName(title);
    if (GeorefService::isCadPath(path)) {
      LayerOps::markReferenceLayer(layer);
      layer->setCustomProperty(QStringLiteral("ka_hgis/imported_reference"), true);
    } else
      LayerOps::markSurveyLayer(layer, QStringLiteral("user:%1").arg(title));
    LayerOps::applySimpleVectorStyle(layer, QColor(0, 0, 0, 0), QColor(0, 0, 0), 0.2, 3.5, true,
                                     false);
    // SHP 등 벡터 레이어 추가 시 명칭 속성 5PT 자동 라벨링 적용
    const QString nameField = LayerOps::detectNameField(layer);
    if (!nameField.isEmpty()) {
      LayerOps::applyNameAttributeLabels(layer, nameField, 5.0, false);
    }
    LayerOps::applyLegendCrsLabel(layer);
    QgsProject::instance()->addMapLayer(layer, true);
    added.append(layer);
  };
  if (path.endsWith(QLatin1String(".gpkg"), Qt::CaseInsensitive)) {
    bool isSurveyGpkg = false;
    for (const QgsProviderSublayerDetails& d : subs) {
      if (d.name() == QLatin1String("survey_area") || d.name() == QLatin1String("feature_poly") ||
          d.name() == QLatin1String("trial_trench") || d.name() == QLatin1String("control_points")) {
        isSurveyGpkg = true;
        break;
      }
    }
    if (isSurveyGpkg) {
      return openSurveyGpkg(path);
    }
  }
  if (!subs.isEmpty()) {
    for (const QgsProviderSublayerDetails& d : subs) {
      if (d.type() != Qgis::LayerType::Vector) continue;
      QgsProviderSublayerDetails::LayerOptions opt(QgsProject::instance()->transformContext());
      auto* ml = d.toLayer(opt);
      auto* vl = qobject_cast<QgsVectorLayer*>(ml);
      // GPKG 등 멀티 테이블 파일에서 피처가 0개인 빈 테이블은 레전드를 어지럽히지 않도록 추가하지 않는다.
      if (subs.size() > 1 && vl && vl->isValid() && vl->featureCount() == 0) {
        delete vl;
        continue;
      }
      takeLayer(vl, d.name().isEmpty() ? baseTitle : d.name());
    }
  } else {
    takeLayer(new QgsVectorLayer(path, baseTitle, QStringLiteral("ogr")), baseTitle);
  }
  if (added.isEmpty()) {
    const QString low = path.toLower();
    if (low.endsWith(QLatin1String(".dwg")) || low.endsWith(QLatin1String(".dxf"))) {
      KaUserError::warn(this, {
          QStringLiteral("파일"),
          QStringLiteral("이 CAD 파일을 열지 못했습니다."),
          QStringLiteral("DXF는 보통 열리고, DWG는 버전·드라이버에 따라 안 열릴 수 있습니다.\n%1")
              .arg(QDir::toNativeSeparators(path)),
          QStringLiteral("AutoCAD에서 DXF로 저장한 뒤 다시 끌어 넣으세요."),
      });
    }
    return false;
  }
  LayerOps::ensureOtfEnabled(QgsProject::instance(), m_canvas, m_workCrs);
  LayerOps::pruneEmptyLegendGroups(QgsProject::instance());
  if (m_layerTree && !added.isEmpty()) m_layerTree->setCurrentLayer(added.first());
  if (m_canvas) {
    LayerOps::syncMapCanvas(QgsProject::instance(), m_canvas, false);
    QgsVectorLayer* zoomLayer = added.first();
    for (QgsVectorLayer* vl : added) {
      if (vl && vl->featureCount() > 0) {
        zoomLayer = vl;
        break;
      }
    }
    if (!layerSitsOnWorkMap(zoomLayer, m_workCrs)) {
      statusBar()->showMessage(
          QStringLiteral("좌표가 없는 도면입니다. 더보기 → 맞추기로 지적 위에 올리세요."), 10000);
    } else if (!LayerOps::zoomToLayerMax(m_canvas, zoomLayer)) {
      statusBar()->showMessage(
          QStringLiteral("레이어는 추가됐지만 도형이 없습니다: %1").arg(zoomLayer->name()), 8000);
    }
  }
  qint64 feats = 0;
  for (QgsVectorLayer* vl : added)
    feats += vl ? vl->featureCount() : 0;
  statusBar()->showMessage(
      QStringLiteral("레이어 %1개 추가 · 도형 %2개 · 화면 %3")
          .arg(added.size())
          .arg(feats)
          .arg(m_workCrs),
      10000);
  return true;
#else
  Q_UNUSED(path);
  return false;
#endif
}

void MainWindow::showAbout() {
  QMessageBox::about(this, QStringLiteral("정보"),
      QStringLiteral("Strata · 필드고고학 GIS  v") + QLatin1String(KA_HGIS_VERSION) +
      QStringLiteral("\n동국문화재연구원 · 만든이: 권영인 · 조유량 · 박종환\n\n"
                     "QGIS를 포크하지 않고 qgis_core / qgis_gui를 링크합니다.\n"
                     "작업 CRS: EPSG:5186/5187 · 업로드: EPSG:5179\n\n"
                     "저작권·라이선스\n") + KaStartupSplash::attributionText() +
      QStringLiteral("\n선택한 지도에 따라 OpenStreetMap·CARTO·OpenTopoMap·NASA GIBS·"
                     "Copernicus DEM·Google 자료를 사용합니다. 각 제공처의 표시·이용조건을 따릅니다.\n\n"
                     "본 소프트웨어는 GNU GPL v2 이상으로 배포됩니다.\n"
                     "자세한 의존 고지는 앱 폴더의 THIRD_PARTY_NOTICES.md를 봅니다.\n\n") +
      KaCrashGuard::dumpHint());
}

bool MainWindow::configureHeritageAccount() {
  KaHeritageAccountDialog dialog(this);
  return dialog.exec() == QDialog::Accepted && HeritageIntranetSettings::hasCredentials();
}

// 조사구역이 속한 시/군의 국가유산 자료를 받아 온다.
// 버튼 한 번으로 끝나야 하고, 묻는 것은 시/군 판정 확인 하나뿐이다.
void MainWindow::fetchNearbyHeritage() {
  // 막히면 조용히 끝내지 않는다. 왜 못 하는지 창으로 말한다.
  auto stopWith = [this](const QString& why) {
    KaUserError::warn(this, {
        QStringLiteral("주변유적 받기"),
        QStringLiteral("주변유적을 받기 전에 막혔습니다."),
        why,
        QStringLiteral("안내를 확인한 뒤 조건이 되면 다시 「주변유적」을 누르세요."),
    });
  };

  auto* areaVl = LayerOps::findByLayerKey(QgsProject::instance(), QStringLiteral("survey_area"));
  if (!areaVl) {
    stopWith(QStringLiteral("조사구역 레이어가 없습니다. 조사구역을 먼저 그리세요."));
    return;
  }
  const TrenchGridGenerator::PickedArea pick = trenchFillFromSurveyLayer(areaVl);
  QgsGeometry area;
  if (!pick.wkb.isEmpty()) area.fromWkb(pick.wkb);
  if (area.isNull() || area.isEmpty()) {
    stopWith(QStringLiteral("조사구역이 비어 있습니다. 조사구역을 먼저 그리세요.\n"
                            "그린 구역으로 어느 시·군인지 판정합니다."));
    return;
  }
  if (m_surveyPath.isEmpty()) {
    stopWith(QStringLiteral("조사를 먼저 열거나 저장하세요.\n"
                            "받은 자료는 그 조사폴더 안에만 둡니다."));
    return;
  }

  // 창을 먼저 띄운다. 판정이 늦어도 사용자가 아무것도 못 보는 일이 없게 한다.
  ensureHeritageBrowser();
  m_heritageBrowser->show();
  m_heritageBrowser->raise();
  m_heritageBrowser->activateWindow();
  m_heritageBrowser->showWaiting(QStringLiteral("조사구역이 속한 시·군을 찾는 중입니다…"));

  if (!m_heritageResolver) {
    m_heritageResolver = new HeritageRegionResolver(this);
    connect(m_heritageResolver, &HeritageRegionResolver::failed, this, [this](const QString& why) {
      // A previous lookup can still be pending (e.g. a repeated click). Its late
      // result must not restart the operation after the user selects a region.
      m_heritageResolver->cancel();
      if (m_heritageBrowser) m_heritageBrowser->showWaiting(why);
      openHeritageBrowserFor({}, why);
    });
    connect(m_heritageResolver, &HeritageRegionResolver::resolved, this,
            [this](const HeritageRegion& region) { openHeritageBrowserFor(region); });
  }
  m_heritageResolver->resolve(area, areaVl->crs(), QgsProject::instance());
}

// 창과 연결을 한 번만 만든다. 판정 전에도 창을 띄우기 위해 따로 뺐다.
void MainWindow::ensureHeritageBrowser() {
  if (m_heritageBrowser) return;
  m_heritageBrowser = new KaHeritageBrowser(this);
  connect(m_heritageBrowser, &KaHeritageBrowser::failed, this, [this](const QString& why) {
    notify(Notice::Warning, QStringLiteral("주변유적 받기"), why);
  });
  connect(m_heritageBrowser, &KaHeritageBrowser::stageChanged, this,
          [this](HeritageStage, const QString& message) {
            statusBar()->showMessage(message, 6000);
          });
  // 한 종류를 받을 때마다 바로 지도에 올린다. 여섯 종을 다 기다리게 하지 않는다.
  connect(m_heritageBrowser, &KaHeritageBrowser::datasetReady, this,
          [this](HeritageDataset dataset, const QStringList& files) {
            const auto result = importHeritageDataset(dataset, files);
            if (!result.ok())
              m_heritageBrowser->rejectDataset(result.error, result.retryableDownload);
          });
  connect(m_heritageBrowser, &KaHeritageBrowser::allFinished, this, [this]() {
    m_heritageBrowser->hide();
    statusBar()->showMessage(QStringLiteral("주변유적 자료 처리를 마쳤습니다. 받은 자료는 「참조 지도」에 있습니다."), 10000);
  });
  // 서약서 동의는 영수증으로 남긴다. 조용히 지나가지 않는다.
  connect(m_heritageBrowser, &KaHeritageBrowser::agreementAccepted, this,
          [this](const QDateTime& when, const QString& terms) {
            saveHeritageAgreementReceipt(when, terms);
          });
}

// 판정 결과를 확인받고 받기를 시작한다. 시·군 경계에 걸친 조사가 흔해서 이 한 번은 묻는다.
void MainWindow::openHeritageBrowserFor(const HeritageRegion& region, const QString& reason) {
  ensureHeritageBrowser();
  KaHeritageRegionDialog choice(region.sido, region.city, reason, m_heritageBrowser);
  if (choice.exec() != QDialog::Accepted) {
    m_heritageBrowser->showWaiting(QStringLiteral("취소했습니다."));
    return;
  }
  const QString sido = choice.sido();
  const QString city = choice.city();
  if (!HeritageIntranetSettings::hasCredentials() && !configureHeritageAccount()) {
    m_heritageBrowser->showWaiting(QStringLiteral(
        "받기를 시작하지 않았습니다. 국가유산 인트라넷 아이디·비밀번호를 저장한 뒤 다시 눌러 주세요."));
    return;
  }

  // **OneDrive 를 거치지 않는다.** 바탕 화면이 동기화 폴더라 내려받는 중에 가로채여
  // 0바이트로 보이는 일이 있었다(2026-09-12). 받는 자리는 로컬로 고정한다.
  // 조사폴더로 옮기는 것은 적재가 끝난 뒤 사용자가 정한다.
  const QString localBase =
      QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
  const QString root = QDir(localBase).filePath(
      QStringLiteral("주변유적/%1 %2/원본").arg(sido, city));
  if (!QDir().mkpath(root)) {
    KaUserError::warn(this, {
        QStringLiteral("주변유적 받기"),
        QStringLiteral("주변유적 자료를 둘 폴더를 만들지 못했습니다."),
        QStringLiteral("저장 위치의 쓰기 권한이나 공간이 부족할 수 있습니다."),
        QStringLiteral("디스크 공간과 폴더 권한을 확인한 뒤 다시 받아 주세요."),
    });
    return;
  }

  m_heritageBrowser->setDownloadRoot(root);
  m_heritageBrowser->setTarget(sido, city, HeritageStyle::allDatasets());
  m_heritageBrowser->show();
  m_heritageBrowser->raise();
  m_heritageBrowser->start();
}

// 받은 파일을 그 자리에서 지도에 올린다. 색·범례는 HeritageStyle 이 건다.
HeritageImport::Result MainWindow::importHeritageDataset(HeritageDataset dataset, const QStringList& files) {
  if (m_surveyPath.isEmpty()) {
    HeritageImport::Result result;
    result.error = QStringLiteral("열린 조사가 없어 자료를 지도에 올리지 못했습니다.");
    return result;
  }
  // 푸는 자리도 로컬이다. OneDrive 동기화 폴더에서 풀면 파일이 잠기거나 0바이트로 보인다.
  const QString archiveRoot =
      QDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation))
          .filePath(QStringLiteral("주변유적/SHP"));
  QDir().mkpath(archiveRoot);

  const HeritageImport::Result result =
      HeritageImport::loadDataset(QgsProject::instance(), dataset, files, archiveRoot);
  for (const QString& message : result.messages)
    statusBar()->showMessage(message, 8000);
  if (!result.ok()) {
    return result;
  }
  LayerOps::applyLayerOrderToLabels(QgsProject::instance(), m_canvas);
  if (m_canvas)
    LayerOps::syncMapCanvas(QgsProject::instance(), m_canvas, false);
  statusBar()->showMessage(QStringLiteral("%1 %2곳을 올렸습니다.")
                               .arg(HeritageStyle::layerName(dataset))
                               .arg(result.featureCount),
                           8000);
  return result;
}

// 서약서 동의 영수증. 언제 무엇에 동의했는지 남긴다.
void MainWindow::saveHeritageAgreementReceipt(const QDateTime& when, const QString& terms) {
  if (m_surveyPath.isEmpty()) return;
  const QString dir =
      QDir(QFileInfo(m_surveyPath).absolutePath()).filePath(QStringLiteral("주변유적/receipts"));
  if (!QDir().mkpath(dir)) return;
  const QString path = QDir(dir).filePath(
      QStringLiteral("서약서-%1.txt").arg(when.toString(QStringLiteral("yyyyMMdd-HHmmss"))));
  QFile file(path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return;
  QTextStream out(&file);
  out.setEncoding(QStringConverter::Utf8);
  out << QStringLiteral("국가유산 공간정보 원본자료 사용 서약서 동의 기록") << Qt::endl;
  out << QStringLiteral("동의 시각: ") << when.toString(Qt::ISODate) << Qt::endl;
  out << QStringLiteral("기관 계정: ") << HeritageIntranetSettings::describeForLog() << Qt::endl;
  out << QStringLiteral("----") << Qt::endl;
  out << terms << Qt::endl;
}
