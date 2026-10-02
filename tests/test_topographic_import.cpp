#include <QtTest>
#include <QSignalSpy>
#include <QComboBox>
#include <QPushButton>
#include <QSettings>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QCryptographicHash>
#include <QFontDatabase>
#include <QLabel>
#include <QTimer>
#include <gdal_priv.h>
#include <ogrsf_frmts.h>
#include <qgsapplication.h>
#include <qgstaskmanager.h>
#include <qgsproject.h>
#include <qgsmapcanvas.h>
#include <qgslayertree.h>
#include <qgsvectorlayer.h>
#include <qgssinglesymbolrenderer.h>
#include <qgssymbol.h>
#include <qgsexpression.h>
#include <qgsexpressioncontext.h>
#include <qgsvectorlayerlabeling.h>
#include <qgsrulebasedrenderer.h>
#include "app/KaTopographicImportDialog.h"
#include "core/LayerOps.h"
#include "core/TopographicShapefileCache.h"
#include <cmath>
#include <memory>

namespace {
bool fixture(const QString& path, bool metadata, double x) {
  auto* driver = GetGDALDriverManager()->GetDriverByName("ESRI Shapefile");
  if (!driver) return false;
  std::unique_ptr<GDALDataset, decltype(&GDALClose)> ds(driver->Create(path.toUtf8().constData(), 0, 0, 0, GDT_Unknown, nullptr), GDALClose);
  if (!ds) return false;
  OGRSpatialReference crs; if (crs.importFromEPSG(5186) != OGRERR_NONE) return false;
  auto* layer = ds->CreateLayer("road", metadata ? &crs : nullptr, wkbLineString, nullptr);
  if (!layer) return false;
  OGRLineString line; line.addPoint(x, 450000.); line.addPoint(x + 100., 450100.);
  std::unique_ptr<OGRFeature, decltype(&OGRFeature::DestroyFeature)> feature(OGRFeature::CreateFeature(layer->GetLayerDefn()), OGRFeature::DestroyFeature);
  return feature->SetGeometry(&line) == OGRERR_NONE && layer->CreateFeature(feature.get()) == OGRERR_NONE;
}
bool polygonFixture(const QString& path) {
  auto* driver = GetGDALDriverManager()->GetDriverByName("ESRI Shapefile");
  if (!driver) return false;
  std::unique_ptr<GDALDataset, decltype(&GDALClose)> ds(driver->Create(path.toUtf8().constData(), 0, 0, 0, GDT_Unknown, nullptr), GDALClose);
  if (!ds) return false;
  OGRSpatialReference crs; if (crs.importFromEPSG(5186) != OGRERR_NONE) return false;
  auto* layer = ds->CreateLayer("building", &crs, wkbPolygon, nullptr);
  if (!layer) return false;
  OGRLinearRing ring;
  ring.addPoint(200000., 450000.);
  ring.addPoint(200080., 450000.);
  ring.addPoint(200080., 450080.);
  ring.addPoint(200000., 450080.);
  ring.addPoint(200000., 450000.);
  OGRPolygon polygon;
  polygon.addRing(&ring);
  std::unique_ptr<OGRFeature, decltype(&OGRFeature::DestroyFeature)> feature(OGRFeature::CreateFeature(layer->GetLayerDefn()), OGRFeature::DestroyFeature);
  return feature->SetGeometry(&polygon) == OGRERR_NONE && layer->CreateFeature(feature.get()) == OGRERR_NONE;
}
QByteArray digest(const QString& path) {
  QFile file(path); if (!file.open(QIODevice::ReadOnly)) return {};
  return QCryptographicHash::hash(file.readAll(), QCryptographicHash::Sha256);
}
QgsMapLayer* topographic() {
  for (auto* layer : QgsProject::instance()->mapLayers())
    if (!layer->customProperty(QStringLiteral("ka_hgis/topographic_source")).toString().isEmpty()) return layer;
  return nullptr;
}
bool topoMentions(QgsMapLayer* layer, const QString& token) {
  if (!layer || token.isEmpty()) return false;
  if (layer->source().contains(token) || layer->name().contains(token)) return true;
  const QStringList srcs = layer->customProperty(QStringLiteral("ka_hgis/topographic_sources")).toStringList();
  for (const QString& s : srcs) {
    if (s.contains(token)) return true;
  }
  return layer->customProperty(QStringLiteral("ka_hgis/topographic_source")).toString().contains(token);
}
bool anyTopoMentions(const QString& token) {
  for (auto* layer : QgsProject::instance()->mapLayers()) {
    if (topoMentions(layer, token)) return true;
  }
  return false;
}
}
class TopographicImportTest : public QObject {
  Q_OBJECT
private slots:
  void init() { QgsProject::instance()->clear(); QSettings().clear(); }
  void cleanup() { QgsProject::instance()->clear(); }
  void preparedSheetsAppearTogetherWithoutIntermediateMapChanges() {
    QTemporaryDir dir,cache;
    for(int i=0;i<12;++i) QVERIFY(fixture(dir.filePath(QStringLiteral("A0010000_%1.shp").arg(i)),true,200000.));
    auto records=TopographicCatalog::scan(dir.path(),nullptr,cache.path()).records;
    QCOMPARE(records.size(),12);
    for(auto& record:records) record.sourceSheet=QStringLiteral("37701");
    auto* project=QgsProject::instance();
    const QgsCoordinateReferenceSystem crs(records.first().crsWkt); project->setCrs(crs);
    QgsMapCanvas canvas; canvas.setDestinationCrs(crs);
    canvas.setExtent(QgsRectangle(199900.,449900.,200200.,450200.));
    auto* background=new QgsVectorLayer(QStringLiteral("LineString?crs=EPSG:5186"),QStringLiteral("기존 참조 지도"),QStringLiteral("memory"));
    project->addMapLayer(background); LayerOps::markReferenceLayer(background);
    LayerOps::syncMapCanvas(project,&canvas,false);
    const auto backgroundId=background->id();
    KaTopographicImportDialog dialog(&canvas);
    QList<int> displayedCounts;
    connect(project,&QgsProject::layersAdded,&dialog,[&] {
      displayedCounts.append(project->mapLayers().size()-1);
    });
    QVERIFY(dialog.importVerified(records));
    QTRY_VERIFY_WITH_TIMEOUT(!dialog.isAutomaticLoading(),10000);
    QCOMPARE(project->mapLayers().size(),2);
    QCOMPARE(displayedCounts,QList<int>{1});
    QVERIFY(project->mapLayer(backgroundId));
    QVERIFY(project->layerTreeRoot()->findLayer(backgroundId)->isVisible());
    canvas.stopRendering(); canvas.setLayers({});
    project->clear();
  }
  void cancelWhilePreparingDiscardsUnpublishedLayers() {
    QTemporaryDir dir,cache;
    for(int i=0;i<3;++i) QVERIFY(fixture(dir.filePath(QStringLiteral("A0010000_%1.shp").arg(i)),true,200000.));
    auto records=TopographicCatalog::scan(dir.path(),nullptr,cache.path()).records;
    QCOMPARE(records.size(),3);
    for(auto& record:records) record.sourceSheet=QStringLiteral("37701");
    auto* project=QgsProject::instance();
    const QgsCoordinateReferenceSystem crs(records.first().crsWkt); project->setCrs(crs);
    QgsMapCanvas canvas; canvas.setDestinationCrs(crs);
    canvas.setExtent(QgsRectangle(199900.,449900.,200200.,450200.));
    KaTopographicImportDialog dialog(&canvas);
    QSignalSpy added(project,&QgsProject::layersAdded);
    bool canceled=false;
    connect(&dialog,&KaTopographicImportDialog::automaticLoadingProgressChanged,&dialog,[&](int prepared,int pending) {
      if(!canceled && prepared==1 && pending>0) {
        canceled=true; dialog.setAutomaticLoadingEnabled(false);
      }
    });
    QVERIFY(dialog.importVerified(records));
    QTRY_VERIFY(canceled);
    QVERIFY(!dialog.isAutomaticLoading());
    QVERIFY(added.isEmpty()); QVERIFY(project->mapLayers().isEmpty());
    dialog.setAutomaticLoadingEnabled(true);
    QTRY_VERIFY_WITH_TIMEOUT(!dialog.isAutomaticLoading(),10000);
    QCOMPARE(project->mapLayers().size(),1); QCOMPARE(added.size(),1);
    canvas.stopRendering(); canvas.setLayers({}); project->clear();
  }
  void scopeChangeDuringPreparationDiscardsObsoleteLayers_data() {
    QTest::addColumn<bool>("clearProject");
    QTest::newRow("new-project")<<true;
    QTest::newRow("other-location")<<false;
  }
  void scopeChangeDuringPreparationDiscardsObsoleteLayers() {
    QFETCH(bool,clearProject);
    QTemporaryDir dir,cache;
    for(int i=0;i<3;++i) QVERIFY(fixture(dir.filePath(QStringLiteral("A0010000_%1.shp").arg(i)),true,200000.));
    auto records=TopographicCatalog::scan(dir.path(),nullptr,cache.path()).records;
    for(auto& record:records) record.sourceSheet=QStringLiteral("37701");
    auto* project=QgsProject::instance();
    const QgsCoordinateReferenceSystem crs(QStringLiteral("EPSG:5186")); project->setCrs(crs);
    QgsMapCanvas canvas; canvas.setDestinationCrs(crs);
    canvas.setExtent(QgsRectangle(199900.,449900.,200200.,450200.));
    KaTopographicImportDialog dialog(&canvas);
    QSignalSpy added(project,&QgsProject::layersAdded);
    bool changed=false;
    connect(&dialog,&KaTopographicImportDialog::automaticLoadingProgressChanged,&dialog,[&](int prepared,int pending) {
      if(changed || prepared!=1 || pending<=0) return;
      changed=true;
      if(clearProject) project->clear();
      else {
        canvas.setExtent(QgsRectangle(299900.,449900.,300200.,450200.));
        dialog.setMapsEnabled(true);
      }
    });
    QVERIFY(dialog.importVerified(records));
    QTRY_VERIFY(changed);
    QVERIFY(!dialog.isAutomaticLoading());
    QCoreApplication::processEvents();
    QVERIFY(added.isEmpty()); QVERIFY(project->mapLayers().isEmpty());
  }
  void cancelAutomaticPreparationStillPublishesPreparedManualLayer() {
    QTemporaryDir dir,cache;
    for(int i=0;i<2;++i) QVERIFY(fixture(dir.filePath(QStringLiteral("A0010000_%1.shp").arg(i)),true,200000.));
    auto records=TopographicCatalog::scan(dir.path(),nullptr,cache.path()).records;
    QCOMPARE(records.size(),2);
    for(auto& record:records) record.sourceSheet=QStringLiteral("37701");
    auto* project=QgsProject::instance();
    const QgsCoordinateReferenceSystem crs(records.first().crsWkt); project->setCrs(crs);
    QgsMapCanvas canvas; canvas.setDestinationCrs(crs);
    canvas.setExtent(QgsRectangle(199900.,449900.,200200.,450200.));
    KaTopographicImportDialog dialog(&canvas);
    dialog.retainForReview({records.first()},QStringLiteral("수동 확인"));
    auto* table=dialog.findChild<QTableWidget*>(QStringLiteral("topographicFiles")); QVERIFY(table);
    table->item(0,0)->setCheckState(Qt::Checked);
    auto* add=dialog.findChild<QPushButton*>(QStringLiteral("importTopographic")); QVERIFY(add); add->click();
    QVERIFY(dialog.importVerified({records.last()}));
    auto* timer=dialog.findChildren<QTimer*>().first();
    for(auto* candidate:dialog.findChildren<QTimer*>()) if(candidate->isActive() && candidate->interval()==0) timer=candidate;
    QVERIFY(timer && timer->isActive() && timer->interval()==0);
    // One real datasource-open tick prepares the manual record. The automatic
    // record remains pending; cancel must leave the manual result publishable.
    QVERIFY(QMetaObject::invokeMethod(timer,"timeout",Qt::DirectConnection));
    QVERIFY(project->mapLayers().isEmpty());
    dialog.setAutomaticLoadingEnabled(false);
    QTRY_COMPARE(project->mapLayers().size(),1);
    QVERIFY(topoMentions(topographic(), QFileInfo(records.first().source).fileName()));
    QVERIFY(!dialog.isAutomaticLoading());
    canvas.stopRendering(); canvas.setLayers({}); project->clear();
  }
  void elevationTextUsesSurveyedValueInsteadOfTextInsertionZ_data() {
    QTest::addColumn<bool>("converted");
    QTest::newRow("original-dxf")<<false;
    QTest::newRow("converted-shp")<<true;
  }
  void elevationTextUsesSurveyedValueInsteadOfTextInsertionZ() {
    QFETCH(bool,converted);
    QTemporaryDir originals,catalog,cache;
    const auto path=originals.filePath(QStringLiteral("336063.dxf"));
    struct Sample { const char* code; const char* text; double z; const char* label; };
    const QList<Sample> samples{{"F0027132","119.2",0.,"119.2"},{"F0027132","",0.,""},
      {"F0027132","unknown",0.,""},{"F0027132","0.0",0.,"0.0"},
      {"F0027217","",0.,"0.0"},{"F0027217","",75.4,"75.4"}};
    {
      auto* driver=GetGDALDriverManager()->GetDriverByName("DXF"); QVERIFY(driver);
      std::unique_ptr<GDALDataset,decltype(&GDALClose)> ds(driver->Create(path.toUtf8().constData(),0,0,0,GDT_Unknown,nullptr),GDALClose);
      QVERIFY(ds); auto* layer=ds->CreateLayer("entities",nullptr,wkbUnknown,nullptr); QVERIFY(layer);
      for(int i=0;i<samples.size();++i) {
        const auto& sample=samples[i];
        std::unique_ptr<OGRFeature,decltype(&OGRFeature::DestroyFeature)> feature(OGRFeature::CreateFeature(layer->GetLayerDefn()),OGRFeature::DestroyFeature);
        feature->SetField("Layer",sample.code);
        if(*sample.text) {
          feature->SetField("Text",sample.text);
          const auto style=QByteArray("LABEL(t:\"")+sample.text+"\",s:2g)";
          feature->SetStyleString(style.constData());
        }
        OGRPoint point(200000.+30.*i,450000.,sample.z);
        QCOMPARE(feature->SetGeometry(&point),OGRERR_NONE); QCOMPARE(layer->CreateFeature(feature.get()),OGRERR_NONE);
      }
    }
    const auto before=digest(path);
    auto records=TopographicCatalog::scan(originals.path(),nullptr,catalog.path()).records;
    QVERIFY(!records.isEmpty());
    const QgsCoordinateReferenceSystem crs(QStringLiteral("EPSG:5186"));
    for(auto& record:records) { record.crsWkt=crs.toWkt(); record.sourceSheet=QStringLiteral("336063"); }
    if(converted) {
      const auto prepared=TopographicShapefileCache::prepare(records,cache.path(),{});
      QVERIFY2(prepared.error.isEmpty(),qPrintable(prepared.error)); records=prepared.records;
    }
    QgsProject::instance()->setCrs(crs);
    QgsMapCanvas canvas; canvas.setDestinationCrs(crs);
    canvas.setExtent(QgsRectangle(199980.,449980.,200200.,450050.));
    KaTopographicImportDialog dialog(&canvas);
    QVERIFY(dialog.importVerified(records));
    QTRY_VERIFY_WITH_TIMEOUT(!dialog.isAutomaticLoading(),10000);
    QCOMPARE(QgsProject::instance()->mapLayers().size(),0);
    for (const auto& record : records)
      QCOMPARE(record.category, TopographicCatalog::Category::ElevationPoint);
    QCOMPARE(digest(path),before);
    canvas.stopRendering(); canvas.setLayers({}); QgsProject::instance()->clear();
  }
  void convertedShpAppearsInReferenceLayersWithGrayThinLines() {
    QTemporaryDir originals, catalog, cache;
    const QString path = originals.filePath(QStringLiteral("37701.dxf"));
    {
      auto* driver = GetGDALDriverManager()->GetDriverByName("DXF"); QVERIFY(driver);
      std::unique_ptr<GDALDataset, decltype(&GDALClose)> ds(driver->Create(path.toUtf8().constData(), 0, 0, 0, GDT_Unknown, nullptr), GDALClose);
      QVERIFY(ds); auto* source = ds->CreateLayer("entities", nullptr, wkbUnknown, nullptr); QVERIFY(source);
      for (int i = 0; i < 2; ++i) {
        std::unique_ptr<OGRFeature, decltype(&OGRFeature::DestroyFeature)> f(OGRFeature::CreateFeature(source->GetLayerDefn()), OGRFeature::DestroyFeature);
        f->SetField("Layer", i == 0 ? "F0017111" : "F0017114");
        OGRLineString line; line.addPoint(200000., 450000.+i*20., 50.); line.addPoint(200050., 450050.+i*20., 50.); line.addPoint(200100., 450000.+i*20., 50.);
        QCOMPARE(f->SetGeometry(&line), OGRERR_NONE); QCOMPARE(source->CreateFeature(f.get()), OGRERR_NONE);
      }
    }
    auto records = TopographicCatalog::scan(originals.path(), nullptr, catalog.path()).records;
    const QgsCoordinateReferenceSystem crs(QStringLiteral("EPSG:5186"));
    for (auto& r : records) { r.crsWkt = crs.toWkt(); r.sourceSheet = QStringLiteral("37701"); }
    const auto converted = TopographicShapefileCache::prepare(records, cache.path(), {});
    QVERIFY2(converted.error.isEmpty(), qPrintable(converted.error)); QCOMPARE(converted.records.size(), 1);
    QgsProject::instance()->setCrs(crs);
    QgsMapCanvas canvas; canvas.resize(800, 500); canvas.setDestinationCrs(crs); canvas.setCanvasColor(Qt::white);
    canvas.setExtent(QgsRectangle(199980., 449980., 200120., 450100.)); canvas.show();
    KaTopographicImportDialog dialog(&canvas); QSignalSpy changed(&dialog, &KaTopographicImportDialog::referenceLayersChanged);
    QSignalSpy loading(&dialog,&KaTopographicImportDialog::automaticLoadingChanged);
    QSignalSpy attention(&dialog,&KaTopographicImportDialog::automaticLoadingAttention);
    QSignalSpy progress(&dialog,&KaTopographicImportDialog::automaticLoadingProgressChanged);
    bool idleBeforeMap=false;
    connect(&dialog,&KaTopographicImportDialog::automaticLoadingChanged,&dialog,[&](bool active) {
      if (!active && !topographic()) idleBeforeMap=true;
    });
    QVERIFY(dialog.importVerified(converted.records));
    QCOMPARE(loading.size(),1); QVERIFY(loading.first().at(0).toBool());
    QVERIFY(!progress.isEmpty()); QCOMPARE(progress.last().at(0).toInt(),0); QCOMPARE(progress.last().at(1).toInt(),1);
    QTRY_VERIFY(!changed.isEmpty());
    QTRY_COMPARE(loading.size(),2); QVERIFY(!loading.last().at(0).toBool());
    QCOMPARE(progress.last().at(0).toInt(),1); QCOMPARE(progress.last().at(1).toInt(),0);
    QVERIFY(!idleBeforeMap); QVERIFY(attention.isEmpty());
    auto* layer = qobject_cast<QgsVectorLayer*>(topographic()); QVERIFY(layer);
    QVERIFY(layer->source().contains(QStringLiteral(".shp"))); QVERIFY(!layer->source().contains(QStringLiteral(".dxf")));
    QCOMPARE(layer->featureCount(), 2); QCOMPARE(layer->crs(), crs); QVERIFY(!layer->startEditing());
    auto* node = QgsProject::instance()->layerTreeRoot()->findLayer(layer); QVERIFY(node); QVERIFY(node->itemVisibilityChecked());
    QVERIFY(LayerOps::isReferenceLayer(layer));
    auto* renderer = dynamic_cast<QgsSingleSymbolRenderer*>(layer->renderer()); QVERIFY(renderer);
    QCOMPARE(renderer->symbol()->color(), QColor(128, 128, 128));
    QCOMPARE(renderer->symbol()->symbolLayer(0)->properties().value(QStringLiteral("line_width")).toDouble(), .2);
    QCOMPARE(renderer->symbol()->symbolLayer(0)->properties().value(QStringLiteral("line_width_unit")).toString(), QStringLiteral("MM"));
    QVERIFY2(!layer->labelsEnabled(), "수치지형도는 선만 보입니다");
    QVERIFY(!layer->labeling());
    QSignalSpy rendered(&canvas,&QgsMapCanvas::mapCanvasRefreshed);
    canvas.setLayers({layer}); canvas.refresh();
    QTRY_VERIFY_WITH_TIMEOUT(!rendered.isEmpty() && !canvas.isDrawing(),10000);
    QImage capture; int grayPixels=0, lineSamples=0;
    const auto captureLines=[&] {
      capture=canvas.grab().toImage(); grayPixels=0; lineSamples=0;
      // Subpixel 0.2 mm strokes blend with white. At 96 DPI their observed
      // neutral grays include 159/207/208, not the unblended source value 128.
      const auto gray=[](const QColor& color) {
        return color.red()>=128 && color.red()<245 && color.red()==color.green() && color.green()==color.blue();
      };
      for (int y=8;y<capture.height()-8;++y) for (int x=8;x<capture.width()-8;++x)
        if (gray(capture.pixelColor(x,y))) ++grayPixels;
      const auto offset=canvas.viewport()->mapTo(&canvas,QPoint());
      const qreal dpr=capture.devicePixelRatio();
      // Check both legs of both known fixture lines, away from the canvas frame.
      for (const auto& point : {QgsPointXY(200025.,450025.),QgsPointXY(200075.,450025.),
                               QgsPointXY(200025.,450045.),QgsPointXY(200075.,450045.)}) {
        const auto pixel=canvas.mapSettings().mapToPixel().transform(point);
        const int cx=qRound((pixel.x()+offset.x())*dpr), cy=qRound((pixel.y()+offset.y())*dpr);
        bool found=false;
        const int radius=qMax(2,qCeil(2*dpr));
        for (int y=qMax(0,cy-radius);y<=qMin(capture.height()-1,cy+radius);++y)
          for (int x=qMax(0,cx-radius);x<=qMin(capture.width()-1,cx+radius);++x)
            if (gray(capture.pixelColor(x,y))) found=true;
        if (found) ++lineSamples;
      }
      return grayPixels>100 && lineSamples==4;
    };
    // A render-complete signal precedes the final canvas paint; verify the
    // actual visible pixels instead of assuming a fixed delay is sufficient.
    QTRY_VERIFY2_WITH_TIMEOUT(captureLines(),qPrintable(QStringLiteral("gray pixels=%1; visible line samples=%2/4")
      .arg(grayPixels).arg(lineSamples)),10000);
    QDir().mkpath(QStringLiteral("build/qa/topographic"));
    QVERIFY(capture.save(QStringLiteral("build/qa/topographic/shp-gray-map.png")));
    dialog.show(); QCoreApplication::processEvents(); QVERIFY(dialog.grab().save(QStringLiteral("build/qa/topographic/shp-gray-inbox.png")));
    canvas.setLayers({}); QgsProject::instance()->clear();
  }
  void failedAutomaticProviderReportsAttentionAndFinishesLoading() {
    QTemporaryDir dir, cache;
    QVERIFY(fixture(dir.filePath(QStringLiteral("A0010000.shp")),true,200000.));
    auto records=TopographicCatalog::scan(dir.path(),nullptr,cache.path()).records;
    QCOMPARE(records.size(),1);
    records[0].sourceSheet=QStringLiteral("37701");
    records[0].source=dir.filePath(QStringLiteral("missing.shp"));
    const QgsCoordinateReferenceSystem crs(records[0].crsWkt);
    QgsProject::instance()->setCrs(crs);
    QgsMapCanvas canvas; canvas.setDestinationCrs(crs);
    canvas.setExtent(QgsRectangle(199900.,449900.,200200.,450200.));
    KaTopographicImportDialog dialog(&canvas);
    QSignalSpy loading(&dialog,&KaTopographicImportDialog::automaticLoadingChanged);
    QSignalSpy attention(&dialog,&KaTopographicImportDialog::automaticLoadingAttention);
    QSignalSpy progress(&dialog,&KaTopographicImportDialog::automaticLoadingProgressChanged);
    QSignalSpy changed(&dialog,&KaTopographicImportDialog::referenceLayersChanged);
    QVERIFY(dialog.importVerified(records));
    QCOMPARE(loading.size(),1); QVERIFY(loading.first().at(0).toBool());
    QTRY_COMPARE_WITH_TIMEOUT(attention.size(),1,10000);
    QVERIFY(!attention.first().at(0).toString().isEmpty());
    QTRY_COMPARE(loading.size(),2); QVERIFY(!loading.last().at(0).toBool());
    QVERIFY(!progress.isEmpty()); QCOMPARE(progress.last().at(0).toInt(),0); QCOMPARE(progress.last().at(1).toInt(),0);
    QCOMPARE(changed.size(),1); QVERIFY(!topographic()); QVERIFY(!dialog.isVisible());
  }
  void cancelStopsPendingAutomaticLoadsAndKeepsManualImportsAvailable() {
    QTemporaryDir dir, cache;
    QVERIFY(fixture(dir.filePath(QStringLiteral("A0010000_first.shp")),true,200000.));
    QVERIFY(fixture(dir.filePath(QStringLiteral("A0010000_second.shp")),true,200000.));
    auto records=TopographicCatalog::scan(dir.path(),nullptr,cache.path()).records;
    QCOMPARE(records.size(),2);
    for (auto& record:records) record.sourceSheet=QStringLiteral("37701");
    const QgsCoordinateReferenceSystem crs(records[0].crsWkt);
    QgsProject::instance()->setCrs(crs);
    QgsMapCanvas canvas; canvas.setDestinationCrs(crs);
    const QgsRectangle near(199900.,449900.,200200.,450200.); canvas.setExtent(near);
    KaTopographicImportDialog dialog(&canvas);
    QVERIFY(dialog.importVerified({records[0]}));
    QTRY_COMPARE(QgsProject::instance()->mapLayers().size(),1);
    const auto firstId=topographic()->id();
    QSignalSpy loading(&dialog,&KaTopographicImportDialog::automaticLoadingChanged);
    QSignalSpy attention(&dialog,&KaTopographicImportDialog::automaticLoadingAttention);
    QSignalSpy progress(&dialog,&KaTopographicImportDialog::automaticLoadingProgressChanged);
    QVERIFY(dialog.importVerified({records[1]}));
    QCOMPARE(loading.size(),1); QVERIFY(loading.first().at(0).toBool());
    QVERIFY(!progress.isEmpty()); QCOMPARE(progress.last().at(0).toInt(),1); QCOMPARE(progress.last().at(1).toInt(),1);
    dialog.setAutomaticLoadingEnabled(false);
    QCOMPARE(loading.size(),2); QVERIFY(!loading.last().at(0).toBool());
    QCOMPARE(progress.last().at(0).toInt(),1); QCOMPARE(progress.last().at(1).toInt(),0);
    QCOMPARE(QgsProject::instance()->mapLayers().size(),1);
    QVERIFY(QgsProject::instance()->mapLayer(firstId));
    // A new coverage pass after cancellation must not revive the pending sheet.
    canvas.setExtent(QgsRectangle(199800.,449800.,200300.,450300.));
    dialog.setMapsEnabled(true); QCoreApplication::processEvents();
    QCOMPARE(QgsProject::instance()->mapLayers().size(),1);
    QVERIFY(QgsProject::instance()->mapLayer(firstId)); QVERIFY(attention.isEmpty());
    // Explicit manual import remains available while background loading is off.
    auto* table=dialog.findChild<QTableWidget*>(QStringLiteral("topographicFiles")); QVERIFY(table);
    QCOMPARE(table->rowCount(),2);
    for (int row=0;row<table->rowCount();++row) table->item(row,0)->setCheckState(Qt::Checked);
    auto* add=dialog.findChild<QPushButton*>(QStringLiteral("importTopographic")); QVERIFY(add);
    add->click();
    QTRY_VERIFY(anyTopoMentions(QStringLiteral("A0010000_second")));
    QVERIFY(anyTopoMentions(QStringLiteral("A0010000_first")));
    QCOMPARE(QgsProject::instance()->mapLayers().size(),1);
    QCOMPARE(loading.size(),2); QVERIFY(attention.isEmpty());
    canvas.stopRendering(); canvas.setLayers({}); QgsProject::instance()->clear();
  }
  void unverifiedRecordsRemainReviewOnlyUntilExplicitManualAdd() {
    QTemporaryDir dir; QTemporaryDir cache;
    QVERIFY(fixture(dir.filePath(QStringLiteral("A0010000.shp")), true, 200000.));
    auto records = TopographicCatalog::scan(dir.path(), nullptr, cache.path()).records;
    QCOMPARE(records.size(), 1); records[0].sourceSheet = QStringLiteral("37701");
    const auto crs = QgsCoordinateReferenceSystem(records[0].crsWkt);
    QgsProject::instance()->setCrs(crs);
    QgsMapCanvas canvas; canvas.setDestinationCrs(crs);
    canvas.setExtent(QgsRectangle(199900., 449900., 200200., 450200.));
    KaTopographicImportDialog dialog(&canvas);
    dialog.retainForReview(records, QStringLiteral("공식 도곽 밖 항목"));
    auto* table = dialog.findChild<QTableWidget*>(QStringLiteral("topographicFiles")); QVERIFY(table);
    QCOMPARE(table->rowCount(), 1); QCOMPARE(table->item(0, 0)->checkState(), Qt::Unchecked);
    QVERIFY(table->item(0, 0)->toolTip().contains(QStringLiteral("공식 도곽 밖 항목")));
    QVERIFY(table->item(0, 0)->text().contains(QStringLiteral("검토 필요")));
    QVERIFY(!topographic());
    dialog.show(); QCoreApplication::processEvents();
    QDir().mkpath(QStringLiteral("build/qa/topographic"));
    QVERIFY(dialog.grab().save(QStringLiteral("build/qa/topographic/review-inbox.png")));
    // Even a later automated refresh containing the same key cannot approve it.
    QSignalSpy changed(&dialog, &KaTopographicImportDialog::referenceLayersChanged);
    QVERIFY(dialog.importVerified(records)); QTRY_VERIFY(!changed.isEmpty()); QVERIFY(!topographic());
    QCOMPARE(table->item(0, 0)->checkState(), Qt::Unchecked);
    dialog.retainForReview(records, QStringLiteral("공식 도곽 밖 항목")); QCOMPARE(table->rowCount(), 1);
    table->item(0, 0)->setCheckState(Qt::Checked);
    auto* add = dialog.findChild<QPushButton*>(QStringLiteral("importTopographic")); QVERIFY(add);
    add->click(); QTRY_VERIFY(topographic());
    // Automated review refresh must not undo a completed manual approval.
    dialog.retainForReview(records, QStringLiteral("공식 도곽 밖 항목"));
    changed.clear(); QVERIFY(dialog.importVerified(records)); QTRY_VERIFY(!changed.isEmpty());
    QCOMPARE(QgsProject::instance()->mapLayers().size(), 1);
    canvas.stopRendering(); canvas.setLayers({}); QgsProject::instance()->clear();
    auto invalid = records; invalid[0].hasExtent = false;
    dialog.retainForReview(invalid, QStringLiteral("도형 범위 없음"));
    QCOMPARE(table->rowCount(), 1); table->item(0, 0)->setCheckState(Qt::Checked); add->click();
    QVERIFY(!topographic());
    const auto* status = dialog.findChild<QLabel*>(QStringLiteral("topographicImportStatus")); QVERIFY(status);
    QVERIFY(status->text().contains(QStringLiteral("유효한 도형 범위")));
  }
  void deletedLayerStaysDeletedAcrossAutomaticCacheRefresh() {
    QTemporaryDir dir; QTemporaryDir cache;
    QVERIFY(fixture(dir.filePath(QStringLiteral("A0010000.shp")), true, 200000.));
    auto records = TopographicCatalog::scan(dir.path(), nullptr, cache.path()).records;
    QCOMPARE(records.size(), 1); records[0].sourceSheet = QStringLiteral("37701");
    const auto crs = QgsCoordinateReferenceSystem(records[0].crsWkt);
    QgsProject::instance()->setCrs(crs);
    QgsMapCanvas canvas; canvas.setDestinationCrs(crs);
    const QgsRectangle near(199900., 449900., 200200., 450200.);
    canvas.setExtent(near);
    KaTopographicImportDialog dialog(&canvas);
    QSignalSpy changed(&dialog, &KaTopographicImportDialog::referenceLayersChanged);
    QVERIFY(dialog.importVerified(records)); QTRY_VERIFY(topographic());
    canvas.stopRendering();
    QgsProject::instance()->removeMapLayer(topographic()); QVERIFY(!topographic());
    changed.clear(); QVERIFY(dialog.importVerified(records));
    QTRY_VERIFY(!changed.isEmpty()); QVERIFY(!topographic());
    canvas.setExtent(QgsRectangle(299900., 449900., 300200., 450200.));
    changed.clear(); QVERIFY(dialog.importVerified(records)); QTRY_VERIFY(!changed.isEmpty());
    canvas.setExtent(near);
    changed.clear(); QVERIFY(dialog.importVerified(records)); QTRY_VERIFY(!changed.isEmpty());
    QVERIFY(!topographic());
    // The explicit manual action opts back in for this source.
    auto* table = dialog.findChild<QTableWidget*>(QStringLiteral("topographicFiles")); QVERIFY(table);
    QCOMPARE(table->rowCount(), 1); table->item(0, 0)->setCheckState(Qt::Checked);
    auto* add = dialog.findChild<QPushButton*>(QStringLiteral("importTopographic")); QVERIFY(add);
    add->click(); QTRY_VERIFY(topographic());
    canvas.stopRendering(); QgsProject::instance()->removeMapLayer(topographic());
    QVERIFY(!topographic());
    // Deletion choices belong to the current investigation, not all new projects.
    QgsProject::instance()->clear(); QgsProject::instance()->setCrs(crs); canvas.setExtent(near);
    QVERIFY(dialog.importVerified(records)); QTRY_VERIFY(topographic());
    canvas.stopRendering(); canvas.setLayers({}); QgsProject::instance()->clear();
  }
  void reopenedProjectLayersAreAdoptedWithoutDuplicatesOrStyleChanges() {
    QTemporaryDir dir; QTemporaryDir cache;
    const auto path = dir.filePath(QStringLiteral("A0010000.shp"));
    QVERIFY(fixture(path, true, 200000.));
    auto records = TopographicCatalog::scan(dir.path(), nullptr, cache.path()).records;
    QCOMPARE(records.size(), 1); records[0].sourceSheet = QStringLiteral("37701");
    auto* project = QgsProject::instance();
    const auto crs = QgsCoordinateReferenceSystem(records[0].crsWkt); project->setCrs(crs);
    QgsMapCanvas canvas; canvas.setDestinationCrs(crs);
    const QgsRectangle near(199900., 449900., 200200., 450200.); canvas.setExtent(near);
    const auto saved = dir.filePath(QStringLiteral("reopen.qgz"));
    {
      KaTopographicImportDialog first(&canvas);
      QVERIFY(first.importVerified(records)); QTRY_VERIFY(topographic());
      auto* layer = qobject_cast<QgsVectorLayer*>(topographic()); QVERIFY(layer);
      auto* renderer = dynamic_cast<QgsSingleSymbolRenderer*>(layer->renderer()); QVERIFY(renderer);
      renderer->symbol()->setColor(QColor(15, 205, 125)); layer->setOpacity(.42);
      project->layerTreeRoot()->findLayer(layer)->setItemVisibilityChecked(false);
      QVERIFY(project->write(saved));
      canvas.stopRendering(); canvas.setLayers({}); project->clear();
    }
    QVERIFY(project->read(saved));
    QCOMPARE(project->mapLayers().size(), 1); QVERIFY(topographic());
    const QString id = topographic()->id();
    KaTopographicImportDialog reopened(&canvas);
    QSignalSpy changed(&reopened, &KaTopographicImportDialog::referenceLayersChanged);
    QVERIFY(reopened.importVerified(records)); QTRY_VERIFY(!changed.isEmpty());
    QCOMPARE(project->mapLayers().size(), 1); QCOMPARE(topographic()->id(), id);
    auto checkStyle = [&] {
      auto* layer = qobject_cast<QgsVectorLayer*>(topographic()); QVERIFY(layer);
      auto* renderer = dynamic_cast<QgsSingleSymbolRenderer*>(layer->renderer()); QVERIFY(renderer);
      QCOMPARE(renderer->symbol()->color(), QColor(15, 205, 125)); QCOMPARE(layer->opacity(), .42);
      QVERIFY(!project->layerTreeRoot()->findLayer(layer)->itemVisibilityChecked());
    };
    checkStyle();
    canvas.setExtent(QgsRectangle(299900., 449900., 300200., 450200.));
    QTRY_VERIFY(topographic());
    QCOMPARE(project->mapLayers().size(), 1);
    checkStyle();
    canvas.setExtent(near); QTRY_VERIFY(topographic()); QCOMPARE(project->mapLayers().size(), 1);
    checkStyle();
    canvas.stopRendering(); canvas.setLayers({}); project->clear();
  }
  void verifiedDxfSeparatesCodesAndLabelsOnlyLevelIndexContours() {
    QTemporaryDir dir; QTemporaryDir cache;
    const QString path = dir.filePath(QStringLiteral("37701.dxf"));
    auto* driver = GetGDALDriverManager()->GetDriverByName("DXF"); QVERIFY(driver);
    {
      std::unique_ptr<GDALDataset, decltype(&GDALClose)> ds(driver->Create(path.toUtf8().constData(), 0, 0, 0, GDT_Unknown, nullptr), GDALClose);
      QVERIFY(ds); auto* layer = ds->CreateLayer("entities", nullptr, wkbUnknown, nullptr); QVERIFY(layer);
      for (int i = 0; i < 8; ++i) {
        std::unique_ptr<OGRFeature, decltype(&OGRFeature::DestroyFeature)> feature(OGRFeature::CreateFeature(layer->GetLayerDefn()), OGRFeature::DestroyFeature);
        feature->SetField("Layer", i < 4 || i == 7 ? "F0017111" : i == 4 ? "A0013111" : i == 5 ? "F0027217" : "unknown_code");
        OGRLineString line;
        const double y = 450000. + 15. * i;
        if (i == 3) { line.addPoint(200000., y); line.addPoint(200100., y + 10.); }
        else { line.addPoint(200000., y, i == 1 ? 60. : 50.); line.addPoint(200100., y + 10., i == 1 ? 60. : i == 2 ? 55. : 50.); }
        OGRPoint point(200050., y, 75.);
        QCOMPARE(feature->SetGeometry(i == 5 || i == 7 ? static_cast<OGRGeometry*>(&point) : static_cast<OGRGeometry*>(&line)), OGRERR_NONE);
        QCOMPARE(layer->CreateFeature(feature.get()), OGRERR_NONE);
      }
    }
    const auto original = digest(path);
    auto result = TopographicCatalog::scan(dir.path(), nullptr, cache.path());
    QVERIFY(result.error.isEmpty()); QCOMPARE(result.records.size(), 5);
    QgsProject::instance()->setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")));
    QgsMapCanvas canvas; canvas.resize(850, 600); canvas.show();
    canvas.setDestinationCrs(QgsProject::instance()->crs());
    canvas.setExtent(QgsRectangle(199980., 449980., 200120., 450120.));
    KaTopographicImportDialog dialog(&canvas);
    QString error;
    QVERIFY(!dialog.importVerified(result.records, &error)); QVERIFY(!error.isEmpty());
    QVERIFY(!topographic());
    for (auto& record : result.records) {
      record.crsWkt = QgsProject::instance()->crs().toWkt(); record.sourceSheet = QStringLiteral("37701");
    }
    QVERIFY2(dialog.importVerified(result.records, &error), qPrintable(error));
    QVERIFY(error.isEmpty());
    // 수치지형도는 밑그림이다. 코드별로 레이어를 가르지 않고 **한 레이어**로 합친다.
    // ("하나로 보이기만 하면 된다" — 2026-09-10 사용자 지시)
    QTRY_COMPARE_WITH_TIMEOUT(QgsProject::instance()->mapLayers().size(), 1, 10000);
    auto* layer = qobject_cast<QgsVectorLayer*>(QgsProject::instance()->mapLayers().values().first());
    QVERIFY(layer);
    QCOMPARE(layer->name(), QStringLiteral("수치지형도"));
    QVERIFY(layer->property("readOnly").toBool()); QVERIFY(!layer->startEditing());
    QCOMPARE(layer->crs().authid(), QStringLiteral("EPSG:5186"));
    QVERIFY(LayerOps::isReferenceLayer(layer));
    // 선만 남긴다. 점·면은 밑그림에 필요 없다.
    QCOMPARE(layer->geometryType(), Qgis::GeometryType::Line);
    auto* node = QgsProject::instance()->layerTreeRoot()->findLayer(layer); QVERIFY(node);
    QVERIFY(node->itemVisibilityChecked());
    // 회색 0.2 선. 밑그림 그 이상은 아니다.
    auto* renderer = dynamic_cast<QgsSingleSymbolRenderer*>(layer->renderer()); QVERIFY(renderer);
    QCOMPARE(renderer->symbol()->color(), QColor(128, 128, 128));
    QCOMPARE(renderer->symbol()->symbolLayer(0)->properties().value(QStringLiteral("line_width")).toDouble(), .2);
    // 도엽별 원본 경로는 합쳐진 뒤에도 남아야 한다. 어디서 왔는지 모르면 안 된다.
    QVERIFY(!layer->customProperty(QStringLiteral("ka_hgis/topographic_sources")).toStringList().isEmpty());

    // 글자는 넣지 않는다. 밑그림이라 회색 선만 있으면 된다
    // ("그 이상의 정보는 포함 안 되어도 된다" — 2026-09-10 사용자 지시).
    // 지형도 글자가 조사 도형의 글자를 덮으면 안 되는 이유도 있다.
    QVERIFY2(!layer->labelsEnabled(), "밑그림에 글자가 켜져 있다");

    int featuresRead = 0;
    QSet<QString> codes;
    auto it = layer->getFeatures(); QgsFeature feature;
    while (it.nextFeature(feature)) {
      ++featuresRead;
      QCOMPARE(feature.geometry().type(), Qgis::GeometryType::Line);
      codes.insert(feature.attribute(QStringLiteral("Layer")).toString());
    }
    QVERIFY2(featuresRead > 0, "합친 레이어가 비어 있다");
    QVERIFY2(codes.contains(QStringLiteral("F0017111")), qPrintable(QStringList(codes.values()).join(QLatin1Char(','))));
    auto* table = dialog.findChild<QTableWidget*>(QStringLiteral("topographicFiles")); QVERIFY(table);
    QCOMPARE(table->rowCount(), 5);
    // A default-off code stays available and an explicit user choice enables it.
    for (int row = 0; row < table->rowCount(); ++row) table->item(row, 0)->setCheckState(Qt::Checked);
    auto* add = dialog.findChild<QPushButton*>(QStringLiteral("importTopographic")); QVERIFY(add); add->click();
    for (auto* raw : QgsProject::instance()->mapLayers())
      QVERIFY(QgsProject::instance()->layerTreeRoot()->findLayer(raw)->itemVisibilityChecked());
    QDir().mkpath(QStringLiteral("build/qa/topographic"));
    canvas.refresh(); QTest::qWait(100); canvas.grab().save(QStringLiteral("build/qa/topographic/dxf-level-contours.png"));
    QCOMPARE(digest(path), original);
    canvas.stopRendering(); canvas.setLayers({}); QgsProject::instance()->clear();
  }
  void confirmCrsPanDeleteReaddAndCorrectCrs() {
    QTemporaryDir dir; QVERIFY(dir.isValid());
    const auto first = dir.filePath(QStringLiteral("A0010000_near.shp"));
    const auto second = dir.filePath(QStringLiteral("A0010000_far.shp"));
    QVERIFY(fixture(first, false, 200000.)); QVERIFY(fixture(second, true, 300000.));
    const auto firstHash = digest(first), secondHash = digest(second);
    QgsProject::instance()->setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")));
    QgsMapCanvas canvas; canvas.resize(850, 600); canvas.show();
    canvas.setDestinationCrs(QgsProject::instance()->crs());
    const QgsRectangle nearExtent(199900., 449900., 200200., 450200.);
    canvas.setExtent(nearExtent);
    KaTopographicImportDialog dialog(&canvas); dialog.scanFolder(dir.path());
    auto* table = dialog.findChild<QTableWidget*>(QStringLiteral("topographicFiles")); QVERIFY(table);
    auto* add = dialog.findChild<QPushButton*>(QStringLiteral("importTopographic")); QVERIFY(add);
    QTRY_COMPARE_WITH_TIMEOUT(table->rowCount(), 2, 15000);
    QTRY_VERIFY(add->isEnabled());
    int unknownRow = -1;
    for (int row = 0; row < table->rowCount(); ++row) {
      table->item(row, 0)->setCheckState(Qt::Checked);
      if (table->item(row, 1)->text().contains(QStringLiteral("near"))) unknownRow = row;
    }
    QVERIFY(unknownRow >= 0);
    add->click(); // Ambiguous CRS must not silently add even the other selected sheet.
    QVERIFY(!topographic());
    auto* choice = qobject_cast<QComboBox*>(table->cellWidget(unknownRow, 2)); QVERIFY(choice);
    for (int i = 0; i < choice->count(); ++i)
      if (QgsCoordinateReferenceSystem(choice->itemData(i).toString()).authid() == QStringLiteral("EPSG:5186")) choice->setCurrentIndex(i);
    add->click();
    QTRY_VERIFY_WITH_TIMEOUT(topographic(), 10000);
    QCOMPARE(topographic()->crs().authid(), QStringLiteral("EPSG:5186"));
    QVERIFY(LayerOps::isReferenceLayer(topographic()));
    QVERIFY(topographic()->property("readOnly").toBool());
    QVERIFY(topoMentions(topographic(), QStringLiteral("near")));
    canvas.setExtent(QgsRectangle(299900., 449900., 300200., 450200.));
    QTRY_VERIFY_WITH_TIMEOUT(topographic() && topoMentions(topographic(), QStringLiteral("far")), 10000);
    QVERIFY(topoMentions(topographic(), QStringLiteral("near")));
    canvas.setExtent(nearExtent);
    QTRY_VERIFY_WITH_TIMEOUT(topographic() && topoMentions(topographic(), QStringLiteral("near")), 10000);
    QgsProject::instance()->removeMapLayer(topographic());
    QVERIFY(!topographic());
    add->click();
    QTRY_VERIFY_WITH_TIMEOUT(topographic(), 10000);
    // A corrected CRS must replace the existing in-memory record and layer.
    for (int i = 0; i < choice->count(); ++i)
      if (QgsCoordinateReferenceSystem(choice->itemData(i).toString()).authid() == QStringLiteral("EPSG:5187")) choice->setCurrentIndex(i);
    canvas.setDestinationCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5187")));
    canvas.setExtent(nearExtent); add->click();
    QTRY_VERIFY_WITH_TIMEOUT(topographic() && topographic()->crs().authid() == QStringLiteral("EPSG:5187"), 10000);
    dialog.setMapsEnabled(false);
    QVERIFY(topographic());
    QVERIFY(!QgsProject::instance()->layerTreeRoot()->findLayer(topographic())->itemVisibilityChecked());
    dialog.setMapsEnabled(true);
    QTRY_VERIFY(topographic());
    QVERIFY(QgsProject::instance()->layerTreeRoot()->findLayer(topographic())->itemVisibilityChecked());
    QDir().mkpath(QStringLiteral("build/qa/topographic"));
    dialog.grab().save(QStringLiteral("build/qa/topographic/crs-confirmation.png"));
    QCOMPARE(digest(first), firstHash); QCOMPARE(digest(second), secondHash);
    canvas.stopRendering(); canvas.setLayers({}); QgsProject::instance()->clear();
  }
  void overviewScaleLoadsBoundaryAndKeepsLayerWhenPannedAway() {
    QTemporaryDir dir; QTemporaryDir cache;
    QVERIFY(fixture(dir.filePath(QStringLiteral("B0010000.shp")), true, 200000.));
    QVERIFY(fixture(dir.filePath(QStringLiteral("G0010000.shp")), true, 200000.));
    auto records = TopographicCatalog::scan(dir.path(), nullptr, cache.path()).records;
    QCOMPARE(records.size(), 2);
    int buildings = 0, boundaries = 0;
    for (auto& record : records) {
      record.sourceSheet = QStringLiteral("37701");
      if (record.source.contains(QLatin1String("B0010000"))) {
        record.category = TopographicCatalog::Category::Building;
        ++buildings;
      } else {
        record.category = TopographicCatalog::Category::Boundary;
        ++boundaries;
      }
    }
    QCOMPARE(buildings, 1); QCOMPARE(boundaries, 1);
    auto* project = QgsProject::instance();
    const auto crs = QgsCoordinateReferenceSystem(records.first().crsWkt);
    project->setCrs(crs);
    QgsMapCanvas canvas; canvas.resize(800, 600); canvas.show();
    canvas.setDestinationCrs(crs);
    canvas.setExtent(QgsRectangle(165000., 415000., 235000., 485000.));
    QVERIFY2(std::isfinite(canvas.scale()) && canvas.scale() > TopographicCatalog::kDetailMaxScale,
             qPrintable(QString::number(canvas.scale(), 'f', 2)));
    KaTopographicImportDialog dialog(&canvas);
    QVERIFY(dialog.importVerified(records));
    QTRY_VERIFY_WITH_TIMEOUT(!dialog.isAutomaticLoading(), 10000);
    // 수치지형도는 한 레이어로 합쳐진다. 도엽별 원본 경로는 레이어 source 가 아니라
    // ka_hgis/topographic_sources 커스텀 속성에 남는다. topoMentions 는 둘 다 본다.
    auto countContaining = [](const char* token) {
      return anyTopoMentions(QString::fromLatin1(token)) ? 1 : 0;
    };
    QCOMPARE(countContaining("B0010000"), 1);
    QCOMPARE(countContaining("G0010000"), 1);
    const QgsRectangle near(199900., 449900., 200200., 450200.);
    canvas.setExtent(near);
    QVERIFY2(canvas.scale() <= TopographicCatalog::kDetailMaxScale,
             qPrintable(QString::number(canvas.scale(), 'f', 2)));
    QTRY_COMPARE_WITH_TIMEOUT(countContaining("B0010000"), 1, 10000);
    QCOMPARE(countContaining("G0010000"), 1);
    canvas.setExtent(QgsRectangle(299900., 449900., 300200., 450200.));
    QTRY_VERIFY_WITH_TIMEOUT(topographic(), 10000);
    QCOMPARE(countContaining("B0010000"), 1);
    QCOMPARE(countContaining("G0010000"), 1);
    canvas.stopRendering(); canvas.setLayers({}); project->clear();
  }
  void scaleBecomingValidUnloadsDetailWithoutMovingExtent() {
    QTemporaryDir dir; QTemporaryDir cache;
    QVERIFY(fixture(dir.filePath(QStringLiteral("B0010000.shp")), true, 200000.));
    QVERIFY(fixture(dir.filePath(QStringLiteral("G0010000.shp")), true, 200000.));
    auto records = TopographicCatalog::scan(dir.path(), nullptr, cache.path()).records;
    QCOMPARE(records.size(), 2);
    for (auto& record : records) {
      record.sourceSheet = QStringLiteral("37701");
      record.category = record.source.contains(QLatin1String("B0010000"))
          ? TopographicCatalog::Category::Building : TopographicCatalog::Category::Boundary;
    }
    auto* project = QgsProject::instance();
    const auto crs = QgsCoordinateReferenceSystem(records.first().crsWkt);
    project->setCrs(crs);
    QgsMapCanvas canvas; canvas.resize(800, 600); canvas.show();
    canvas.setDestinationCrs(crs);
    const QgsRectangle near(199900., 449900., 200200., 450200.);
    canvas.setExtent(near);
    QVERIFY2(std::isfinite(canvas.scale()) && canvas.scale() <= TopographicCatalog::kDetailMaxScale,
             qPrintable(QString::number(canvas.scale(), 'f', 2)));
    KaTopographicImportDialog dialog(&canvas);
    QVERIFY(dialog.importVerified(records));
    QTRY_VERIFY_WITH_TIMEOUT(!dialog.isAutomaticLoading(), 10000);
    // 수치지형도는 한 레이어로 합쳐진다. 도엽별 원본 경로는 레이어 source 가 아니라
    // ka_hgis/topographic_sources 커스텀 속성에 남는다. topoMentions 는 둘 다 본다.
    auto countContaining = [](const char* token) {
      return anyTopoMentions(QString::fromLatin1(token)) ? 1 : 0;
    };
    QCOMPARE(countContaining("B0010000"), 1);
    QCOMPARE(countContaining("G0010000"), 1);
    const auto kept = canvas.extent();
    canvas.resize(40, 30);
    QCOMPARE(canvas.extent().center().x(), kept.center().x());
    QCOMPARE(canvas.extent().center().y(), kept.center().y());
    QVERIFY2(std::isfinite(canvas.scale()) && canvas.scale() > TopographicCatalog::kDetailMaxScale,
             qPrintable(QString::number(canvas.scale(), 'f', 2)));
    QTRY_COMPARE_WITH_TIMEOUT(countContaining("B0010000"), 1, 10000);
    QCOMPARE(countContaining("G0010000"), 1);
    canvas.stopRendering(); canvas.setLayers({}); project->clear();
  }
  void surveyZoomOmitsVegetationAndKeepsBuildings() {
    QTemporaryDir dir; QTemporaryDir cache;
    QVERIFY(fixture(dir.filePath(QStringLiteral("B0010000.shp")), true, 200000.));
    QVERIFY(fixture(dir.filePath(QStringLiteral("D0010000.shp")), true, 200000.));
    auto records = TopographicCatalog::scan(dir.path(), nullptr, cache.path()).records;
    QCOMPARE(records.size(), 2);
    for (auto& record : records) {
      record.sourceSheet = QStringLiteral("37701");
      record.category = record.source.contains(QLatin1String("B0010000"))
          ? TopographicCatalog::Category::Building : TopographicCatalog::Category::Vegetation;
    }
    auto* project = QgsProject::instance();
    const auto crs = QgsCoordinateReferenceSystem(records.first().crsWkt);
    project->setCrs(crs);
    QgsMapCanvas canvas; canvas.resize(800, 600); canvas.show();
    canvas.setDestinationCrs(crs);
    canvas.setExtent(QgsRectangle(199900., 449900., 200200., 450200.));
    QVERIFY2(std::isfinite(canvas.scale()) && canvas.scale() <= TopographicCatalog::kDetailMaxScale,
             qPrintable(QString::number(canvas.scale(), 'f', 2)));
    KaTopographicImportDialog dialog(&canvas);
    QVERIFY(dialog.importVerified(records));
    QTRY_VERIFY_WITH_TIMEOUT(!dialog.isAutomaticLoading(), 10000);
    // 수치지형도는 한 레이어로 합쳐진다. 도엽별 원본 경로는 레이어 source 가 아니라
    // ka_hgis/topographic_sources 커스텀 속성에 남는다. topoMentions 는 둘 다 본다.
    auto countContaining = [](const char* token) {
      return anyTopoMentions(QString::fromLatin1(token)) ? 1 : 0;
    };
    QCOMPARE(countContaining("B0010000"), 1);
    QCOMPARE(countContaining("D0010000"), 0);
    canvas.stopRendering(); canvas.setLayers({}); project->clear();
  }
  void fiveKilometerCoverageLoadsNeighborSheetNotFarSheet() {
    QTemporaryDir dir; QTemporaryDir cache;
    QVERIFY(fixture(dir.filePath(QStringLiteral("A0010000_near.shp")), true, 200000.));
    QVERIFY(fixture(dir.filePath(QStringLiteral("A0010000_mid.shp")), true, 204000.));
    QVERIFY(fixture(dir.filePath(QStringLiteral("A0010000_far.shp")), true, 220000.));
    auto records = TopographicCatalog::scan(dir.path(), nullptr, cache.path()).records;
    QCOMPARE(records.size(), 3);
    for (auto& record : records) record.sourceSheet = QStringLiteral("37701");
    auto* project = QgsProject::instance();
    const auto crs = QgsCoordinateReferenceSystem(records.first().crsWkt);
    project->setCrs(crs);
    QgsMapCanvas canvas; canvas.resize(800, 600); canvas.show();
    canvas.setDestinationCrs(crs);
    canvas.setExtent(QgsRectangle(199900., 449900., 200200., 450200.));
    QVERIFY2(std::isfinite(canvas.scale()) && canvas.scale() <= TopographicCatalog::kDetailMaxScale,
             qPrintable(QString::number(canvas.scale(), 'f', 2)));
    KaTopographicImportDialog dialog(&canvas);
    QVERIFY(dialog.importVerified(records));
    QTRY_VERIFY_WITH_TIMEOUT(!dialog.isAutomaticLoading(), 10000);
    QVERIFY(anyTopoMentions(QStringLiteral("near")));
    QVERIFY(anyTopoMentions(QStringLiteral("mid")));
    QVERIFY(!anyTopoMentions(QStringLiteral("far")));
    int topoLayers = 0;
    for (auto* layer : QgsProject::instance()->mapLayers()) {
      if (!layer->customProperty(QStringLiteral("ka_hgis/topographic_source")).toString().isEmpty())
        ++topoLayers;
    }
    QCOMPARE(topoLayers, 1);
    canvas.setExtent(QgsRectangle(299900., 449900., 300200., 450200.));
    QTRY_VERIFY_WITH_TIMEOUT(topographic(), 10000);
    QVERIFY(anyTopoMentions(QStringLiteral("near")));
    QVERIFY(anyTopoMentions(QStringLiteral("mid")));
    canvas.stopRendering(); canvas.setLayers({}); project->clear();
  }
  void panDoesNotReopenPublishedOrDiscardedSources() {
    QTemporaryDir dir; QTemporaryDir cache;
    QVERIFY(fixture(dir.filePath(QStringLiteral("A0010000.shp")), true, 200000.));
    QVERIFY(polygonFixture(dir.filePath(QStringLiteral("B0010000.shp"))));
    auto records = TopographicCatalog::scan(dir.path(), nullptr, cache.path()).records;
    QCOMPARE(records.size(), 2);
    for (auto& record : records) record.sourceSheet = QStringLiteral("37701");
    const auto crs = QgsCoordinateReferenceSystem(records.first().crsWkt);
    QgsProject::instance()->setCrs(crs);
    QgsMapCanvas canvas; canvas.setDestinationCrs(crs);
    canvas.setExtent(QgsRectangle(199900., 449900., 200200., 450200.));
    KaTopographicImportDialog dialog(&canvas);
    QVERIFY(dialog.importVerified(records));
    QTRY_VERIFY_WITH_TIMEOUT(!dialog.isAutomaticLoading() && topographic(), 10000);
    QCOMPARE(QgsProject::instance()->mapLayers().size(), 1);
    const QString id = topographic()->id();
    QSignalSpy loading(&dialog, &KaTopographicImportDialog::automaticLoadingChanged);
    QSignalSpy changed(&dialog, &KaTopographicImportDialog::referenceLayersChanged);
    canvas.setExtent(QgsRectangle(199950., 449950., 200250., 450250.));
    QTest::qWait(700);
    QCoreApplication::processEvents();
    QCOMPARE(loading.size(), 0);
    QCOMPARE(changed.size(), 0);
    QCOMPARE(QgsProject::instance()->mapLayers().size(), 1);
    QCOMPARE(topographic()->id(), id);
    QVERIFY(!dialog.isAutomaticLoading());
    canvas.setExtent(QgsRectangle(200020., 450020., 200320., 450320.));
    QTest::qWait(700);
    QCOMPARE(loading.size(), 0);
    QCOMPARE(topographic()->id(), id);
    canvas.stopRendering(); canvas.setLayers({}); QgsProject::instance()->clear();
  }
  void mergedSheetsStayInSurveyFolderAndAppendNewSheets() {
    QTemporaryDir first, second, cache, survey;
    QVERIFY(fixture(first.filePath(QStringLiteral("A0010000_1.shp")), true, 200000.));
    QVERIFY(fixture(first.filePath(QStringLiteral("A0010000_2.shp")), true, 200050.));
    QVERIFY(fixture(second.filePath(QStringLiteral("A0010000_3.shp")), true, 200100.));
    auto records = TopographicCatalog::scan(first.path(), nullptr, cache.path()).records;
    auto later = TopographicCatalog::scan(second.path(), nullptr, cache.path()).records;
    QCOMPARE(records.size(), 2); QCOMPARE(later.size(), 1);
    for (auto& record : records) record.sourceSheet = QStringLiteral("37701");
    later[0].sourceSheet = QStringLiteral("37702");
    const QgsCoordinateReferenceSystem crs(records.first().crsWkt);
    auto* project = QgsProject::instance(); project->setCrs(crs);
    QgsMapCanvas canvas; canvas.setDestinationCrs(crs);
    canvas.setExtent(QgsRectangle(199900., 449900., 200300., 450300.));
    KaTopographicImportDialog dialog(&canvas);
    const QString mergeFolder = QDir(survey.path()).filePath(QStringLiteral("지형도/합침"));
    dialog.setMergeDirectory(mergeFolder);
    QVERIFY(dialog.importVerified(records));
    QTRY_VERIFY_WITH_TIMEOUT(!dialog.isAutomaticLoading() && topographic(), 10000);
    auto* merged = qobject_cast<QgsVectorLayer*>(topographic()); QVERIFY(merged);
    const QString mergedFile = merged->source().section(QLatin1Char('|'), 0, 0);
    // Never the process working directory or a source folder: the survey's own folder.
    QCOMPARE(QFileInfo(mergedFile).absolutePath(), QDir(mergeFolder).absolutePath());
    QCOMPARE(merged->featureCount(), 2);
    const QString id = merged->id();
    const QByteArray firstDigest = digest(first.filePath(QStringLiteral("A0010000_1.shp")));
    // A newly received sheet is appended to the same published file.
    QVERIFY(dialog.importVerified(later));
    QTRY_VERIFY_WITH_TIMEOUT(!dialog.isAutomaticLoading() && topographic() &&
                             qobject_cast<QgsVectorLayer*>(topographic())->featureCount() == 3, 10000);
    QCOMPARE(topographic()->id(), id);
    QCOMPARE(topographic()->source().section(QLatin1Char('|'), 0, 0), mergedFile);
    QCOMPARE(project->mapLayers().size(), 1);
    QCOMPARE(topographic()->customProperty(QStringLiteral("ka_hgis/topographic_source_keys")).toStringList().size(), 3);
    QCOMPARE(digest(first.filePath(QStringLiteral("A0010000_1.shp"))), firstDigest);  // originals untouched
    canvas.stopRendering(); canvas.setLayers({}); project->clear();
  }
  void destroyedDuringScanDoesNotAddLayers() {
    QTemporaryDir dir;
    QVERIFY(fixture(dir.filePath(QStringLiteral("road.shp")), true, 200000.));
    QgsMapCanvas canvas;
    auto* dialog = new KaTopographicImportDialog(&canvas);
    dialog->scanFolder(dir.path()); delete dialog;
    QCoreApplication::processEvents();
    QVERIFY(!topographic());
  }
};
int main(int argc, char** argv) {
  QgsApplication app(argc, argv, true);
  QFontDatabase::addApplicationFont(QStringLiteral("C:/Windows/Fonts/malgun.ttf"));
  app.setFont(QFont(QStringLiteral("Malgun Gothic"), 9));
  app.setOrganizationName(QStringLiteral("ka-hgis-tests"));
  app.setApplicationName(QStringLiteral("topographic-import"));
  QTemporaryDir settings;
  QSettings::setDefaultFormat(QSettings::IniFormat);
  QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settings.path());
  QgsApplication::setPrefixPath(qEnvironmentVariable("QGIS_PREFIX_PATH"), true);
  QgsApplication::initQgis(); GDALAllRegister();
  TopographicImportTest test; const int result = QTest::qExec(&test, argc, argv);
  QgsApplication::taskManager()->cancelAll();
  QgsApplication::exitQgis(); return result;
}
#include "test_topographic_import.moc"
