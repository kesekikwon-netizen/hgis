#include <QtTest>
#include <cmath>
#include <QComboBox>
#include <QCheckBox>
#include <QHeaderView>
#include <QLineEdit>
#include <QImage>
#include <QPainter>
#include <QFileDialog>
#include <QFileInfo>
#include <QDirIterator>
#include <QFontDatabase>
#include <QFontMetrics>
#include <QSettings>
#include <QSpinBox>
#include <QScrollArea>
#include <QScrollBar>
#include <QSplitter>
#include <QTemporaryDir>
#include <QTimer>
#include <QTabWidget>
#include <QMessageBox>
#include <QDialogButtonBox>
#include <QLabel>
#include <QMenu>
#include <QPushButton>
#include <QToolButton>
#include <QUrlQuery>
#include <QCryptographicHash>
#include <QSet>
#include <gdal.h>
#include <cpl_error.h>
#include <cpl_conv.h>
#include <memory>

#include "app/MainWindow.h"
#include "app/KaWindowGeometry.h"
#include "app/KaCaptureMapTool.h"
#include "app/KaAttributeMapTool.h"
#include "app/KaFeatureSelectTool.h"
#include "app/KaVertexEditTool.h"
#include "app/KaDrawingStudio.h"
#include "app/KaLayerInformation.h"
#include "app/KaTheme.h"
#include "app/KaRegionLocator.h"
#include "app/KaTopographicBrowser.h"
#include "app/KaTopographicImportDialog.h"
#include "app/KaTopographicScopePanel.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QElapsedTimer>
#include "core/LayerOps.h"
#include "core/RecentSurveys.h"
#include "core/SurveyProjectFactory.h"
#include "core/SurveyStorage.h"
#include "core/DemPresentation.h"
#include "core/DemColorRampLegend.h"
#include "core/HeritageStyle.h"
#include "core/HeritageLayoutNumbers.h"
#include <qgsapplication.h>
#include <qgscategorizedsymbolrenderer.h>
#include <qgsfeature.h>
#include <qgsexpression.h>
#include <qgsexpressioncontext.h>
#include <qgsexception.h>
#include <qgsgeometry.h>
#include <qgscoordinatetransform.h>
#include <qgscolorramplegendnode.h>
#include <qgslayertree.h>
#include <qgslayertreemodel.h>
#include <qgslayertreemodellegendnode.h>
#include <qgslayertreeregistrybridge.h>
#include <qgslayertreeview.h>
#include <qgslayout.h>
#include <qgslayoutitemmap.h>
#include <qgslabelingresults.h>
#include <qgslayoutitemlabel.h>
#include <qgslayoutitemlegend.h>
#include <qgslayoutitemscalebar.h>
#include <qgslayoutexporter.h>
#include <QDoubleSpinBox>
#include <qgslayoutview.h>
#include <qgsmapcanvas.h>
#include <qgsmaplayerstyle.h>
#include <qgsmaptopixel.h>
#include <qgsmessagebar.h>
#include <qgsmessagebaritem.h>
#include <qgsmarkersymbollayer.h>
#include <qgsnetworkaccessmanager.h>
#include <qgsproject.h>
#include <qgsrasterlayer.h>
#include <qgsrastershader.h>
#include <qgssinglebandpseudocolorrenderer.h>
#include <qgssinglesymbolrenderer.h>
#include <qgsfillsymbol.h>
#include <qgsvectorlayer.h>
#include <qgsvectordataprovider.h>
#include <qgsvectorfilewriter.h>
#include <qgsvectorlayerlabeling.h>
#include <qgspallabeling.h>
#include <qgstextbackgroundsettings.h>

static QString s_testSettingsPath;

class TestSaveOpen : public QObject {
  Q_OBJECT
private:
  QTemporaryDir m_files;
  QString makeSurvey(const QString& name, bool registryOnly = false) {
    QString error;
    const QString path = SurveyProjectFactory::createNewSurvey(
        m_files.path(), name, &error, QStringLiteral("EPSG:5187"));
    if (path.isEmpty()) return {};
    QgsProject project;
    project.setTitle(name);
    project.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5187")));
    auto* layer = LayerOps::ensureDomainLayer(&project, path, QStringLiteral("survey_area"),
                                             name, &error);
    if (!layer || !layer->startEditing()) return {};
    QgsFeature feature(layer->fields());
    feature.setAttribute(QStringLiteral("survey_name"), name);
    feature.setGeometry(QgsGeometry::fromRect(QgsRectangle(190000, 560000, 190100, 560100)));
    if (!layer->addFeature(feature) || !layer->commitChanges()) return {};
    layer->setRenderer(new QgsSingleSymbolRenderer(QgsFillSymbol::createSimple(
        {{QStringLiteral("color"), QStringLiteral("217,43,43,255")}}).release()));
    if (registryOnly) {
      // Reproduce field files whose data registry survived but whose legend was lost.
      project.layerTreeRegistryBridge()->setEnabled(false);
      project.layerTreeRoot()->removeLayer(layer);
      project.layerTreeRegistryBridge()->setEnabled(true);
    }
    const QString rasterPath = m_files.filePath(QStringLiteral("background.png"));
    QImage pixel(8, 8, QImage::Format_RGB32);
    pixel.fill(Qt::white);
    if (!pixel.save(rasterPath)) return {};
    QFile worldFile(m_files.filePath(QStringLiteral("background.pgw")));
    if (!worldFile.open(QIODevice::WriteOnly)) return {};
    worldFile.write("1\n0\n0\n-1\n190000.5\n560007.5\n");
    worldFile.close();
    for (const QString& title : {QStringLiteral("위성"), QStringLiteral("지적")}) {
      auto* raster = new QgsRasterLayer(rasterPath, title, QStringLiteral("gdal"));
      if (!raster->isValid()) { delete raster; return {}; }
      raster->setCrs(project.crs());
      project.addMapLayer(raster);
    }
    if (!SurveyStorage::writeEmbedded(&project, path, &error)) return {};
    return path;
  }
  static void disableRendering(MainWindow& window) {
    window.setRestoreLastSurveyEnabled(false);
    if (auto* canvas = window.findChild<QgsMapCanvas*>()) canvas->setRenderFlag(false);
  }
  static QByteArray contents(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return {};
    return file.readAll();
  }
  // 20초 자동 저장은 없앴다. 저장은 「저장」을 누를 때만 일어난다. 예전에는 이 타이머를
  // 찾아 timeout 을 쏘아 자동 저장을 흉내 냈지만, 이제 저장 경로를 직접 부른다.
  static bool saveNow(MainWindow& window) {
    bool ok = false;
    return QMetaObject::invokeMethod(&window, "persistSurveyWork", Qt::DirectConnection,
                                     Q_RETURN_ARG(bool, ok)) && ok;
  }
  static bool hasNoAutosaveTimer(MainWindow& window) {
    for (auto* timer : window.findChildren<QTimer*>(QString(), Qt::FindDirectChildrenOnly))
      if (timer->isActive() && timer->objectName() != QLatin1String("layerWatchTimer") &&
          timer->interval() > 0 && timer->interval() <= 60000)
        return false;
    return true;
  }
  struct MenuActionState {
    QString id;
    QString text;
    QString toolTip;
    bool enabled = false;
    bool separator = false;
  };
  struct LayerMenuState {
    bool seen = false;
    QString name;
    QList<MenuActionState> actions;
  };
  static LayerMenuState inspectLayerMenu(MainWindow& window, QgsLayerTreeView* tree,
                                         const QPoint& viewportPosition,
                                         const QString& triggerId = {}) {
    LayerMenuState state;
    QTimer inspect;
    inspect.setSingleShot(true);
    connect(&inspect, &QTimer::timeout, &window, [&window, &state, triggerId] {
      auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
      if (!menu) return;
      state.seen = true;
      state.name = menu->objectName();
      for (const auto* action : menu->actions())
        state.actions.append({action->objectName(), action->text(), action->toolTip(),
                              action->isEnabled(), action->isSeparator()});
      const QString output = qEnvironmentVariable("KA_HGIS_QA_OUTPUT_DIR");
      const QString sample = window.property("qaMenuKind").toString();
      if (!output.isEmpty() && !sample.isEmpty())
        menu->grab().save(QDir(output).filePath(QStringLiteral("menu-%1.png").arg(sample)));
      if (!triggerId.isEmpty()) {
        for (auto* action : menu->actions()) {
          if (action->objectName() == triggerId && action->isEnabled()) {
            action->trigger();
            break;
          }
        }
      }
      menu->close();
    });
    inspect.start(0);
    window.showLayerTreeContextMenu(tree, viewportPosition);
    return state;
  }
  static LayerMenuState inspectMapMenu(MainWindow& window, const QPoint& point) {
    LayerMenuState state;
    QTimer inspect;
    inspect.setSingleShot(true);
    connect(&inspect, &QTimer::timeout, &window, [&state] {
      auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
      if (!menu) return;
      state.seen = true;
      state.name = menu->objectName();
      for (const auto* action : menu->actions())
        state.actions.append({action->objectName(), action->text(), action->toolTip(),
                              action->isEnabled(), action->isSeparator()});
      menu->close();
    });
    inspect.start(0);
    if (!QMetaObject::invokeMethod(&window, "onMapContextMenu", Qt::DirectConnection, Q_ARG(QPoint, point)))
      return {};
    return state;
  }
  QgsMapLayer* makeMenuReference(const QString& kind) {
    std::unique_ptr<QgsMapLayer> layer;
    if (kind == QLatin1String("satellite")) {
      layer = std::make_unique<QgsRasterLayer>(
          QStringLiteral("type=xyz&url=https://menu-fixture.invalid/{z}/{x}/{y}.png&zmin=0&zmax=18"),
          QStringLiteral("위성"), QStringLiteral("wms"));
    } else if (kind == QLatin1String("cadastral")) {
      QUrlQuery uri;
      uri.addQueryItem(QStringLiteral("url"), QUrl::fromLocalFile(
          QDir(s_testSettingsPath).filePath(QStringLiteral("cadastral-capabilities.xml"))).toString());
      uri.addQueryItem(QStringLiteral("layers"), QStringLiteral("lp_pa_cbnd_bonbun"));
      uri.addQueryItem(QStringLiteral("styles"), QStringLiteral("lp_pa_cbnd_bonbun"));
      uri.addQueryItem(QStringLiteral("format"), QStringLiteral("image/png"));
      uri.addQueryItem(QStringLiteral("crs"), QStringLiteral("EPSG:5187"));
      layer = std::make_unique<QgsRasterLayer>(uri.toString(QUrl::FullyEncoded),
                                               QStringLiteral("지적"), QStringLiteral("wms"));
    } else if (kind == QLatin1String("dem")) {
      const QString path = m_files.filePath(QStringLiteral("menu-elevation.tif"));
      GDALDriverH driver = GDALGetDriverByName("GTiff");
      if (!driver) return nullptr;
      GDALDatasetH dataset = GDALCreate(driver, path.toUtf8().constData(), 4, 4, 1, GDT_Float32, nullptr);
      if (!dataset) return nullptr;
      double transform[] = {190000.0, 10.0, 0.0, 560040.0, 0.0, -10.0};
      const bool written = GDALSetGeoTransform(dataset, transform) == CE_None &&
          GDALSetProjection(dataset, QgsProject::instance()->crs().toWkt().toUtf8().constData()) == CE_None &&
          GDALFillRaster(GDALGetRasterBand(dataset, 1), 100.0, 0.0) == CE_None;
      GDALClose(dataset);
      if (!written) return nullptr;
      layer = std::make_unique<QgsRasterLayer>(path, QStringLiteral("DEM"), QStringLiteral("gdal"));
    } else if (kind == QLatin1String("imported_raster")) {
      const QString path = m_files.filePath(QStringLiteral("menu-imported.png"));
      QImage image(8, 8, QImage::Format_RGB32);
      image.fill(Qt::white);
      if (!image.save(path)) return nullptr;
      QFile worldFile(m_files.filePath(QStringLiteral("menu-imported.pgw")));
      if (!worldFile.open(QIODevice::WriteOnly) ||
          worldFile.write("1\n0\n0\n-1\n190000.5\n560007.5\n") < 0) return nullptr;
      worldFile.close();
      layer = std::make_unique<QgsRasterLayer>(path, QStringLiteral("가져온 현장 도면"), QStringLiteral("gdal"));
      layer->setCrs(QgsProject::instance()->crs());
      layer->setCustomProperty(QStringLiteral("ka_hgis/imported_reference"), true);
    } else if (kind == QLatin1String("external_shp")) {
      QgsVectorLayer source(QStringLiteral("Polygon?crs=EPSG:5187&field=name:string"),
                            QStringLiteral("fixture"), QStringLiteral("memory"));
      QgsFeature feature(source.fields());
      feature.setAttribute(0, QStringLiteral("외부 경계"));
      feature.setGeometry(QgsGeometry::fromRect(QgsRectangle(190000, 560000, 190100, 560100)));
      if (!source.isValid() || !source.dataProvider()->addFeature(feature)) return nullptr;
      const QString path = m_files.filePath(QStringLiteral("menu-external.shp"));
      QgsVectorFileWriter::SaveVectorOptions options;
      options.driverName = QStringLiteral("ESRI Shapefile");
      options.fileEncoding = QStringLiteral("UTF-8");
      if (QgsVectorFileWriter::writeAsVectorFormatV3(&source, path,
          QgsProject::instance()->transformContext(), options) != QgsVectorFileWriter::NoError)
        return nullptr;
      // A cadastral-looking display name must not turn an actual vector into WMS.
      layer = std::make_unique<QgsVectorLayer>(path, QStringLiteral("지적 외부 경계"), QStringLiteral("ogr"));
    }
    if (!layer || !layer->isValid()) return nullptr;
    if (kind != QLatin1String("external_shp")) LayerOps::markReferenceLayer(layer.get());
    return QgsProject::instance()->addMapLayer(layer.release());
  }
private slots:
  void compactLayerListKeepsHierarchyAndMapLabelSize() {
    QgsProject project;
    auto* group=project.layerTreeRoot()->addGroup(QStringLiteral("참조 지도"));
    auto* layersGroup=group->addGroup(QStringLiteral("수치지형도"));
    QgsVectorLayer* first=nullptr;
    for(int i=0;i<18;++i) {
      auto* layer=new QgsVectorLayer(QStringLiteral("LineString?crs=EPSG:5187&field=name:string"),
          QStringLiteral("%1 · %2").arg(i+1).arg(i%2?QStringLiteral("지형 등고선"):QStringLiteral("조사 주변 경계")),QStringLiteral("memory"));
      QVERIFY(layer->isValid());
      LayerOps::applyNameAttributeLabels(layer,QStringLiteral("name"),8.,false);
      project.addMapLayer(layer,false);
      auto* node=layersGroup->addLayer(layer);
      if(i==0) first=layer;
      if(i==2) node->setItemVisibilityChecked(false);
    }
    const auto labelSize=LayerOps::labelFontSize(first,8.);
    QgsLayerTreeModel baselineModel(project.layerTreeRoot());
    QgsLayerTreeView baseline;
    baseline.setModel(&baselineModel); baseline.resize(420,460); baseline.show(); baseline.expandAll();
    KaLayerInformationModel model(&project,false);
    model.setFlag(QgsLayerTreeModel::AllowNodeChangeVisibility);
    KaLayerInformationView tree;
    tree.setModel(&model); KaLayerInformationModel::configureView(&tree);
    tree.resize(420,460); tree.show(); tree.expandAll();
    QTest::qWait(50);
    const auto groupItem=tree.node2index(group);
    const auto layerItem=tree.node2index(project.layerTreeRoot()->findLayer(first->id()));
    const int oldHeight=baseline.visualRect(baseline.node2index(group)).height();
    const int newHeight=tree.visualRect(groupItem).height();
    qInfo()<<"LAYER_ROW_HEIGHT"<<oldHeight<<newHeight<<"FONT_PX"<<baseline.font().pixelSize()<<tree.font().pixelSize();
    QCOMPARE(tree.font().pixelSize(),10);
    QVERIFY(newHeight>0 && newHeight<=24 && newHeight<oldHeight);
    QVERIFY(tree.alternatingRowColors());
    const auto baseGroup=model.node2index(group);
    const auto baseLayer=model.node2index(project.layerTreeRoot()->findLayer(first->id()));
    QVERIFY(model.data(baseGroup,Qt::FontRole).value<QFont>().bold());
    QVERIFY(!model.data(baseLayer,Qt::FontRole).value<QFont>().bold());
    QVERIFY(model.data(baseGroup,Qt::ForegroundRole)!=model.data(baseLayer,Qt::ForegroundRole));
    const auto visibleColor=model.data(baseLayer,Qt::ForegroundRole);
    group->setItemVisibilityChecked(false);
    QCoreApplication::processEvents();
    QVERIFY(model.data(baseLayer,Qt::ForegroundRole)!=visibleColor);
    QVERIFY(model.data(baseLayer,Qt::FontRole).value<QFont>().italic());
    group->setItemVisibilityChecked(true);
    QCoreApplication::processEvents();
    QCOMPARE(model.data(baseLayer,Qt::ForegroundRole),visibleColor);
    QCOMPARE(LayerOps::labelFontSize(first,8.),labelSize);
    // Click the compact row's native visibility control, preserving label settings.
    const QRect row=tree.visualRect(layerItem);
    QTest::mouseClick(tree.viewport(),Qt::LeftButton,Qt::NoModifier,QPoint(row.left()+10,row.center().y()));
    QTRY_VERIFY(!project.layerTreeRoot()->findLayer(first->id())->isVisible());
    QCOMPARE(LayerOps::labelFontSize(first,8.),labelSize);
    tree.clearSelection();
    const auto output=qEnvironmentVariable("KA_HGIS_QA_OUTPUT_DIR");
    if(!output.isEmpty()) {
      QVERIFY(QDir().mkpath(output));
      QVERIFY(tree.grab().save(QDir(output).filePath(QStringLiteral("compact-layer-list.png"))));
    }
  }

  void layerInformationControlsIntegrateWithMapAndDrawingStudio_data() {
    QTest::addColumn<QSize>("requestedSize");
    QTest::addColumn<bool>("maximized");
    if (qEnvironmentVariableIsEmpty("KA_HGIS_SMALL_SCREEN_QA")) {
      QTest::newRow("default") << QSize(1280, 900) << false;
      return;
    }
    const qreal factor = qEnvironmentVariable("QT_SCALE_FACTOR", "1").toDouble();
    QTest::newRow("1920-maximized") << QSize(qRound(1920 / factor), qRound(1080 / factor)) << true;
    QTest::newRow("1920-restored") << QSize(qRound(1600 / factor), qRound(900 / factor)) << false;
    if (qFuzzyCompare(factor, 1.0))
      QTest::newRow("1366-client") << QSize(1366, 768) << false;
  }

  void layerInformationControlsIntegrateWithMapAndDrawingStudio() {
    QFETCH(QSize, requestedSize);
    QFETCH(bool, maximized);
    const QString path = makeSurvey(QStringLiteral("글자 체크 합성 조사"));
    QVERIFY(!path.isEmpty());
    MainWindow window;
    disableRendering(window);
    QVERIFY(window.openSurveyGpkg(path));
    disableRendering(window);
    if (qEnvironmentVariableIsEmpty("KA_HGIS_SMALL_SCREEN_QA")) window.setAttribute(Qt::WA_DontShowOnScreen);
    window.resize(requestedSize);
    if (maximized) window.showMaximized(); else window.show();
    auto* project = QgsProject::instance();
    auto* canvas = window.findChild<QgsMapCanvas*>(QStringLiteral("mapCanvas"));
    auto* tree = window.findChild<QgsLayerTreeView*>(QStringLiteral("layerTree"));
    auto* panel = window.findChild<KaLayerInformationPanel*>();
    auto* model = tree ? qobject_cast<KaLayerInformationModel*>(tree->layerTreeModel()) : nullptr;
    QVERIFY(canvas && tree && panel && model);
    QVERIFY(!model->layoutMode());
    QCOMPARE(model->columnCount(), 2);
    auto* layer = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5187&field=nm:string(80)"),
                                      QStringLiteral("합성 주변유적"), QStringLiteral("memory"));
    QVERIFY(layer->isValid());
    QVERIFY(layer->startEditing());
    QgsFeature feature(layer->fields());
    feature.setAttribute(QStringLiteral("nm"), QStringLiteral("체크로 표시하는 유적"));
    feature.setGeometry(QgsGeometry::fromRect(QgsRectangle(190020., 560020., 190060., 560060.)));
    QVERIFY(layer->addFeature(feature));
    QVERIFY(layer->commitChanges());
    QVERIFY(layer->getFeatures().nextFeature(feature));
    QVERIFY(HeritageStyle::apply(layer, HeritageDataset::SurfaceSurveyArea, QStringLiteral("nm")).ok);
    QgsPalLayerSettings labels;
    labels.fieldName = QStringLiteral("nm");
    layer->setLabeling(new QgsVectorLayerSimpleLabeling(labels));
    layer->setLabelsEnabled(true);
    LayerOps::markReferenceLayer(layer);
    project->addMapLayer(layer, false);
    auto* reference = project->layerTreeRoot()->findGroup(QStringLiteral("참조 지도"));
    if (!reference) reference = project->layerTreeRoot()->addGroup(QStringLiteral("참조 지도"));
    auto* node = reference->addGroup(HeritageStyle::layerName(HeritageDataset::SurfaceSurveyArea))->addLayer(layer);
    const auto originalGeometry = layer->getFeature(feature.id()).geometry().asWkb();
    tree->setFixedWidth(220);
    tree->expandAll();
    tree->setCurrentLayer(layer);
    const auto item = tree->node2index(node).siblingAtColumn(1);
    QVERIFY(item.isValid());
    tree->scrollTo(item);
    QCoreApplication::processEvents();
    QVERIFY(tree->isVisible() && panel->isVisible());
    QVERIFY(!tree->isHeaderHidden());
    QTRY_VERIFY(tree->header()->sectionViewportPosition(1) + tree->header()->sectionSize(1) <= tree->viewport()->width());
    QVERIFY(tree->header()->sectionSize(0) > 0);
    QVERIFY(tree->header()->sectionSize(1) >= 70);
    auto* checkbox = panel->findChild<QCheckBox*>(QStringLiteral("layerLabelVisible"));
    QVERIFY(checkbox);
    QTRY_VERIFY(checkbox->isChecked());
    const QRect cell = tree->visualRect(item);
    qInfo()<<"LAYER_INFORMATION_VIEWPORT"<<tree->viewport()->rect()<<"CELL"<<cell;
    if(!qEnvironmentVariableIsEmpty("KA_HGIS_QA_OUTPUT_DIR")) {
      const auto diagnosticDirectory=qEnvironmentVariable("KA_HGIS_QA_OUTPUT_DIR");
      QDir().mkpath(diagnosticDirectory);
      window.grab().save(QDir(diagnosticDirectory).filePath(QStringLiteral("layer-list-integration-before-click.png")));
    }
    QVERIFY(tree->viewport()->rect().contains(cell.center()));
    QVERIFY2(tree->viewport()->height() >= 4 * 22, "four layer rows remain available");
    QTest::mouseClick(tree->viewport(), Qt::LeftButton, Qt::NoModifier, QPoint(cell.left() + 10, cell.center().y()));
    QTRY_VERIFY(!layer->labelsEnabled());
    QVERIFY(node->isVisible());
    QCOMPARE(layer->featureCount(), 1);
    QCOMPARE(layer->getFeature(feature.id()).geometry().asWkb(), originalGeometry);
    QTRY_VERIFY(!checkbox->isChecked());
    auto* settingsToggle = panel->findChild<QToolButton*>(QStringLiteral("layerInformationToggle"));
    QVERIFY(settingsToggle);
    QTest::mouseClick(settingsToggle, Qt::LeftButton);
    QTRY_VERIFY(checkbox->isVisible());
    auto* settingsScroll = panel->findChild<QScrollArea*>(QStringLiteral("layerInformationScroll"));
    QVERIFY(settingsScroll);
    settingsScroll->ensureWidgetVisible(checkbox);
    QCoreApplication::processEvents();
    QVERIFY(settingsScroll->viewport()->rect().contains(checkbox->mapTo(settingsScroll->viewport(), checkbox->rect().center())));
    QTest::mouseClick(checkbox, Qt::LeftButton, Qt::NoModifier, QPoint(8, checkbox->height() / 2));
    QTRY_VERIFY(layer->labelsEnabled());
    QCOMPARE(layer->labeling()->settings().fieldName, QStringLiteral("nm"));

