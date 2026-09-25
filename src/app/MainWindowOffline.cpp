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
