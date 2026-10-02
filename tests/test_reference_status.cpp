#include <QtTest>
#include <QDir>
#include <QFile>
#include <QLabel>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTreeWidget>
#include <QUrl>
#include <qgsapplication.h>
#include <qgscategorizedsymbolrenderer.h>
#include <qgsfeature.h>
#include <qgsgeometry.h>
#include <qgsproject.h>
#include <qgsrasterlayer.h>
#include <qgsvectorfilewriter.h>
#include <qgsvectorlayer.h>
#include "app/KaReferenceStatusDialog.h"
#include "core/LayerOps.h"
#include "core/PaleoLandformService.h"
#include "core/ReferenceInventory.h"
#include "core/ReferenceSheetSnapshot.h"
#include "core/ReferenceStorage.h"

namespace {
// A small EPSG:5186 polygon GPKG, like a downloaded reference map.
QString writeGpkg(const QString& path) {
  QgsVectorLayer memory(QStringLiteral("Polygon?crs=EPSG:5186&field=name:string"), QStringLiteral("m"),
                        QStringLiteral("memory"));
  QgsFeature feature(memory.fields());
  feature.setGeometry(QgsGeometry::fromRect(QgsRectangle(200000, 450000, 203000, 452000)));
  feature.setAttribute(0, QStringLiteral("시험"));
  QgsFeatureList features{feature};
  memory.dataProvider()->addFeatures(features);
  QgsVectorFileWriter::SaveVectorOptions options;
  options.driverName = QStringLiteral("GPKG");
  options.layerName = QStringLiteral("reference");
  return QgsVectorFileWriter::writeAsVectorFormatV3(&memory, path, QgsCoordinateTransformContext(), options) ==
                 QgsVectorFileWriter::NoError
             ? path + QStringLiteral("|layername=reference")
             : QString();
}
// Plain literals: a multi-line raw string with quotes before the Q_OBJECT class confuses moc.
QByteArray capabilities() {
  return QByteArray("<WMS_Capabilities version=\"1.3.0\" xmlns=\"http://www.opengis.net/wms\" xmlns:xlink=\"http://www.w3.org/1999/xlink\">\n"
      "<Service><Name>WMS</Name><Title>Test</Title></Service><Capability>\n<Request><GetMap><Format>image/png</Format><DCPType><HTTP><Get>"
      "<OnlineResource xlink:href=\"http://127.0.0.1:9/maps?\"/></Get></HTTP></DCPType></GetMap></Request>\n<Layer><Title>Root</Title><CRS>EPSG:4326</CRS>"
      "<EX_GeographicBoundingBox><westBoundLongitude>124</westBoundLongitude><eastBoundLongitude>130</eastBoundLongitude><southBoundLatitude>33</southBoundLatitude>"
      "<northBoundLatitude>39</northBoundLatitude></EX_GeographicBoundingBox>\n<Layer><Name>test</Name><Title>Test layer</Title><CRS>EPSG:4326</CRS>"
      "<BoundingBox CRS=\"EPSG:4326\" minx=\"33\" miny=\"124\" maxx=\"39\" maxy=\"130\"/></Layer></Layer>\n</Capability></WMS_Capabilities>");
}
}  // namespace

class ReferenceStatusTest : public QObject {
  Q_OBJECT
private slots:
  void cleanup() { QgsProject::instance()->clear(); }