    canvas->setDestinationCrs(project->crs());
    canvas->setLayers({layer});
    canvas->setExtent(QgsRectangle(189900., 559900., 190200., 560200.));
    const QString output = qEnvironmentVariable("KA_HGIS_QA_OUTPUT_DIR");
    if (!output.isEmpty()) {
      QVERIFY(QDir().mkpath(output));
      QSignalSpy rendered(canvas, &QgsMapCanvas::mapCanvasRefreshed);
      canvas->setRenderFlag(true);
      canvas->refresh();
      QTRY_VERIFY_WITH_TIMEOUT(!rendered.isEmpty(), 5000);
      tree->setMaximumWidth(QWIDGETSIZE_MAX);
      tree->setMinimumWidth(300);
      QCoreApplication::processEvents();
      QVERIFY(window.grab().save(QDir(output).filePath(QStringLiteral("layer-information-map.png"))));
      QVERIFY(panel->parentWidget()->grab().save(QDir(output).filePath(QStringLiteral("layer-information-controls.png"))));
      canvas->setRenderFlag(false);
    }

    KaDrawingStudio studio(project, canvas, 297., 210.);
    if (qEnvironmentVariableIsEmpty("KA_HGIS_SMALL_SCREEN_QA")) studio.setAttribute(Qt::WA_DontShowOnScreen);
    studio.resize(requestedSize);
    if (maximized) studio.showMaximized(); else studio.show();
    studio.centerOnMapCanvas();
    auto* layoutTree = studio.findChild<QgsLayerTreeView*>(QStringLiteral("layoutLayerTree"));
    auto* layoutPanel = studio.findChild<KaLayerInformationPanel*>();
    auto* layoutModel = layoutTree ? qobject_cast<KaLayerInformationModel*>(layoutTree->layerTreeModel()) : nullptr;
    auto* view = studio.findChild<QgsLayoutView*>();
    QVERIFY(layoutTree && layoutPanel && layoutModel && view && view->currentLayout());
    QVERIFY(layoutModel->layoutMode());
    layoutTree->setFixedWidth(220);
    layoutTree->expandAll();
    layoutTree->setCurrentLayer(layer);
    const auto layoutItem = layoutTree->node2index(node).siblingAtColumn(1);
    QVERIFY(layoutItem.isValid());
    layoutTree->scrollTo(layoutItem);
    layoutTree->setCurrentIndex(layoutItem);
    layoutTree->setFocus();
    QTRY_VERIFY(layoutPanel->isVisible());
    QTRY_VERIFY(layoutTree->header()->sectionViewportPosition(1) + layoutTree->header()->sectionSize(1) <= layoutTree->viewport()->width());
    auto* map = dynamic_cast<QgsLayoutItemMap*>(view->currentLayout()->itemById(QStringLiteral("ka_map")));
    QVERIFY(map);
    QTRY_VERIFY_WITH_TIMEOUT(map->layerStyleOverrides().contains(layer->id()), 5000);
    auto* numbers = HeritageLayoutNumbers::forMap(map);
    QVERIFY(numbers);
    QTRY_COMPARE_WITH_TIMEOUT(numbers->entries().size(), 1, 5000);
    auto* layoutCheckbox = layoutPanel->findChild<QCheckBox*>(QStringLiteral("layerLabelVisible"));
    QVERIFY(layoutCheckbox);
    QTRY_VERIFY(layoutCheckbox->text().contains(QStringLiteral("번호")));
    QTest::keyClick(layoutTree, Qt::Key_Space);
    QTRY_VERIFY(!layer->customProperty(QStringLiteral("ka_hgis/layout_numbers_visible"), true).toBool());
    QTRY_VERIFY_WITH_TIMEOUT(numbers->entries().isEmpty(), 5000);
    QVERIFY(layer->labelsEnabled());
    QVERIFY(node->isVisible());
    QCOMPARE(layer->getFeature(feature.id()).geometry().asWkb(), originalGeometry);
    std::unique_ptr<QgsVectorLayer> drawing(layer->clone());
    QgsMapLayerStyle(map->layerStyleOverrides().value(layer->id())).writeToLayer(drawing.get());
    QVERIFY(!drawing->labelsEnabled());
    auto* layoutSettingsToggle = layoutPanel->findChild<QToolButton*>(QStringLiteral("layerInformationToggle"));
    QVERIFY(layoutSettingsToggle);
    QTest::mouseClick(layoutSettingsToggle, Qt::LeftButton);
    QTRY_VERIFY(layoutCheckbox->isVisible());
    auto* layoutSettingsScroll = layoutPanel->findChild<QScrollArea*>(QStringLiteral("layerInformationScroll"));
    QVERIFY(layoutSettingsScroll);
    layoutSettingsScroll->ensureWidgetVisible(layoutCheckbox);
    QCoreApplication::processEvents();
    QVERIFY(layoutSettingsScroll->viewport()->rect().contains(layoutCheckbox->mapTo(layoutSettingsScroll->viewport(), layoutCheckbox->rect().center())));
    QTest::mouseClick(layoutCheckbox, Qt::LeftButton, Qt::NoModifier, QPoint(8, layoutCheckbox->height() / 2));
    QTRY_VERIFY(layer->customProperty(QStringLiteral("ka_hgis/layout_numbers_visible"), true).toBool());
    QTRY_COMPARE_WITH_TIMEOUT(numbers->entries().size(), 1, 5000);
    QCOMPARE(numbers->entries().first().number, 1);
    if (!output.isEmpty()) {
      QSignalSpy rendered(map, &QgsLayoutItemMap::previewRefreshed);
      map->refresh();
      view->viewport()->update();
      QTRY_VERIFY_WITH_TIMEOUT(!rendered.isEmpty(), 5000);
      layoutTree->setMaximumWidth(QWIDGETSIZE_MAX);
      layoutTree->setMinimumWidth(300);
      QCoreApplication::processEvents();
      QVERIFY(studio.grab().save(QDir(output).filePath(QStringLiteral("layer-information-studio.png"))));
    }
    project->setDirty(false);
  }

  void geoTiffButtonOnlyEnabledOnMapTab() {
    MainWindow window;
    auto* tabs = window.findChild<QTabWidget*>(QStringLiteral("viewTabs"));
    auto* button = window.findChild<QToolButton*>(QStringLiteral("btnMapGeoTiff"));
    QVERIFY(tabs && button);
    QVERIFY(!button->isEnabled());
    int mapIndex = -1;
    for (int i = 0; i < tabs->count(); ++i)
      if (tabs->tabText(i) == QStringLiteral("지도")) mapIndex = i;
    QVERIFY(mapIndex >= 0);
    tabs->setCurrentIndex(mapIndex);
    QVERIFY(button->isEnabled());
    QVERIFY(button->defaultAction());
    QCOMPARE(button->defaultAction()->objectName(), QStringLiteral("actionMapGeoTiff"));
    tabs->setCurrentIndex(0);
    QVERIFY(!button->isEnabled());
  }
  void windowGeometryFitsLogicalMonitors_data() {
    QTest::addColumn<QRect>("available");
    QTest::newRow("1024x768") << QRect(0, 0, 1024, 728);
    QTest::newRow("1366x768") << QRect(0, 0, 1366, 728);
    QTest::newRow("1920x1080") << QRect(0, 0, 1920, 1040);
    QTest::newRow("150-percent") << QRect(0, 0, 910, 472);
    QTest::newRow("200-percent") << QRect(0, 0, 960, 500);
    QTest::newRow("negative-monitor") << QRect(-1920, -1080, 1920, 1040);
  }
  void windowGeometryFitsLogicalMonitors() {
    QFETCH(QRect, available);
    const QMargins frame(8, 32, 8, 8);
    for (const QRect& requested : {QRect(3000, 1600, 1280, 900), QRect(-3000, -1800, 980, 720)}) {
      const QRect fitted = KaWindowGeometry::fittedClientRect(requested, available, frame);
      QVERIFY(available.contains(fitted.marginsAdded(frame)));
      QCOMPARE(KaWindowGeometry::fittedClientRect(fitted, available, frame), fitted);
    }
  }
  void mainWindowFitsAvailableScreenOnShow() {
    MainWindow window;
    disableRendering(window);
    window.resize(1920, 1080);
    window.show();
    QCoreApplication::processEvents();
    const QRect available = window.screen()->availableGeometry();
    QVERIFY2(available.contains(window.frameGeometry()), qPrintable(
        QStringLiteral("screen %1x%2, window %3x%4")
            .arg(available.width()).arg(available.height())
            .arg(window.frameGeometry().width()).arg(window.frameGeometry().height())));
    QgsProject::instance()->setDirty(false);
  }
  void topographicActualWindowRemainsStableAfterCompletion() {
    const QDir cache(qEnvironmentVariable("KA_HGIS_QA_TOPOGRAPHIC_SHP"));
    if(qEnvironmentVariableIsEmpty("KA_HGIS_QA_TOPOGRAPHIC_SHP")) QSKIP("Opt-in real SHP rendering/idle diagnostic");
    QList<TopographicCatalog::Record> records;
    for(const auto& index:cache.entryInfoList({QStringLiteral("*.json")},QDir::Files)) {
      QFile file(index.absoluteFilePath()); QVERIFY(file.open(QIODevice::ReadOnly));
      const auto json=QJsonDocument::fromJson(file.readAll()).object();
      const QDir data(cache.filePath(json.value("directory").toString()));
      for(const auto& entry:json.value("records").toArray()) {
        const auto object=entry.toObject(); const auto bounds=object.value("bounds").toArray();
        QCOMPARE(bounds.size(),4);
        TopographicCatalog::Record r;
        r.source=data.filePath(object.value("file").toString()); r.layerName=object.value("layer").toString();
        r.sourceSheet=object.value("sheet").toString(); r.displayName=object.value("name").toString();
        r.crsWkt=object.value("crs").toString(); r.category=static_cast<TopographicCatalog::Category>(object.value("category").toInt());
        r.extent=QgsRectangle(bounds[0].toDouble(),bounds[1].toDouble(),bounds[2].toDouble(),bounds[3].toDouble());
        r.hasExtent=true; r.signature=index.baseName(); records.append(r);
      }
    }
    QVERIFY(!records.isEmpty());
    MainWindow window;
    window.setRestoreLastSurveyEnabled(false);
    QString path=makeSurvey(QStringLiteral("topographic-stability")); QVERIFY(!path.isEmpty());
    const auto surveySource=qEnvironmentVariable("KA_HGIS_QA_TOPOGRAPHIC_SURVEY");
    QByteArray originalSurvey;
    if(!surveySource.isEmpty()) {
      originalSurvey=contents(surveySource); QVERIFY(!originalSurvey.isEmpty());
      path=m_files.filePath(QStringLiteral("field-survey-copy.gpkg"));
      QVERIFY(QFile::copy(surveySource,path));
    }
    QVERIFY(window.openSurveyGpkg(path,MainWindow::OpenSurveyMode::LayersOnly));
    window.resize(1500,950); window.show();
    auto* canvas=window.findChild<QgsMapCanvas*>(); QVERIFY(canvas);
    QTest::qWait(1000); // Let initial window geometry and project-open signals settle.
    connect(canvas,&QgsMapCanvas::extentsChanged,&window,[canvas] {
      qInfo()<<"extent"<<canvas->extent().toString()<<"crs"<<canvas->mapSettings().destinationCrs().authid()<<"scale"<<canvas->scale();
    });
    const auto crs=canvas->mapSettings().destinationCrs();
    QgsRectangle bounds;
    for(const auto& record:records) bounds.combineExtentWith(QgsCoordinateTransform(QgsCoordinateReferenceSystem(record.crsWkt),crs,QgsProject::instance()).transformBoundingBox(record.extent));
    qInfo()<<"requested bounds"<<bounds.toString()<<"records"<<records.size();
    if(surveySource.isEmpty()) canvas->setExtent(bounds);
    else {
      bool framed=false;
      for(auto* raw:QgsProject::instance()->mapLayers()) {
        if(raw->customProperty(QStringLiteral("ka_hgis/layer_key")).toString()!=QLatin1String("survey_area")) continue;
        auto* survey=qobject_cast<QgsVectorLayer*>(raw); QVERIFY(survey);
        QgsFeature feature;
        auto features=survey->getFeatures(); QVERIFY(features.nextFeature(feature));
        auto area=QgsCoordinateTransform(survey->crs(),crs,QgsProject::instance()).transformBoundingBox(feature.geometry().boundingBox());
        QVERIFY(!area.isEmpty()); area.scale(1.3); canvas->setExtent(area); framed=true; break;
      }
      QVERIFY(framed);
    }
    canvas->refresh();
    const auto initialExtent=canvas->extent();
    auto topographicCount=[] {
      int n=0;
      for (auto* layer:QgsProject::instance()->mapLayers())
        if (!layer->customProperty(QStringLiteral("ka_hgis/topographic_source")).toString().isEmpty()) ++n;
      return n;
    };
    QStringList preservedIds;
    for (auto* layer:QgsProject::instance()->mapLayers())
      if (layer->customProperty(QStringLiteral("ka_hgis/topographic_source")).toString().isEmpty())
        preservedIds.append(layer->id());
    QVERIFY(!preservedIds.isEmpty());
    const auto expected=TopographicCatalog::query(records,TopographicCatalog::coverageBounds(initialExtent),crs.toWkt(),QgsProject::instance()->transformContext(),
        nullptr,{},canvas->scale());
    QVERIFY(!expected.matches.isEmpty());
    KaTopographicImportDialog importer(canvas,&window);
    QTemporaryDir library;
    KaTopographicScopePanel scope(canvas,nullptr,&importer,&window,library.path()); scope.hide();
    QSignalSpy added(QgsProject::instance(),&QgsProject::layersAdded);
    QSignalSpy removed(QgsProject::instance(),&QgsProject::layersRemoved);
    QSignalSpy rendered(canvas,&QgsMapCanvas::mapCanvasRefreshed);
    QSignalSpy canceled(canvas,&QgsMapCanvas::mapRefreshCanceled);
    QElapsedTimer elapsed; elapsed.start();
    QString error; QVERIFY2(importer.importVerified(records,&error),qPrintable(error));
    QTRY_VERIFY_WITH_TIMEOUT(!importer.isAutomaticLoading(),120000);
    QTRY_VERIFY_WITH_TIMEOUT(!canvas->isDrawing() && !rendered.isEmpty(),120000);
    QSet<int> expectedGroups;
    for (const auto& record : expected.matches)
      expectedGroups.insert(static_cast<int>(record.category));
    QCOMPARE(topographicCount(),expectedGroups.size());
    QCOMPARE(QgsProject::instance()->mapLayers().size(),preservedIds.size()+expectedGroups.size());
    QCOMPARE(canvas->extent(),initialExtent);
    QVERIFY(added.size()<=1);
    qInfo()<<"initial load milliseconds"<<elapsed.elapsed()<<"adds"<<added.size()<<"renders"<<rendered.size()<<"cancels"<<canceled.size()
        <<"scale"<<canvas->scale()<<"topo"<<expected.matches.size()<<"preserved"<<preservedIds.size();
    const auto ids=QgsProject::instance()->mapLayers().keys();
    QMap<QString,bool> checks;
    for(auto* node:QgsProject::instance()->layerTreeRoot()->findLayers()) checks.insert(node->layerId(),node->itemVisibilityChecked());
    const int priorRenders=rendered.size(), priorAdds=added.size(), priorRemoves=removed.size();
    // Leave the real scope watcher and MainWindow's 30 s health timer active.
    QTest::qWait(35000);
    QCOMPARE(QgsProject::instance()->mapLayers().keys(),ids);
    for(auto* node:QgsProject::instance()->layerTreeRoot()->findLayers()) QCOMPARE(node->itemVisibilityChecked(),checks.value(node->layerId()));
    QCOMPARE(added.size(),priorAdds); QCOMPARE(removed.size(),priorRemoves);
    QVERIFY2(rendered.size()-priorRenders<=2,"Idle must not keep repainting/reloading");
    const auto output=qEnvironmentVariable("KA_HGIS_QA_OUTPUT_DIR");
    if(!output.isEmpty()) { QVERIFY(QDir().mkpath(output)); QVERIFY(window.grab().save(QDir(output).filePath(QStringLiteral("actual-window-idle.png")))); }
    qInfo()<<"idle renders"<<rendered.size()-priorRenders<<"layer count"<<ids.size()<<"scope active"<<scope.isActive();
    if(!surveySource.isEmpty()) {
      int visit=0;
      for(const auto& view:{bounds,initialExtent}) {
        const auto before=rendered.size();
        elapsed.restart(); canvas->setExtent(view); canvas->refresh();
        // Wait until the extent debounce has processed this view and the final
        // map render finishes; the watcher remains enabled throughout.
        QTest::qWait(600);
        QTRY_VERIFY_WITH_TIMEOUT(!importer.isAutomaticLoading() && !canvas->isDrawing() && rendered.size()>before,120000);
        const auto visible=TopographicCatalog::query(records,TopographicCatalog::coverageBounds(canvas->extent()),crs.toWkt(),QgsProject::instance()->transformContext(),
            nullptr,{},canvas->scale());
        QSet<int> visibleGroups;
        for (const auto& record : visible.matches)
          visibleGroups.insert(static_cast<int>(record.category));
        QCOMPARE(topographicCount(),visibleGroups.size());
        QCOMPARE(QgsProject::instance()->mapLayers().size(),preservedIds.size()+visibleGroups.size());
        for(const auto& id:preservedIds) {
          QVERIFY(QgsProject::instance()->mapLayer(id));
          auto* node=QgsProject::instance()->layerTreeRoot()->findLayer(id); QVERIFY(node);
          QCOMPARE(node->itemVisibilityChecked(),checks.value(id));
        }
        const auto picture=canvas->grab().toImage();
        int mapPixels=0;
        for(int y=100;y<picture.height()-40;++y) for(int x=100;x<picture.width()-40;++x) {
          const auto c=picture.pixelColor(x,y);
          if(c.red()>=120 && c.red()<200 && c.red()==c.green() && c.green()==c.blue()) ++mapPixels;
        }
        QVERIFY2(mapPixels>100,"Reference geometry must remain visible after navigation");
        qInfo()<<"navigation"<<visit<<"ms"<<elapsed.elapsed()<<"layers"<<visible.matches.size()<<"gray pixels"<<mapPixels;
        if(!output.isEmpty()) QVERIFY(window.grab().save(QDir(output).filePath(QStringLiteral("actual-window-navigation-%1.png").arg(visit))));
        ++visit;
      }
      QCOMPARE(contents(surveySource),originalSurvey);
    }
    QgsProject::instance()->setDirty(false);
  }
  void topographicDownloadKeepsMapVisibleAndUsesSurveyFolder() {
    MainWindow window;
    disableRendering(window);
    const QString path=makeSurvey(QStringLiteral("browser-key-isolation"));
    QVERIFY(!path.isEmpty());
    QVERIFY(window.openSurveyGpkg(path,MainWindow::OpenSurveyMode::LayersOnly));
    const QString directory=QFileInfo(path).absoluteDir().filePath(QStringLiteral("지형도"));
    QVERIFY(QFileInfo(directory).isDir());
    window.show();
    auto* tabs = window.findChild<QTabWidget*>(QStringLiteral("viewTabs"));
    QVERIFY(tabs);
    QWidget* map=tabs->currentWidget();
    QVERIFY(!window.findChild<KaTopographicBrowser*>());
    auto* downloadButton=window.findChild<QToolButton*>(QStringLiteral("btnTopographic"));
    QVERIFY(downloadButton);
    downloadButton->click();
    auto* browser = window.findChild<KaTopographicBrowser*>();
    QVERIFY(browser);
    browser->stopAutomatic();
    browser->navigate(QUrl(QStringLiteral("about:blank")));
    QCOMPARE(browser->parentWidget(),&window);
    QCOMPARE(browser->windowModality(),Qt::NonModal);
    QCOMPARE(browser->windowType(),Qt::Dialog);
    QVERIFY(browser->property("kaDownloadWindow").toBool());
    auto* title=browser->findChild<QLabel*>(QStringLiteral("downloadTitle"));
    auto* stage=browser->findChild<QLabel*>(QStringLiteral("topographicCurrentStage"));
    auto* trail=browser->findChild<QLabel*>(QStringLiteral("topographicStageTrail"));
    QVERIFY(title && title->isVisible());
    QVERIFY(stage && stage->isVisible());
    QVERIFY(trail && !trail->isVisible());
    QCOMPARE(tabs->currentWidget(),map);
    QCOMPARE(tabs->indexOf(browser),-1);
    auto* details=browser->findChild<QWidget*>(QStringLiteral("topographicOfficialDetails"));
    QVERIFY(details && details->isHidden());
    const auto downloadQa=qEnvironmentVariable("KA_HGIS_QA_OUTPUT_DIR");
    if(!downloadQa.isEmpty()) {
      QVERIFY(QDir().mkpath(downloadQa));
      QVERIFY(browser->grab().save(QDir(downloadQa).filePath(QStringLiteral("topographic-from-ribbon.png"))));
    }
    browser->hide();
    QVERIFY(QMetaObject::invokeMethod(&window, "openTopographicDownload", Qt::DirectConnection));
    browser->stopAutomatic();
    browser->navigate(QUrl(QStringLiteral("about:blank")));
    QCOMPARE(window.findChild<KaTopographicBrowser*>(), browser);
    QCOMPARE(tabs->currentWidget(),map);
    QVERIFY(details->isHidden());
    // A main-window shortcut must not edit the survey while its download window is active.
    QString error;
    auto* survey=LayerOps::ensureDomainLayer(QgsProject::instance(),path,QStringLiteral("survey_area"),QStringLiteral("조사구역"),&error);
    QVERIFY(survey && survey->isValid());
    survey->selectAll();
    auto* canvas=window.findChild<QgsMapCanvas*>(); QVERIFY(canvas);
    canvas->setCurrentLayer(survey);
    const auto before=survey->featureCount();
    QVERIFY(before>0);
    QApplication::setActiveWindow(browser);
    QCOMPARE(QApplication::activeWindow(),browser);
    for(auto* action:window.actions())
      if(action->shortcut()==QKeySequence(QKeySequence::Delete)) action->trigger();
    QCOMPARE(survey->featureCount(),before);
    browser->hide();
    QgsProject::instance()->setDirty(false);
    const QString secondSource=makeSurvey(QStringLiteral("second-survey-directory"));
    QVERIFY(!secondSource.isEmpty());
    const QString secondFolder=m_files.filePath(QStringLiteral("another-folder"));
    QVERIFY(QDir().mkpath(secondFolder));
    const QString other=QDir(secondFolder).filePath(QStringLiteral("another.gpkg"));
    QVERIFY(QFile::copy(secondSource,other));
    QVERIFY(window.openSurveyGpkg(other,MainWindow::OpenSurveyMode::LayersOnly));
    QVERIFY(QFileInfo(QFileInfo(other).absoluteDir().filePath(QStringLiteral("지형도"))).isDir());
    QVERIFY(QFileInfo(directory).isDir());
    QgsProject::instance()->setDirty(false);
  }
  void cleanup() { QgsProject::instance()->clear(); }
  void provinceChipMovesMapWithoutLoadingLayers_data() {
    QTest::addColumn<QString>("crs");
    for (const QString& crs : {QStringLiteral("EPSG:5186"), QStringLiteral("EPSG:5187")})
      QTest::newRow(qPrintable(crs)) << crs;
  }
  void provinceChipMovesMapWithoutLoadingLayers() {
    QFETCH(QString, crs);
    MainWindow window;
    disableRendering(window);
    window.resize(1800, 1000);
    window.show();
    auto* canvas = window.findChild<QgsMapCanvas*>();
    auto* locator = window.findChild<KaRegionLocator*>();
    QVERIFY(canvas && locator);
    QVERIFY(QMetaObject::invokeMethod(&window, crs.endsWith('6') ? "setWorkCrs5186" : "setWorkCrs5187", Qt::DirectConnection));
    const auto layerIds = QgsProject::instance()->mapLayers().keys();
    const QgsCoordinateTransform transform(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:4326")),
                                           QgsCoordinateReferenceSystem(crs), QgsProject::instance());
    struct Visit { const char* chip; double lon; double lat; };
    for (const Visit& visit : {Visit{"서울", 126.978, 37.5665}, Visit{"울산", 129.3114, 35.5396},
                              Visit{"제주", 126.531, 33.4996}}) {
      QToolButton* chip = nullptr;
      for (auto* button : locator->findChildren<QToolButton*>())
        if (button->text() == QString::fromUtf8(visit.chip)) chip = button;
      QVERIFY(chip);
      chip->click();
      QCoreApplication::processEvents();
      QCOMPARE(canvas->mapSettings().destinationCrs().authid(), crs);
      QCOMPARE(QgsProject::instance()->crs().authid(), crs);
      const QgsPointXY expected = transform.transform(QgsPointXY(visit.lon, visit.lat));
      QVERIFY2(canvas->extent().contains(expected), qPrintable(QStringLiteral("%1 is outside %2").arg(chip->text(), canvas->extent().toString())));
      QVERIFY(canvas->extent().width() > 10000.);
      QCOMPARE(QgsProject::instance()->mapLayers().keys(), layerIds);
      QVERIFY(!locator->findChild<QPushButton*>(QStringLiteral("regionFieldMap")));
    }
    QgsProject::instance()->setDirty(false);
  }
  void drawingStudio_decorationsMoveAndShrink() {
    QgsProject project;
    project.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5187")));
    QgsMapCanvas canvas;
    canvas.setRenderFlag(false);
    canvas.setDestinationCrs(project.crs());
    canvas.setExtent(QgsRectangle(190000, 560000, 191000, 561000));
    KaDrawingStudio studio(&project, &canvas, 297., 210.);
    studio.setAttribute(Qt::WA_DontShowOnScreen);
    studio.show();
    QCoreApplication::processEvents();
    auto* view = studio.findChild<QgsLayoutView*>();
    QVERIFY(view && view->currentLayout());
    auto* ly = view->currentLayout();
    auto* map = dynamic_cast<QgsLayoutItemMap*>(ly->itemById(QStringLiteral("ka_map")));
    QVERIFY(map);
    auto* size = studio.findChild<QDoubleSpinBox*>(QStringLiteral("decorationSize"));
    QVERIFY(QMetaObject::invokeMethod(&studio, "useSelectTool"));
    QMap<QString, QPointF> positions;
    for (const QString& id : {QStringLiteral("ka_scalebar"), QStringLiteral("ka_scale"),
                              QStringLiteral("ka_crs"), QStringLiteral("ka_north")}) {
      auto* item = ly->itemById(id);
      QVERIFY2(item, qPrintable(id));
      QVERIFY2(!item->isLocked(), qPrintable(id + QStringLiteral(" is locked")));
      QVERIFY(size);
      // Zoom in so the small compass's rotation handles do not cover its body.
      view->setZoomLevel(2.);
      view->centerOn(item);
      ly->setSelectedItem(item);
      QCoreApplication::processEvents();
      QVERIFY(size->isEnabled());
      const QRectF before(item->pos(), item->rect().size());
      const QPoint from = view->mapFromScene(item->mapToScene(item->rect().center()));
      const QPoint to = from + QPoint(18, -24);
      QMouseEvent hover(QEvent::MouseMove, QPointF(from), view->viewport()->mapToGlobal(from),
                        Qt::NoButton, Qt::NoButton, Qt::NoModifier);
      QApplication::sendEvent(view->viewport(), &hover);
      QTest::mousePress(view->viewport(), Qt::LeftButton, Qt::NoModifier, from);
      QMouseEvent drag(QEvent::MouseMove, QPointF(to), view->viewport()->mapToGlobal(to),
                       Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
      QApplication::sendEvent(view->viewport(), &drag);
      // QGIS mouse handles use lastScenePos(), so supply consecutive movement events
      // as a real pointer does before releasing the button.
      QApplication::sendEvent(view->viewport(), &drag);
      QTest::mouseRelease(view->viewport(), Qt::LeftButton, Qt::NoModifier, to);
      const QPointF moved = item->pos();
      qInfo() << "decoration drag" << id << before << from << to << moved << item->rect().size();
      const QString dragOutput = qEnvironmentVariable("KA_HGIS_QA_OUTPUT_DIR");
      if (!dragOutput.isEmpty() && QLineF(before.topLeft(), moved).length() <= 3.)
        studio.grab().save(QDir(dragOutput).filePath(QStringLiteral("decoration-drag-failure.png")));
      QVERIFY2(QLineF(before.topLeft(), moved).length() > 3., qPrintable(id + QStringLiteral(" did not drag")));
      QVERIFY(qAbs(item->itemRotation()) < .001);
      positions.insert(id, moved);
      const auto* bar = dynamic_cast<QgsLayoutItemScaleBar*>(item);
      const double distance = bar ? bar->unitsPerSegment() : 0.;
      const auto* label = dynamic_cast<QgsLayoutItemLabel*>(item);
      const double fontSize = label ? label->font().pointSizeF() : 0.;
      size->setValue(50.);
      QVERIFY(item->rect().width() < before.width() * .8);
      QVERIFY(item->rect().height() < before.height());
      QVERIFY(QLineF(item->pos(), moved).length() < .01);
      if (bar) QVERIFY(qAbs(bar->unitsPerSegment() / distance - .5) < .001);
      if (label) QVERIFY(qAbs(label->font().pointSizeF() / fontSize - .5) < .001);
      view->setFocus();
      studio.undoLastChange();
      item = ly->itemById(id); // QGIS undo may replace the layout item.
      QVERIFY(item);
      QCOMPARE(size->value(), 100.);
      QVERIFY(qAbs(item->rect().width() - before.width()) < .01);
      size->setValue(50.);
      const QSizeF shrunk = item->rect().size();
      // A normal scale refresh used to reset and lock every decoration.
      QVERIFY(QMetaObject::invokeMethod(&studio, "flushHeavyScaleSync"));
      QVERIFY(QLineF(item->pos(), moved).length() < .01);
      QVERIFY(qAbs(item->rect().width() - shrunk.width()) < .01);
      QVERIFY(!item->isLocked());
    }
    auto* bar = dynamic_cast<QgsLayoutItemScaleBar*>(ly->itemById(QStringLiteral("ka_scalebar")));
    QVERIFY(bar);
    const QPointF barPosition = bar->pos();
    const double oldDistance = bar->unitsPerSegment();
    map->setScale(map->scale() * 2.);
    QVERIFY(QMetaObject::invokeMethod(&studio, "flushHeavyScaleSync"));
    QVERIFY(bar->unitsPerSegment() > oldDistance);
    QCOMPARE(bar->linkedMap(), map);
    QVERIFY(QLineF(bar->pos(), barPosition).length() < .01);
    studio.refreshMapFromProject();
    QVERIFY(QLineF(bar->pos(), barPosition).length() < .01);
    ly->setSelectedItem(bar);
    QCOMPARE(size->value(), 50.);
    for (auto it = positions.cbegin(); it != positions.cend(); ++it)
      QVERIFY(QLineF(ly->itemById(it.key())->pos(), it.value()).length() < .01);
    // The lower bound must shrink the rendered bar and distance again.
    ly->setSelectedItem(bar);
    const double halfDistance = bar->unitsPerSegment();
    const double halfWidth = bar->rect().width();
    size->setValue(25.);
    QVERIFY(qAbs(bar->unitsPerSegment() / halfDistance - .5) < .001);
    QVERIFY(bar->rect().width() < halfWidth * .8);
    size->setValue(50.);
    // Exercise style switches after shrinking: these used to recreate/reset items.
    QVERIFY(QMetaObject::invokeMethod(&studio, "beginPlaceNorth", Q_ARG(QString, QStringLiteral("arrows/NorthArrow_04.svg"))));
    QVERIFY(QMetaObject::invokeMethod(&studio, "beginPlaceScaleBar", Q_ARG(QString, QStringLiteral("Line Ticks Up"))));
    QVERIFY(QMetaObject::invokeMethod(&studio, "beginPlaceScaleLabel"));
    QCoreApplication::processEvents();
    studio.refreshMapFromProject();
    for (auto it = positions.cbegin(); it != positions.cend(); ++it)
      QVERIFY(QLineF(ly->itemById(it.key())->pos(), it.value()).length() < .01);
    QgsLayoutExporter exporter(ly);
    const QImage page = exporter.renderPageToImage(0, QSize(), 150.);
    QVERIFY(!page.isNull());
    const QString output = qEnvironmentVariable("KA_HGIS_QA_OUTPUT_DIR");
    QTemporaryDir temp;
    const QString pdf = QDir(output.isEmpty() ? temp.path() : output).filePath(QStringLiteral("decoration-resized.pdf"));
    QgsLayoutExporter::PdfExportSettings settings;
    QCOMPARE(exporter.exportToPdf(pdf, settings), QgsLayoutExporter::Success);
    QVERIFY(QFileInfo(pdf).size() > 1000);
    if (!output.isEmpty()) {
      QVERIFY(QMetaObject::invokeMethod(&studio, "zoomPaperVisible"));
      QVERIFY(page.save(QDir(output).filePath(QStringLiteral("decoration-resized.png"))));
      QVERIFY(studio.grab().save(QDir(output).filePath(QStringLiteral("decoration-editor.png"))));
    }
  }
  void drawingStudio_heritageRefreshRequestsCoalesceAndReuseOnRevisit() {
    constexpr int categoryCount = 200;
    QgsProject project;
    project.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5187")));
    auto* layer = new QgsVectorLayer(
        QStringLiteral("Polygon?crs=EPSG:5187&field=nm:string(80)"),
        QStringLiteral("합성 지표 자료"), QStringLiteral("memory"));
    QVERIFY(layer->isValid());
    QVERIFY(layer->startEditing());
    for (int i = 0; i < categoryCount; ++i) {
      const double x = 190050. + (i % 20) * 80.;
      const double y = 560050. + (i / 20) * 80.;
      QgsFeature feature(layer->fields());
      feature.setAttribute(QStringLiteral("nm"), QStringLiteral("합성 유적 %1").arg(i, 3, 10, QLatin1Char('0')));
      feature.setGeometry(QgsGeometry::fromRect(QgsRectangle(x, y, x + 30., y + 30.)));
      QVERIFY(layer->addFeature(feature));
    }
    QVERIFY(layer->commitChanges());
    QVERIFY(HeritageStyle::apply(layer, HeritageDataset::SurfaceSurveyArea, QStringLiteral("nm")).ok);
    LayerOps::markReferenceLayer(layer);
    project.addMapLayer(layer, false);
    project.layerTreeRoot()->addGroup(QStringLiteral("참조 지도"))
        ->addGroup(HeritageStyle::layerName(HeritageDataset::SurfaceSurveyArea))->addLayer(layer);
    QgsMapCanvas canvas;
    canvas.setRenderFlag(false);
    canvas.setDestinationCrs(project.crs());
    canvas.setLayers({layer});
    canvas.setExtent(QgsRectangle(190000., 560000., 192000., 562000.));
    QElapsedTimer elapsed;
    elapsed.start();
    KaDrawingStudio studio(&project, &canvas, 297., 210.);
    studio.setAttribute(Qt::WA_DontShowOnScreen);
    studio.resize(1400, 900);
    studio.show();
    auto* view = studio.findChild<QgsLayoutView*>();
    QVERIFY(view && view->currentLayout());
    auto* layout = view->currentLayout();
    QTRY_VERIFY_WITH_TIMEOUT(layout->itemById(QStringLiteral("ka_map")), 5000);
    auto* map = dynamic_cast<QgsLayoutItemMap*>(layout->itemById(QStringLiteral("ka_map")));
    QVERIFY(map);
    // Match MainWindow's real first-entry centering before measuring revisits.
    studio.centerOnMapCanvas();
    QTRY_VERIFY_WITH_TIMEOUT(map->layerStyleOverrides().contains(layer->id()), 5000);
    const qint64 firstReadyMs = elapsed.elapsed();
    QVERIFY(QMetaObject::invokeMethod(&studio, "beginPlaceLegend"));
    auto* legend = dynamic_cast<QgsLayoutItemLegend*>(layout->itemById(QStringLiteral("ka_legend")));
    QVERIFY(legend);
    QSignalSpy overridesChanged(map, &QgsLayoutItemMap::layerStyleOverridesChanged);
    QSignalSpy previewRendered(map, &QgsLayoutItemMap::previewRefreshed);
    map->invalidateCache();
    map->refresh();
    view->viewport()->update();
    QTRY_VERIFY_WITH_TIMEOUT(previewRendered.count() > 0, 5000);
    // Let the 80 ms layer and 280 ms scale timers settle after first paint.
    QTest::qWait(450);
    QTRY_VERIFY_WITH_TIMEOUT(legend->model()->rootGroup()->findLayer(layer->id()), 5000);
    overridesChanged.clear();
    auto* legendLayer = legend->model()->rootGroup()->findLayer(layer->id());
    QVERIFY(legendLayer);
    const auto legendNodes = legend->model()->layerLegendNodes(legendLayer);
    QVERIFY(!legendNodes.isEmpty());
    QPointer<QgsLayerTreeModelLegendNode> retainedLegendNode(legendNodes.first());
    for (int i = 0; i < 10; ++i) {
      studio.centerOnMapCanvas();
      studio.refreshMapFromProject();
    }
    QTest::qWait(450);
    QCOMPARE(overridesChanged.count(), 0);
    QVERIFY(!retainedLegendNode.isNull());
    QCOMPARE(legend->model()->layerLegendNodes(legendLayer).first(), retainedLegendNode.data());

    studio.hide();
    for (int i = 0; i < 10; ++i) studio.refreshMapFromProject();
    QTest::qWait(450);
    QCOMPARE(overridesChanged.count(), 0);
    elapsed.restart();
    studio.show();
    studio.centerOnMapCanvas();
    studio.refreshMapFromProject();
    QCoreApplication::processEvents();
    const qint64 revisitShowMs = elapsed.elapsed();
    QTest::qWait(450);
    QCOMPARE(overridesChanged.count(), 0);
    QVERIFY(!retainedLegendNode.isNull());

    auto recolorSource = [&](const QColor& color) {
      auto* renderer = dynamic_cast<QgsCategorizedSymbolRenderer*>(layer->renderer()->clone());
      if (!renderer) return false;
      for (int i = 0; i < renderer->categories().size(); ++i) {
        auto* symbol = renderer->categories().at(i).symbol()->clone();
        symbol->setColor(color);
        renderer->updateCategorySymbol(i, symbol);
      }
      layer->setRenderer(renderer);
      return true;
    };
    auto overrideFill = [&]() {
      std::unique_ptr<QgsVectorLayer> drawing(layer->clone());
      QgsMapLayerStyle(map->layerStyleOverrides().value(layer->id())).writeToLayer(drawing.get());
      return drawing->labeling() ? drawing->labeling()->settings().format().background().fillColor() : QColor();
    };
    const QColor visibleColor(QStringLiteral("#3178cc"));
    auto* numberOwner = HeritageLayoutNumbers::forMap(map);
    QVERIFY(numberOwner);
    quint64 candidateRevision = numberOwner->revision();
    QVERIFY(recolorSource(visibleColor));
    for (int i = 0; i < 10; ++i) studio.refreshMapFromProject();
    QTRY_COMPARE_WITH_TIMEOUT(numberOwner->revision(), candidateRevision + 1, 5000);
    QTest::qWait(450);
    QCOMPARE(numberOwner->revision(), candidateRevision + 1);
    QCOMPARE(overrideFill(), visibleColor);

    overridesChanged.clear();
    studio.hide();
    candidateRevision = numberOwner->revision();
    const QColor hiddenColor(QStringLiteral("#a43b92"));
    QVERIFY(recolorSource(hiddenColor));
    for (int i = 0; i < 10; ++i) studio.refreshMapFromProject();
    QTest::qWait(450);
    QCOMPARE(overridesChanged.count(), 0);
    studio.show();
    QTRY_COMPARE_WITH_TIMEOUT(numberOwner->revision(), candidateRevision + 1, 5000);
    QTest::qWait(450);
    QCOMPARE(numberOwner->revision(), candidateRevision + 1);
    QCOMPARE(overrideFill(), hiddenColor);

    // Editing only the legend's presentation must retain numbered nodes.
    QTRY_VERIFY_WITH_TIMEOUT(legend->model()->rootGroup()->findLayer(layer->id()), 5000);
    legendLayer = legend->model()->rootGroup()->findLayer(layer->id());
    QVERIFY(legendLayer);
    retainedLegendNode = legend->model()->layerLegendNodes(legendLayer).first();
    QLineEdit* legendTitle = nullptr;
    QSpinBox* legendFont = nullptr;
    for (auto* edit : studio.findChildren<QLineEdit*>())
      if (edit->placeholderText() == QStringLiteral("제목을 입력하세요")) legendTitle = edit;
    for (auto* spin : studio.findChildren<QSpinBox*>())
      if (spin->suffix() == QStringLiteral(" pt") && spin->maximum() == 24) legendFont = spin;
    QVERIFY(legendTitle && legendFont);
    overridesChanged.clear();
    legendTitle->setText(QStringLiteral("합성 범례 조절"));
    legendFont->setValue(11);
    QCoreApplication::processEvents();
    QCOMPARE(legend->title(), QStringLiteral("합성 범례 조절"));
    QVERIFY(!retainedLegendNode.isNull());
    QCOMPARE(legend->model()->layerLegendNodes(legendLayer).first(), retainedLegendNode.data());
    QCOMPARE(overridesChanged.count(), 0);
    QCOMPARE(legend->cacheMode(), QGraphicsItem::DeviceCoordinateCache);

    legend->attemptResize(QgsLayoutSize(70., legend->rect().height()));
    QTRY_COMPARE_WITH_TIMEOUT(legend->columnCount(), 1, 5000);
    QTest::qWait(450);
    overridesChanged.clear();
    candidateRevision = numberOwner->revision();
    QVERIFY(QMetaObject::invokeMethod(&studio, "useSelectTool"));
    const QPoint pressAt(3, 3);
    QTest::mousePress(view->viewport(), Qt::LeftButton, Qt::NoModifier, pressAt);
    QVERIFY(studio.property("ka_interacting").toBool());
    QVERIFY(legend->property("ka_interacting").toBool());
    const double originalScale = studio.drawingScale();
    const double finalScale = std::round(originalScale * 1.4);
    for (int i = 1; i <= 4; ++i) {
      studio.setDrawingScale(originalScale * (1. + i * .1));
      studio.refreshMapFromProject();
    }
    legend->attemptResize(QgsLayoutSize(190., legend->rect().height()));
    // Longer than both the scale and legend debounce intervals: held input
    // must keep deferring feature/legend work instead of merely delaying it once.
    QTest::qWait(400);
    QVERIFY(studio.property("ka_interacting").toBool());
    QCOMPARE(overridesChanged.count(), 0);
    QCOMPARE(legend->columnCount(), 1);
    QVERIFY(qAbs(studio.drawingScale() - finalScale) < .5);
    QTest::mouseRelease(view->viewport(), Qt::LeftButton, Qt::NoModifier, pressAt);
    QTRY_VERIFY_WITH_TIMEOUT(!studio.property("ka_interacting").toBool(), 5000);
    QTRY_COMPARE_WITH_TIMEOUT(numberOwner->revision(), candidateRevision + 1, 5000);
    QTRY_VERIFY_WITH_TIMEOUT(legend->columnCount() > 1, 5000);
    QTest::qWait(450);
    QCOMPARE(numberOwner->revision(), candidateRevision + 1);
    QVERIFY(qAbs(legend->rect().width() - 190.) < .1);
    auto* paperScale = dynamic_cast<QgsLayoutItemLabel*>(layout->itemById(QStringLiteral("ka_scale")));
    QVERIFY(paperScale);
    QCOMPARE(paperScale->text(), QStringLiteral("축척 1 : %1").arg(finalScale, 0, 'f', 0));
    QTRY_VERIFY_WITH_TIMEOUT(legend->model()->rootGroup()->findLayer(layer->id()), 5000);
    legendLayer = legend->model()->rootGroup()->findLayer(layer->id());
    QVERIFY(legendLayer);
    auto placedNumbers = [&]() {
      QSet<QString> result;
      if (auto* labels = map->previewLabelingResults())
        for (const auto& label : labels->allLabels())
          if (!label.isUnplaced && !label.isDiagram && label.layerID == layer->id())
            result.insert(label.labelText);
      return result;
    };
    auto legendNumbers = [&]() {
      QSet<QString> result;
      if (auto* tree = legend->model()->rootGroup()->findLayer(layer->id()))
        for (auto* node : legend->model()->layerLegendNodes(tree)) {
          auto* symbol = dynamic_cast<QgsSymbolLegendNode*>(node);
          if (!symbol || !symbol->customSymbol()) continue;
          for (int i = 0; i < symbol->customSymbol()->symbolLayerCount(); ++i)
            if (auto* glyph = dynamic_cast<QgsFontMarkerSymbolLayer*>(symbol->customSymbol()->symbolLayer(i)))
              result.insert(glyph->character());
        }
      return result;
    };
    QVERIFY(!placedNumbers().isEmpty());
    QTRY_COMPARE_WITH_TIMEOUT(legendNumbers(), placedNumbers(), 5000);
    legendLayer = legend->model()->rootGroup()->findLayer(layer->id());
    QVERIFY(legendLayer);
    const auto finalNodes = legend->model()->layerLegendNodes(legendLayer);
    QVERIFY(finalNodes.size() <= categoryCount);
    auto* sourceRenderer = dynamic_cast<QgsCategorizedSymbolRenderer*>(layer->renderer());
    QVERIFY(sourceRenderer);
    QMap<QString, int> displayedNumbers;
    int nextNumber = 0;
    for (auto* node : finalNodes) {
      auto* symbolNode = dynamic_cast<QgsSymbolLegendNode*>(node);
      QVERIFY(symbolNode && symbolNode->customSymbol());
      QString badge;
      for (int i = 0; i < symbolNode->customSymbol()->symbolLayerCount(); ++i)
        if (auto* font = dynamic_cast<QgsFontMarkerSymbolLayer*>(symbolNode->customSymbol()->symbolLayer(i)))
          badge = font->character();
      const QString name = node->data(Qt::DisplayRole).toString();
      const int category = sourceRenderer->categoryIndexForValue(name);
      QVERIFY(category >= 0);
      QCOMPARE(badge, QString::number(++nextNumber));
      displayedNumbers.insert(name, nextNumber);
    }
    std::unique_ptr<QgsVectorLayer> drawing(layer->clone());
    QgsMapLayerStyle(map->layerStyleOverrides().value(layer->id())).writeToLayer(drawing.get());
    QVERIFY(drawing->labeling());
    QgsExpression labelExpression(drawing->labeling()->settings().fieldName);
    QVERIFY(!labelExpression.hasParserError());
    auto expressionContext = drawing->createExpressionContext();
    QgsFeature firstFeature;
    QVERIFY(drawing->getFeatures().nextFeature(firstFeature));
    expressionContext.setFeature(firstFeature);
    QCOMPARE(labelExpression.evaluate(&expressionContext).toInt(),
             displayedNumbers.value(firstFeature.attribute(QStringLiteral("nm")).toString()));
    QVERIFY(!labelExpression.hasEvalError());

    // Record repeated scene paints while moving the same cached legend. These
    // timings are diagnostic observations, not a machine-dependent pass limit.
    const QPointF legendPosition = legend->pos();
    QImage sceneImage(640, 480, QImage::Format_ARGB32_Premultiplied);
    elapsed.restart();
    for (int i = 0; i < 4; ++i) {
      legend->attemptMove(QgsLayoutPoint(legendPosition + QPointF(i, 0.)));
      sceneImage.fill(Qt::white);
      QPainter painter(&sceneImage);
      layout->render(&painter, QRectF(0., 0., 640., 480.), legend->sceneBoundingRect());
    }
    const qint64 legendMovePaintMs = elapsed.elapsed();
    legend->attemptMove(QgsLayoutPoint(legendPosition));
    qInfo() << "LAYOUT_INTERACTION held_ms=400 held_overrides=0 released_candidate_updates=1"
            << "legend_columns=" << legend->columnCount()
            << "legend_move_scene_paints=4 elapsed_ms=" << legendMovePaintMs;
    qInfo().noquote() << QStringLiteral(
        "LAYOUT_STUDIO_PERF categories=%1 first_ready_ms=%2 revisit_show_ms=%3 unchanged_refreshes=10 unchanged_overrides=0 visible_candidate_updates=1 hidden_candidate_updates=1")
        .arg(categoryCount).arg(firstReadyMs).arg(revisitShowMs);
    // A parent checkbox hides its descendants without erasing their own choices.
    auto* references = project.layerTreeRoot()->findGroup(QStringLiteral("참조 지도"));
    QVERIFY(references);
    auto* sourceNode = references->findLayer(layer->id());
    QVERIFY(sourceNode && sourceNode->itemVisibilityChecked());
    references->setItemVisibilityChecked(false);
    QTRY_VERIFY_WITH_TIMEOUT(!map->layers().contains(layer), 5000);
    QTRY_VERIFY_WITH_TIMEOUT(!map->layerStyleOverrides().contains(layer->id()), 5000);
    QTRY_VERIFY_WITH_TIMEOUT(!legend->model()->rootGroup()->findLayer(layer->id()), 5000);
    QVERIFY(sourceNode->itemVisibilityChecked());
    references->setItemVisibilityChecked(true);
    QTRY_VERIFY_WITH_TIMEOUT(map->layers().contains(layer), 5000);
    QTRY_VERIFY_WITH_TIMEOUT(legend->model()->rootGroup()->findLayer(layer->id()), 5000);
    QTRY_VERIFY_WITH_TIMEOUT(!placedNumbers().isEmpty(), 5000);
    QTRY_COMPARE_WITH_TIMEOUT(legendNumbers(), placedNumbers(), 5000);
    // Moving the geographic map partly off paper changes the numbered subset
    // without changing its extent. Compact that subset, then restore all source
    // candidates when the map is brought back onto the page.
    const QPointF beforeMove = map->pos();
    const int beforeMoveCount = legendNumbers().size();
    map->attemptMove(QgsLayoutPoint(-map->rect().width() * .5, beforeMove.y()));
    view->viewport()->update();
    auto compactedSubset = [&]() {
      const auto& entries = numberOwner->entries();
      return std::any_of(entries.cbegin(), entries.cend(), [](const auto& entry) { return entry.number == 0; });
    };
    QTRY_VERIFY_WITH_TIMEOUT(compactedSubset(), 10000);
    QTRY_VERIFY_WITH_TIMEOUT(!legendNumbers().isEmpty() && legendNumbers().size() < beforeMoveCount, 10000);
    QTRY_COMPARE_WITH_TIMEOUT(legendNumbers(), placedNumbers(), 10000);
    const auto compacted = legendNumbers();
    for (int n = 1; n <= compacted.size(); ++n) QVERIFY(compacted.contains(QString::number(n)));
    map->attemptMove(QgsLayoutPoint(beforeMove));
    view->viewport()->update();
    QTRY_COMPARE_WITH_TIMEOUT(legendNumbers().size(), beforeMoveCount, 10000);
    QTRY_COMPARE_WITH_TIMEOUT(legendNumbers(), placedNumbers(), 10000);
  }
  void drawingStudioAboveLabelsMapDrawsGeometryWithoutDuplicateNumberLabels() {
    QgsProject project;
    project.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5187")));
    QVector<QgsVectorLayer*> sources;
    const QList<HeritageDataset> datasets = {
        HeritageDataset::DesignatedHeritage, HeritageDataset::SurfaceSurveyArea};
    auto* references = project.layerTreeRoot()->addGroup(QStringLiteral("참조 지도"));
    for (int i = 0; i < datasets.size(); ++i) {
      auto* layer = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5187&field=nm:string(80)"),
          QStringLiteral("합성 원본 %1").arg(i + 1), QStringLiteral("memory"));
      QVERIFY(layer->isValid());
      QVERIFY(layer->startEditing());
      QgsFeature feature(layer->fields());
      feature.setAttribute(QStringLiteral("nm"), QStringLiteral("원본 유적명 %1").arg(i + 1));
      const double offset = i * 30.;
      feature.setGeometry(QgsGeometry::fromRect(QgsRectangle(
          190020. + offset, 560020. + offset, 190140. + offset, 560140. + offset)));
      QVERIFY(layer->addFeature(feature));
      QVERIFY(layer->commitChanges());
      QVERIFY(HeritageStyle::apply(layer, datasets[i], QStringLiteral("nm")).ok);
      QVERIFY(LayerOps::applyNameAttributeLabels(layer, QStringLiteral("nm"), 5., false));
      LayerOps::markReferenceLayer(layer);
      project.addMapLayer(layer, false);
      references->addGroup(HeritageStyle::layerName(datasets[i]))->addLayer(layer);
      sources.append(layer);
    }
    // Establish the existing map's label-order properties before capturing the
    // source baseline; the drawing must not change those styles or labels.
    LayerOps::applyLayerOrderToLabels(&project, nullptr);
    QCOMPARE(LayerOps::layersDrawnAboveLabels(&project), QList<QgsMapLayer*>({sources[0]}));
    QMap<QString, QString> sourceStyles;
    QMap<QString, QString> sourceUris;
    for (auto* source : sources) {
      QgsMapLayerStyle style;
      style.readFromLayer(source);
      sourceStyles.insert(source->id(), style.xmlData());
      sourceUris.insert(source->id(), source->source());
      QVERIFY(source->labelsEnabled());
    }
    QgsMapCanvas canvas;
    canvas.setRenderFlag(false);
    canvas.setDestinationCrs(project.crs());
    canvas.setLayers({sources[0], sources[1]});
    canvas.setExtent(QgsRectangle(189950., 559950., 190250., 560250.));
    KaDrawingStudio studio(&project, &canvas, 297., 210.);
    studio.setAttribute(Qt::WA_DontShowOnScreen);
    studio.show();
    auto* view = studio.findChild<QgsLayoutView*>();
    QVERIFY(view && view->currentLayout());
    auto* layout = view->currentLayout();
    QTRY_VERIFY_WITH_TIMEOUT(layout->itemById(QStringLiteral("ka_map")), 5000);
    auto* base = dynamic_cast<QgsLayoutItemMap*>(layout->itemById(QStringLiteral("ka_map")));
    QVERIFY(base);
    QTRY_VERIFY_WITH_TIMEOUT(base->layerStyleOverrides().contains(sources[1]->id()), 5000);
    auto* above = dynamic_cast<QgsLayoutItemMap*>(layout->itemById(QStringLiteral("ka_map_above")));
    QVERIFY(above);
    QCOMPARE(above->layers(), QList<QgsMapLayer*>({sources[0]}));
    QVERIFY(above->keepLayerStyles());
    QVERIFY(above->layerStyleOverrides().contains(sources[0]->id()));
    for (auto* source : sources) {
      std::unique_ptr<QgsVectorLayer> drawing(source->clone());
      QgsMapLayerStyle(base->layerStyleOverrides().value(source->id())).writeToLayer(drawing.get());
      QVERIFY(drawing->labelsEnabled());
      QVERIFY(drawing->labeling());
      const auto settings = drawing->labeling()->settings();
      QVERIFY(settings.isExpression);
      QgsExpression number(settings.fieldName);
      QVERIFY(!number.hasParserError());
      QgsFeature feature;
      QVERIFY(drawing->getFeatures().nextFeature(feature));
      auto context = drawing->createExpressionContext();
      context.setFeature(feature);
      QCOMPARE(number.evaluate(&context).toInt(), 1); // Each dataset starts at 1.
      QVERIFY(!number.hasEvalError());
      if (above->layers().contains(source)) {
        std::unique_ptr<QgsVectorLayer> geometryOnly(source->clone());
        QgsMapLayerStyle(above->layerStyleOverrides().value(source->id())).writeToLayer(geometryOnly.get());
        QVERIFY(!geometryOnly->labelsEnabled());
        QVERIFY(geometryOnly->renderer());
        QCOMPARE(geometryOnly->renderer()->type(), drawing->renderer()->type());
      }
      QgsMapLayerStyle unchanged;
      unchanged.readFromLayer(source);
      QCOMPARE(unchanged.xmlData(), sourceStyles.value(source->id()));
      QCOMPARE(source->source(), sourceUris.value(source->id()));
      QCOMPARE(source->featureCount(), 1LL);
      QVERIFY(source->labelsEnabled());
    }
  }

  void drawingStudio_largeScaleChipsApplyToMapAndInput() {
    QgsProject project;
    project.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5187")));
    QgsMapCanvas canvas;
    canvas.setRenderFlag(false);
    canvas.setDestinationCrs(project.crs());
    canvas.setExtent(QgsRectangle(190000, 560000, 191000, 561000));
    KaDrawingStudio studio(&project, &canvas, 297.0, 210.0);
    studio.setAttribute(Qt::WA_DontShowOnScreen);
    auto* view = studio.findChild<QgsLayoutView*>();
    QVERIFY(view && view->currentLayout());
    view->setUpdatesEnabled(false);
    studio.show();
    // Complete the studio's queued initial map placement before selecting a scale.
    QCoreApplication::processEvents();
    auto* map = dynamic_cast<QgsLayoutItemMap*>(view->currentLayout()->itemById(QStringLiteral("ka_map")));
    QVERIFY(map);
    QCOMPARE(map->crs().authid(), QStringLiteral("EPSG:5187"));
    auto* spin = studio.findChild<QSpinBox*>(QStringLiteral("drawingScale"));
    QVERIFY(spin);
    const QFontMetrics font(spin->font());
    QVERIFY(font.inFont(QChar(u'1')));
    QVERIFY(font.inFont(QChar(u'한')));
    auto* input = spin->findChild<QLineEdit*>();
    QVERIFY(input);
    const auto chips = studio.findChildren<QToolButton*>(QStringLiteral("scaleChip"));
    // Exercise both new choices and a return to an existing choice through the
    // actual clicked signal; setting the spinbox directly would miss bad wiring.
    for (const int denominator : {10000, 25000, 5000}) {
      QToolButton* target = nullptr;
      int matches = 0;
      for (QToolButton* chip : chips) {
        if (chip->property("denom").toInt() == denominator) {
          target = chip;
          ++matches;
        }
      }
      QCOMPARE(matches, 1);
      QVERIFY(target && target->isEnabled());
      target->click();
      QCOMPARE(spin->value(), denominator);
      QCOMPARE(input->text(), QString::number(denominator));
      for (QToolButton* chip : chips)
        QCOMPARE(chip->isChecked(), chip->property("denom").toInt() == denominator);
      QVERIFY2(qAbs(map->scale() - denominator) < 0.5,
               qPrintable(QStringLiteral("chip 1:%1 applied map scale %2")
                              .arg(denominator).arg(map->scale(), 0, 'f', 3)));
    }
    const QString output = qEnvironmentVariable("KA_HGIS_QA_OUTPUT_DIR");
    if (!output.isEmpty() && QDir(output).exists()) {
      const QString path = QDir(output).filePath(QStringLiteral("drawing-scale-chips-widget-render.png"));
      QVERIFY2(spin->parentWidget()->grab().save(path), qPrintable(path));
      qInfo().noquote() << "Automatic Qt widget render; not a portable field screenshot:" << path;
    }
  }
  void drawingInspectorUsesHeightWithoutOverlapping_data() {
    QTest::addColumn<int>("height");
    QTest::newRow("normal") << 930;
    QTest::newRow("tall") << 1150;
    QTest::newRow("short") << 620;
  }
  void drawingInspectorUsesHeightWithoutOverlapping() {
    QFETCH(int, height);
    QgsProject project;
    project.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5187")));
    QgsMapCanvas canvas;
    canvas.setRenderFlag(false);
    canvas.setDestinationCrs(project.crs());
    canvas.setExtent(QgsRectangle(190000, 560000, 191000, 561000));
    QTabWidget tabs;
    KaDrawingStudio studio(&project, &canvas, 210., 297., &tabs);
    studio.setParent(&tabs, Qt::Widget);
    tabs.addTab(&studio, QStringLiteral("레이아웃"));
    tabs.resize(1800, height);
    tabs.show();
    QCoreApplication::processEvents();
    auto* scale = studio.findChild<QSpinBox*>(QStringLiteral("drawingScale"));
    QVERIFY(scale);
    QWidget* card = scale->parentWidget();
    QWidget* panel = card->parentWidget();
    auto* scroll = studio.findChild<QScrollArea*>(QStringLiteral("drawingInspectorScroll"));
    const auto buttons = card->findChildren<QToolButton*>();
    QToolButton* crsButton = nullptr;
    for (auto* button : buttons) if (button->text() == QStringLiteral("좌표계")) crsButton = button;
    QVERIFY(crsButton);
    const QString output = qEnvironmentVariable("KA_HGIS_QA_OUTPUT_DIR");
    const QString tag = QString::fromLatin1(QTest::currentDataTag());
    if (!output.isEmpty()) {
      QVERIFY(tabs.grab().save(QDir(output).filePath(tag + QStringLiteral("-studio.png"))));
      QVERIFY((scroll ? static_cast<QWidget*>(scroll) : panel)->grab().save(
          QDir(output).filePath(tag + QStringLiteral("-panel.png"))));
    }
    const int bottomGap = panel->height() - crsButton->mapTo(panel, QPoint(0, crsButton->height())).y();
    qInfo() << "inspector metrics: requested height" << height << "actual" << tabs.height()
            << "panel" << panel->size() << "bottom gap" << bottomGap;
    QVERIFY2(tabs.height() <= height, "Inspector must not force the app beyond its requested height");
    QVERIFY2(scroll, "Short windows need a scrollable inspector");
    QVERIFY2(bottomGap <= 24, qPrintable(QStringLiteral("Unused space below the last row: %1 px").arg(bottomGap)));
    for (auto* button : panel->findChildren<QToolButton*>()) {
      QVERIFY2(button->height() >= button->sizeHint().height(), qPrintable(
          QStringLiteral("%1 button clipped to %2 px; content needs %3 px")
              .arg(button->text()).arg(button->height()).arg(button->sizeHint().height())));
      QVERIFY(button->parentWidget()->rect().contains(button->geometry()));
    }
    QList<QToolButton*> firstColumn;
    for (int denominator : {100, 300, 1000, 10000}) {
      for (auto* button : buttons)
        if (button->property("denom").toInt() == denominator) firstColumn.append(button);
    }
    QCOMPARE(firstColumn.size(), 4);
    for (int i = 1; i < firstColumn.size(); ++i) {
      const auto* before = firstColumn[i - 1];
      const auto* after = firstColumn[i];
      const int gap = after->y() - before->geometry().bottom() - 1;
      qInfo() << "preset row gap" << gap;
      QVERIFY2(gap >= 8, qPrintable(QStringLiteral("Preset rows have only %1 px separation").arg(gap)));
    }
    for (int i = 0; i < buttons.size(); ++i)
      for (int j = i + 1; j < buttons.size(); ++j)
        QVERIFY(!buttons[i]->geometry().intersects(buttons[j]->geometry()));
    if (height < 700) {
      QVERIFY(scroll->verticalScrollBar()->maximum() > 0);
      scroll->ensureWidgetVisible(crsButton);
      QCoreApplication::processEvents();
      const QRect lastRow(crsButton->mapTo(scroll->viewport(), QPoint()), crsButton->size());
      QVERIFY(scroll->viewport()->rect().contains(lastRow));
      if (!output.isEmpty()) QVERIFY(scroll->grab().save(QDir(output).filePath(tag + QStringLiteral("-scrolled.png"))));
      auto* splitter = studio.findChild<QSplitter*>(QStringLiteral("studioMainSplit"));
      QVERIFY(splitter);
      splitter->setSizes({268, 1200, 1});
      QCoreApplication::processEvents();
      QVERIFY(scroll->viewport()->width() >= panel->minimumSizeHint().width());
      for (auto* button : panel->findChildren<QToolButton*>()) {
        QVERIFY(button->parentWidget()->rect().contains(button->geometry()));
        QVERIFY2(button->width() >= button->sizeHint().width(), qPrintable(button->text()));
      }
      if (!output.isEmpty()) QVERIFY(scroll->grab().save(QDir(output).filePath(tag + QStringLiteral("-narrow.png"))));
    }
    scroll->ensureWidgetVisible(firstColumn.last());
    QTest::mouseClick(firstColumn.last(), Qt::LeftButton);
    QCOMPARE(scale->value(), 10000);
    auto* view = studio.findChild<QgsLayoutView*>();
    QVERIFY(view && view->currentLayout());
    auto* map = dynamic_cast<QgsLayoutItemMap*>(view->currentLayout()->itemById(QStringLiteral("ka_map")));
    QVERIFY(map && qAbs(map->scale() - 10000.) < .5);
    QCOMPARE(map->crs().authid(), QStringLiteral("EPSG:5187"));
  }

  void layerContextMenu_matchesLayerKind_data() {
    QTest::addColumn<QString>("kind");
    QTest::addColumn<QString>("firstAction");
    QTest::newRow("survey-area") << QStringLiteral("survey_area") << QStringLiteral("layer.import");
    QTest::newRow("feature-polygon") << QStringLiteral("feature_poly") << QStringLiteral("layer.import");
    QTest::newRow("control-points") << QStringLiteral("control_points") << QStringLiteral("layer.import");
    QTest::newRow("section-line") << QStringLiteral("section_line") << QStringLiteral("layer.import");
    QTest::newRow("trial-trench") << QStringLiteral("trial_trench") << QStringLiteral("layer.import");
    QTest::newRow("satellite-xyz") << QStringLiteral("satellite") << QStringLiteral("layer.import");
    QTest::newRow("cadastral-wms") << QStringLiteral("cadastral") << QStringLiteral("layer.import");
    QTest::newRow("dem-raster") << QStringLiteral("dem") << QStringLiteral("layer.import");
    QTest::newRow("external-shapefile") << QStringLiteral("external_shp") << QStringLiteral("layer.import");
    QTest::newRow("imported-reference-raster") << QStringLiteral("imported_raster") << QStringLiteral("layer.import");
  }
  void layerContextMenu_matchesLayerKind() {
    QFETCH(QString, kind);
    QFETCH(QString, firstAction);
    QString error;
    const QString path = SurveyProjectFactory::createNewSurvey(
        m_files.path(), QStringLiteral("menu_%1").arg(kind), &error, QStringLiteral("EPSG:5187"));
    QVERIFY2(!path.isEmpty(), qPrintable(error));
    MainWindow window;
    window.setProperty("qaMenuKind", kind);
    disableRendering(window);
    QVERIFY(window.openSurveyGpkg(path));
    disableRendering(window);
    const bool domain = LayerOps::domainLayerKeys().contains(kind);
    QgsMapLayer* layer = domain ? LayerOps::ensureDomainLayer(
        QgsProject::instance(), path, kind, kind, &error) : makeMenuReference(kind);
    QVERIFY2(layer && layer->isValid(), qPrintable(error));
    if (domain) {
      auto* vector = qobject_cast<QgsVectorLayer*>(layer);
      QVERIFY(vector && vector->startEditing());
      QgsFeature feature(vector->fields());
      if (vector->geometryType() == Qgis::GeometryType::Point)
        feature.setGeometry(QgsGeometry::fromPointXY(QgsPointXY(190000, 560000)));
      else if (vector->geometryType() == Qgis::GeometryType::Line)
        feature.setGeometry(QgsGeometry::fromPolylineXY({QgsPointXY(190000, 560000), QgsPointXY(190100, 560100)}));
      else
        feature.setGeometry(QgsGeometry::fromRect(QgsRectangle(190000, 560000, 190100, 560100)));
      QVERIFY(vector->addFeature(feature));
      QVERIFY(vector->commitChanges());
    }
    auto* tree = window.findChild<QgsLayerTreeView*>(QStringLiteral("layerTree"));
    QVERIFY(tree);
    window.show();
    tree->expandAll();
    tree->setCurrentLayer(layer);
    QApplication::processEvents();
    const QModelIndex index = tree->layerTreeModel()->node2index(
        QgsProject::instance()->layerTreeRoot()->findLayer(layer));
    QVERIFY(index.isValid());
    tree->scrollTo(index);
    const LayerMenuState menu = inspectLayerMenu(window, tree, tree->visualRect(index).center());
    QVERIFY(menu.seen);
    QCOMPARE(menu.name, QStringLiteral("layerContextMenu"));
    QStringList ids;
    for (const auto& action : menu.actions) {
      if (action.separator) continue;
      QVERIFY2(!action.id.isEmpty(), qPrintable(action.text));
      ids.append(action.id);
      if (!action.enabled) {
        QVERIFY2(!action.toolTip.trimmed().isEmpty(), qPrintable(action.id));
        QVERIFY2(action.toolTip != action.text, qPrintable(action.id));
      }
    }
    // Common positions are intentional: the user requested the earlier shared menu.
    QCOMPARE(ids.first(), firstAction);
    QCOMPARE(ids.last(), QStringLiteral("layer.remove"));
    int destructiveStart = menu.actions.size() - 1;
    if (ids.contains(QStringLiteral("layer.clear"))) {
      QVERIFY(domain);
      QCOMPARE(menu.actions.at(destructiveStart - 1).id, QStringLiteral("layer.clear"));
      --destructiveStart;
    }
    QVERIFY(destructiveStart > 0);
    QVERIFY(menu.actions.at(destructiveStart - 1).separator);
    for (const QString& common : {QStringLiteral("layer.import"), QStringLiteral("layer.rename"),
        QStringLiteral("layer.style"), QStringLiteral("layer.attributes"), QStringLiteral("layer.labels"),
        QStringLiteral("layer.opacity"), QStringLiteral("layer.zoom"), QStringLiteral("layer.fullExtent")})
      QVERIFY2(ids.contains(common), qPrintable(common));
    if (!domain) QVERIFY(!ids.contains(QStringLiteral("layer.clear")));
    if (auto* vector = qobject_cast<QgsVectorLayer*>(layer)) {
      // Removal keeps the live layer and edit buffer in undo history.
      QVERIFY(vector->startEditing());
      QgsFeature extra = vector->getFeature(*vector->allFeatureIds().constBegin());
      extra.setId(FID_NULL);
      QVERIFY(vector->addFeature(extra));
      QVERIFY(vector->isModified());
      const LayerMenuState dirtyMenu = inspectLayerMenu(window, tree, tree->visualRect(index).center());
      QVERIFY(dirtyMenu.seen);
      bool removeFound = false;
      for (const auto& action : dirtyMenu.actions) {
        if (action.id != QLatin1String("layer.remove")) continue;
        removeFound = true;
        QVERIFY(action.enabled);
        QVERIFY(!action.toolTip.trimmed().isEmpty());
        QVERIFY(action.toolTip != action.text);
      }
      QVERIFY(removeFound);
      QVERIFY(vector->rollBack());
    }
  }
  void changingLabelFontKeepsEveryLayerAndItsVisibility() {
    const QString path = makeSurvey(QStringLiteral("글자 크기 검증"));
    QVERIFY(!path.isEmpty());
    MainWindow window;
    disableRendering(window);
    QVERIFY(window.openSurveyGpkg(path));
    auto* project = QgsProject::instance();
    auto* layer = LayerOps::findByLayerKey(project, QStringLiteral("survey_area"));
    auto* tree = window.findChild<QgsLayerTreeView*>(QStringLiteral("layerTree"));
    auto* canvas = window.findChild<QgsMapCanvas*>();
    QVERIFY(layer && tree && canvas);
    QVERIFY(LayerOps::applyAreaM2Labels(layer));
    const QString areaExpression = layer->labeling()->settings().fieldName;
    const auto ids = project->mapLayers().keys();
    QMap<QString, bool> visibility;
    for (auto* node : project->layerTreeRoot()->findLayers()) visibility[node->layerId()] = node->itemVisibilityChecked();
    window.resize(1800, 1000);
    window.show();
    tree->expandAll();
    tree->setCurrentLayer(layer);
    canvas->setExtent(QgsRectangle(189980, 559980, 190130, 560130));
    QSignalSpy rendered(canvas, &QgsMapCanvas::mapCanvasRefreshed);
    canvas->setRenderFlag(true);
    canvas->refresh();
    QTRY_VERIFY_WITH_TIMEOUT(!rendered.isEmpty(), 15000);
    const QString output = qEnvironmentVariable("KA_HGIS_QA_OUTPUT_DIR");
    if (!output.isEmpty()) QVERIFY(window.grab().save(QDir(output).filePath(QStringLiteral("map-before-font.png"))));
    bool changed = false;
    bool areaChecked = false;
    QTimer::singleShot(0, &window, [&]() {
      auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
      if (!menu) return;
      auto* areaAction = menu->findChild<QAction*>(QStringLiteral("layer.labelArea"));
      areaChecked = areaAction && areaAction->isChecked();
      auto* sizeAction = menu->findChild<QAction*>(QStringLiteral("layer.labelSize"));
      auto* sizes = sizeAction ? sizeAction->menu() : nullptr;
      if (sizes) {
        for (auto* action : sizes->actions()) if (action->text() == QLatin1String("12 pt")) {
          action->trigger(); changed = true; break;
        }
      }
      menu->close();
    });
    const QModelIndex index = tree->layerTreeModel()->node2index(project->layerTreeRoot()->findLayer(layer));
    window.showLayerTreeContextMenu(tree, tree->visualRect(index).center());
    QVERIFY(changed);
    QVERIFY(areaChecked);
    QCOMPARE(LayerOps::labelFontSize(layer), 12.0);
    rendered.clear(); canvas->refresh();
    QTRY_VERIFY_WITH_TIMEOUT(!rendered.isEmpty(), 15000);
    QCOMPARE(project->mapLayers().keys(), ids);
    for (auto it = visibility.cbegin(); it != visibility.cend(); ++it) {
      auto* node = project->layerTreeRoot()->findLayer(it.key());
      QVERIFY(node);
      QCOMPARE(node->itemVisibilityChecked(), it.value());
    }
    if (!output.isEmpty()) QVERIFY(window.grab().save(QDir(output).filePath(QStringLiteral("map-after-font.png"))));
    QCOMPARE(layer->labeling()->settings().fieldName, areaExpression);
    QVERIFY(LayerOps::labelShowArea(layer));
    canvas->setRenderFlag(false);
  }

  void trenchPresetRespectsAreaAndDimensions_data() {
    QTest::addColumn<bool>("changedCanvasCrs");
    QTest::newRow("work-5187") << false;
    QTest::newRow("canvas-changed-to-5186") << true;
  }
  void trenchPresetRespectsAreaAndDimensions() {
    QFETCH(bool, changedCanvasCrs);
    const QString path = makeSurvey(QStringLiteral("trench_limits_%1").arg(QString::fromLatin1(QTest::currentDataTag())));
    QVERIFY(!path.isEmpty());
    MainWindow window;
    disableRendering(window);
    QVERIFY(window.openSurveyGpkg(path));
    disableRendering(window);
    auto* project = QgsProject::instance();
    auto* area = LayerOps::findByLayerKey(project, QStringLiteral("survey_area"));
    auto* canvas = window.findChild<QgsMapCanvas*>();
    auto* tree = window.findChild<QgsLayerTreeView*>(QStringLiteral("layerTree"));
    QVERIFY(area && canvas && tree);
    QgsGeometry boundary = QgsGeometry::fromRect(QgsRectangle(190000, 560000, 190025, 560115));
    QVERIFY(area->startEditing());
    QVERIFY(area->changeGeometry(*area->allFeatureIds().constBegin(), boundary));
    QVERIFY(area->commitChanges());
    area->setRenderer(new QgsSingleSymbolRenderer(QgsFillSymbol::createSimple(
        {{QStringLiteral("color"), QStringLiteral("255,255,255,0")},
         {QStringLiteral("outline_color"), QStringLiteral("194,93,20,255")},
         {QStringLiteral("outline_width"), QStringLiteral("0.6")}}).release()));
    tree->setCurrentLayer(area);
    window.resize(1280, 860);
    window.show();
    QCoreApplication::processEvents();
    if (changedCanvasCrs)
      QVERIFY(QMetaObject::invokeMethod(&window, "setWorkCrs5186", Qt::DirectConnection));
    QVERIFY(QMetaObject::invokeMethod(&window, "applyTrenchByRatio", Qt::DirectConnection, Q_ARG(double, 10.)));
    auto* trenches = LayerOps::findByLayerKey(project, QStringLiteral("trial_trench"));
    QVERIFY(trenches && trenches->featureCount() > 0);
    const qint64 firstCount = trenches->featureCount();
    // Loading/generating schedules renders. Finish their cancellation before
    // measuring the frame requested below, not the old survey-opening frame.
    canvas->setRenderFlag(false);
    canvas->stopRendering();
    QCoreApplication::processEvents();
    QTRY_VERIFY_WITH_TIMEOUT(!canvas->isDrawing(), 15000);
    QgsCoordinateTransform toCanvas(area->crs(), canvas->mapSettings().destinationCrs(), project);
    const QgsRectangle viewBounds = toCanvas.transformBoundingBox(QgsRectangle(189935, 559990, 190090, 560125));
    canvas->setExtent(viewBounds);
    canvas->setLayers({trenches, area});
    QSignalSpy rendered(canvas, &QgsMapCanvas::mapCanvasRefreshed);
    canvas->setRenderFlag(true);
    canvas->refresh();
    QTRY_VERIFY_WITH_TIMEOUT(!rendered.isEmpty(), 15000);
    QTRY_VERIFY_WITH_TIMEOUT(!canvas->isDrawing(), 15000);
    QVERIFY(canvas->extent().contains(toCanvas.transformBoundingBox(boundary.boundingBox())));
    QCOMPARE(canvas->mapSettings().destinationCrs().authid(), changedCanvasCrs ? QStringLiteral("EPSG:5186") : QStringLiteral("EPSG:5187"));
    QCOMPARE(trenches->crs().authid(), area->crs().authid());
    const QString output = qEnvironmentVariable("KA_HGIS_QA_OUTPUT_DIR");
    if (!output.isEmpty()) QVERIFY(window.grab().save(QDir(output).filePath(changedCanvasCrs
        ? QStringLiteral("trench-preset-canvas5186.png") : QStringLiteral("trench-preset.png"))));
    canvas->setRenderFlag(false);
    double total = 0.;
    QgsFeature feature;
    auto iterator = trenches->getFeatures();
    while (iterator.nextFeature(feature)) {
      const QgsGeometry geometry = feature.geometry();
      total += geometry.area();
      QgsExpressionContext labelContext;
      labelContext.setFields(trenches->fields());
      labelContext.setFeature(feature);
      QgsExpression labelExpression(trenches->labeling()->settings().fieldName);
      const QString label = labelExpression.evaluate(&labelContext).toString();
      QVERIFY(label.contains(feature.attribute(QStringLiteral("name")).toString()));
      QVERIFY(geometry.difference(boundary).area() < 1e-6);
      const auto ring = geometry.asPolygon().constFirst();
      QVERIFY(ring.size() >= 5);
      QVERIFY(ring[0].distance(ring[1]) <= 2. + 1e-7);
      QVERIFY(ring[1].distance(ring[2]) <= 20. + 1e-7);
    }
    QVERIFY2(qAbs(total - boundary.area() * .1) < 1e-4, qPrintable(QString::number(total, 'f', 6)));
    tree->setCurrentLayer(area);
    QVERIFY(QMetaObject::invokeMethod(&window, "applyTrenchByRatio", Qt::DirectConnection, Q_ARG(double, 10.)));
    QCOMPARE(trenches->featureCount(), firstCount);
    QVERIFY(trenches->isEditable() || trenches->startEditing());
    const auto fid = *trenches->allFeatureIds().constBegin();
    QVERIFY(trenches->changeAttributeValue(fid, trenches->fields().indexOf(QStringLiteral("name")), QStringLiteral("작성 중")));
    tree->setCurrentLayer(area);
    QVERIFY(QMetaObject::invokeMethod(&window, "applyTrenchByRatio", Qt::DirectConnection, Q_ARG(double, 10.)));
    QVERIFY(trenches->isModified());
    QCOMPARE(trenches->getFeature(fid).attribute(QStringLiteral("name")).toString(), QStringLiteral("작성 중"));
    auto* messages = window.findChild<QgsMessageBar*>();
    QVERIFY(messages && messages->currentItem());
    QCOMPARE(messages->currentItem()->level(), Qgis::MessageLevel::Warning);
    QVERIFY(messages->currentItem()->text().contains(QStringLiteral("저장하지 않은 편집")));
    QVERIFY(trenches->rollBack());
  }

  void layerDeleteKeyPreservesSourceAndUndoRestoresPendingEdits() {
    const QString path = makeSurvey(QStringLiteral("delete_key_undo"));
    QVERIFY(!path.isEmpty());
    MainWindow window;
    disableRendering(window);
    QVERIFY(window.openSurveyGpkg(path));
    disableRendering(window);
    auto* tree = window.findChild<QgsLayerTreeView*>(QStringLiteral("layerTree"));
    auto* project = QgsProject::instance();
    auto* layer = LayerOps::findByLayerKey(project, QStringLiteral("survey_area"));
    QVERIFY(tree && layer);
    const QString id = layer->id();
    QVERIFY(layer->startEditing());
    const auto ids = layer->allFeatureIds();
    const QgsFeatureId fid = *ids.constBegin();
    const int field = layer->fields().indexOf(QStringLiteral("survey_name"));
    QVERIFY(layer->changeAttributeValue(fid, field, QStringLiteral("저장 전 기록")));
    window.show();
    QApplication::setActiveWindow(&window);
    tree->setCurrentLayer(layer);
    tree->setFocus();
    QApplication::processEvents();
    const QByteArray original = contents(path);
    QTest::keyClick(tree, Qt::Key_Delete);
    QVERIFY(!project->mapLayer(id));
    QCOMPARE(contents(path), original);
    QTest::keyClick(tree, Qt::Key_Z, Qt::ControlModifier);
    QCOMPARE(project->mapLayer(id), layer);
    QVERIFY(project->layerTreeRoot()->findLayer(id));
    QVERIFY(layer->isModified());
    QCOMPARE(layer->getFeature(fid).attribute(field).toString(), QStringLiteral("저장 전 기록"));
    QCOMPARE(contents(path), original);
    QVERIFY(layer->rollBack());
  }

  void ctrlZRestoresVertexEditsAndGroupedFeatureDeletion() {
    const QString path = makeSurvey(QStringLiteral("vertex_undo"));
    QVERIFY(!path.isEmpty());
    MainWindow window;
    disableRendering(window);
    QVERIFY(window.openSurveyGpkg(path));
    disableRendering(window);
    auto* layer = LayerOps::findByLayerKey(QgsProject::instance(), QStringLiteral("survey_area"));
    auto* canvas = window.findChild<QgsMapCanvas*>();
    QVERIFY(layer && canvas);
    const auto ids = layer->allFeatureIds();
    const QgsFeatureId fid = *ids.constBegin();
    const QgsGeometry original = layer->getFeature(fid).geometry();
    QVERIFY(QMetaObject::invokeMethod(&window, "startSelectTool", Qt::DirectConnection));
    auto* select = window.findChild<KaFeatureSelectTool*>();
    auto* vertex = select ? select->findChild<KaVertexEditTool*>() : nullptr;
    QVERIFY(vertex);
    window.show();
    QApplication::setActiveWindow(&window);
    canvas->setFocus();
    QApplication::processEvents();
    vertex->setTarget(layer, fid);
    QVERIFY(vertex->moveVertexTo(0, QgsPointXY(190020, 560020)));
    QVERIFY(!layer->getFeature(fid).geometry().equals(original));
    QTest::keyClick(canvas, Qt::Key_Z, Qt::ControlModifier);
    QVERIFY(layer->getFeature(fid).geometry().equals(original));
    vertex->setTarget(layer, fid);
    QVERIFY(vertex->insertVertexAt(1, QgsPointXY(190040, 560000)));
    QTest::keyClick(canvas, Qt::Key_Z, Qt::ControlModifier);
    QVERIFY(layer->getFeature(fid).geometry().equals(original));
    vertex->setTarget(layer, fid);
    QVERIFY(vertex->deleteVertexAt(1));
    QTest::keyClick(canvas, Qt::Key_Z, Qt::ControlModifier);
    QVERIFY(layer->getFeature(fid).geometry().equals(original));
    if (!layer->isEditable()) QVERIFY(layer->startEditing());
    QgsFeature second(layer->fields());
    second.setGeometry(QgsGeometry::fromRect(QgsRectangle(190200, 560000, 190250, 560050)));
    QVERIFY(layer->addFeature(second));
    QVERIFY(layer->commitChanges());
    layer->selectAll();
    canvas->setLayers({layer});
    QTest::keyClick(canvas, Qt::Key_Delete);
    QCOMPARE(layer->featureCount(), 0);
    QTest::keyClick(canvas, Qt::Key_Z, Qt::ControlModifier);
    QCOMPARE(layer->featureCount(), 2);
    QTest::keyClick(canvas, Qt::Key_Z, Qt::ControlModifier);
    QCOMPARE(layer->featureCount(), 2); // no duplicated fallback history
    // Restoring a deletion into a dirty edit buffer creates a temporary FID.
    // Saving must remap the earlier geometry command to the committed FID.
    const auto restoredIds = layer->allFeatureIds();
    const auto editedId = *restoredIds.constBegin();
    const auto previous = layer->getFeature(editedId).geometry();
    if (!layer->isEditable()) QVERIFY(layer->startEditing());
    const int nameField = layer->fields().indexOf(QStringLiteral("survey_name"));
    QVERIFY(layer->changeAttributeValue(editedId, nameField, QStringLiteral("미저장 기록")));
    vertex->setTarget(layer, editedId);
    QVERIFY(vertex->moveVertexTo(0, QgsPointXY(190025, 560025)));
    layer->selectByIds({editedId});
    QTest::keyClick(canvas, Qt::Key_Delete);
    QTest::keyClick(canvas, Qt::Key_Z, Qt::ControlModifier);
    QVERIFY(saveNow(window));
    QTest::keyClick(canvas, Qt::Key_Z, Qt::ControlModifier);
    bool previousFound = false;
    QgsFeature restored;
    auto iterator = layer->getFeatures();
    while (iterator.nextFeature(restored)) previousFound |= restored.geometry().equals(previous);
    QVERIFY(previousFound);
  }

  void layerContextMenu_usesClickedRowAndLeavesSourceIntact() {
    const QString path = makeSurvey(QStringLiteral("menu_target"));
    QVERIFY(!path.isEmpty());
    MainWindow window;
    disableRendering(window);
    QVERIFY(window.openSurveyGpkg(path));
    disableRendering(window);
    auto* project = QgsProject::instance();
    auto* survey = LayerOps::findByLayerKey(project, QStringLiteral("survey_area"));
    QVERIFY(survey);
    auto* external = makeMenuReference(QStringLiteral("external_shp"));
    QVERIFY(external && external->isValid());
    const QString externalId = external->id();
    const QString surveyId = survey->id();
    const QString externalPath = external->source().section(QLatin1Char('|'), 0, 0);
    const QByteArray originalSurvey = contents(path);
    const QByteArray originalShp = contents(externalPath);
    QVERIFY(!originalSurvey.isEmpty() && !originalShp.isEmpty());
    auto* tree = window.findChild<QgsLayerTreeView*>(QStringLiteral("layerTree"));
    QVERIFY(tree);
    window.resize(1280, 900);
    tree->setMinimumHeight(360);
    window.show();
    tree->expandAll();
    tree->setCurrentLayer(survey);
    QApplication::processEvents();
    tree->collapseAll();
    const QPoint blank(tree->viewport()->width() / 2, tree->viewport()->height() - 2);
    QVERIFY(!tree->indexAt(blank).isValid());
    const LayerMenuState blankMenu = inspectLayerMenu(window, tree, blank);
    QVERIFY(blankMenu.seen);
    for (const auto& action : blankMenu.actions) {
      QVERIFY(action.id != QLatin1String("layer.remove"));
      QVERIFY(action.id != QLatin1String("layer.clear"));
    }
    tree->expandAll();
    tree->setCurrentLayer(survey);
    const QModelIndex clicked = tree->layerTreeModel()->node2index(project->layerTreeRoot()->findLayer(external));
    QVERIFY(clicked.isValid());
    tree->scrollTo(clicked);
    const LayerMenuState clickedMenu = inspectLayerMenu(window, tree,
        tree->visualRect(clicked).center(), QStringLiteral("layer.remove"));
    QVERIFY(clickedMenu.seen);
    QVERIFY(!project->mapLayer(externalId));
    QCOMPARE(project->mapLayer(surveyId), survey);
    QCOMPARE(survey->featureCount(), 1LL);
    QCOMPARE(contents(path), originalSurvey);
    QCOMPARE(contents(externalPath), originalShp);
    // Removing a survey legend entry must likewise preserve its saved features.
    tree->setCurrentLayer(survey);
    const QModelIndex surveyIndex = tree->layerTreeModel()->node2index(project->layerTreeRoot()->findLayer(survey));
    tree->scrollTo(surveyIndex);
    const LayerMenuState surveyMenu = inspectLayerMenu(window, tree,
        tree->visualRect(surveyIndex).center(), QStringLiteral("layer.remove"));
    QVERIFY(surveyMenu.seen);
    QVERIFY(!project->mapLayer(surveyId));
    QCOMPARE(contents(path), originalSurvey);
    QgsVectorLayer stored(path + QStringLiteral("|layername=survey_area"),
                          QStringLiteral("stored"), QStringLiteral("ogr"));
    QVERIFY(stored.isValid());
    QCOMPARE(stored.featureCount(), 1LL);
  }
  void layerContextMenu_moveToBottomPreservesRegisteredLayer() {
    const QString path = makeSurvey(QStringLiteral("menu_reorder"));
    QVERIFY(!path.isEmpty());
    MainWindow window;
    disableRendering(window);
    QVERIFY(window.openSurveyGpkg(path));
    disableRendering(window);
    auto* project = QgsProject::instance();
    project->setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5187")));
    auto* dem = makeMenuReference(QStringLiteral("dem"));
    QVERIFY(dem && dem->isValid());
    const QString source = dem->source();
    const QByteArray originalFile = contents(source);
    QVERIFY(!originalFile.isEmpty());
    LayerOps::placeInLegendGroup(project, dem, QStringLiteral("참조 지도"));
    auto* originalNode = project->layerTreeRoot()->findLayer(dem);
    QVERIFY(originalNode);
    auto* group = qobject_cast<QgsLayerTreeGroup*>(originalNode->parent());
    QVERIFY(group);
    auto* other = new QgsRasterLayer(source, QStringLiteral("다른 고도 자료"), QStringLiteral("gdal"));
    QVERIFY(other->isValid());
    LayerOps::markReferenceLayer(other);
    QVERIFY(project->addMapLayer(other, false));
    group->addLayer(other);
    QVERIFY(group->children().last() != originalNode);
    const QString id = dem->id();
    const QPointer<QgsMapLayer> guard(dem);
    const int layerCount = project->mapLayers().size();
    const int nodeCount = project->layerTreeRoot()->findLayers().size();
    const int siblingCount = group->children().size();
    auto* tree = window.findChild<QgsLayerTreeView*>(QStringLiteral("layerTree"));
    QVERIFY(tree);
    window.show();
    tree->expandAll();
    tree->setCurrentLayer(dem);
    QApplication::processEvents();
    const QModelIndex index = tree->layerTreeModel()->node2index(originalNode);
    QVERIFY(index.isValid());
    tree->scrollTo(index);
    const LayerMenuState menu = inspectLayerMenu(window, tree, tree->visualRect(index).center(),
                                                QStringLiteral("layer.bottom"));
    QVERIFY(menu.seen);
    bool enabledBottom = false;
    for (const auto& action : menu.actions)
      if (action.id == QLatin1String("layer.bottom")) enabledBottom = action.enabled;
    QVERIFY(enabledBottom);
    // The registry bridge defers layer deletion after its last tree node disappears.
    // Drain that work before asserting preservation, rather than checking only the
    // synchronous clone/move result.
    QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
    QApplication::processEvents();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QVERIFY(guard);
    QCOMPARE(project->mapLayer(id), guard.data());
    QCOMPARE(project->mapLayers().size(), layerCount);
    QCOMPARE(project->layerTreeRoot()->findLayers().size(), nodeCount);
    auto* movedNode = project->layerTreeRoot()->findLayer(id);
    QVERIFY(movedNode);
    QCOMPARE(movedNode->parent(), group);
    QCOMPARE(group->children().size(), siblingCount);
    QCOMPARE(group->children().last(), movedNode);
    QCOMPARE(guard->source(), source);
    QCOMPARE(contents(source), originalFile);
  }
  void mapContextMenu_onlyOffersRecordEditingForSurveyFeatures() {
    const QString path = makeSurvey(QStringLiteral("map_menu_scope"));
    QVERIFY(!path.isEmpty());
    MainWindow window;
    disableRendering(window);
    QVERIFY(window.openSurveyGpkg(path));
    disableRendering(window);
    auto* survey = LayerOps::findByLayerKey(QgsProject::instance(), QStringLiteral("survey_area"));
    auto* external = qobject_cast<QgsVectorLayer*>(makeMenuReference(QStringLiteral("external_shp")));
    QVERIFY(survey && external && external->isValid());
    QVERIFY(LayerOps::layerKeyOf(external).isEmpty());
    QCOMPARE(external->providerType(), QStringLiteral("ogr"));
    auto* canvas = window.findChild<QgsMapCanvas*>(QStringLiteral("mapCanvas"));
    QVERIFY(canvas);
    window.show();
    QApplication::processEvents();
    canvas->setDestinationCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5187")));
    canvas->setExtent(QgsRectangle(189950, 559950, 190150, 560150));
    for (auto* target : {external, survey}) {
      canvas->setLayers({target});
      const QgsPointXY pixel = canvas->getCoordinateTransform()->transform(190050, 560050);
      const QPoint point(qRound(pixel.x()), qRound(pixel.y()));
      KaAttributeMapTool picker(canvas);
      QgsVectorLayer* picked = nullptr;
      QgsFeature feature;
      QVERIFY(picker.pickAtScreen(point, &picked, &feature));
      QCOMPARE(picked, target);
      QVERIFY(feature.isValid());
      const LayerMenuState menu = inspectMapMenu(window, point);
      QVERIFY(menu.seen);
      QCOMPARE(menu.name, QStringLiteral("mapContextMenu"));
      bool recordAction = false;
      int actionCount = 0;
      for (const auto& action : menu.actions) {
        if (action.separator) continue;
        ++actionCount;
        if (action.id == QLatin1String("map.attributes")) {
          recordAction = true;
          QVERIFY(action.enabled);
        }
        if (!action.enabled) {
          QVERIFY(!action.toolTip.trimmed().isEmpty());
          QVERIFY(action.toolTip != action.text);
        }
      }
      QCOMPARE(recordAction, target == survey);
      QVERIFY(actionCount <= 10);
    }
  }
  void newSurvey_selectedCrsSurvivesSaveAndOpen_data() {
    QTest::addColumn<QString>("authId");
    QTest::newRow("5186") << QStringLiteral("EPSG:5186");
    QTest::newRow("5187") << QStringLiteral("EPSG:5187");
  }
  static bool executeGpkgSql(const QString& path, const char* sql) {
    GDALDatasetH dataset = GDALOpenEx(path.toUtf8().constData(),
        GDAL_OF_VECTOR | GDAL_OF_UPDATE, nullptr, nullptr, nullptr);
    if (!dataset) return false;
    CPLErrorReset();
    OGRLayerH result = GDALDatasetExecuteSQL(dataset, sql, nullptr, nullptr);
    const bool ok = CPLGetLastErrorType() < CE_Failure;
    if (result) GDALDatasetReleaseResultSet(dataset, result);
    GDALClose(dataset);
    return ok;
  }
  static bool selectSaveAs(MainWindow& window, const QString& target) {
    QTimer choose;
    bool selected = false;
    connect(&choose, &QTimer::timeout, [&] {
      if (auto* dialog = qobject_cast<QFileDialog*>(QApplication::activeModalWidget())) {
        if (selected) return;
        dialog->setOption(QFileDialog::DontConfirmOverwrite);
        dialog->setDirectory(QFileInfo(target).absolutePath());
        dialog->selectFile(QFileInfo(target).fileName());
        if (auto* name = dialog->findChild<QLineEdit*>(QStringLiteral("fileNameEdit")))
          name->setText(QFileInfo(target).fileName());
        const QStringList files = dialog->selectedFiles();
        selected = files.size() == 1 && QFileInfo(files.first()).absoluteFilePath() == QFileInfo(target).absoluteFilePath();
        qInfo() << "Save As selected file:" << files << "matches target:" << selected;
        if (!selected) { choose.stop(); dialog->reject(); return; }
        QMetaObject::invokeMethod(dialog, "accept", Qt::DirectConnection);
      } else if (auto* message = qobject_cast<QMessageBox*>(QApplication::activeModalWidget())) {
        message->accept();
      }
    });
    choose.start(20);
    QTimer::singleShot(5000, &choose, [&] {
      choose.stop();
      if (auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget())) dialog->reject();
    });
    return QMetaObject::invokeMethod(&window, "saveProjectAs", Qt::DirectConnection) && selected;
  }
  static bool openWithAnswer(MainWindow& window, const QString& path,
                             QMessageBox::StandardButton answer, bool* prompted = nullptr) {
    QTimer choose;
    connect(&choose, &QTimer::timeout, [&] {
      if (auto* question = qobject_cast<QMessageBox*>(QApplication::activeModalWidget())) {
        if (question->standardButtons().testFlag(answer)) {
          if (prompted) *prompted = true;
          question->button(answer)->click();
        } else question->reject();
      }
    });
    choose.start(20);
    return window.openSurveyGpkg(path);
  }
  void newSurvey_selectedCrsSurvivesSaveAndOpen() {
    QFETCH(QString, authId);
    const QString name = QStringLiteral("crs_%1").arg(authId.mid(5));
    const QString path = m_files.filePath(name + QStringLiteral(".gpkg"));
    MainWindow window;
    disableRendering(window);
    window.show();
    QTimer choose;
    bool selected = false;
    bool folderChosen = false;
    connect(&choose, &QTimer::timeout, [&] {
      auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
      if (!dialog) return;
      if (auto* folder = qobject_cast<QFileDialog*>(dialog)) {
        folder->setDirectory(m_files.path());
        folder->selectFile(m_files.path());
        folderChosen = true;
        choose.stop();
        QMetaObject::invokeMethod(folder, "accept", Qt::DirectConnection);
      } else if (!selected && dialog->windowTitle() == QStringLiteral("새 조사")) {
        dialog->findChild<QLineEdit*>()->setText(name);
        for (auto* button : dialog->findChildren<QPushButton*>()) {
          if (button->text().startsWith(authId.mid(5))) {
            button->click();
            selected = button->isChecked();
          }
        }
        dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->click();
      }
    });
    choose.start(20);
    QTimer::singleShot(15000, &choose, [&] {
      choose.stop();
      if (auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget())) dialog->reject();
    });
    QVERIFY(QMetaObject::invokeMethod(&window, "newSurvey", Qt::DirectConnection));
    choose.stop();
    QVERIFY(selected && folderChosen);
    QVERIFY(QFile::exists(path));
    auto* canvas = window.findChild<QgsMapCanvas*>(QStringLiteral("mapCanvas"));
    auto* chip = window.findChild<QToolButton*>(QStringLiteral("crsButton"));
    auto* upload = window.findChild<QLabel*>(QStringLiteral("uploadCrsChip"));
    QVERIFY(canvas && chip && upload);
    QCOMPARE(QgsProject::instance()->crs().authid(), authId);
    QCOMPARE(canvas->mapSettings().destinationCrs().authid(), authId);
    QCOMPARE(chip->text(), QStringLiteral("작업 %1").arg(authId.mid(5)));
    QCOMPARE(upload->text(), QStringLiteral("→ 제출 5179"));
    QgsVectorLayer stored(path + QStringLiteral("|layername=survey_area"), name, QStringLiteral("ogr"));
    QVERIFY(stored.isValid());
    QCOMPARE(stored.crs().authid(), authId);
    QVERIFY(saveNow(window));
    const QString other = authId.endsWith(QLatin1String("5186"))
        ? QStringLiteral("EPSG:5187") : QStringLiteral("EPSG:5186");
    QTimer selectCrs;
    connect(&selectCrs, &QTimer::timeout, [&] {
      if (auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget())) {
        for (auto* action : menu->actions()) {
          if (action->text().contains(other)) {
            menu->setActiveAction(action);
            selectCrs.stop();
            QTest::keyClick(menu, Qt::Key_Return);
            break;
          }
        }
      }
    });
    selectCrs.start(20);
    chip->click();
    selectCrs.stop();
    QCOMPARE(QgsProject::instance()->crs().authid(), other);
    QCOMPARE(canvas->mapSettings().destinationCrs().authid(), other);
    QCOMPARE(chip->text(), QStringLiteral("작업 %1").arg(other.mid(5)));
    QCOMPARE(upload->text(), QStringLiteral("→ 제출 5179"));
    QVERIFY(openWithAnswer(window, path, QMessageBox::Discard));
    QCOMPARE(QgsProject::instance()->crs().authid(), authId);
    QCOMPARE(canvas->mapSettings().destinationCrs().authid(), authId);
    QCOMPARE(chip->text(), QStringLiteral("작업 %1").arg(authId.mid(5)));
    QCOMPARE(upload->text(), QStringLiteral("→ 제출 5179"));
  }
  void embeddedOpenRestoresDemInActualTree_data() {
    QTest::addColumn<QString>("preset");
    QTest::addColumn<bool>("reliefEnabled");
    QTest::newRow("legacy-gray") << QStringLiteral("legacy") << false;
    QTest::newRow("lowland-relief-off") << QStringLiteral("lowland") << false;
    QTest::newRow("viewport-relief-off") << QStringLiteral("viewport") << false;
    QTest::newRow("national-relief-on") << QStringLiteral("national") << true;
  }
  void embeddedOpenRestoresDemInActualTree() {
    QFETCH(QString, preset);
    QFETCH(bool, reliefEnabled);
    const QString name = QStringLiteral("dem-embedded-") + preset;
    const QString rasterPath = m_files.filePath(name + QStringLiteral(".tif"));
    GDALAllRegister();
    const auto driver = GDALGetDriverByName("GTiff"); QVERIFY(driver);
    {
      std::unique_ptr<void, decltype(&GDALClose)> dataset(
          GDALCreate(driver, rasterPath.toUtf8().constData(), 16, 16, 1, GDT_Float32, nullptr), GDALClose);
      QVERIFY(dataset);
      double transform[] = {190000., 10., 0., 560000., 0., -10.};
      QCOMPARE(GDALSetGeoTransform(dataset.get(), transform), CE_None);
      const QgsCoordinateReferenceSystem crs(QStringLiteral("EPSG:5187"));
      QCOMPARE(GDALSetProjection(dataset.get(), crs.toWkt().toUtf8().constData()), CE_None);
      float elevations[256];
      for (int i = 0; i < 256; ++i) elevations[i] = 50.f + float(i % 16) * 50.f;
      QCOMPARE(GDALRasterIO(GDALGetRasterBand(dataset.get(), 1), GF_Write, 0, 0, 16, 16,
                            elevations, 16, 16, GDT_Float32, 0, 0), CE_None);
    }
    QString error;
    const QString path = SurveyProjectFactory::createNewSurvey(m_files.path(), name, &error,
                                                               QStringLiteral("EPSG:5187"));
    QVERIFY2(!path.isEmpty(), qPrintable(error));
    const auto colorAt = [](QgsRasterLayer* layer, double elevation) {
      auto* renderer = dynamic_cast<QgsSingleBandPseudoColorRenderer*>(layer->renderer());
      if (!renderer || !renderer->shader()) return QColor();
      int r = 0, g = 0, b = 0, a = 0;
      if (!renderer->shader()->shade(elevation, &r, &g, &b, &a)) return QColor();
      return QColor(r, g, b, a);
    };
    QColor savedColor;
    double savedMinimum = 0., savedMaximum = 2000.;
    {
      QgsProject saved;
      saved.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5187")));
      auto dem = std::make_unique<QgsRasterLayer>(rasterPath, QStringLiteral("DEM"), QStringLiteral("gdal"));
      QVERIFY(dem->isValid());
      if (preset != QLatin1String("legacy")) {
        QVERIFY(DemPresentation::apply(dem.get(), preset, dem->extent()));
        savedColor = colorAt(dem.get(), 100.); QVERIFY(savedColor.isValid());
        auto* renderer = dynamic_cast<QgsSingleBandPseudoColorRenderer*>(dem->renderer());
        savedMinimum = renderer->classificationMin(); savedMaximum = renderer->classificationMax();
      }
      LayerOps::markReferenceLayer(dem.get());
      auto* layer = dem.get(); QVERIFY(saved.addMapLayer(layer)); dem.release();
      if (preset != QLatin1String("legacy")) {
        auto* shade = LayerOps::ensureDemRelief(&saved, layer); QVERIFY(shade);
        auto* shadeNode = saved.layerTreeRoot()->findLayer(shade->id()); QVERIFY(shadeNode);
        shadeNode->setItemVisibilityChecked(reliefEnabled);
      }
      layer->setCustomProperty(QStringLiteral("ka_hgis/dem_relief_enabled"), reliefEnabled);
      QVERIFY2(SurveyStorage::writeEmbedded(&saved, path, &error), qPrintable(error));
    }
    QVERIFY(SurveyStorage::hasEmbeddedProject(path));
    MainWindow window;
    disableRendering(window);
    auto* tree = window.findChild<QgsLayerTreeView*>(QStringLiteral("layerTree")); QVERIFY(tree);
    auto* originalModel = tree->layerTreeModel();
    QVERIFY(window.openSurveyGpkg(path));
    auto* project = QgsProject::instance();
    const auto layers = project->mapLayersByName(QStringLiteral("DEM")); QCOMPARE(layers.size(), 1);
    auto* dem = qobject_cast<QgsRasterLayer*>(layers.first()); QVERIFY(dem && dem->isValid());
    auto* node = project->layerTreeRoot()->findLayer(dem->id()); QVERIFY(node);
    QCOMPARE(tree->layerTreeModel(), originalModel);
    const auto legendNodes = originalModel->layerLegendNodes(node);
    QCOMPARE(legendNodes.size(), 1);
    QVERIFY2(dynamic_cast<DemColorRampLegend*>(dem->legend()), "Actual MainWindow open must reinstall the DEM legend.");
    QVERIFY2(dynamic_cast<QgsColorRampLegendNode*>(legendNodes.first()), "The actual tree must show the continuous elevation ramp.");
    QVERIFY(!legendNodes.first()->data(Qt::DisplayRole).toString().contains(QStringLiteral("Gray")));
    QCOMPARE(dem->customProperty(QStringLiteral("ka_hgis/dem_preset")).toString(),
             preset == QLatin1String("legacy") ? QStringLiteral("national") : preset);
    auto* renderer = dynamic_cast<QgsSingleBandPseudoColorRenderer*>(dem->renderer()); QVERIFY(renderer);
    QCOMPARE(renderer->classificationMin(), savedMinimum);
    QCOMPARE(renderer->classificationMax(), savedMaximum);
    if (savedColor.isValid()) QCOMPARE(colorAt(dem, 100.), savedColor);
    QVERIFY(dem->findChild<QTimer*>(QStringLiteral("demViewportTimer")));
    QCOMPARE(dem->customProperty(QStringLiteral("ka_hgis/dem_relief_enabled")).toBool(), reliefEnabled);
    const auto shades = project->mapLayersByName(QStringLiteral("지형 음영"));
    QCOMPARE(shades.size(), preset == QLatin1String("legacy") ? 0 : 1);
    if (!shades.isEmpty()) {
      auto* shadeNode = project->layerTreeRoot()->findLayer(shades.first()->id()); QVERIFY(shadeNode);
      QCOMPARE(shadeNode->itemVisibilityChecked(), reliefEnabled);
    }
  }
  void saveAndReopen_keepsTreeGeometryAttributesAndStyle() {
    const QString path = makeSurvey(QStringLiteral("왕복조사"));
    QVERIFY(!path.isEmpty());
    MainWindow window;
    disableRendering(window);
    auto* tree = window.findChild<QgsLayerTreeView*>(QStringLiteral("layerTree"));
    QVERIFY(tree);
    auto* originalModel = tree->layerTreeModel();
    QVERIFY(window.openSurveyGpkg(path));
    auto* layer = LayerOps::findByLayerKey(QgsProject::instance(), QStringLiteral("survey_area"));
    QVERIFY(layer && layer->isValid());
    QVERIFY(QgsProject::instance()->layerTreeRoot()->findLayer(layer->id()));
    QVERIFY(layer->startEditing());
    const auto id = *layer->allFeatureIds().constBegin();
    QVERIFY(layer->changeAttributeValue(id, layer->fields().indexOf(QStringLiteral("survey_name")),
                                        QStringLiteral("수정한 조사")));
    QVERIFY(QMetaObject::invokeMethod(&window, "saveProject", Qt::DirectConnection));
    QVERIFY(window.openSurveyGpkg(path));
    layer = LayerOps::findByLayerKey(QgsProject::instance(), QStringLiteral("survey_area"));
    QVERIFY(layer && layer->isValid());
    QCOMPARE(layer->featureCount(), 1LL);
    QCOMPARE(layer->getFeature(id).attribute(QStringLiteral("survey_name")).toString(),
             QStringLiteral("수정한 조사"));
    QVERIFY(layer->getFeature(id).geometry().equals(
        QgsGeometry::fromRect(QgsRectangle(190000, 560000, 190100, 560100)))) ;
    auto* node = QgsProject::instance()->layerTreeRoot()->findLayer(layer->id());
    QVERIFY2(node, "Saved data must have a legend node after reopening");
    QVERIFY(node->isVisible());
    QCOMPARE(tree->layerTreeModel(), originalModel);
    auto* renderer = dynamic_cast<QgsSingleSymbolRenderer*>(layer->renderer());
    QVERIFY(renderer);
    QCOMPARE(renderer->symbol()->color(), QColor(217, 43, 43));
    QCOMPARE(QgsProject::instance()->crs().authid(), QStringLiteral("EPSG:5187"));
  }
  void open_repairsRegistryOnlyLayers() {
    const QString path = makeSurvey(QStringLiteral("목록복구"), true);
    QVERIFY(!path.isEmpty());
    MainWindow window;
    disableRendering(window);
    QVERIFY(window.openSurveyGpkg(path));
    auto* layer = LayerOps::findByLayerKey(QgsProject::instance(), QStringLiteral("survey_area"));
    QVERIFY(layer && layer->isValid());
    QCOMPARE(layer->featureCount(), 1LL);
    QVERIFY2(QgsProject::instance()->layerTreeRoot()->findLayer(layer->id()),
             "Registry-only survey must be restored to the legend");
    QVERIFY(window.findChild<QgsMapCanvas*>()->layers().contains(layer));
  }
  void startupHome_ignoresRememberedSurveyUntilUserOpensIt() {
    const QString path = makeSurvey(QStringLiteral("수동열기"));
    QVERIFY(!path.isEmpty());
    QSettings st = RecentSurveys::userSettings();
    QVERIFY2(QFileInfo(st.fileName()).absoluteFilePath().startsWith(
                 QFileInfo(s_testSettingsPath).absoluteFilePath()),
             qPrintable(QStringLiteral("RecentSurveys settings escaped test dir: %1")
                            .arg(st.fileName())));
    RecentSurveys::remember(st, path, QStringLiteral("수동열기"));

    MainWindow window;
    window.show();
    QTest::qWait(250);

    auto* tabs = window.findChild<QTabWidget*>(QStringLiteral("viewTabs"));
    QVERIFY(tabs);
    QCOMPARE(tabs->tabText(tabs->currentIndex()), QStringLiteral("홈"));
    auto* canvas = window.findChild<QgsMapCanvas*>(QStringLiteral("mapCanvas"));
    QVERIFY(canvas);
    QCOMPARE(QgsProject::instance()->mapLayers().size(), 0);
    QCOMPARE(canvas->layers().size(), 0);
    const QString screenshot = qEnvironmentVariable("KA_HGIS_STARTUP_SCREENSHOT");
    if (!screenshot.isEmpty())
      QVERIFY(window.grab().save(screenshot));

    disableRendering(window);
    QVERIFY(window.openSurveyGpkg(path));
    QCOMPARE(tabs->tabText(tabs->currentIndex()), QStringLiteral("지도"));
    QVERIFY(LayerOps::findByLayerKey(QgsProject::instance(), QStringLiteral("survey_area")));
  }
  // 축척 칸은 하나뿐이어야 한다. 예전에는 자유 입력 QLineEdit 과 프리셋 QComboBox 가
  // 따로 있어서 같은 축척인데도 어느 쪽으로 넣었느냐에 따라 화면이 달랐다.
  void scaleControlFollowsActiveDrawingWithoutChangingMapCanvas() {
    MainWindow window;
    disableRendering(window);
    window.setAttribute(Qt::WA_DontShowOnScreen);
    window.show();
    auto* tabs = window.findChild<QTabWidget*>(QStringLiteral("viewTabs"));
    auto* canvas = window.findChild<QgsMapCanvas*>(QStringLiteral("mapCanvas"));
    auto* combo = window.findChild<QComboBox*>(QStringLiteral("scaleCombo"));
    QVERIFY(tabs && canvas && combo && combo->lineEdit());
    QWidget* mapPage = nullptr;
    for (int i = 0; i < tabs->count(); ++i)
      if (tabs->tabText(i) == QStringLiteral("지도")) mapPage = tabs->widget(i);
    QVERIFY(mapPage);
    tabs->setCurrentWidget(mapPage);
    QCoreApplication::processEvents();
    canvas->setDestinationCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5187")));
    canvas->setExtent(QgsRectangle(190000., 560000., 191000., 561000.));
    canvas->zoomScale(25000.);

    QTimer acceptPaper;
    acceptPaper.setInterval(10);
    connect(&acceptPaper, &QTimer::timeout, &window, [&] {
      auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
      if (dialog && dialog->windowTitle() == QStringLiteral("용지 설정")) {
        acceptPaper.stop();
        dialog->accept();
      }
    });
    acceptPaper.start();
    QVERIFY(QMetaObject::invokeMethod(&window, "openLayoutDesigner"));
    acceptPaper.stop();
    QCoreApplication::processEvents();
    auto* studio = window.findChild<KaDrawingStudio*>();
    QVERIFY(studio && tabs->currentWidget() == studio);
    auto* view = studio->findChild<QgsLayoutView*>();
    QVERIFY(view && view->currentLayout());
    auto* map = dynamic_cast<QgsLayoutItemMap*>(view->currentLayout()->itemById(QStringLiteral("ka_map")));
    auto* paperScale = dynamic_cast<QgsLayoutItemLabel*>(view->currentLayout()->itemById(QStringLiteral("ka_scale")));
    auto* drawingScale = studio->findChild<QSpinBox*>(QStringLiteral("drawingScale"));
    auto* apply = studio->findChild<QPushButton*>(QStringLiteral("scaleApply"));
    QVERIFY(map && paperScale && drawingScale && apply);
    const QgsRectangle canvasExtent = canvas->extent();
    const double canvasScale = canvas->scale();
    const auto bottomScale = [&] { return MainWindow::scaleDenominatorFromUi(combo->lineEdit()->text()); };

    // The drawing's existing Apply control must update the shared bottom field.
    drawingScale->setValue(10000);
    apply->click();
    QVERIFY(qAbs(map->scale() - 10000.) < .5);
    QCOMPARE(bottomScale(), 10000.);
    QCOMPARE(canvas->extent(), canvasExtent);
    QCOMPARE(canvas->scale(), canvasScale);

    // Enter in that bottom field now targets the active drawing, not the hidden map.
    combo->lineEdit()->setText(QStringLiteral("1:5000"));
    QTest::keyClick(combo->lineEdit(), Qt::Key_Return);
    QVERIFY(qAbs(map->scale() - 5000.) < .5);
    QCOMPARE(drawingScale->value(), 5000);
    QCOMPARE(paperScale->text(), QStringLiteral("축척 1 : 5000"));
    QCOMPARE(bottomScale(), 5000.);
    QCOMPARE(canvas->extent(), canvasExtent);
    QCOMPARE(canvas->scale(), canvasScale);

    // A canvas notification while its tab is hidden must not overwrite drawing scale.
    QVERIFY(QMetaObject::invokeMethod(&window, "onCanvasScaleChanged", Q_ARG(double, canvasScale)));
    QCOMPARE(bottomScale(), 5000.);
    // Frame resizing reports the actual map scale to both labels.
    map->attemptResize(QgsLayoutSize(map->rect().width() * .8, map->rect().height()));
    QCoreApplication::processEvents();
    const double resizedScale = std::round(map->scale());
    QCOMPARE(bottomScale(), resizedScale);
    QCOMPARE(drawingScale->value(), static_cast<int>(resizedScale));
    QCOMPARE(paperScale->text(), QStringLiteral("축척 1 : %1").arg(resizedScale, 0, 'f', 0));

    tabs->setCurrentWidget(mapPage);
    QCoreApplication::processEvents();
    QCOMPARE(bottomScale(), std::round(canvas->scale()));
    tabs->setCurrentWidget(studio);
    QCoreApplication::processEvents();
    QCOMPARE(bottomScale(), resizedScale);
    QVERIFY(qAbs(studio->drawingScale() - resizedScale) < .5);
    QgsProject::instance()->setDirty(false);
  }

  void scaleControl_isSingleWidgetAcceptingBothForms() {
    MainWindow window;
    disableRendering(window);
    auto* combo = window.findChild<QComboBox*>(QStringLiteral("scaleCombo"));
    QVERIFY2(combo, "축척 콤보가 없다");
    QVERIFY2(combo->isEditable(), "축척 콤보는 직접 입력이 돼야 한다");
    QVERIFY2(combo->lineEdit(), "콤보에 입력줄이 있어야 한다");

    // 입력줄은 콤보의 것 하나뿐 — 따로 떠 있는 축척 칸이 있으면 안 된다.
    int standalone = 0;
    for (auto* e : window.findChildren<QLineEdit*>(QStringLiteral("scaleEdit")))
      if (e != combo->lineEdit()) ++standalone;
    QCOMPARE(standalone, 0);

    // 발굴 도면 축척이 프리셋에 있어야 한다.
    QVERIFY2(combo->findData(200) >= 0, "1:200 프리셋이 없다");
    QVERIFY2(combo->findData(500) >= 0, "1:500 프리셋이 없다");

    // "2000" 과 "1:2000" 이 같은 값으로 읽혀야 한 칸으로 합친 의미가 있다.
    QCOMPARE(MainWindow::scaleDenominatorFromUi(QStringLiteral("2000")), 2000.0);
    QCOMPARE(MainWindow::scaleDenominatorFromUi(QStringLiteral("1:2000")), 2000.0);
    QCOMPARE(MainWindow::scaleDenominatorFromUi(QStringLiteral(" 1 : 2,000 ")), 2000.0);
    QCOMPARE(MainWindow::scaleDenominatorFromUi(QStringLiteral("1:200")), 200.0);
    QCOMPARE(MainWindow::scaleDenominatorFromUi(QStringLiteral("메롱")), 0.0);
    QCOMPARE(MainWindow::scaleDenominatorFromUi(QString()), 0.0);
  }

  void open_blocksNestedOpenAndAutosave() {
    const QString first = makeSurvey(QStringLiteral("이전조사"));
    const QString second = makeSurvey(QStringLiteral("다음조사"));
    QVERIFY(!first.isEmpty() && !second.isEmpty());
    MainWindow window;
    disableRendering(window);
    QVERIFY(window.openSurveyGpkg(first));
    const QByteArray before = contents(first);
    QVERIFY(!before.isEmpty());
    bool invoked = false;
    bool nestedOpened = true;
    // 주기적으로 파일에 쓰는 타이머가 남아 있으면 안 된다.
    QVERIFY2(hasNoAutosaveTimer(window), "20초 자동 저장 타이머가 아직 살아 있다");
    bool saveDuringRead = true;
    const auto connection = connect(QgsProject::instance(), &QgsProject::readProject,
        &window, [&](const QDomDocument&) {
          if (invoked) return;
          invoked = true;
          saveDuringRead = saveNow(window);
          nestedOpened = window.openSurveyGpkg(first);
        });
    const bool opened = window.openSurveyGpkg(second);
    disconnect(connection);
    QVERIFY(invoked);
    QVERIFY2(!saveDuringRead, "프로젝트를 읽는 중에는 저장이 끼어들면 안 된다");
    QVERIFY(opened);
    QVERIFY2(!nestedOpened, "A nested open must not replace a project being read");
    QCOMPARE(contents(first), before);
    QgsProject savedFirst;
    QVERIFY(savedFirst.read(SurveyStorage::projectUri(first),
                            Qgis::ProjectReadFlag::DontResolveLayers | Qgis::ProjectReadFlag::DontLoadLayouts));
    QCOMPARE(savedFirst.title(), QStringLiteral("이전조사"));
    auto* layer = LayerOps::findByLayerKey(QgsProject::instance(), QStringLiteral("survey_area"));
    QVERIFY(layer);
    QCOMPARE(layer->getFeature(*layer->allFeatureIds().constBegin())
             .attribute(QStringLiteral("survey_name")).toString(), QStringLiteral("다음조사"));
  }
  void partialCommit_preservesRemainingBufferAndCreatesIndependentRecovery() {
    const QString path = makeSurvey(QStringLiteral("뒤레이어커밋실패"));
    QVERIFY(!path.isEmpty());
    MainWindow window;
    disableRendering(window);
    QVERIFY(window.openSurveyGpkg(path));
    auto* project = QgsProject::instance();
    QString error;
    auto* line = LayerOps::ensureDomainLayer(project, path, QStringLiteral("feature_line"),
                                             QStringLiteral("실패검사 선"), &error);
    QVERIFY2(line, qPrintable(error));
    QVERIFY(line->startEditing());
    QgsFeature feature(line->fields());
    feature.setAttribute(QStringLiteral("note"), QStringLiteral("기존 선"));
    feature.setGeometry(QgsGeometry::fromWkt(QStringLiteral("LineString (190000 560000, 190010 560010)")));
    QVERIFY(line->addFeature(feature));
    QVERIFY(saveNow(window));
    QList<QgsVectorLayer*> order;
    for (auto* item : project->mapLayers())
      if (auto* vector = qobject_cast<QgsVectorLayer*>(item)) order.append(vector);
    QCOMPARE(order.size(), 2);
    QMap<QString, QgsFeatureId> ids;
    QStringList keys;
    for (auto* layer : order) keys.append(LayerOps::layerKeyOf(layer));
    for (int i = 0; i < order.size(); ++i) {
      auto* layer = order.at(i);
      if (!layer->isEditable()) QVERIFY(layer->startEditing());
      const QgsFeatureId id = *layer->allFeatureIds().constBegin();
      ids.insert(layer->id(), id);
      QVERIFY(layer->changeAttributeValue(id, layer->fields().indexOf(QStringLiteral("note")),
                                          QStringLiteral("미저장 변경 %1").arg(i)));
    }
    auto* first = order.first();
    auto* failed = order.last();
    failed->setAllowCommit(false);
    project->setTitle(QStringLiteral("전체 저장은 아직 실패"));
    const QString companion = QFileInfo(path).dir().filePath(
        QFileInfo(path).completeBaseName() + QStringLiteral(".qgz"));
    const QByteArray companionBefore = contents(companion);
    const auto gpkgFiles = [&]() {
      QSet<QString> files;
      QDirIterator iterator(m_files.path(), {QStringLiteral("*.gpkg")}, QDir::Files,
                            QDirIterator::Subdirectories);
      while (iterator.hasNext()) files.insert(iterator.next());
      return files;
    };
    const QSet<QString> beforeFiles = gpkgFiles();
    QVERIFY(!saveNow(window));
    QVERIFY(project->isDirty());
    QVERIFY(!first->isModified());
    QVERIFY(failed->isEditable() && failed->isModified());
    QCOMPARE(contents(companion), companionBefore);
    for (int i = 0; i < order.size(); ++i) {
      auto* layer = order.at(i);
      QCOMPARE(layer->getFeature(ids.value(layer->id())).attribute(QStringLiteral("note")).toString(),
               QStringLiteral("미저장 변경 %1").arg(i));
      QgsVectorLayer disk(layer->source(), QStringLiteral("디스크 확인"), QStringLiteral("ogr"));
      QVERIFY(disk.isValid());
      const QString saved = disk.getFeature(ids.value(layer->id())).attribute(QStringLiteral("note")).toString();
      if (i == 0) QCOMPARE(saved, QStringLiteral("미저장 변경 0"));
      else QVERIFY(saved != QStringLiteral("미저장 변경 1"));
    }
    const QSet<QString> created = gpkgFiles() - beforeFiles;
    QCOMPARE(created.size(), 1);
    const QString recovery = *created.constBegin();
    QVERIFY(!contents(recovery).isEmpty());
    QgsProject recovered;
    QVERIFY(recovered.read(SurveyStorage::projectUri(recovery)));
    for (int i = 0; i < order.size(); ++i) {
      auto* copied = LayerOps::findByLayerKey(&recovered, LayerOps::layerKeyOf(order.at(i)));
      QVERIFY(copied && copied->isValid());
      QCOMPARE(copied->featureCount(), 1LL);
      QgsFeature value;
      QVERIFY(copied->getFeatures().nextFeature(value));
      QCOMPARE(value.attribute(QStringLiteral("note")).toString(), QStringLiteral("미저장 변경 %1").arg(i));
    }
    recovered.clear();
    // 기준 바이트는 복구본을 읽은 뒤에 잡는다. 읽기만 해도 SQLite 가 WAL 을 정리해
    // 파일 헤더(저널 모드·변경 카운터)가 바뀐다. 읽기 전 값과 비교하면 항상 다르다.
    const QByteArray recoveryBefore = contents(recovery);
    QVERIFY(!recoveryBefore.isEmpty());
    auto* bar = window.findChild<QgsMessageBar*>();
    QVERIFY(bar);
    bool recoveryWarning = false;
    for (auto* item : bar->items())
      if (item->level() == Qgis::MessageLevel::Warning &&
          item->text().contains(QStringLiteral("복구"))) recoveryWarning = true;
    QVERIFY(recoveryWarning);
    QVERIFY(!saveNow(window)); // Persistent failure creates another snapshot, never overwrites.
    QCOMPARE((gpkgFiles() - beforeFiles).size(), 2);
    QCOMPARE(contents(recovery), recoveryBefore);
    QVERIFY(failed->isModified());
    failed->setAllowCommit(true);
    QVERIFY(saveNow(window));
    QVERIFY(window.openSurveyGpkg(path));
    for (int i = 0; i < order.size(); ++i) {
      auto* reopened = LayerOps::findByLayerKey(project, keys.at(i));
      QVERIFY(reopened && reopened->isValid());
      QCOMPARE(reopened->featureCount(), 1LL);
      QgsFeature value;
      QVERIFY(reopened->getFeatures().nextFeature(value));
      QCOMPARE(value.attribute(QStringLiteral("note")).toString(), QStringLiteral("미저장 변경 %1").arg(i));
    }
  }


  void failedCommit_doesNotOverwriteSavedWorkspace() {
    const QString path = makeSurvey(QStringLiteral("커밋실패"));
    QVERIFY(!path.isEmpty());
    MainWindow window;
    disableRendering(window);
    QVERIFY(window.openSurveyGpkg(path));
    auto* project = QgsProject::instance();
    auto* layer = LayerOps::findByLayerKey(project, QStringLiteral("survey_area"));
    QVERIFY(layer);
    QVERIFY(layer->startEditing());
    QVERIFY(layer->changeAttributeValue(*layer->allFeatureIds().constBegin(),
        layer->fields().indexOf(QStringLiteral("survey_name")), QStringLiteral("저장실패한 편집")));
    layer->setAllowCommit(false);
    project->setTitle(QStringLiteral("커밋에 실패하면 이 작업공간도 쓰지 않는다"));
    const QByteArray before = contents(path);
    QVERIFY(!before.isEmpty());
    QTimer dismiss;
    connect(&dismiss, &QTimer::timeout, [] {
      for (auto* widget : QApplication::topLevelWidgets())
        if (auto* message = qobject_cast<QMessageBox*>(widget)) message->accept();
    });
    dismiss.start(20);
    QVERIFY(QMetaObject::invokeMethod(&window, "saveProject", Qt::DirectConnection));
    dismiss.stop();
    const QByteArray after = contents(path);
    const bool stillModified = layer->isModified();
    layer->setAllowCommit(true);
    layer->rollBack();
    QVERIFY2(stillModified, "A failed save must retain the edit buffer for retry");
    QgsProject saved;
    QVERIFY(saved.read(SurveyStorage::projectUri(path),
                       Qgis::ProjectReadFlag::DontResolveLayers | Qgis::ProjectReadFlag::DontLoadLayouts));
    QCOMPARE(saved.title(), QStringLiteral("커밋실패"));
    QCOMPARE(after, before);
  }
  void invalidSurvey_doesNotReplaceCurrentWork_data() {
    QTest::addColumn<QByteArray>("invalidContents");
    QTest::newRow("corrupt-file") << QByteArray("not a GeoPackage");
    QTest::newRow("other-vector-format") << QByteArray(
        R"({"type":"FeatureCollection","features":[{"type":"Feature","properties":{},"geometry":{"type":"Point","coordinates":[127,37]}}]})");
  }
  void emptyGpkgWithoutWorkspace_opensWithoutDomainLayers() {
    QString error;
    const QString path = SurveyProjectFactory::createNewSurvey(
        m_files.path(), QStringLiteral("빈조사"), &error, QStringLiteral("EPSG:5187"));
    QVERIFY2(!path.isEmpty(), qPrintable(error));
    // Exercise the on-disk schema alone, without the factory's companion layer tree.
    const QFileInfo file(path);
    QVERIFY(QFile::remove(file.dir().filePath(file.completeBaseName() + QStringLiteral(".qgz"))));
    MainWindow window;
    disableRendering(window);
    QVERIFY(window.openSurveyGpkg(path));
    QCOMPARE(window.domainLayerCount(), 0);
  }
  void invalidSurvey_doesNotReplaceCurrentWork() {
    QFETCH(QByteArray, invalidContents);
    const QString path = makeSurvey(QStringLiteral("유지할조사_") + QString::fromLatin1(QTest::currentDataTag()));
    QVERIFY(!path.isEmpty());
    MainWindow window;
    disableRendering(window);
    QVERIFY(window.openSurveyGpkg(path));
    const QStringList layerIds = QgsProject::instance()->mapLayers().keys();
    QgsProject::instance()->setTitle(QStringLiteral("열기 실패 후 유지한 작업"));
    const QString invalid = m_files.filePath(QStringLiteral("잘못된.gpkg"));
    QFile file(invalid);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QCOMPARE(file.write(invalidContents), invalidContents.size());
    file.close();
    QTimer dismiss;
    connect(&dismiss, &QTimer::timeout, [] {
      for (auto* widget : QApplication::topLevelWidgets())
        if (auto* message = qobject_cast<QMessageBox*>(widget)) message->accept();
    });
    dismiss.start(20);
    const bool opened = window.openSurveyGpkg(invalid);
    dismiss.stop();
    QVERIFY2(!opened, "Invalid survey must not be reported as opened");
    QCOMPARE(QgsProject::instance()->mapLayers().keys(), layerIds);
    QCOMPARE(QgsProject::instance()->title(), QStringLiteral("열기 실패 후 유지한 작업"));
    QVERIFY(saveNow(window));
    QgsProject saved;
    QVERIFY(saved.read(SurveyStorage::projectUri(path),
                        Qgis::ProjectReadFlag::DontResolveLayers | Qgis::ProjectReadFlag::DontLoadLayouts));
    QCOMPARE(saved.title(), QStringLiteral("열기 실패 후 유지한 작업"));
    QCOMPARE(contents(invalid), invalidContents);
    QgsProject::instance()->setTitle(QStringLiteral("열기 실패 후 수동 저장"));
    QVERIFY(QMetaObject::invokeMethod(&window, "saveProject", Qt::DirectConnection));
    QVERIFY(saved.read(SurveyStorage::projectUri(path),
                        Qgis::ProjectReadFlag::DontResolveLayers | Qgis::ProjectReadFlag::DontLoadLayouts));
    QCOMPARE(saved.title(), QStringLiteral("열기 실패 후 수동 저장"));
    QCOMPARE(contents(invalid), invalidContents);
  }
  void openWhileDrawing_resetsCaptureBeforeReplacingLayers() {
    const QString first = makeSurvey(QStringLiteral("그리던조사"));
    const QString second = makeSurvey(QStringLiteral("전환한조사"));
    QVERIFY(!first.isEmpty() && !second.isEmpty());
    MainWindow window;
    disableRendering(window);
    QVERIFY(window.openSurveyGpkg(first));
    auto* project = QgsProject::instance();
    QVERIFY(QMetaObject::invokeMethod(&window, "startEditFeaturePoly", Qt::DirectConnection));
    QPointer<QgsVectorLayer> oldLayer = LayerOps::findByLayerKey(project, QStringLiteral("feature_poly"));
    QVERIFY(oldLayer);
    auto* capture = window.findChild<KaCaptureMapTool*>();
    auto* canvas = window.findChild<QgsMapCanvas*>();
    QVERIFY(capture && canvas);
    QCOMPARE(canvas->mapTool(), capture);
    const QgsGeometry geometry = QgsGeometry::fromRect(QgsRectangle(190300, 560300, 190350, 560350));
    capture->geometryCaptured(geometry);
    QCOMPARE(oldLayer->featureCount(), 1LL);
    canvas->setRenderFlag(false);
    QVERIFY(openWithAnswer(window, second, QMessageBox::Discard));
    QVERIFY(oldLayer.isNull());
    QVERIFY2(canvas->mapTool() != capture, "Opening another survey must stop the previous drawing session");
    QCOMPARE(capture->pointCount(), 0);
    auto* layer = LayerOps::findByLayerKey(project, QStringLiteral("survey_area"));
    QVERIFY(layer);
    capture->geometryCaptured(geometry); // A late capture cannot use the deleted edit layer.
    QCOMPARE(layer->featureCount(), 1LL);
    window.findChild<QgsLayerTreeView*>(QStringLiteral("layerTree"))->setCurrentLayer(layer);
    QVERIFY(QMetaObject::invokeMethod(&window, "startEditFeaturePoly", Qt::DirectConnection));
    capture->geometryCaptured(geometry);
    auto* newDrawingLayer = LayerOps::findByLayerKey(project, QStringLiteral("feature_poly"));
    QVERIFY(newDrawingLayer);
    QCOMPARE(newDrawingLayer->featureCount(), 1LL);
    canvas->setRenderFlag(false);
  }
  void open_unsavedWorkRespectsAnswer_data() {
    QTest::addColumn<int>("answer");
    QTest::addColumn<bool>("vetoCommit");
    QTest::newRow("cancel") << int(QMessageBox::Cancel) << false;
    QTest::newRow("save-fails") << int(QMessageBox::Save) << true;
    QTest::newRow("discard") << int(QMessageBox::Discard) << false;
    QTest::newRow("save") << int(QMessageBox::Save) << false;
  }
  void open_unsavedWorkRespectsAnswer() {
    QFETCH(int, answer);
    QFETCH(bool, vetoCommit);
    const QString suffix = QString::number(answer) + QString::number(vetoCommit);
    const QString source = makeSurvey(QStringLiteral("열기전현재_") + suffix);
    const QString next = makeSurvey(QStringLiteral("열기전다음_") + suffix);
    QVERIFY(!source.isEmpty() && !next.isEmpty());
    MainWindow window;
    disableRendering(window);
    QVERIFY(window.openSurveyGpkg(source));
    auto* project = QgsProject::instance();
    QPointer<QgsVectorLayer> oldLayer = LayerOps::findByLayerKey(project, QStringLiteral("survey_area"));
    QVERIFY(oldLayer && oldLayer->startEditing());
    const QgsFeatureId fid = *oldLayer->allFeatureIds().constBegin();
    const QgsGeometry original = oldLayer->getFeature(fid).geometry();
    QgsGeometry pending = QgsGeometry::fromRect(QgsRectangle(190010, 560011, 190121, 560122));
    QVERIFY(oldLayer->changeGeometry(fid, pending));
    const QStringList layerIds = project->mapLayers().keys();
    const QByteArray before = QCryptographicHash::hash(contents(source), QCryptographicHash::Sha256);
    if (vetoCommit) oldLayer->setAllowCommit(false);
    bool prompted = false;
    const bool opened = openWithAnswer(window, next, QMessageBox::StandardButton(answer), &prompted);
    if (oldLayer) oldLayer->setAllowCommit(true);
    QVERIFY(prompted);
    const bool shouldOpen = answer != int(QMessageBox::Cancel) && !vetoCommit;
    QCOMPARE(opened, shouldOpen);
    if (!shouldOpen) {
      QVERIFY(oldLayer && oldLayer->isEditable() && oldLayer->isModified());
      QCOMPARE(project->mapLayers().keys(), layerIds);
      QVERIFY(oldLayer->getFeature(fid).geometry().equals(pending));
      QCOMPARE(QCryptographicHash::hash(contents(source), QCryptographicHash::Sha256), before);
    } else {
      QVERIFY(oldLayer.isNull());
      auto* current = LayerOps::findByLayerKey(project, QStringLiteral("survey_area"));
      QVERIFY(current && current->isValid());
      QCOMPARE(QFileInfo(current->source().section(QLatin1Char('|'), 0, 0)).absoluteFilePath(),
               QFileInfo(next).absoluteFilePath());
      QgsVectorLayer stored(source + QStringLiteral("|layername=survey_area"), QStringLiteral("저장본"), QStringLiteral("ogr"));
      QVERIFY(stored.isValid());
      QVERIFY(stored.getFeature(fid).geometry().equals(answer == int(QMessageBox::Save) ? pending : original));
    }
  }
  void saveAs_preservesActualTablesAndReopensCopy() {
    const QString path = makeSurvey(QStringLiteral("여러구역"));
    QVERIFY(!path.isEmpty());
    MainWindow window;
    disableRendering(window);
    QVERIFY(window.openSurveyGpkg(path));
    auto* project = QgsProject::instance();
    QString error;
    auto* second = LayerOps::createSurveyAreaLayer(project, path, QStringLiteral("두번째 구역"),
                                                  Qt::black, QColor(25, 80, 170), 0.3, &error);
    QVERIFY2(second, qPrintable(error));
    QVERIFY(second->startEditing());
    QgsFeature feature(second->fields());
    QVERIFY(feature.setAttribute(QStringLiteral("name"), QStringLiteral("독립 구역")));
    feature.setGeometry(QgsGeometry::fromRect(QgsRectangle(190200, 560200, 190250, 560250)));
    QVERIFY(second->addFeature(feature));
    QVERIFY(second->commitChanges());
    const QString secondId = second->id();
    const QString tableOptions = second->source().section(QLatin1Char('|'), 1);
    const QString target = m_files.filePath(QStringLiteral("새 이름.gpkg"));
    QVERIFY(selectSaveAs(window, target));
    QVERIFY(QFile::exists(target));
    QCOMPARE(second->source().section(QLatin1Char('|'), 1), tableOptions);
    QCOMPARE(QFileInfo(second->source().section(QLatin1Char('|'), 0, 0)).absoluteFilePath(),
             QFileInfo(target).absoluteFilePath());
    QVERIFY(window.openSurveyGpkg(target));
    second = qobject_cast<QgsVectorLayer*>(project->mapLayer(secondId));
    QVERIFY(second && second->isValid());
    QCOMPARE(second->featureCount(), 1LL);
    QCOMPARE(second->getFeature(*second->allFeatureIds().constBegin())
                 .attribute(QStringLiteral("name")).toString(), QStringLiteral("독립 구역"));
  }
  void newSurvey_existingNamePreservesUnsavedCurrentSurvey() {
    const QString name = QStringLiteral("새조사실패보존");
    const QString path = makeSurvey(name);
    QVERIFY(!path.isEmpty());
    MainWindow window;
    disableRendering(window);
    QVERIFY(window.openSurveyGpkg(path));
    auto* project = QgsProject::instance();
    auto* layer = LayerOps::findByLayerKey(project, QStringLiteral("survey_area"));
    QVERIFY(layer && layer->startEditing());
    const QgsFeatureId fid = *layer->allFeatureIds().constBegin();
    QgsGeometry changed = QgsGeometry::fromRect(QgsRectangle(190010, 560020, 190110, 560120));
    QVERIFY(layer->changeGeometry(fid, changed));
    const QStringList layerIds = project->mapLayers().keys();
    const QString projectTitle = project->title();
    const QString windowTitle = window.windowTitle();
    const QByteArray before = QCryptographicHash::hash(contents(path), QCryptographicHash::Sha256);
    const QString qgz = QFileInfo(path).dir().filePath(name + QStringLiteral(".qgz"));
    const QByteArray companionBefore = contents(qgz);
    QTimer choose;
    bool discarded = false;
    bool folderChosen = false;
    int nameSubmissions = 0;
    int folderSubmissions = 0;
    QStringList selectedFolders;
    QString submittedName;
    connect(&choose, &QTimer::timeout, [&] {
      auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
      if (!dialog) return;
      if (auto* question = qobject_cast<QMessageBox*>(dialog)) {
        if (question->standardButtons().testFlag(QMessageBox::Discard)) {
          discarded = true;
          question->button(QMessageBox::Discard)->click();
        } else question->accept();
      } else if (auto* folder = qobject_cast<QFileDialog*>(dialog)) {
        ++folderSubmissions;
        const QFileInfo desired(m_files.path());
        folder->setDirectory(desired.absolutePath());
        folder->selectFile(desired.fileName());
        if (auto* pathEdit = folder->findChild<QLineEdit*>(QStringLiteral("fileNameEdit")))
          pathEdit->setText(desired.fileName());
        selectedFolders = folder->selectedFiles();
        folderChosen = selectedFolders.size() == 1 &&
            QFileInfo(selectedFolders.first()).canonicalFilePath() == desired.canonicalFilePath();
        qInfo() << "New survey selected folders:" << selectedFolders << "matches fixture:" << folderChosen;
        choose.stop();
        if (!folderChosen) { folder->reject(); return; }
        QMetaObject::invokeMethod(folder, "accept", Qt::DirectConnection);
      } else if (dialog->windowTitle() == QStringLiteral("새 조사")) {
        ++nameSubmissions;
        auto* nameEdit = dialog->findChild<QLineEdit*>();
        nameEdit->setText(name);
        submittedName = nameEdit->text();
        dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->click();
      }
    });
    choose.start(20);
    QTimer::singleShot(5000, &choose, [&] {
      choose.stop();
      if (auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget())) dialog->reject();
    });
    QVERIFY(QMetaObject::invokeMethod(&window, "newSurvey", Qt::DirectConnection));
    choose.stop();
    qInfo() << "New survey dialog state:" << discarded << nameSubmissions << folderSubmissions << submittedName;
    QVERIFY(discarded && folderChosen);
    QCOMPARE(nameSubmissions, 1);
    QCOMPARE(folderSubmissions, 1);
    QCOMPARE(submittedName, name);
    QCOMPARE(QFileInfo(selectedFolders.first()).canonicalFilePath(), QFileInfo(m_files.path()).canonicalFilePath());
    QCOMPARE(project->mapLayers().keys(), layerIds);
    QCOMPARE(project->title(), projectTitle);
    QCOMPARE(window.windowTitle(), windowTitle);
    QVERIFY(layer->isEditable() && layer->isModified());
    QVERIFY(layer->getFeature(fid).geometry().equals(changed));
    QCOMPARE(QCryptographicHash::hash(contents(path), QCryptographicHash::Sha256), before);
    QCOMPARE(contents(qgz), companionBefore);
  }
  void closeSave_preservesMemoryReferenceVectorOnReopen() {
    const QString path = makeSurvey(QStringLiteral("닫기저장참조벡터"));
    QVERIFY(!path.isEmpty());
    MainWindow window;
    disableRendering(window);
    QVERIFY(window.openSurveyGpkg(path));
    auto* project = QgsProject::instance();
    auto* reference = new QgsVectorLayer(QStringLiteral("Point?crs=EPSG:5186&field=note:string"),
                                        QStringLiteral("현장참고점"), QStringLiteral("memory"));
    QVERIFY(reference->isValid());
    LayerOps::markReferenceLayer(reference);
    project->addMapLayer(reference);
    const QString referenceId = reference->id();
    QVERIFY(reference->startEditing());
    QgsFeature feature(reference->fields());
    feature.setGeometry(QgsGeometry::fromPointXY(QgsPointXY(210000, 550000)));
    feature.setAttribute(QStringLiteral("note"), QStringLiteral("다음날 다시 볼 지점"));
    QVERIFY(reference->addFeature(feature));
    QTimer choose;
    bool prompted = false;
    connect(&choose, &QTimer::timeout, [&] {
      if (auto* question = qobject_cast<QMessageBox*>(QApplication::activeModalWidget())) {
        if (question->standardButtons().testFlag(QMessageBox::Save)) {
          prompted = true;
          question->button(QMessageBox::Save)->click();
        } else question->reject();
      }
    });
    choose.start(20);
    const bool closed = window.close();
    choose.stop();
    QVERIFY(prompted && closed);
    QVERIFY(window.openSurveyGpkg(path));
    reference = qobject_cast<QgsVectorLayer*>(project->mapLayer(referenceId));
    QVERIFY(reference && reference->isValid());
    QVERIFY(LayerOps::isReferenceLayer(reference));
    QCOMPARE(reference->crs().authid(), QStringLiteral("EPSG:5186"));
    QCOMPARE(reference->featureCount(), 1LL);
    const QgsFeature restored = reference->getFeature(*reference->allFeatureIds().constBegin());
    QCOMPARE(restored.geometry().asPoint(), QgsPointXY(210000, 550000));
    QCOMPARE(restored.attribute(QStringLiteral("note")).toString(), QStringLiteral("다음날 다시 볼 지점"));
    QCOMPARE(QFileInfo(reference->source().section(QLatin1Char('|'), 0, 0)).absoluteFilePath(),
             QFileInfo(path).absoluteFilePath());
  }
  void saveAs_embeddedFailureRestoresSourcesAndRemainsUnsaved() {
    const QString path = makeSurvey(QStringLiteral("사본실패복귀"));
    QVERIFY(!path.isEmpty());
    // The copied database retains these triggers, rejecting only workspace writes.
    QVERIFY(executeGpkgSql(path, "CREATE TRIGGER deny_project_insert BEFORE INSERT ON qgis_projects BEGIN SELECT RAISE(ABORT, 'test workspace failure'); END"));
    QVERIFY(executeGpkgSql(path, "CREATE TRIGGER deny_project_update BEFORE UPDATE ON qgis_projects BEGIN SELECT RAISE(ABORT, 'test workspace failure'); END"));
    QVERIFY(executeGpkgSql(path, "CREATE TRIGGER deny_project_delete BEFORE DELETE ON qgis_projects BEGIN SELECT RAISE(ABORT, 'test workspace failure'); END"));
    MainWindow window;
    disableRendering(window);
    QVERIFY(window.openSurveyGpkg(path));
    auto* project = QgsProject::instance();
    auto* layer = LayerOps::findByLayerKey(project, QStringLiteral("survey_area"));
    QVERIFY(layer);
    const QString source = layer->source();
    const QString projectFile = project->fileName();
    const QString projectHome = project->presetHomePath();
    auto* reference = new QgsVectorLayer(QStringLiteral("Point?crs=EPSG:5186&field=note:string"),
                                        QStringLiteral("실패후에도남을참고점"), QStringLiteral("memory"));
    QVERIFY(reference->isValid());
    LayerOps::markReferenceLayer(reference);
    project->addMapLayer(reference);
    const QString referenceId = reference->id();
    QVERIFY(reference->startEditing());
    QgsFeature point(reference->fields());
    point.setGeometry(QgsGeometry::fromPointXY(QgsPointXY(210010, 550020)));
    point.setAttribute(QStringLiteral("note"), QStringLiteral("보존할 메모"));
    QVERIFY(reference->addFeature(point));
    project->setTitle(QStringLiteral("사본 실패 뒤에도 저장할 작업"));
    project->setDirty(true);
    const QString target = m_files.filePath(QStringLiteral("실패할사본.gpkg"));
    QVERIFY(selectSaveAs(window, target));
    QVERIFY(QFile::exists(target));
    QVERIFY(layer->isValid());
    QCOMPARE(layer->source(), source);
    QCOMPARE(project->fileName(), projectFile);
    QCOMPARE(project->presetHomePath(), projectHome);
    QVERIFY(project->isDirty());
    QCOMPARE(project->mapLayer(referenceId), reference);
    QVERIFY(reference->isValid() && reference->isEditable());
    QCOMPARE(reference->providerType(), QStringLiteral("memory"));
    QCOMPARE(reference->crs().authid(), QStringLiteral("EPSG:5186"));
    QCOMPARE(reference->featureCount(), 1LL);
    const QgsFeature restoredPoint = reference->getFeature(*reference->allFeatureIds().constBegin());
    QCOMPARE(restoredPoint.geometry().asPoint(), QgsPointXY(210010, 550020));
    QCOMPARE(restoredPoint.attribute(QStringLiteral("note")).toString(), QStringLiteral("보존할 메모"));
    QVERIFY(LayerOps::isReferenceLayer(reference));
    QVERIFY(executeGpkgSql(path, "DROP TRIGGER deny_project_insert"));
    QVERIFY(executeGpkgSql(path, "DROP TRIGGER deny_project_update"));
    QVERIFY(executeGpkgSql(path, "DROP TRIGGER deny_project_delete"));
    QVERIFY(saveNow(window));
    QgsProject saved;
    QVERIFY(saved.read(SurveyStorage::projectUri(path), Qgis::ProjectReadFlag::DontResolveLayers));
    QCOMPARE(saved.title(), QStringLiteral("사본 실패 뒤에도 저장할 작업"));
    QVERIFY(!project->isDirty());
  }
  void saveAs_existingDestinationIsNotOverwritten_data() {
    QTest::addColumn<bool>("companionOnly");
    QTest::newRow("existing-gpkg") << false;
    QTest::newRow("existing-qgz") << true;
  }
  void saveAs_existingDestinationIsNotOverwritten() {
    QFETCH(bool, companionOnly);
    const QString source = makeSurvey(QStringLiteral("덮어쓰기방지원본_%1").arg(companionOnly));
    QVERIFY(!source.isEmpty());
    const QString target = m_files.filePath(QStringLiteral("기존자료_%1.gpkg").arg(companionOnly));
    const QString existing = companionOnly
        ? QFileInfo(target).dir().filePath(QFileInfo(target).completeBaseName() + QStringLiteral(".qgz"))
        : target;
    const QString sourceQgz = QFileInfo(source).dir().filePath(QFileInfo(source).completeBaseName() + QStringLiteral(".qgz"));
    QVERIFY(QFile::copy(companionOnly ? sourceQgz : source, existing));
    const QByteArray before = QCryptographicHash::hash(contents(existing), QCryptographicHash::Sha256);
    MainWindow window;
    disableRendering(window);
    QVERIFY(window.openSurveyGpkg(source));
    auto* project = QgsProject::instance();
    auto* layer = LayerOps::findByLayerKey(project, QStringLiteral("survey_area"));
    QVERIFY(layer && layer->startEditing());
    const QString originalSource = layer->source();
    const QgsFeatureId fid = *layer->allFeatureIds().constBegin();
    QgsGeometry pending = QgsGeometry::fromRect(QgsRectangle(190001, 560002, 190111, 560112));
    QVERIFY(layer->changeGeometry(fid, pending));
    QVERIFY(selectSaveAs(window, target));
    QCOMPARE(QCryptographicHash::hash(contents(existing), QCryptographicHash::Sha256), before);
    QCOMPARE(layer->source(), originalSource);
    QVERIFY(layer->isModified());
    QVERIFY(layer->getFeature(fid).geometry().equals(pending));
    if (companionOnly) QVERIFY(!QFileInfo::exists(target));
    auto* bar = window.findChild<QgsMessageBar*>();
    QVERIFY(bar);
    bool warned = false;
    for (auto* item : bar->items())
      if (item->level() == Qgis::MessageLevel::Warning && item->text().contains(QStringLiteral("이름"))) warned = true;
    QVERIFY(warned);
  }
  void save_qgisExceptionStaysInsideSlot_data() {
    QTest::addColumn<bool>("saveAs");
    QTest::newRow("persist") << false;
    QTest::newRow("save-as") << true;
  }
  void save_qgisExceptionStaysInsideSlot() {
    QFETCH(bool, saveAs);
    const QString source = makeSurvey(QStringLiteral("예외보존_%1").arg(saveAs));
    QVERIFY(!source.isEmpty());
    MainWindow window;
    disableRendering(window);
    QVERIFY(window.openSurveyGpkg(source));
    auto* project = QgsProject::instance();
    auto* layer = LayerOps::findByLayerKey(project, QStringLiteral("survey_area"));
    QVERIFY(layer);
    const QString originalSource = layer->source();
    const QString originalProjectFile = project->fileName();
    const QByteArray before = QCryptographicHash::hash(contents(source), QCryptographicHash::Sha256);
    project->setTitle(QStringLiteral("예외 뒤에도 남을 제목"));
    project->setDirty(true);
    bool injected = false;
    const auto connection = connect(project, &QgsProject::writeProject, &window,
        [&](QDomDocument&) {
          injected = true;
          throw QgsException(QStringLiteral("synthetic non-std write exception"));
        }, Qt::DirectConnection);
    bool escaped = false;
    bool result = false;
    try {
      result = saveAs ? selectSaveAs(window, m_files.filePath(QStringLiteral("예외사본.gpkg")))
                      : saveNow(window);
    } catch (...) {
      escaped = true;
    }
    disconnect(connection);
    QVERIFY2(!escaped, "QGIS exceptions must not escape the save slot into the GUI event loop");
    QVERIFY(injected);
    if (saveAs) QVERIFY(result);
    else QVERIFY(!result);
    QCOMPARE(layer->source(), originalSource);
    QCOMPARE(project->fileName(), originalProjectFile);
    QVERIFY(project->isDirty());
    QCOMPARE(project->title(), QStringLiteral("예외 뒤에도 남을 제목"));
    QCOMPARE(QCryptographicHash::hash(contents(source), QCryptographicHash::Sha256), before);
    auto* bar = window.findChild<QgsMessageBar*>();
    QVERIFY(bar);
    bool warned = false;
    for (auto* item : bar->items())
      if (item->level() == Qgis::MessageLevel::Warning && item->text().contains(QStringLiteral("다시 저장"))) warned = true;
    QVERIFY(warned);
    QVERIFY(saveNow(window));
  }
  void save_companionFailureKeepsEmbeddedWorkspaceAndWarns() {
    const QString name = QStringLiteral("보조사본실패");
    const QString path = makeSurvey(name);
    QVERIFY(!path.isEmpty());
    MainWindow window;
    disableRendering(window);
    QVERIFY(window.openSurveyGpkg(path));
    const QString companion = QFileInfo(path).dir().filePath(name + QStringLiteral(".qgz"));
    QVERIFY(QFile::remove(companion));
    QVERIFY(QDir().mkdir(companion)); // A directory cannot be atomically replaced by a QGZ file.
    auto* project = QgsProject::instance();
    project->setTitle(QStringLiteral("내장 구성은 저장됨"));
    QVERIFY(saveNow(window));
    QVERIFY(QFileInfo(companion).isDir());
    QgsProject saved;
    QVERIFY(saved.read(SurveyStorage::projectUri(path), Qgis::ProjectReadFlag::DontResolveLayers));
    QCOMPARE(saved.title(), QStringLiteral("내장 구성은 저장됨"));
    auto* bar = window.findChild<QgsMessageBar*>();
    QVERIFY(bar);
    bool warned = false;
    for (auto* item : bar->items()) {
      if (item->level() == Qgis::MessageLevel::Warning && item->text().contains(QLatin1String("QGZ")) &&
          item->text().contains(QStringLiteral("다시 저장"))) warned = true;
    }
    QVERIFY2(warned, "A failed companion write must show an actionable warning even after GPKG success");
    QVERIFY(!project->isDirty());
  }
  void fieldCopy_restoresAllValidLayersAndSavesTree() {
    const QString input = qEnvironmentVariable("KA_HGIS_FIELD_GPKG");
    if (input.isEmpty()) QSKIP("Set KA_HGIS_FIELD_GPKG to a disposable field-file copy");
    const QString copy = m_files.filePath(QFileInfo(input).fileName());
    QVERIFY(QFile::copy(input, copy));
    MainWindow window;
    disableRendering(window);
    QVERIFY(window.openSurveyGpkg(copy));
    auto* project = QgsProject::instance();
    QMap<QString, qint64> counts;
    for (auto* layer : project->mapLayers()) {
      if (!layer->isValid()) continue;
      QVERIFY2(project->layerTreeRoot()->findLayer(layer->id()), qPrintable(layer->name()));
      if (auto* vector = qobject_cast<QgsVectorLayer*>(layer)) {
        QCOMPARE(QFileInfo(vector->source().section(QLatin1Char('|'), 0, 0)).canonicalFilePath(),
                 QFileInfo(copy).canonicalFilePath());
        counts.insert(vector->id(), vector->featureCount());
      }
    }
    QVERIFY(!counts.isEmpty());
    QVERIFY(QMetaObject::invokeMethod(&window, "saveProject", Qt::DirectConnection));
    QVERIFY(window.openSurveyGpkg(copy));
    for (auto it = counts.cbegin(); it != counts.cend(); ++it) {
      auto* vector = qobject_cast<QgsVectorLayer*>(project->mapLayer(it.key()));
      QVERIFY(vector && vector->isValid());
      QCOMPARE(vector->featureCount(), it.value());
      QVERIFY(project->layerTreeRoot()->findLayer(vector->id()));
    }
    qInfo() << "Field copy restored vector layers:" << counts.size()
            << "registry:" << project->mapLayers().size()
            << "legend:" << project->layerTreeRoot()->findLayers().size();
    const QString screenshot = qEnvironmentVariable("KA_HGIS_FIELD_SCREENSHOT");
    if (!screenshot.isEmpty()) {
      window.show();
      window.findChild<QgsLayerTreeView*>(QStringLiteral("layerTree"))->expandAll();
      QApplication::processEvents();
      QVERIFY(window.grab().save(screenshot));
    }
  }
};

