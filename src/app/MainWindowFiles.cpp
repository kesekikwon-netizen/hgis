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

}  // namespace

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