  void inventoryDescribesKindSizeDateAndOfflineUse() {
    QTemporaryDir directory;
    auto* project = QgsProject::instance();
    project->setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5187")));
    const QString uri = writeGpkg(directory.filePath(QStringLiteral("지질도_KIGAM.gpkg")));
    QVERIFY(!uri.isEmpty());
    auto* local = new QgsVectorLayer(uri, QStringLiteral("지질도(KIGAM 1:5만)"), QStringLiteral("ogr"));
    QVERIFY(local->isValid());
    LayerOps::markReferenceLayer(local);
    auto* online = new QgsRasterLayer(QStringLiteral("type=xyz&url=https://tile.example.invalid/%7Bz%7D/%7Bx%7D/%7By%7D.png&zmax=19&zmin=0"),
                                      QStringLiteral("OSM 시험"), QStringLiteral("wms"));
    auto* temporary = new QgsVectorLayer(QStringLiteral("LineString?crs=EPSG:5187"), QStringLiteral("임시 참조"),
                                         QStringLiteral("memory"));
    LayerOps::markReferenceLayer(temporary);
    auto* survey = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5187"), QStringLiteral("조사구역"),
                                      QStringLiteral("memory"));
    survey->setCustomProperty(QStringLiteral("ka_hgis/layer_key"), QStringLiteral("survey_area"));
    project->addMapLayers({local, online, temporary, survey});

