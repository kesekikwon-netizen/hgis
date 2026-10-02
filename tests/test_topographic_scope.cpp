#include <QtTest>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QDir>
#include <QFontDatabase>
#include <QLabel>
#include <QTemporaryDir>
#include <QSettings>
#include <QSignalSpy>
#include <QThread>
#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <QGraphicsScene>
#include <QJsonDocument>
#include <QJsonArray>
#include <QElapsedTimer>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <gdal_priv.h>
#include <ogrsf_frmts.h>
#include <qgsapplication.h>
#include <qgscoordinatetransform.h>
#include <qgsmapcanvas.h>
#include <qgsproject.h>
#include "app/KaTopographicScopePanel.h"
#include "core/KaPortableRuntime.h"
#include "app/KaTopographicImportDialog.h"
#include "core/LayerOps.h"
#include "core/TopographicSheets.h"
#include "core/TopographicSourceCrs.h"
#include <qgsrubberband.h>
#include <qgsvectorlayer.h>
#include <qgsvectordataprovider.h>
#include <qgslayertree.h>
#include <qgslayertreemodel.h>
#include <qgslayertreeview.h>
#include <qgssinglesymbolrenderer.h>
#include <qgssymbol.h>
#include <qgslabelingresults.h>
#include <memory>
#include <vector>

namespace {
QJsonObject doduMetadata() {
  return {{"source","NGII suchiQuery.do"},{"num","336062"},{"name",QStringLiteral("도두")},
    {"scale","25000"},{"fileExt","dxf"},{"projection","GRS80"},{"makeYer","2024"},
    {"minx",895501.3086},{"miny",1501351.6203},{"maxx",907246.5125},{"maxy",1515330.2946}};
}
bool createFrameDxf(const QString& path) {
  GDALAllRegister();
  auto* driver=GetGDALDriverManager()->GetDriverByName("DXF");
  if (!driver) return false;
  std::unique_ptr<GDALDataset,decltype(&GDALClose)> data(driver->Create(path.toUtf8().constData(),0,0,0,GDT_Unknown,nullptr),GDALClose);
  if (!data) return false;
  auto* layer=data->CreateLayer("entities",nullptr,wkbUnknown,nullptr);
  if (!layer) return false;
  std::unique_ptr<OGRFeature,decltype(&OGRFeature::DestroyFeature)> feature(OGRFeature::CreateFeature(layer->GetLayerDefn()),OGRFeature::DestroyFeature);
  feature->SetField("Layer","H0017334");
  OGRLineString frame;
  frame.addPoint(141939.66,103589.26); frame.addPoint(153618.79,103589.26);
  frame.addPoint(153618.79,117516.92); frame.addPoint(141939.66,117516.92); frame.addPoint(141939.66,103589.26);
  return feature->SetGeometry(&frame)==OGRERR_NONE && layer->CreateFeature(feature.get())==OGRERR_NONE;
}
QByteArray fileHash(const QString& path) {
  QFile input(path); if (!input.open(QIODevice::ReadOnly)) return {};
  QCryptographicHash hash(QCryptographicHash::Sha256);
  if (!hash.addData(&input)) return {};
  return hash.result().toHex();
}
}