static QByteArray localCadastralCapabilities() {
  return QByteArray(R"(<WMS_Capabilities version="1.3.0" xmlns="http://www.opengis.net/wms" xmlns:xlink="http://www.w3.org/1999/xlink">
<Service><Name>WMS</Name><Title>Offline save/open fixture</Title></Service>
<Capability><Request>
<GetCapabilities><Format>text/xml</Format><DCPType><HTTP><Get><OnlineResource xlink:href="https://save-open.invalid/wms"/></Get></HTTP></DCPType></GetCapabilities>
<GetMap><Format>image/png</Format><DCPType><HTTP><Get><OnlineResource xlink:href="https://save-open.invalid/wms"/></Get></HTTP></DCPType></GetMap>
</Request><Layer><Title>Local cadastral fixture</Title>
<CRS>EPSG:3857</CRS><CRS>EPSG:5186</CRS><CRS>EPSG:5187</CRS><CRS>EPSG:4326</CRS>
<EX_GeographicBoundingBox><westBoundLongitude>124</westBoundLongitude><eastBoundLongitude>132</eastBoundLongitude><southBoundLatitude>32</southBoundLatitude><northBoundLatitude>40</northBoundLatitude></EX_GeographicBoundingBox>
<BoundingBox CRS="EPSG:3857" minx="13500000" miny="3500000" maxx="14900000" maxy="4900000"/>
<BoundingBox CRS="EPSG:5186" minx="-100000" miny="-100000" maxx="700000" maxy="1000000"/>
<BoundingBox CRS="EPSG:5187" minx="-100000" miny="-100000" maxx="700000" maxy="1000000"/>
<BoundingBox CRS="EPSG:4326" minx="32" miny="124" maxx="40" maxy="132"/>
<Layer queryable="0"><Name>lp_pa_cbnd_bonbun</Name><Title>Local parcels</Title><Style><Name>lp_pa_cbnd_bonbun</Name><Title>Local parcels</Title></Style></Layer>
<Layer queryable="0"><Name>lp_pa_cbnd_bubun</Name><Title>Local parcel labels</Title><Style><Name>lp_pa_cbnd_bubun</Name><Title>Local parcel labels</Title></Style></Layer>
</Layer></Capability></WMS_Capabilities>)");
}

