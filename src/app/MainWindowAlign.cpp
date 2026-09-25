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