class TopographicScopeTest : public QObject {
  Q_OBJECT
private slots:
  void cleanup() { QgsProject::instance()->clear(); }
  void currentViewportWinsOverExistingSurvey_data() {
    QTest::addColumn<QString>("workAuth");
    QTest::newRow("central") << QStringLiteral("EPSG:5186");
    QTest::newRow("east") << QStringLiteral("EPSG:5187");
  }
  void currentViewportWinsOverExistingSurvey() {
    QFETCH(QString, workAuth);
    QTemporaryDir library;
    auto* project = QgsProject::instance();
    const QgsCoordinateReferenceSystem work(workAuth);
    project->setCrs(work);
    QgsCoordinateTransform transform(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:4737")), work, project);
    const auto surveyCenter = transform.transform(QgsPointXY(126.3, 33.4));
    auto* survey = new QgsVectorLayer(QStringLiteral("Polygon?crs=%1").arg(workAuth), QStringLiteral("기존 조사구역"), QStringLiteral("memory"));
    QVERIFY(survey->isValid());
    survey->setCustomProperty(QStringLiteral("ka_hgis/layer_key"), QStringLiteral("survey_area"));
    QgsFeature feature(survey->fields());
    feature.setGeometry(QgsGeometry::fromRect(QgsRectangle(surveyCenter.x()-100, surveyCenter.y()-100, surveyCenter.x()+100, surveyCenter.y()+100)));
    QVERIFY(survey->dataProvider()->addFeature(feature));
    project->addMapLayer(survey);
    QgsMapCanvas canvas;
    canvas.resize(900,600); canvas.setDestinationCrs(work);
    canvas.setExtent(QgsRectangle(surveyCenter.x()-1000,surveyCenter.y()-1000,surveyCenter.x()+1000,surveyCenter.y()+1000));
    KaTopographicScopePanel panel(&canvas,nullptr,nullptr,nullptr,library.path());
    panel.refreshScope();
    auto* count=panel.findChild<QLabel*>(QStringLiteral("topographicSheetCount")); QVERIFY(count);
    for (const auto& position : {QgsPointXY(126.98,37.56), QgsPointXY(128.9,37.75), QgsPointXY(126.3,33.4)}) {
      const auto center=transform.transform(position);
      canvas.setExtent(QgsRectangle(center.x()-1000,center.y()-1000,center.x()+1000,center.y()+1000));
      const auto expected=TopographicSheets::select(center,work,5.,project->transformContext());
      QVERIFY2(expected.error.isEmpty(),qPrintable(expected.error));
      QStringList numbers; for(const auto& sheet:expected.sheets) numbers.append(sheet.number);
      // Exercise the real extent-change debounce, without calling refreshScope.
      QTRY_COMPARE_WITH_TIMEOUT(count->toolTip(),numbers.join(QStringLiteral(", ")),3000);
      bool centered=false;
      for(const auto* item:canvas.scene()->items()) {
        const auto* band=dynamic_cast<const QgsRubberBand*>(item); if(!band) continue;
        const auto box=band->asGeometry().boundingBox();
        centered |= qAbs(box.width()-10000.)<.001 && box.center().distance(center)<.001;
      }
      QVERIFY(centered);
      QCOMPARE(survey->featureCount(),1);
    }
  }
  void sameSheetSetDoesNotRestartAfterSmallCenterWobble() {
    QTemporaryDir library;
    const QgsCoordinateReferenceSystem work(QStringLiteral("EPSG:5186"));
    auto* project=QgsProject::instance();
    project->setCrs(work);
    const QgsCoordinateTransform transform(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:4737")),
                                          work, project);
    const auto center=transform.transform(QgsPointXY(126.98,37.56));
    QgsMapCanvas canvas;
    canvas.resize(900,600); canvas.setDestinationCrs(work);
    canvas.setExtent(QgsRectangle(center.x()-1000,center.y()-1000,center.x()+1000,center.y()+1000));
    KaTopographicScopePanel panel(&canvas,nullptr,nullptr,nullptr,library.path());
    panel.refreshScope();
    auto* count=panel.findChild<QLabel*>(QStringLiteral("topographicSheetCount"));
    auto* status=panel.findChild<QLabel*>(QStringLiteral("topographicScopeStatus"));
    QVERIFY(count && status);
    QTRY_VERIFY_WITH_TIMEOUT(!count->toolTip().isEmpty(),3000);
    const QString sheets=count->toolTip();
    QSignalSpy statusChanged(&panel,&KaTopographicScopePanel::statusChanged);
    canvas.setExtent(QgsRectangle(center.x()-997,center.y()-1003,center.x()+1003,center.y()+997));
    QTest::qWait(600);
    QCOMPARE(count->toolTip(),sheets);
    QCOMPARE(statusChanged.size(),0);
  }
  void fixedFiveKilometresIgnoresPreviousTwentyKilometreSettings() {
    QSettings().setValue(QStringLiteral("topographic/radiusKm"), 20.);
    QTemporaryDir cache;
    QWidget evidenceHost;
    QgsMapCanvas canvas;
    const QgsCoordinateReferenceSystem work(QStringLiteral("EPSG:5187"));
    QgsProject::instance()->setCrs(work); canvas.setDestinationCrs(work);
    const QgsCoordinateTransform transform(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5179")), work, QgsProject::instance());
    const auto center = transform.transform(QgsPointXY(1125000, 1980000));
    canvas.setExtent(QgsRectangle(center.x()-1000, center.y()-1000, center.x()+1000, center.y()+1000));
    KaTopographicImportDialog importer(&canvas);
    KaTopographicScopePanel panel(&canvas, nullptr, &importer, nullptr, cache.path());
    auto* radius = panel.findChild<QLabel*>(QStringLiteral("topographicRadiusKm"));
    auto* count = panel.findChild<QLabel*>(QStringLiteral("topographicSheetCount"));
    QVERIFY(radius && count);
    QCOMPARE(radius->text(), QStringLiteral("5 km"));
    QVERIFY(panel.findChildren<QDoubleSpinBox*>().isEmpty());
    panel.refreshScope();
    const auto five=TopographicSheets::select(center,work,5.,QgsProject::instance()->transformContext());
    const auto twenty=TopographicSheets::select(center,work,20.,QgsProject::instance()->transformContext());
    QVERIFY2(five.error.isEmpty(),qPrintable(five.error));
    QVERIFY2(twenty.error.isEmpty(),qPrintable(twenty.error));
    QVERIFY(!five.sheets.isEmpty()); QVERIFY(five.sheets.size()<twenty.sheets.size());
    QCOMPARE(count->property("candidateCount").toInt(),five.sheets.size());
    QStringList wanted;
    for (const auto& sheet:five.sheets) wanted.append(sheet.number);
    QCOMPARE(count->toolTip(),wanted.join(QStringLiteral(", ")));
    bool foundFiveKilometreCircle=false;
    for (const auto* item:canvas.scene()->items()) {
      const auto* band=dynamic_cast<const QgsRubberBand*>(item);
      if (!band) continue;
      const auto bounds=band->asGeometry().boundingBox();
      if (qAbs(bounds.width()-10000.)<.001 && qAbs(bounds.height()-10000.)<.001 &&
          qAbs(bounds.center().x()-center.x())<.001 && qAbs(bounds.center().y()-center.y())<.001)
        foundFiveKilometreCircle=true;
    }
    QVERIFY(foundFiveKilometreCircle);
    QCOMPARE(QgsProject::instance()->mapLayers().size(), 0);
    const QString outputDirectory=qEnvironmentVariable("KA_HGIS_QA_OUTPUT_DIR");
    if (!outputDirectory.isEmpty()) {
      // The synthetic query/CRS remains unchanged. Frame its full search circle
      // for the opt-in pixel evidence instead of the initial 2 km map view.
      auto* layout=new QVBoxLayout(&evidenceHost);
      layout->addWidget(&panel);
      layout->addWidget(new QLabel(QStringLiteral("합성 검증 · EPSG:5187 · 5km 원과 예상 도엽 경계"),&evidenceHost));
      layout->addWidget(&canvas,1);
      std::vector<std::unique_ptr<QgsRubberBand>> sheetBoundaries;
      for (const auto& sheet:five.sheets) {
        auto band=std::make_unique<QgsRubberBand>(&canvas,Qgis::GeometryType::Polygon);
        band->setColor(QColor(120,120,120)); band->setFillColor(Qt::transparent); band->setWidth(1);
        band->setToGeometry(QgsGeometry::fromRect(sheet.geographicExtent).densifyByDistance(.001),
          QgsCoordinateReferenceSystem(QStringLiteral("EPSG:4737")));
        sheetBoundaries.push_back(std::move(band));
      }
      evidenceHost.setWindowTitle(QStringLiteral("수치지형도 반경 5km — 합성 범위 검증"));
      evidenceHost.resize(1000,850); evidenceHost.show();
      auto visible=five.searchArea.boundingBox(); visible.scale(1.12); canvas.setExtent(visible);
      canvas.refresh();
      QImage screenshot;
      const auto boundaryVisible=[&] {
        screenshot=evidenceHost.grab().toImage();
        const auto mapImage=canvas.grab().toImage();
        int bluePixels=0;
        for (int y=0;y<mapImage.height();++y) for (int x=0;x<mapImage.width();++x) {
          const auto color=mapImage.pixelColor(x,y);
          if (color.blue()>color.red()+30 && color.green()>color.red()+20 && color.blue()>color.green()) ++bluePixels;
        }
        return bluePixels>200;
      };
      QTRY_VERIFY_WITH_TIMEOUT(boundaryVisible(),10000);
      QVERIFY(QDir().mkpath(outputDirectory));
      QVERIFY(screenshot.save(QDir(outputDirectory).filePath(QStringLiteral("topographic-radius-5km.png"))));
    }
  }
  void receivedDxfIsValidatedAndLoadedWithoutAnImportDialog() {
    QTemporaryDir source, library;
    const QString path = source.filePath(QStringLiteral("378044.dxf"));
    const QgsCoordinateReferenceSystem dataCrs(QStringLiteral("EPSG:5187"));
    const QgsCoordinateReferenceSystem workCrs(QStringLiteral("EPSG:5186"));
    const auto context = QgsProject::instance()->transformContext();
    const auto point = QgsCoordinateTransform(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:4737")), dataCrs, context)
        .transform(QgsPointXY(128.9375, 37.8125));
    GDALAllRegister();
    auto* driver = GetGDALDriverManager()->GetDriverByName("DXF"); QVERIFY(driver);
    {
      std::unique_ptr<GDALDataset, decltype(&GDALClose)> dataset(driver->Create(path.toUtf8().constData(),0,0,0,GDT_Unknown,nullptr), GDALClose);
      QVERIFY(dataset);
      auto* layer = dataset->CreateLayer("entities", nullptr, wkbUnknown, nullptr); QVERIFY(layer);
      std::unique_ptr<OGRFeature, decltype(&OGRFeature::DestroyFeature)> feature(OGRFeature::CreateFeature(layer->GetLayerDefn()),OGRFeature::DestroyFeature);
      feature->SetField("Layer", "F0017111");
      OGRLineString line; line.addPoint(point.x(),point.y(),50.); line.addPoint(point.x()+100.,point.y()+50.,50.);
      QCOMPARE(feature->SetGeometry(&line), OGRERR_NONE); QCOMPARE(layer->CreateFeature(feature.get()), OGRERR_NONE);
    }
    QFile original(path); QVERIFY(original.open(QIODevice::ReadOnly));
    const auto hash = QCryptographicHash::hash(original.readAll(),QCryptographicHash::Sha256); original.close();
    QgsProject::instance()->setCrs(workCrs);
    QgsMapCanvas canvas; canvas.setDestinationCrs(workCrs);
    const auto center = QgsCoordinateTransform(dataCrs,workCrs,context).transform(point);
    canvas.setExtent(QgsRectangle(center.x()-500.,center.y()-500.,center.x()+500.,center.y()+500.));
    KaTopographicImportDialog importer(&canvas);
    {
      KaTopographicScopePanel panel(&canvas,nullptr,&importer,nullptr,library.path());
      QSignalSpy finished(&panel,&KaTopographicScopePanel::importFinished);
      QSignalSpy processing(&panel,&KaTopographicScopePanel::processingChanged);
      QSignalSpy attention(&panel,&KaTopographicScopePanel::attentionRequired);
      QSignalSpy phases(&panel,&KaTopographicScopePanel::preparationProgressChanged);
      bool phaseOutsideGui=false;
      connect(&panel,&KaTopographicScopePanel::preparationProgressChanged,&panel,[&] {
        phaseOutsideGui|=QThread::currentThread()!=panel.thread();
      },Qt::DirectConnection);
      const QJsonObject metadata{{"source","NGII suchiQuery.do"},{"num","378044"},{"name",QStringLiteral("강릉")},
        {"scale","25000"},{"fileExt","dxf"},{"projection","GRS80"},{"makeYer","2024"},
        {"minx",1120928.8369},{"miny",1973152.5162},{"maxx",1132145.6322},{"maxy",1987191.4655}};
      // The downloaded sheet waits behind the offline restore. The compact
      // progress indicator must stay busy across both operations.
      panel.refreshScope();
      panel.acceptDownload(path,metadata);
      QCOMPARE(processing.size(),1);
      QVERIFY(processing.first().at(0).toBool());
      QTRY_VERIFY_WITH_TIMEOUT(!finished.isEmpty(),20000);
      QVERIFY(finished.last().at(0).toBool());
      // The completion status restates the same 5 km sheet radius as the panel.
      auto* scopeStatus=panel.findChild<QLabel*>(QStringLiteral("topographicScopeStatus"));
      QVERIFY(scopeStatus);
      QVERIFY2(scopeStatus->text().contains(QStringLiteral("반경 5km")),qPrintable(scopeStatus->text()));
      QVERIFY(!scopeStatus->text().contains(QStringLiteral("10km")));
      QTRY_COMPARE_WITH_TIMEOUT(processing.size(),2,10000);
      QVERIFY(!processing.last().at(0).toBool());
      QVERIFY(attention.isEmpty());
      QVERIFY(!phaseOutsideGui);
      QVERIFY(!phases.isEmpty()); QVERIFY(phases.last().at(0).toString().isEmpty());
      QStringList observedPhases;
      for (const auto& event:phases) observedPhases.append(event.at(0).toString());
      QVERIFY(observedPhases.contains(QStringLiteral("보관 자료 확인")));
      QVERIFY(observedPhases.contains(QStringLiteral("좌표계 확인")));
      QVERIFY(observedPhases.contains(QStringLiteral("SHP 변환")));
      QVERIFY(observedPhases.contains(QStringLiteral("지도에 올리기")));
      QVERIFY(!observedPhases.contains(QStringLiteral("압축 해제"))); // This fixture is a plain DXF.
      QTRY_COMPARE_WITH_TIMEOUT(QgsProject::instance()->mapLayers().size(),1,10000);
      auto* layer = qobject_cast<QgsVectorLayer*>(QgsProject::instance()->mapLayers().cbegin().value()); QVERIFY(layer);
      QCOMPARE(layer->crs().authid(),QStringLiteral("EPSG:5187"));
      QVERIFY(LayerOps::isReferenceLayer(layer));
      QCOMPARE(QFileInfo(layer->source().section(QLatin1Char('|'),0,0)).suffix().toLower(), QStringLiteral("shp"));
      QCOMPARE(canvas.mapSettings().destinationCrs().authid(),QStringLiteral("EPSG:5186"));
      QVERIFY(!importer.isVisible());
      QVERIFY(original.open(QIODevice::ReadOnly));
      QCOMPARE(QCryptographicHash::hash(original.readAll(),QCryptographicHash::Sha256),hash); original.close();
      const auto completedLayers=QgsProject::instance()->mapLayers().keys();
      panel.stop();
      QVERIFY(!panel.isActive());
      QCOMPARE(processing.size(),2);
      QCOMPARE(QgsProject::instance()->mapLayers().keys(),completedLayers);
      QTemporaryDir nextSurvey;
      panel.setLibraryDirectory(nextSurvey.path());
      QVERIFY(!panel.isActive());
      QCOMPARE(QgsProject::instance()->mapLayers().keys(),completedLayers);
      QgsProject::instance()->clear();
    }
    // Reopening the explicit command reuses the verified local sheet offline.
    // The clean first preparation left a size/time snapshot beside its index,
    // so the kept sheet is neither re-hashed nor re-converted (F202).
    QCOMPARE(QDir(library.filePath(QStringLiteral("index"))).entryList({QStringLiteral("*.snapshot")},QDir::Files).size(),1);
    canvas.setDestinationCrs(workCrs);
    QgsProject::instance()->setCrs(workCrs);
    canvas.setExtent(QgsRectangle(center.x()-500.,center.y()-500.,center.x()+500.,center.y()+500.));
    {
      KaTopographicScopePanel reopened(&canvas,nullptr,&importer,nullptr,library.path());
      QSignalSpy reopenedPhases(&reopened,&KaTopographicScopePanel::preparationProgressChanged);
      QTRY_COMPARE_WITH_TIMEOUT(QgsProject::instance()->mapLayers().size(),1,10000);
      QVERIFY(!importer.isVisible());
      for (const auto& event:reopenedPhases) {
        QVERIFY(event.at(0).toString()!=QStringLiteral("좌표계 확인"));
        QVERIFY(event.at(0).toString()!=QStringLiteral("SHP 변환"));
      }
      QgsProject::instance()->clear();
    }
  }
  void reviewReceiptPreventsRepeatRequestsUntilItsSourceOrMetadataChanges_data() {
    QTest::addColumn<int>("change");
    QTest::newRow("unchanged-review")<<0;
    QTest::newRow("source-bytes-changed")<<1;
    QTest::newRow("official-metadata-changed")<<2;
    QTest::newRow("path-outside-library")<<3;
    QTest::newRow("different-sheet-with-current-hash")<<4;
  }
  void reviewReceiptPreventsRepeatRequestsUntilItsSourceOrMetadataChanges() {
    QFETCH(int,change);
    QTemporaryDir library, external;
    QVERIFY(QDir().mkpath(library.filePath(QStringLiteral("원본"))));
    const auto source=library.filePath(QStringLiteral("원본/336062.dxf"));
    QVERIFY(createFrameDxf(source));
    QFile original(source); QVERIFY(original.open(QIODevice::ReadOnly));
    const auto originalBytes=original.readAll(); original.close();
    const QgsCoordinateReferenceSystem work(QStringLiteral("EPSG:5186"));
    QgsProject::instance()->setCrs(work);
    QgsMapCanvas canvas; canvas.setDestinationCrs(work);
    const auto center=QgsCoordinateTransform(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5179")),work,QgsProject::instance())
      .transform(QgsPointXY(901000.,1505000.));
    canvas.setExtent(QgsRectangle(center.x()-500.,center.y()-500.,center.x()+500.,center.y()+500.));
    KaTopographicImportDialog importer(&canvas);
    {
      KaTopographicScopePanel panel(&canvas,nullptr,&importer,nullptr,library.path());
      QSignalSpy finished(&panel,&KaTopographicScopePanel::importFinished);
      QSignalSpy processing(&panel,&KaTopographicScopePanel::processingChanged);
      QSignalSpy attention(&panel,&KaTopographicScopePanel::attentionRequired);
      panel.refreshScope(); panel.acceptDownload(source,doduMetadata());
      QTRY_COMPARE_WITH_TIMEOUT(finished.size(),1,10000);
      QVERIFY(!finished.first().at(0).toBool());
      QTRY_VERIFY_WITH_TIMEOUT(!processing.isEmpty() && !processing.last().at(0).toBool(),10000);
      QVERIFY(!attention.isEmpty());
      const auto message=attention.last().at(0).toString();
      QVERIFY(message.contains(QStringLiteral("336062")) && message.contains(QStringLiteral("도두")));
      QVERIFY(message.contains(QStringLiteral("도곽만 있음")) && message.contains(QStringLiteral("검토 필요")));
      QCOMPARE(QgsProject::instance()->mapLayers().size(),0);
      QVERIFY(!importer.isVisible());
    }
    const QDir receipts(library.filePath(QStringLiteral("receipts")));
    const auto entries=receipts.entryInfoList({QStringLiteral("*.json")},QDir::Files);
    QCOMPARE(entries.size(),1);
    QFile receipt(entries.first().absoluteFilePath()); QVERIFY(receipt.open(QIODevice::ReadOnly));
    auto payload=QJsonDocument::fromJson(receipt.readAll()).object(); receipt.close();
    QCOMPARE(payload.value("state").toString(),QStringLiteral("review_required"));
    QVERIFY(QDir::isRelativePath(payload.value("source").toString()));
    QCOMPARE(payload.value("sourceSha256").toString().toLatin1(),QCryptographicHash::hash(originalBytes,QCryptographicHash::Sha256).toHex());
    if (change==1) {
      QVERIFY(original.open(QIODevice::Append)); original.write("\n999\nchanged fixture\n"); original.close();
    } else if (change==2) {
      auto metadata=payload.value("official").toObject(); metadata.insert("num","336063"); payload.insert("official",metadata);
    } else if (change==3) {
      const auto outside=external.filePath(QStringLiteral("336062.dxf")); QVERIFY(QFile::copy(source,outside));
      payload.insert("source",outside);
    } else if (change==4) {
      const auto other=library.filePath(QStringLiteral("원본/336063.dxf")); QVERIFY(QFile::copy(source,other));
      payload.insert("source",QDir(library.path()).relativeFilePath(other));
    }
    if (change>=2) {
      const auto bytes=QJsonDocument(payload).toJson(QJsonDocument::Compact);
      QVERIFY(receipt.open(QIODevice::WriteOnly|QIODevice::Truncate)); QCOMPARE(receipt.write(bytes),bytes.size()); receipt.close();
      if (change>=3) {
        // Even a self-consistent receipt cannot import an outside path or a
        // different sheet merely by updating its payload fingerprint.
        const auto name=QString::fromLatin1(QCryptographicHash::hash(bytes,QCryptographicHash::Sha256).toHex())+QStringLiteral(".json");
        QVERIFY(receipt.rename(receipts.filePath(name)));
      }
    }
    {
      KaTopographicScopePanel reopened(&canvas,nullptr,&importer,nullptr,library.path());
      QSignalSpy processing(&reopened,&KaTopographicScopePanel::processingChanged);
      QSignalSpy attention(&reopened,&KaTopographicScopePanel::attentionRequired);
      QSignalSpy finished(&reopened,&KaTopographicScopePanel::importFinished);
      reopened.refreshScope();
      QTRY_VERIFY_WITH_TIMEOUT(!processing.isEmpty() && !processing.last().at(0).toBool(),10000);
      const auto* count=reopened.findChild<QLabel*>(QStringLiteral("topographicSheetCount")); QVERIFY(count);
      QVERIFY2(count->text().contains(change==0?QStringLiteral("검토 필요 1장"):QStringLiteral("검토 필요 0장")),qPrintable(count->text()));
      QVERIFY(!attention.isEmpty());
      if (change==0) {
        const auto message=attention.last().at(0).toString();
        QVERIFY(message.contains(QStringLiteral("336062")) && message.contains(QStringLiteral("도두")));
      }
      QVERIFY(finished.isEmpty()); QCOMPARE(QgsProject::instance()->mapLayers().size(),0);
      QVERIFY(!importer.isVisible());
    }
    QVERIFY(original.open(QIODevice::ReadOnly));
    if (change!=1) QCOMPARE(original.readAll(),originalBytes);
  }
  void oldReviewReceiptsRecheckLocalOriginalWithoutDownloading_data() {
    QTest::addColumn<bool>("terrain");
    QTest::addColumn<bool>("versionField");
    QTest::addColumn<bool>("failIndex");
    QTest::newRow("legacy-review-remains-review")<<false<<false<<false;
    QTest::newRow("previous-solver-review-remains-review")<<false<<true<<false;
    QTest::newRow("legacy-now-valid-local-sheet-is-imported")<<true<<false<<false;
    QTest::newRow("index-write-failure-keeps-received-sheet-in-review")<<true<<false<<true;
  }
  void oldReviewReceiptsRecheckLocalOriginalWithoutDownloading() {
    QFETCH(bool,terrain); QFETCH(bool,versionField); QFETCH(bool,failIndex);
    QTemporaryDir library;
    QVERIFY(QDir().mkpath(library.filePath(QStringLiteral("원본"))));
    QVERIFY(QDir().mkpath(library.filePath(QStringLiteral("receipts"))));
    const QString number=terrain?QStringLiteral("378044"):QStringLiteral("336062");
    const QString relative=QStringLiteral("원본/%1.dxf").arg(number);
    const auto source=library.filePath(relative);
    auto metadata=doduMetadata();
    const QgsCoordinateReferenceSystem work(QStringLiteral("EPSG:5186"));
    QgsProject::instance()->setCrs(work);
    if(terrain) {
      metadata={{"source","NGII suchiQuery.do"},{"num",number},{"name",QStringLiteral("강릉")},
        {"scale","25000"},{"fileExt","dxf"},{"projection","GRS80"},{"makeYer","2024"},
        {"minx",1120928.8369},{"miny",1973152.5162},{"maxx",1132145.6322},{"maxy",1987191.4655}};
      const auto point=QgsCoordinateTransform(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:4737")),
        QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5187")),QgsProject::instance()).transform(QgsPointXY(128.9375,37.8125));
      auto* driver=GetGDALDriverManager()->GetDriverByName("DXF"); QVERIFY(driver);
      std::unique_ptr<GDALDataset,decltype(&GDALClose)> ds(driver->Create(source.toUtf8().constData(),0,0,0,GDT_Unknown,nullptr),GDALClose);
      QVERIFY(ds); auto* layer=ds->CreateLayer("entities",nullptr,wkbUnknown,nullptr); QVERIFY(layer);
      std::unique_ptr<OGRFeature,decltype(&OGRFeature::DestroyFeature)> feature(OGRFeature::CreateFeature(layer->GetLayerDefn()),OGRFeature::DestroyFeature);
      feature->SetField("Layer","F0017111"); OGRLineString line;
      line.addPoint(point.x(),point.y(),50.); line.addPoint(point.x()+100.,point.y()+50.,50.);
      QCOMPARE(feature->SetGeometry(&line),OGRERR_NONE); QCOMPARE(layer->CreateFeature(feature.get()),OGRERR_NONE);
    } else QVERIFY(createFrameDxf(source));
    const auto originalHash=fileHash(source); QVERIFY(!originalHash.isEmpty());
    QJsonObject receipt{{"schema",1},{"state","review_required"},{"source",relative},
      {"sourceSha256",QString::fromLatin1(originalHash)},{"official",metadata},
      {"dataFiles",QJsonArray{QFileInfo(source).fileName()}},{"reason",QStringLiteral("이전 좌표 판별의 검토 결과")}};
    if(versionField)receipt.insert("solverVersion",TopographicSourceCrs::kSolverVersion-1);
    const auto bytes=QJsonDocument(receipt).toJson(QJsonDocument::Compact);
    const auto receiptPath=library.filePath(QStringLiteral("receipts/%1.json")
      .arg(QString::fromLatin1(QCryptographicHash::hash(bytes,QCryptographicHash::Sha256).toHex())));
    QFile saved(receiptPath); QVERIFY(saved.open(QIODevice::WriteOnly)); QCOMPARE(saved.write(bytes),bytes.size()); saved.close();
    if(failIndex) {
      QFile blocked(library.filePath(QStringLiteral("index"))); QVERIFY(blocked.open(QIODevice::WriteOnly));
      blocked.write("fixture prevents creating the index directory"); blocked.close();
    }
    const auto point=QgsCoordinateTransform(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5179")),work,QgsProject::instance())
      .transform(QgsPointXY((metadata.value("minx").toDouble()+metadata.value("maxx").toDouble())/2.,
        (metadata.value("miny").toDouble()+metadata.value("maxy").toDouble())/2.));
    QgsMapCanvas canvas; canvas.setDestinationCrs(work);
    canvas.setExtent(QgsRectangle(point.x()-500.,point.y()-500.,point.x()+500.,point.y()+500.));
    KaTopographicImportDialog importer(&canvas);
    {
      KaTopographicScopePanel panel(&canvas,nullptr,&importer,nullptr,library.path());
      QSignalSpy processing(&panel,&KaTopographicScopePanel::processingChanged);
      QSignalSpy phases(&panel,&KaTopographicScopePanel::preparationProgressChanged);
      QSignalSpy downloaded(&panel,&KaTopographicScopePanel::importFinished);
      QSignalSpy attention(&panel,&KaTopographicScopePanel::attentionRequired);
      panel.refreshScope();
      QTRY_VERIFY_WITH_TIMEOUT(!processing.isEmpty() && !processing.last().at(0).toBool(),15000);
      bool checkedCrs=false;
      for(const auto& event:phases)if(event.at(0).toString()==QStringLiteral("좌표계 확인"))checkedCrs=true;
      QVERIFY2(checkedCrs,"기존 영수증을 그대로 믿지 않고 보관한 원본의 좌표계를 다시 확인해야 합니다.");
      QVERIFY(downloaded.isEmpty()); QVERIFY(!importer.isVisible());
      QCOMPARE(QgsProject::instance()->mapLayers().size(),terrain && !failIndex?1:0);
      QCOMPARE(QFileInfo::exists(receiptPath),failIndex);
      if(failIndex) {
        const auto* count=panel.findChild<QLabel*>(QStringLiteral("topographicSheetCount")); QVERIFY(count);
        QVERIFY2(count->text().contains(QStringLiteral("검토 필요 1장")),qPrintable(count->text()));
        const auto* status=panel.findChild<QLabel*>(QStringLiteral("topographicScopeStatus")); QVERIFY(status);
        QVERIFY(status->toolTip().contains(QStringLiteral("보관 정보를 저장하지 못했습니다")));
        QVERIFY(!attention.isEmpty());
        QVERIFY(attention.last().first().toString().contains(QStringLiteral("보관 정보를 저장하지 못했습니다")));
      }
      const QDir receipts(library.filePath(QStringLiteral("receipts")));
      const auto current=receipts.entryList({QStringLiteral("*.json")},QDir::Files);
      QCOMPARE(current.size(),terrain && !failIndex?0:1);
      if(!terrain) {
        QFile result(receipts.filePath(current.first())); QVERIFY(result.open(QIODevice::ReadOnly));
        const auto payload=QJsonDocument::fromJson(result.readAll()).object();
        QCOMPARE(payload.value("solverVersion").toInt(),TopographicSourceCrs::kSolverVersion);
        QVERIFY(payload.value("reason").toString().contains(QStringLiteral("도곽만 있음")));
      }
      QCOMPARE(fileHash(source),originalHash);
      canvas.stopRendering(); canvas.setLayers({}); QgsProject::instance()->clear();
    }
  }
  void realDownloadedSheetsUseAutomaticPreparationAndRenderReadOnlyShp() {
    const auto manifestPath=qEnvironmentVariable("KA_HGIS_REAL_TOPOGRAPHIC_MANIFEST");
    if (manifestPath.isEmpty()) QSKIP("실제 받은 도엽 manifest를 명시한 QA 실행에서만 확인합니다.");
    const auto output=qEnvironmentVariable("KA_HGIS_QA_OUTPUT_DIR"); QVERIFY(!output.isEmpty());
    QVERIFY(QDir().mkpath(output));
    QFile manifest(manifestPath); QVERIFY(manifest.open(QIODevice::ReadOnly));
    const auto document=QJsonDocument::fromJson(manifest.readAll());
    QJsonArray downloads=document.isArray()?document.array():document.object().value("files").toArray();
    if (downloads.isEmpty() && document.object().contains("path")) downloads.append(document.object());
    QVERIFY2(!downloads.isEmpty(),"실제 파일과 공식 도엽 metadata 목록이 비어 있습니다.");
    QTemporaryDir run(QDir(output).filePath(QStringLiteral("real-import-XXXXXX"))); QVERIFY(run.isValid());
    run.setAutoRemove(false); // Preserve only this QA copy and its evidence.
    const auto library=run.filePath(QStringLiteral("지형도")); QVERIFY(QDir().mkpath(library));
    QJsonArray inputs;
    QList<QPair<QString,QJsonObject>> prepared;
    for (const auto& entry:downloads) {
      const auto item=entry.toObject(); const auto record=item.value("record").toObject();
      const auto source=item.value("path").toString();
      const auto hash=fileHash(source); QVERIFY(!hash.isEmpty());
      const auto directory=QDir(library).filePath(QStringLiteral("원본/%1").arg(prepared.size()));
      QVERIFY(QDir().mkpath(directory));
      const auto copy=QDir(directory).filePath(QFileInfo(source).fileName()); QVERIFY(QFile::copy(source,copy));
      const auto xml=QFileInfo(source).absoluteDir().filePath(QFileInfo(source).completeBaseName()+QStringLiteral(".xml"));
      if (QFileInfo(xml).isFile()) QVERIFY(QFile::copy(xml,QDir(directory).filePath(QFileInfo(xml).fileName())));
      QCOMPARE(fileHash(copy),hash);
      prepared.append({copy,record});
      inputs.append(QJsonObject{{"original",source},{"copy",copy},{"sha256",QString::fromLatin1(hash)},
        {"sheet",record.value("num")},{"name",record.value("name")},{"bytes",QFileInfo(source).size()}});
    }
    QWidget evidenceHost;
    QgsMapCanvas canvas; canvas.setCanvasColor(Qt::white);
    const QgsCoordinateReferenceSystem work(QStringLiteral("EPSG:5186"));
    QgsProject::instance()->setCrs(work); canvas.setDestinationCrs(work);
    const auto scope=prepared.first().second.value("requestScope").toObject();
    const auto record=prepared.first().second;
    const double x=scope.contains("x")?scope.value("x").toDouble():
      (record.value("minx").toDouble()+record.value("maxx").toDouble())/2.;
    const double y=scope.contains("y")?scope.value("y").toDouble():
      (record.value("miny").toDouble()+record.value("maxy").toDouble())/2.;
    const auto center=QgsCoordinateTransform(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5179")),work,QgsProject::instance())
      .transform(QgsPointXY(x,y));
    auto* layout=new QHBoxLayout(&evidenceHost);
    QgsLayerTreeView tree; tree.setMinimumWidth(250); tree.setMaximumWidth(340);
    tree.setModel(new QgsLayerTreeModel(QgsProject::instance()->layerTreeRoot(),&tree));
    layout->addWidget(&tree); layout->addWidget(&canvas,1);
    evidenceHost.setWindowTitle(QStringLiteral("실제 수신 도엽 · 자동 SHP 변환/참조 지도 · EPSG:5186"));
    evidenceHost.resize(1500,950); evidenceHost.show();
    canvas.setExtent(QgsRectangle(center.x()-12000.,center.y()-10000.,center.x()+12000.,center.y()+10000.));
    KaTopographicImportDialog importer(&canvas);
    KaTopographicScopePanel panel(&canvas,nullptr,&importer,nullptr,library);
    QSignalSpy finished(&panel,&KaTopographicScopePanel::importFinished);
    QSignalSpy processing(&panel,&KaTopographicScopePanel::processingChanged);
    QSignalSpy attention(&panel,&KaTopographicScopePanel::attentionRequired);
    QJsonArray transitions,completed;
    QElapsedTimer timer; timer.start();
    connect(&panel,&KaTopographicScopePanel::preparationProgressChanged,&panel,[&](const QString& phase,const QString& detail) {
      transitions.append(QJsonObject{{"elapsedMs",timer.elapsed()},{"phase",phase},{"detail",detail}});
    });
    connect(&panel,&KaTopographicScopePanel::importFinished,&panel,[&](bool success) {
      const auto index=completed.size();
      const auto* status=panel.findChild<QLabel*>(QStringLiteral("topographicScopeStatus"));
      completed.append(QJsonObject{{"sheet",prepared[index].second.value("num")},{"success",success},{"elapsedMs",timer.elapsed()},
        {"status",status?status->text():QString()},{"crsEvidence",success && status?status->toolTip():QString()}});
    });
    panel.refreshScope();
    for (const auto& item:prepared) panel.acceptDownload(item.first,item.second);
    QTRY_COMPARE_WITH_TIMEOUT(finished.size(),prepared.size(),240000);
    QTRY_VERIFY_WITH_TIMEOUT(!processing.isEmpty() && !processing.last().at(0).toBool(),240000);
    const auto preparationMs=timer.elapsed();
    QVERIFY(!importer.isVisible());
    int successfulSheets=0;
    for (const auto& event:finished) if (event.at(0).toBool()) ++successfulSheets;
    QJsonArray layers;
    qint64 featureCount=0;
    for (auto* raw:QgsProject::instance()->mapLayers()) {
      auto* layer=qobject_cast<QgsVectorLayer*>(raw); QVERIFY(layer);
      QVERIFY2(LayerOps::isReferenceLayer(layer),qPrintable(layer->name()));
      QVERIFY(layer->customProperty(QStringLiteral("ka_hgis/layer_key")).toString().isEmpty());
      const auto path=layer->source().section(QLatin1Char('|'),0,0);
      QCOMPARE(QFileInfo(path).suffix().toLower(),QStringLiteral("shp"));
      QVERIFY(!layer->startEditing()); QVERIFY(layer->crs().isValid());
      auto* node=QgsProject::instance()->layerTreeRoot()->findLayer(layer); QVERIFY(node);
      // The product keeps a flat layer list; persisted role, not an optional
      // parent heading, separates reference maps from editable survey data.
      QCOMPARE(layer->customProperty(QString::fromUtf8(LayerOps::kPropLayerRole)).toString(),
        QString::fromUtf8(LayerOps::kRoleReference));
      auto* renderer=dynamic_cast<QgsSingleSymbolRenderer*>(layer->renderer()); QVERIFY(renderer);
      QCOMPARE(renderer->symbol()->color(),QColor(128,128,128));
      const auto properties=renderer->symbol()->symbolLayer(0)->properties();
      const bool line=layer->geometryType()==Qgis::GeometryType::Line;
      QCOMPARE(properties.value(line?QStringLiteral("line_width"):QStringLiteral("outline_width")).toDouble(),.2);
      QCOMPARE(properties.value(line?QStringLiteral("line_width_unit"):QStringLiteral("outline_width_unit")),QStringLiteral("MM"));
      featureCount+=layer->featureCount();
      layers.append(QJsonObject{{"name",layer->name()},{"uri",layer->source()},{"crs",layer->crs().authid()},
        {"features",layer->featureCount()},{"visible",node->itemVisibilityChecked()},
        {"role",layer->customProperty(QString::fromUtf8(LayerOps::kPropLayerRole)).toString()},
        {"sheet",layer->customProperty(QStringLiteral("ka_hgis/topographic_sheet")).toString()}});
    }
    QJsonArray warnings;
    for (const auto& event:attention) if (!event.at(0).toString().isEmpty()) warnings.append(event.at(0).toString());
    panel.stop(); // Remove QA search overlays; completed reference maps remain.
    tree.expandAll();
    QSignalSpy rendered(&canvas,&QgsMapCanvas::mapCanvasRefreshed);
    QSignalSpy renderCanceled(&canvas,&QgsMapCanvas::mapRefreshCanceled);
    canvas.refresh(); QTRY_VERIFY_WITH_TIMEOUT(!rendered.isEmpty() && !canvas.isDrawing(),30000);
    QImage screenshot; int grayPixels=0;
    const auto capture=[&] {
      screenshot=canvas.grab().toImage(); grayPixels=0;
      for (int py=8;py<screenshot.height()-8;++py) for (int px=8;px<screenshot.width()-8;++px) {
        const auto color=screenshot.pixelColor(px,py);
        if (color.red()>=128 && color.red()<245 && color.red()==color.green() && color.green()==color.blue()) ++grayPixels;
      }
      return grayPixels>500;
    };
    if (successfulSheets>0) QTRY_VERIFY_WITH_TIMEOUT(capture(),30000);
    else capture();
    QVERIFY(screenshot.save(run.filePath(QStringLiteral("real-shp-map.png"))));
    QVERIFY(evidenceHost.grab().save(run.filePath(QStringLiteral("real-shp-reference-layers.png"))));
    const int overviewGrayPixels=grayPixels;
    QJsonObject zoomEvidence;
    if (successfulSheets>0) {
      QString focusSheet;
      for(const auto& item:completed)if(item.toObject().value("success").toBool()) {
        focusSheet=item.toObject().value("sheet").toString();break;
      }
      QgsRectangle focus;
      for(auto* layer:canvas.layers()) {
        if(layer->customProperty(QStringLiteral("ka_hgis/topographic_sheet")).toString()!=focusSheet)continue;
        const auto bounds=QgsCoordinateTransform(layer->crs(),work,QgsProject::instance()).transformBoundingBox(layer->extent());
        if(focus.isNull())focus=bounds;else focus.combineExtentWith(bounds);
      }
      rendered.clear(); renderCanceled.clear(); canvas.setCenter(focus.center()); canvas.zoomScale(25000.); canvas.refresh();
      QElapsedTimer zoomClock; zoomClock.start();
      while((rendered.isEmpty() || canvas.isDrawing()) && zoomClock.elapsed()<30000) QTest::qWait(50);
      qInfo()<<"Actual-sheet zoom:"<<canvas.scale()<<canvas.extent().toString()
        <<"layers"<<canvas.layers().size()<<"rendered"<<rendered.size()<<"canceled"<<renderCanceled.size()<<"drawing"<<canvas.isDrawing();
      if(rendered.isEmpty() || canvas.isDrawing())canvas.stopRendering();
      QVERIFY(!rendered.isEmpty() && !canvas.isDrawing());
      QTRY_VERIFY_WITH_TIMEOUT(capture(),30000);
      QVERIFY(canvas.grab().save(run.filePath(QStringLiteral("real-shp-25000-map.png"))));
      QVERIFY(evidenceHost.grab().save(run.filePath(QStringLiteral("real-shp-25000-reference-layers.png"))));
      QJsonArray renderedLabels;
      if(const auto* labeling=canvas.labelingResults(false))for(const auto& label:labeling->allLabels())
        renderedLabels.append(QJsonObject{{"text",label.labelText},{"layer",label.layerID},{"feature",label.featureId}});
      zoomEvidence={{"sheet",focusSheet},{"scale",canvas.scale()},{"centerX",canvas.center().x()},{"centerY",canvas.center().y()},
        {"grayPixels",grayPixels},{"renderedLabels",renderedLabels},{"renderCanceled",renderCanceled.size()}};
    }
    for (const auto& input:inputs) {
      const auto item=input.toObject();
      QCOMPARE(fileHash(item.value("original").toString()),item.value("sha256").toString().toLatin1());
      QCOMPARE(fileHash(item.value("copy").toString()),item.value("sha256").toString().toLatin1());
    }
    const QJsonObject report{{"library",library},{"inputs",inputs},{"completed",completed},{"layers",layers},
      {"warnings",warnings},{"phases",transitions},{"preparationMs",preparationMs},{"successfulSheets",successfulSheets},
      {"features",featureCount},{"grayPixels",overviewGrayPixels},{"zoom25000",zoomEvidence},{"workCrs",work.authid()},
      {"alignmentNote",QStringLiteral("실제 자동 좌표 확인/레이어/화소 검증. 위성·지적과의 독립 정합 검증은 별도입니다.")}};
    QFile evidence(run.filePath(QStringLiteral("REPORT.json"))); QVERIFY(evidence.open(QIODevice::WriteOnly));
    const auto reportBytes=QJsonDocument(report).toJson(); QCOMPARE(evidence.write(reportBytes),reportBytes.size()); evidence.close();
    qInfo().noquote()<<QStringLiteral("실자료 검증 보고서: %1").arg(evidence.fileName());
    QVERIFY2(successfulSheets>0,"실제 받은 도엽 중 자동 적재가 검증된 양성 도엽이 없습니다. 도곽 자료를 강제로 승인하지 않았습니다.");
    QVERIFY(featureCount>0); QVERIFY(grayPixels>500);
    canvas.setLayers({}); QgsProject::instance()->clear();
  }
  void unsupportedScopeStopsAutomaticWork() {
    QTemporaryDir cache;
    QgsProject::instance()->setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:4326")));
    QgsMapCanvas canvas;
    canvas.setDestinationCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:4326")));
    canvas.setExtent(QgsRectangle(128.,37.,129.,38.));
    KaTopographicImportDialog importer(&canvas);
    KaTopographicScopePanel panel(&canvas,nullptr,&importer,nullptr,cache.path());
    QSignalSpy attention(&panel,&KaTopographicScopePanel::attentionRequired);
    panel.refreshScope();
    auto* enabled=panel.findChild<QCheckBox*>(); QVERIFY(enabled);
    QVERIFY(!enabled->isChecked());
    QCOMPARE(attention.size(),1);
    QVERIFY(!attention.first().at(0).toString().isEmpty());
    QCOMPARE(QgsProject::instance()->mapLayers().size(),0);
  }
  void cancelClearsQueuedProcessingImmediately() {
    QTemporaryDir library;
    const QgsCoordinateReferenceSystem work(QStringLiteral("EPSG:5186"));
    QgsProject::instance()->setCrs(work);
    QgsMapCanvas canvas; canvas.setDestinationCrs(work);
    const auto center=QgsCoordinateTransform(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5179")),work,QgsProject::instance())
      .transform(QgsPointXY(1125000.,1980000.));
    canvas.setExtent(QgsRectangle(center.x()-500.,center.y()-500.,center.x()+500.,center.y()+500.));
    KaTopographicImportDialog importer(&canvas);
    KaTopographicScopePanel panel(&canvas,nullptr,&importer,nullptr,library.path());
    QSignalSpy processing(&panel,&KaTopographicScopePanel::processingChanged);
    QSignalSpy finished(&panel,&KaTopographicScopePanel::importFinished);
    QSignalSpy attention(&panel,&KaTopographicScopePanel::attentionRequired);
    QSignalSpy phases(&panel,&KaTopographicScopePanel::preparationProgressChanged);
    connect(&panel,&KaTopographicScopePanel::preparationProgressChanged,&panel,[&](const QString& phase) {
      if (!phase.isEmpty()) panel.stop();
    });
    panel.refreshScope();
    panel.acceptDownload(library.filePath(QStringLiteral("queued.dxf")),{});
    QCOMPARE(processing.size(),1);
    QVERIFY(processing.first().at(0).toBool());
    QTRY_VERIFY_WITH_TIMEOUT(!panel.isActive(),10000);
    QCOMPARE(processing.size(),2);
    QVERIFY(!processing.last().at(0).toBool());
    QVERIFY(!panel.isActive());
    QVERIFY(finished.isEmpty());
    QVERIFY(attention.isEmpty());
    QVERIFY(!phases.isEmpty()); QVERIFY(phases.last().at(0).toString().isEmpty());
    const auto phaseCount=phases.size();
    QCoreApplication::processEvents();
    QCOMPARE(phases.size(),phaseCount); // Queued pre-cancel stage callbacks are discarded.
    QCOMPARE(QgsProject::instance()->mapLayers().size(),0);
  }
  void rejectedDownloadRequestsAttentionWithoutImporting() {
    QTemporaryDir library;
    const QgsCoordinateReferenceSystem work(QStringLiteral("EPSG:5186"));
    QgsProject::instance()->setCrs(work);
    QgsMapCanvas canvas; canvas.setDestinationCrs(work);
    const auto center=QgsCoordinateTransform(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5179")),work,QgsProject::instance())
      .transform(QgsPointXY(1125000.,1980000.));
    canvas.setExtent(QgsRectangle(center.x()-500.,center.y()-500.,center.x()+500.,center.y()+500.));
    KaTopographicImportDialog importer(&canvas);
    KaTopographicScopePanel panel(&canvas,nullptr,&importer,nullptr,library.path());
    QSignalSpy processing(&panel,&KaTopographicScopePanel::processingChanged);
    QSignalSpy finished(&panel,&KaTopographicScopePanel::importFinished);
    QSignalSpy attention(&panel,&KaTopographicScopePanel::attentionRequired);
    QSignalSpy phases(&panel,&KaTopographicScopePanel::preparationProgressChanged);
    panel.refreshScope();
    panel.acceptDownload(library.filePath(QStringLiteral("unverified.dxf")),{});
    QTRY_COMPARE_WITH_TIMEOUT(finished.size(),1,10000);
    QVERIFY(!finished.first().at(0).toBool());
    QCOMPARE(attention.size(),1);
    QVERIFY(!attention.first().at(0).toString().isEmpty());
    QTRY_COMPARE_WITH_TIMEOUT(processing.size(),2,10000);
    QVERIFY(!processing.last().at(0).toBool());
    QVERIFY(!phases.isEmpty()); QVERIFY(phases.last().at(0).toString().isEmpty());
    for (const auto& event:phases) {
      QVERIFY(event.at(0).toString()!=QStringLiteral("SHP 변환"));
      QVERIFY(event.at(0).toString()!=QStringLiteral("지도에 올리기"));
    }
    QVERIFY(!importer.isVisible());
    QCOMPARE(QgsProject::instance()->mapLayers().size(),0);
  }
};
int main(int argc, char** argv) {
  QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
  KaPortableRuntime::applyWebEngineFlags();
  QgsApplication app(argc, argv, true);
  QFontDatabase::addApplicationFont(QStringLiteral("C:/Windows/Fonts/malgun.ttf"));
  QgsApplication::setPrefixPath(qEnvironmentVariable("QGIS_PREFIX_PATH"), true);
  QgsApplication::initQgis();
  QTemporaryDir settings;
  QSettings::setDefaultFormat(QSettings::IniFormat);
  QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settings.path());
  TopographicScopeTest test;
  const int result = QTest::qExec(&test, argc, argv);
  QgsApplication::exitQgis();
  return result;
}
#include "test_topographic_scope.moc"
