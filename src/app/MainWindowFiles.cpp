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
#include "KaMeasureMapTool.h"
#include "core/DemAnalyzer.h"
#include "core/TilePackService.h"
#include "core/TrenchGridGenerator.h"
#include "KaAboveLabelsOverlay.h"
#include "KaCanvasGridOverlay.h"
#include "KaTrenchMoveTool.h"
#include "KaFeatureSelectTool.h"
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
  bool cad = false;  // 도면은 취소·실패해도 그 흐름이 알렸다: 파일함에서 고른 다른 파일로 넘어가지 않는다
  for (const QString& path : paths) {
    const QString low = path.toLower();
    if (!(low.endsWith(QLatin1String(".shp")) || low.endsWith(QLatin1String(".dxf")) ||
          low.endsWith(QLatin1String(".dwg")) || low.endsWith(QLatin1String(".gpkg")) ||
          low.endsWith(QLatin1String(".geojson")) || low.endsWith(QLatin1String(".json")) ||
          GeorefService::isImagePath(path)))
      continue;
    const bool raster = GeorefService::isImagePath(path);
    cad = cad || GeorefService::isCadPath(path);
    if (raster ? addRasterFromPath(path) : addVectorFromPath(path))
      ++n;
  }
  if (n > 0) {
    if (m_layersCard && !m_layersCard->isVisible())
      m_layersCard->setVisible(true);
    statusBar()->showMessage(QStringLiteral("레이어 %1개 추가됨 (파일→지도)").arg(n), 5000);
  }
  return n > 0 || cad;
}