int main(int argc, char** argv) {
  QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
  QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
  // GDAL WMS uses libcurl outside QGIS's request preprocessor. Keep its remote
  // requests offline too; these options are scoped to this test process.
  CPLSetConfigOption("GDAL_HTTP_PROXY", "127.0.0.1:1");
  CPLSetConfigOption("GDAL_HTTP_TIMEOUT", "1");
  QgsApplication app(argc, argv, true);
#ifdef Q_OS_WIN
  if (QGuiApplication::platformName() == QLatin1String("offscreen")) {
    // Match the installed field font: offscreen Qt does not discover it itself.
    const QDir windows(qEnvironmentVariable("WINDIR"));
    for (const QString& file : {QStringLiteral("malgun.ttf"), QStringLiteral("malgunbd.ttf")}) {
      const QString path = windows.filePath(QStringLiteral("Fonts/") + file);
      if (QFontDatabase::addApplicationFont(path) < 0) {
        qCritical().noquote() << "Could not load the installed QA font:" << path;
        return 1;
      }
    }
  }
#endif
  QTemporaryDir settings;
  const QString capabilitiesPath = settings.filePath(QStringLiteral("cadastral-capabilities.xml"));
  const QString mapPath = settings.filePath(QStringLiteral("white-map.png"));
  QFile capabilities(capabilitiesPath);
  const QByteArray capabilitiesXml = localCadastralCapabilities();
  if (!capabilities.open(QIODevice::WriteOnly) || capabilities.write(capabilitiesXml) != capabilitiesXml.size())
    return 1;
  capabilities.close();
  QImage mapImage(512, 512, QImage::Format_RGB32);
  mapImage.fill(Qt::white);
  if (!mapImage.save(mapPath)) return 1;
  QgsApplication::setPrefixPath(qEnvironmentVariable("QGIS_PREFIX_PATH"), true);
  QgsApplication::initQgis();
  // Save/open fixtures use local rasters. Incidental basemap refreshes must not
  // depend on external servers; network behavior has its own dedicated suites.
  const QUrl capabilitiesUrl = QUrl::fromLocalFile(capabilitiesPath);
  const QUrl mapUrl = QUrl::fromLocalFile(mapPath);
  const QString networkIsolation = QgsNetworkAccessManager::setRequestPreprocessor([capabilitiesUrl, mapUrl](QNetworkRequest* request) {
    if (!request) return;
    const QUrl url = request->url();
    const QString scheme = url.scheme().toLower();
    const QString host = url.host().toLower();
    if ((scheme == QLatin1String("http") || scheme == QLatin1String("https")) &&
        host != QLatin1String("localhost") && host != QLatin1String("127.0.0.1") &&
        host != QLatin1String("::1")) {
      bool capabilitiesRequest = false;
      for (const auto& item : QUrlQuery(url).queryItems())
        if (item.first.compare(QLatin1String("request"), Qt::CaseInsensitive) == 0 &&
            item.second.compare(QLatin1String("GetCapabilities"), Qt::CaseInsensitive) == 0)
          capabilitiesRequest = true;
      // Valid local capabilities avoid the GDAL failure fallback. The entire URL
      // is replaced so credentials from remote query/path stay private.
      request->setUrl(capabilitiesRequest ? capabilitiesUrl : mapUrl);
    }
  });
  QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settings.path());
  s_testSettingsPath = settings.path();
  KaTheme::apply(&app);
  TestSaveOpen tests;
  const int result = QTest::qExec(&tests, argc, argv);
  QgsApplication::exitQgis();
  QgsNetworkAccessManager::removeRequestPreprocessor(networkIsolation);
  return result;
}

#include "test_save_open.moc"