    const auto items = ReferenceInventory::collect(project);
    QCOMPARE(items.size(), 3);  // the survey domain layer is not reference data
    const auto find = [&items](const QString& name) {
      for (const auto& item : items) if (item.name == name) return item;
      return ReferenceInventory::Item{};
    };
    const auto geology = find(QStringLiteral("지질도(KIGAM 1:5만)"));
    QCOMPARE(geology.availability, ReferenceInventory::Availability::Offline);
    QCOMPARE(geology.kind, QStringLiteral("참조 지도"));
    QVERIFY(geology.bytes > 0);
    QCOMPARE(geology.receivedText.size(), 16);  // yyyy-MM-dd HH:mm
    QCOMPARE(geology.extentText, QStringLiteral("약 3.0 × 2.0 km"));
    QVERIFY(geology.location.endsWith(QStringLiteral("지질도_KIGAM.gpkg")));
    const auto tiles = find(QStringLiteral("OSM 시험"));
    QCOMPARE(tiles.availability, ReferenceInventory::Availability::Online);
    QCOMPARE(tiles.kind, QStringLiteral("배경 지도"));
    QCOMPARE(tiles.bytes, qint64(-1));
    QCOMPARE(tiles.location, QStringLiteral("tile.example.invalid"));
    // An unknown tile host may be packed only after checking its terms (F059 policy).
    QCOMPARE(tiles.offlineNote, QStringLiteral("오프라인 저장 전 제공처 조건 확인"));
    QVERIFY(geology.offlineNote.isEmpty());
    QCOMPARE(find(QStringLiteral("임시 참조")).availability, ReferenceInventory::Availability::Unsaved);
    const QString summary = ReferenceInventory::summary(items);
    QVERIFY(summary.contains(QStringLiteral("현장 사용 가능 1개")));
    QVERIFY(summary.contains(QStringLiteral("인터넷 필요 1개")));
    QCOMPARE(ReferenceInventory::summary({}), QStringLiteral("이 조사에 올린 참조 자료가 없습니다."));
    QCOMPARE(project->mapLayers().size(), 4);  // collecting changes nothing
  }

  void statusDialogIsReadOnlyAndRefreshesOnRequest() {
    auto* project = QgsProject::instance();
    auto* first = new QgsVectorLayer(QStringLiteral("LineString?crs=EPSG:5187"), QStringLiteral("첫 참조"),
                                     QStringLiteral("memory"));
    LayerOps::markReferenceLayer(first);
    project->addMapLayer(first);
    KaReferenceStatusDialog dialog(project);
    auto* table = dialog.findChild<QTreeWidget*>(QStringLiteral("referenceStatusTable"));
    auto* summary = dialog.findChild<QLabel*>(QStringLiteral("referenceStatusSummary"));
    auto* again = dialog.findChild<QPushButton*>(QStringLiteral("referenceStatusRefresh"));
    QVERIFY(table && summary && again);
    QCOMPARE(table->topLevelItemCount(), 1);
    QCOMPARE(table->topLevelItem(0)->text(0), QStringLiteral("첫 참조"));
    QCOMPARE(table->topLevelItem(0)->text(5), QStringLiteral("저장 전 임시"));
    QCOMPARE(table->editTriggers(), QAbstractItemView::NoEditTriggers);
    auto* second = new QgsVectorLayer(QStringLiteral("LineString?crs=EPSG:5187"), QStringLiteral("둘째 참조"),
                                      QStringLiteral("memory"));
    LayerOps::markReferenceLayer(second);
    project->addMapLayer(second);
    QCOMPARE(table->topLevelItemCount(), 1);  // no automatic polling or loading
    again->click();
    QCOMPARE(table->topLevelItemCount(), 2);
    QCOMPARE(project->mapLayers().size(), 2);
  }

  void generatedFolderNamesAreRecognisedInEveryLayerForm() {
    QVERIFY(ReferenceStorage::isGeneratedFolder(QStringLiteral("ka-hgis-reference-Ab12Cd")));
    QVERIFY(ReferenceStorage::isGeneratedFolder(QStringLiteral("C:/조사/ka-hgis-reference-ABCDEF")));
    QVERIFY(!ReferenceStorage::isGeneratedFolder(QStringLiteral("ka-hgis-reference-")));
    QVERIFY(!ReferenceStorage::isGeneratedFolder(QStringLiteral("ka-hgis-reference-ABCDEFG")));
    QVERIFY(!ReferenceStorage::isGeneratedFolder(QStringLiteral("지형도")));
    QVERIFY(ReferenceStorage::surveyFolder(QString()).isEmpty());
    QVERIFY(ReferenceStorage::surveyFolder(QStringLiteral("Z:/없는/조사.gpkg")).isEmpty());

    QTemporaryDir directory;
    QVERIFY(QDir(directory.path()).mkpath(QStringLiteral("ka-hgis-reference-Vec123")));
    QVERIFY(QDir(directory.path()).mkpath(QStringLiteral("ka-hgis-reference-Wms456")));
    const QString uri = writeGpkg(directory.filePath(QStringLiteral("ka-hgis-reference-Vec123/geology.gpkg")));
    QVERIFY(!uri.isEmpty());
    const QString xml = directory.filePath(QStringLiteral("ka-hgis-reference-Wms456/capabilities.xml"));
    QFile file(xml);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(capabilities());
    file.close();
    auto* project = QgsProject::instance();
    project->addMapLayer(new QgsVectorLayer(uri, QStringLiteral("지질도"), QStringLiteral("ogr")));
    // Geology fallback form: the local capabilities URL is percent-encoded.
    const QString wms = QStringLiteral("crs=EPSG:4326&format=image/png&layers=test&styles=&url=%1")
        .arg(QString::fromLatin1(QUrl::toPercentEncoding(QUrl::fromLocalFile(xml).toString())));
    project->addMapLayer(new QgsRasterLayer(wms, QStringLiteral("공식 지질도"), QStringLiteral("wms")));
    const auto names = ReferenceStorage::referencedFolderNames(project);
    QVERIFY(names.contains(QStringLiteral("ka-hgis-reference-Vec123")));
    QVERIFY(names.contains(QStringLiteral("ka-hgis-reference-Wms456")));
    project->clear();
  }

  void onlyReplacedFoldersOfThisSessionAreRemovedAtSave() {
    QTemporaryDir directory;
    const QString survey = directory.filePath(QStringLiteral("조사.gpkg"));
    QFile surveyFile(survey);
    QVERIFY(surveyFile.open(QIODevice::WriteOnly));
    surveyFile.close();
    QCOMPARE(ReferenceStorage::surveyFolder(survey), QDir::cleanPath(directory.path()));
    const QString oldFolder = directory.filePath(QStringLiteral("ka-hgis-reference-Old111"));
    const QString newFolder = directory.filePath(QStringLiteral("ka-hgis-reference-New222"));
    const QString earlier = directory.filePath(QStringLiteral("ka-hgis-reference-Pre333"));
    for (const QString& folder : {oldFolder, newFolder, earlier}) {
      QVERIFY(QDir().mkpath(folder));
      QFile data(QDir(folder).filePath(QStringLiteral("map.geojson")));
      QVERIFY(data.open(QIODevice::WriteOnly));
      data.write("{}");
    }
    ReferenceStorage::FolderLedger ledger;
    ledger.adopt(oldFolder, survey);
    ledger.adopt(directory.filePath(QStringLiteral("지형도")), survey);  // not a generated folder
    QVERIFY(ledger.retired().isEmpty());
    ledger.adopt(newFolder, survey);
    // The new download replaced the old one; the earlier-session folder was never adopted.
    const QSet<QString> before{QStringLiteral("ka-hgis-reference-Old111"), QStringLiteral("ka-hgis-reference-Pre333")};
    const QSet<QString> after{QStringLiteral("ka-hgis-reference-New222")};
    ledger.supersede(before, after);
    QCOMPARE(ledger.retired(), QStringList{QDir::cleanPath(oldFolder)});
    QVERIFY(ledger.removable(after, directory.filePath(QStringLiteral("다른 조사.gpkg"))).isEmpty());
    QVERIFY(ledger.removable(before, survey).isEmpty());  // still referenced somewhere: keep
    const QStringList removable = ledger.removable(after, survey);
    QCOMPARE(removable, QStringList{QDir::cleanPath(oldFolder)});
    QVERIFY(ReferenceStorage::removeGeneratedFolder(removable.first()));
    ledger.forget(removable.first());
    QVERIFY(!QFileInfo::exists(oldFolder));
    QVERIFY(QFileInfo::exists(newFolder));
    QVERIFY(QFileInfo::exists(earlier));
    QVERIFY(ledger.retired().isEmpty());
    // Never removes anything that is not a generated download folder.
    const QString other = directory.filePath(QStringLiteral("지형도"));
    QVERIFY(QDir().mkpath(other));
    QVERIFY(!ReferenceStorage::removeGeneratedFolder(other));
    QVERIFY(QFileInfo::exists(other));
  }

  void sheetSnapshotIsReusedOnlyWhileEveryFileIsUnchanged() {
    QTemporaryDir directory;
    const QString source = directory.filePath(QStringLiteral("378044.zip"));
    QFile original(source);
    QVERIFY(original.open(QIODevice::WriteOnly));
    original.write("PK-original");
    original.close();
    const QString shp = directory.filePath(QStringLiteral("SHP/F0017111.shp"));
    QVERIFY(QDir().mkpath(QFileInfo(shp).absolutePath()));
    for (const QString& path : {shp, directory.filePath(QStringLiteral("SHP/F0017111.dbf"))}) {
      QFile part(path);
      QVERIFY(part.open(QIODevice::WriteOnly));
      part.write("shape");
    }
    TopographicCatalog::Record record;
    record.source = shp;
    record.layerName = QStringLiteral("F0017111");
    record.displayName = QStringLiteral("등고선");
    record.sourceSheet = QStringLiteral("378044");
    record.crsWkt = QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5187")).toWkt();
    record.extent = QgsRectangle(1, 2, 3, 4);
    record.hasExtent = true;
    record.category = TopographicCatalog::Category::Contour;
    const QJsonObject snapshot = ReferenceSheetSnapshot::capture(source, {record}, 4,
        QStringLiteral("동부원점(GRS80) · EPSG:5187"), QStringLiteral("공식 도곽 일치"));
    QVERIFY(!snapshot.isEmpty());
    const auto reused = ReferenceSheetSnapshot::restore(snapshot, source, 4);
    QVERIFY(reused.valid);
    QCOMPARE(reused.records.size(), 1);
    QCOMPARE(reused.records.first().source, QFileInfo(shp).absoluteFilePath());
    QCOMPARE(reused.records.first().category, TopographicCatalog::Category::Contour);
    QVERIFY(reused.records.first().extent == QgsRectangle(1, 2, 3, 4));
    QCOMPARE(reused.evidence, QStringLiteral("공식 도곽 일치"));
    QVERIFY(!ReferenceSheetSnapshot::restore(snapshot, source, 5).valid);  // solver changed
    QVERIFY(!ReferenceSheetSnapshot::restore(snapshot, directory.filePath(QStringLiteral("other.zip")), 4).valid);
    // Sidecar beside the index entry; never a *.json the index scan would read.
    const QString index = directory.filePath(QStringLiteral("index/abc.json"));
    const QString sidecar = directory.filePath(QStringLiteral("index/abc.snapshot"));
    QVERIFY(QDir().mkpath(QFileInfo(index).absolutePath()));
    ReferenceSheetSnapshot::rememberBeside(index, source, {record}, true, 4, QString(), QString());
    QCOMPARE(ReferenceSheetSnapshot::sidecarPath(index), sidecar);
    QVERIFY(QFileInfo::exists(sidecar));
    QVERIFY(ReferenceSheetSnapshot::restoreBeside(index, source, QStringLiteral("378044"), 4).valid);
    QVERIFY(!ReferenceSheetSnapshot::restoreBeside(index, source, QStringLiteral("378043"), 4).valid);
    ReferenceSheetSnapshot::rememberBeside(index, source, {record}, false, 4, QString(), QString());
    QVERIFY(!QFileInfo::exists(sidecar));  // a result with warnings keeps the full check
    QFile dbf(directory.filePath(QStringLiteral("SHP/F0017111.dbf")));
    QVERIFY(dbf.open(QIODevice::Append));
    dbf.write("changed");  // any SHP component change forces the full check
    dbf.close();
    QVERIFY(!ReferenceSheetSnapshot::restore(snapshot, source, 4).valid);
  }

  void automaticPaleoSeedsSayAutomaticHypothesisInTheLegend() {
    QgsVectorLayer paleo(QStringLiteral("Polygon?crs=EPSG:5186&field=kind:string&field=note:string&field=status:string"),
                         QStringLiteral("고지형 판독"), QStringLiteral("memory"));
    QVERIFY(paleo.isValid());
    const auto add = [&paleo](const QString& kind, const QString& note, double x) {
      QgsFeature feature(paleo.fields());
      feature.setGeometry(QgsGeometry::fromRect(QgsRectangle(x, 0, x + 50, 50)));
      QgsAttributes attributes;
      attributes << kind << note << QStringLiteral("가설");
      feature.setAttributes(attributes);
      QgsFeatureList features{feature};
      return paleo.dataProvider()->addFeatures(features);
    };
    QVERIFY(add(QStringLiteral("구하도"), QStringLiteral("자동: 하성평탄 안쪽"), 0));
    QVERIFY(add(QStringLiteral("미분류"), QStringLiteral("손으로 그림"), 100));
    QVERIFY(PaleoLandformService::applyInterpretationStyle(&paleo));
    auto* renderer = dynamic_cast<QgsCategorizedSymbolRenderer*>(paleo.renderer());
    QVERIFY(renderer);
    QHash<QString, QString> labels;
    for (const auto& category : renderer->categories()) labels.insert(category.value().toString(), category.label());
    QCOMPARE(labels.value(QStringLiteral("구하도")), QStringLiteral("구하도 · 자동 가설"));
    QCOMPARE(labels.value(QStringLiteral("미분류")), QStringLiteral("미분류 · 가설"));
    QCOMPARE(labels.value(QStringLiteral("선상지")), QStringLiteral("선상지 · 가설"));
    for (const auto& label : labels) QVERIFY(label.contains(QStringLiteral("가설")));
  }
};

int main(int argc, char** argv) {
  QgsApplication app(argc, argv, true);
  QgsApplication::setPrefixPath(qEnvironmentVariable("QGIS_PREFIX_PATH", "A:/OSGeo4W/apps/qgis-dev"), true);
  QgsApplication::initQgis();
  ReferenceStatusTest test;
  const int result = QTest::qExec(&test, argc, argv);
  QgsApplication::exitQgis();
  return result;
}

#include "test_reference_status.moc"
