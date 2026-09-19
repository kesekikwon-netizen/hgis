#include <QtTest>
#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTimeZone>
#include <qgsapplication.h>
#include <qgscoordinatetransform.h>
#include <qgsfeature.h>
#include <qgsfillsymbollayer.h>
#include <qgslayertree.h>
#include <qgslabelingresults.h>
#include <qgsmaprendererparalleljob.h>
#include <qgsmaprenderersequentialjob.h>
#include <qgstextformat.h>
#include <QFont>
#include <qgsmapsettings.h>
#include <qgsmapcanvas.h>
#include <qgspallabeling.h>
#include <qgsproject.h>
#include <qgssinglesymbolrenderer.h>
#include <qgssymbol.h>
#include <qgsvectorfilewriter.h>
#include <qgsvectorlayer.h>
#include <qgsvectorlayerlabeling.h>
#include "core/CadastralImport.h"
#include "core/CadastralPortal.h"
#include "core/LayerOps.h"
#include "core/VworldSettings.h"

namespace {
QgsCoordinateReferenceSystem sourceCrs() { return QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")); }
QgsGeometry box(double x, double y, double width, double height) {
  return QgsGeometry::fromRect(QgsRectangle(x, y, x + width, y + height));
}
QString pnu(int parcel) {
  return QStringLiteral("471901010010%1").arg(parcel, 7, 10, QLatin1Char('0'));
}
struct Parcel { int id; QString jibun; QgsGeometry geometry; };

// Real OGR files exercise provider encoding, CRS metadata and read-only inputs.
QString writeParcels(const QString& path, const QList<Parcel>& parcels,
                     bool withJibun = true, bool validCrs = true) {
  QString uri = QStringLiteral("Polygon?field=PNU:string(19)&field=OWNER:string(80)");
  if (withJibun) uri += QStringLiteral("&field=JIBUN:string(40)");
  if (validCrs) uri += QStringLiteral("&crs=EPSG:5186");
  QgsVectorLayer layer(uri, QStringLiteral("synthetic parcels"), QStringLiteral("memory"));
  if (!validCrs) layer.setCrs(QgsCoordinateReferenceSystem());
  if (!layer.isValid() || !layer.startEditing()) return {};
  for (const Parcel& parcel : parcels) {
    QgsFeature feature(layer.fields());
    feature.setAttribute(QStringLiteral("PNU"), pnu(parcel.id));
    feature.setAttribute(QStringLiteral("OWNER"), QStringLiteral("제외할 개인정보"));
    if (withJibun) feature.setAttribute(QStringLiteral("JIBUN"), parcel.jibun);
    feature.setGeometry(parcel.geometry);
    if (!layer.addFeature(feature)) return {};
  }
  if (!layer.commitChanges()) return {};
  QgsVectorFileWriter::SaveVectorOptions options;
  const bool shp = path.endsWith(QLatin1String(".shp"));
  options.driverName = shp ? QStringLiteral("ESRI Shapefile") : QStringLiteral("GPKG");
  options.fileEncoding = QStringLiteral("UTF-8");
  options.layerName = QStringLiteral("parcels");
  if (QgsVectorFileWriter::writeAsVectorFormatV3(&layer, path, {}, options)
      != QgsVectorFileWriter::NoError) return {};
  return path;
}
QMap<QString, QByteArray> fileHashes(const QString& directory) {
  QMap<QString, QByteArray> hashes;
  for (const QString& name : QDir(directory).entryList(QDir::Files)) {
    QFile file(QDir(directory).filePath(name));
    if (file.open(QIODevice::ReadOnly))
      hashes.insert(name, QCryptographicHash::hash(file.readAll(), QCryptographicHash::Sha256));
  }
  return hashes;
}
QString outputUri(const PreparedReferenceMap& result) {
  return result.gpkgPath + QStringLiteral("|layername=") + result.tableName;
}
}

class CadastralTest : public QObject {
  Q_OBJECT
private slots:
  void focusesOnlyExactPnuWithoutEditing_data() {
    QTest::addColumn<QString>("destination");
    QTest::newRow("5186") << QStringLiteral("EPSG:5186");
    QTest::newRow("5187") << QStringLiteral("EPSG:5187");
  }

  void focusesOnlyExactPnuWithoutEditing() {
    QFETCH(QString, destination);
    QTemporaryDir files;
    QVERIFY(files.isValid());
    const auto first = box(200000, 550000, 25, 25);
    const auto second = box(200060, 550000, 20, 20);
    const QString path = writeParcels(files.filePath(QStringLiteral("lookup.gpkg")),
      {{1, QStringLiteral("1"), first}, {1, QStringLiteral("1"), second},
       {2, QStringLiteral("2"), box(202000, 550000, 20, 20)}});
    QVERIFY(!path.isEmpty());
    const auto original = fileHashes(files.path());
    QgsProject project;
    const QgsCoordinateReferenceSystem dest(destination);
    project.setCrs(dest);
    QgsMapCanvas canvas;
    canvas.setRenderFlag(false);
    canvas.resize(800, 600);
    canvas.setDestinationCrs(dest);
    auto* layer = new QgsVectorLayer(path, QStringLiteral("지적도"), QStringLiteral("ogr"));
    QVERIFY(layer->isValid());
    QVERIFY(CadastralImport::applyStyle(layer));
    project.addMapLayer(layer);
    auto* node = project.layerTreeRoot()->findLayer(layer->id());
    node->setItemVisibilityChecked(false);
    QgsPointXY marker;
    QVERIFY(CadastralImport::focusParcel(&project, &canvas, pnu(1), &marker));
    QCOMPARE(layer->selectedFeatureCount(), 2);
    QVERIFY(node->isVisible());
    QgsCoordinateTransform transform(sourceCrs(), dest, project.transformContext());
    QVERIFY(canvas.extent().contains(transform.transform(QgsPointXY(200010, 550010))));
    QVERIFY(canvas.extent().contains(transform.transform(QgsPointXY(200070, 550010))));
    QVERIFY(!canvas.extent().contains(transform.transform(QgsPointXY(202010, 550010))));
    QVERIFY(canvas.extent().contains(marker));
    QCOMPARE(project.crs().authid(), destination);
    QCOMPARE(layer->crs().authid(), QStringLiteral("EPSG:5186"));
    QVERIFY(!layer->isEditable());
    const auto selection = layer->selectedFeatureIds();
    const auto extent = canvas.extent();
    QVERIFY(!CadastralImport::focusParcel(&project, &canvas, pnu(999), &marker));
    QVERIFY(!CadastralImport::focusParcel(&project, &canvas, QStringLiteral("1' OR 1=1"), &marker));
    QCOMPARE(layer->selectedFeatureIds(), selection);
    QCOMPARE(canvas.extent(), extent);
    canvas.setLayers({});
    project.removeAllMapLayers();
    QCOMPARE(fileHashes(files.path()), original);
  }

  void importsWholeIntersectingParcels_data() {
    QTest::addColumn<QString>("suffix");
    QTest::addColumn<QString>("workAuthId");
    QTest::newRow("shp-same-crs") << QStringLiteral("shp") << QStringLiteral("EPSG:5186");
    QTest::newRow("gpkg-reproject") << QStringLiteral("gpkg") << QStringLiteral("EPSG:5187");
  }
  void importsWholeIntersectingParcels() {
    QFETCH(QString, suffix);
    QFETCH(QString, workAuthId);
    QTemporaryDir input;
    QTemporaryDir output;
    QVERIFY(input.isValid() && output.isValid());
    const QgsGeometry boundaryParcel = box(200045., 550000., 40., 10.);
    const QgsGeometry insideParcel = box(199995., 549995., 10., 10.);
    // The third parcel intersects the scope bounding box, but not its diamond.
    const QList<Parcel> parcels{{1, QStringLiteral("산 12-3"), boundaryParcel},
                               {2, QStringLiteral("15"), insideParcel},
                               {3, QStringLiteral("999"), box(200080., 550080., 10., 10.)}};
    const QString source = writeParcels(input.filePath(QStringLiteral("source.") + suffix), parcels);
    QVERIFY(!source.isEmpty());
    const auto originals = fileHashes(input.path());
    QVERIFY(!originals.isEmpty());
    QgsGeometry scope = QgsGeometry::fromWkt(QStringLiteral(
        "POLYGON((200000 549900,200100 550000,200000 550100,199900 550000,200000 549900))"));
    // An additional parcel protrudes through the diamond edge; never crop its outline.
    const QgsGeometry crossing = box(200075., 550000., 40., 10.);
    const QString extra = writeParcels(input.filePath(QStringLiteral("crossing.gpkg")),
                                        {{4, QStringLiteral("16-1"), crossing}});
    QVERIFY(!extra.isEmpty());
    const auto before = fileHashes(input.path());
    const QgsCoordinateReferenceSystem workCrs(workAuthId);
    const QgsCoordinateTransformContext context;
    QgsCoordinateTransform transform(sourceCrs(), workCrs, context);
    scope.transform(transform);
    const auto result = CadastralImport::prepare({source, extra}, scope, workCrs, context, output.path());
    QVERIFY2(result.isReady(), qPrintable(result.error));
    QCOMPARE(result.tableName, QStringLiteral("cadastral"));
    QgsVectorLayer layer(outputUri(result), QStringLiteral("result"), QStringLiteral("ogr"));
    QVERIFY(layer.isValid());
    QCOMPARE(layer.crs(), workCrs);
    QCOMPARE(layer.featureCount(), 3LL);
    QCOMPARE(layer.hasSpatialIndex(), Qgis::SpatialIndexPresence::Present);
    QSet<QString> fields;
    for (const QgsField& field : layer.fields()) fields.insert(field.name().toUpper());
    QVERIFY(fields.contains(QStringLiteral("PNU")));
    QVERIFY(fields.contains(QStringLiteral("JIBUN")));
    fields.remove(QStringLiteral("PNU"));
    fields.remove(QStringLiteral("JIBUN"));
    fields.remove(QStringLiteral("FID"));
    QVERIFY2(fields.isEmpty(), "Prepared cadastral data must not retain owner/private fields");
    QMap<QString, QgsGeometry> expected{{pnu(1), boundaryParcel}, {pnu(2), insideParcel}, {pnu(4), crossing}};
    QMap<QString, QString> labels{{pnu(1), QStringLiteral("산 12-3")}, {pnu(2), QStringLiteral("15")},
                                  {pnu(4), QStringLiteral("16-1")}};
    QgsFeature feature;
    auto features = layer.getFeatures();
    while (features.nextFeature(feature)) {
      const QString key = feature.attribute(QStringLiteral("PNU")).toString();
      QVERIFY(expected.contains(key));
      QCOMPARE(feature.attribute(QStringLiteral("JIBUN")).toString(), labels.value(key));
      QgsGeometry fullParcel = expected.take(key);
      fullParcel.transform(transform);
      const QgsGeometry difference = feature.geometry().symDifference(fullParcel);
      QVERIFY(!difference.isNull());
      QVERIFY(difference.area() < 0.01);
    }
    QVERIFY(expected.isEmpty());
    QCOMPARE(fileHashes(input.path()), before);
  }

  void overlappingSourcesDeduplicatePnu() {
    QTemporaryDir input;
    QTemporaryDir output;
    const Parcel a{1, QStringLiteral("1"), box(200000., 550000., 10., 10.)};
    const Parcel b{2, QStringLiteral("2"), box(200020., 550000., 10., 10.)};
    const QString first = writeParcels(input.filePath(QStringLiteral("first.gpkg")), {a});
    const QString second = writeParcels(input.filePath(QStringLiteral("second.shp")), {a, b});
    QVERIFY(!first.isEmpty() && !second.isEmpty());
    const auto result = CadastralImport::prepare({first, second}, box(199900., 549900., 400., 400.),
                                               sourceCrs(), {}, output.path());
    QVERIFY2(result.isReady(), qPrintable(result.error));
    QgsVectorLayer layer(outputUri(result), QStringLiteral("result"), QStringLiteral("ogr"));
    QCOMPARE(layer.featureCount(), 2LL);
    QSet<QString> unique;
    QgsFeature feature;
    auto iterator = layer.getFeatures();
    while (iterator.nextFeature(feature)) unique.insert(feature.attribute(QStringLiteral("PNU")).toString());
    QCOMPARE(unique, QSet<QString>({pnu(1), pnu(2)}));
  }

  void reusesCompletedCacheWithoutRewritingOriginalOrOutput() {
    QTemporaryDir input;
    QTemporaryDir output;
    const QString source = writeParcels(input.filePath(QStringLiteral("source.gpkg")),
        {{1, QStringLiteral("산 12-3"), box(200000., 550000., 10., 10.)}});
    QVERIFY(!source.isEmpty());
    const auto originals = fileHashes(input.path());
    const auto scope = box(199900., 549900., 400., 400.);
    const auto first = CadastralImport::prepare({source}, scope, sourceCrs(), {}, output.path());
    QVERIFY2(first.isReady(), qPrintable(first.error));
    // An old timestamp distinguishes a cache hit from a fast identical rewrite.
    QFile preparedFile(first.gpkgPath);
    QVERIFY(preparedFile.open(QIODevice::ReadWrite));
    QVERIFY(preparedFile.setFileTime(QDateTime::fromSecsSinceEpoch(946684800, QTimeZone::UTC),
                                     QFileDevice::FileModificationTime));
    preparedFile.close();
    const auto modified = QFileInfo(first.gpkgPath).lastModified();
    const auto preparedHashes = fileHashes(output.path());
    const auto second = CadastralImport::prepare({source}, scope, sourceCrs(), {}, output.path());
    QVERIFY2(second.isReady(), qPrintable(second.error));
    QCOMPARE(second.gpkgPath, first.gpkgPath);
    QCOMPARE(QFileInfo(second.gpkgPath).lastModified(), modified);
    QCOMPARE(fileHashes(output.path()), preparedHashes);
    QCOMPARE(fileHashes(input.path()), originals);
    QgsVectorLayer cached(outputUri(second), QStringLiteral("cached"), QStringLiteral("ogr"));
    QVERIFY(cached.isValid());
    QCOMPARE(cached.featureCount(), 1LL);
    QgsFeature feature;
    QVERIFY(cached.getFeatures().nextFeature(feature));
    QCOMPARE(feature.attribute(QStringLiteral("JIBUN")).toString(), QStringLiteral("산 12-3"));
  }

  void samePnuKeepsDistinctGeometryAndDeduplicatesEquivalentRings() {
    QTemporaryDir input;
    QTemporaryDir output;
    const QgsGeometry firstGeometry = box(200000., 550000., 10., 10.);
    const QgsGeometry secondGeometry = box(200030., 550000., 10., 10.);
    const QgsGeometry reversedRing = QgsGeometry::fromWkt(QStringLiteral(
        "POLYGON((200010 550010,200010 550000,200000 550000,200000 550010,200010 550010))"));
    const QString first = writeParcels(input.filePath(QStringLiteral("first.gpkg")),
        {{1, QStringLiteral("1"), firstGeometry}});
    const QString second = writeParcels(input.filePath(QStringLiteral("second.gpkg")),
        {{1, QStringLiteral("1"), reversedRing}, {1, QStringLiteral("1"), secondGeometry}});
    QVERIFY(!first.isEmpty() && !second.isEmpty());
    const auto result = CadastralImport::prepare({first, second}, box(199900., 549900., 400., 400.),
                                               sourceCrs(), {}, output.path());
    QVERIFY2(result.isReady(), qPrintable(result.error));
    QgsVectorLayer layer(outputUri(result), QStringLiteral("result"), QStringLiteral("ogr"));
    QCOMPARE(layer.featureCount(), 2LL);
    QgsGeometry combined;
    QgsFeature feature;
    auto iterator = layer.getFeatures();
    while (iterator.nextFeature(feature)) {
      QCOMPARE(feature.attribute(QStringLiteral("PNU")).toString(), pnu(1));
      combined = combined.isNull() ? feature.geometry() : combined.combine(feature.geometry());
    }
    const auto difference = combined.symDifference(firstGeometry.combine(secondGeometry));
    QVERIFY(!difference.isNull());
    QVERIFY(difference.area() < 0.01);
  }

  void rejectsInvalidInputs_data() {
    QTest::addColumn<QString>("caseName");
    for (const char* value : {"missing-jibun", "missing-crs", "missing-file", "invalid-work-crs", "empty-scope"})
      QTest::newRow(value) << QString::fromLatin1(value);
  }
  void rejectsInvalidInputs() {
    QFETCH(QString, caseName);
    QTemporaryDir input;
    QTemporaryDir output;
    QString source = writeParcels(input.filePath(QStringLiteral("source.shp")),
        {{1, QStringLiteral("1"), box(200000., 550000., 10., 10.)}},
        caseName != QLatin1String("missing-jibun"), caseName != QLatin1String("missing-crs"));
    QVERIFY(!source.isEmpty());
    if (caseName == QLatin1String("missing-file")) source = input.filePath(QStringLiteral("absent.shp"));
    const auto result = CadastralImport::prepare({source},
        caseName == QLatin1String("empty-scope") ? QgsGeometry() : box(199900., 549900., 400., 400.),
        caseName == QLatin1String("invalid-work-crs") ? QgsCoordinateReferenceSystem() : sourceCrs(),
        {}, output.path());
    QCOMPARE(result.status, PreparedReferenceMap::Status::Failed);
    QVERIFY(!result.error.isEmpty());
    QVERIFY(!result.isReady());
  }

  void cancellationDoesNotReturnReady() {
    QTemporaryDir input;
    QTemporaryDir output;
    const QString source = writeParcels(input.filePath(QStringLiteral("source.gpkg")),
        {{1, QStringLiteral("1"), box(200000., 550000., 10., 10.)}});
    QVERIFY(!source.isEmpty());
    const auto original = fileHashes(input.path());
    const auto result = CadastralImport::prepare({source}, box(199900., 549900., 400., 400.),
                                               sourceCrs(), {}, output.path(), [] { return true; });
    QCOMPARE(result.status, PreparedReferenceMap::Status::Cancelled);
    QVERIFY(!result.isReady());
    QVERIFY(!result.outputCommitted);
    QCOMPARE(fileHashes(input.path()), original);
  }

  void cancellationDuringPreparationLeavesNoCompletedMap() {
    QTemporaryDir input;
    QTemporaryDir output;
    const QString source = writeParcels(input.filePath(QStringLiteral("source.gpkg")),
        {{1, QStringLiteral("1"), box(200000., 550000., 10., 10.)}});
    QVERIFY(!source.isEmpty());
    bool cancel = false;
    const auto result = CadastralImport::prepare({source}, box(199900., 549900., 400., 400.),
        sourceCrs(), {}, output.path(), [&] { return cancel; },
        [&](int progress, const QString&) { if (progress < 100) cancel = true; });
    QVERIFY(cancel);
    QCOMPARE(result.status, PreparedReferenceMap::Status::Cancelled);
    QVERIFY(!result.outputCommitted);
    QVERIFY(QDir(output.path()).entryList({QStringLiteral("*.gpkg")}, QDir::Files).isEmpty());
  }

  void referenceLayerHasOutlineAndOptionalJibunLabels() {
    QTemporaryDir input;
    QTemporaryDir output;
    const QString source = writeParcels(input.filePath(QStringLiteral("source.gpkg")),
        {{1, QStringLiteral("산 23-1"), box(200000., 550000., 10., 10.)}});
    QVERIFY(!source.isEmpty());
    const auto prepared = CadastralImport::prepare({source}, box(199900., 549900., 400., 400.),
                                                 sourceCrs(), {}, output.path());
    QVERIFY2(prepared.isReady(), qPrintable(prepared.error));
    QgsProject project;
    project.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5187")));
    auto* survey = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5187"),
                                      QStringLiteral("기존 조사"), QStringLiteral("memory"));
    project.addMapLayer(survey);
    const QString surveyId = survey->id();
    QString error;
    auto* layer = CadastralImport::addPrepared(&project, nullptr, prepared, &error);
    QVERIFY2(layer, qPrintable(error));
    QCOMPARE(project.mapLayers().size(), 2);
    QCOMPARE(project.mapLayer(surveyId), survey);
    QCOMPARE(project.crs().authid(), QStringLiteral("EPSG:5187"));
    QVERIFY(LayerOps::isReferenceLayer(layer));
    auto* references = project.layerTreeRoot()->findGroup(QStringLiteral("참조 지도"));
    QVERIFY(references && references->findLayer(layer->id()));
    auto* renderer = dynamic_cast<QgsSingleSymbolRenderer*>(layer->renderer());
    QVERIFY(renderer && renderer->symbol());
    auto* fill = dynamic_cast<QgsSimpleFillSymbolLayer*>(renderer->symbol()->symbolLayer(0));
    QVERIFY(fill);
    QCOMPARE(fill->brushStyle(), Qt::NoBrush);
    QCOMPARE(fill->strokeColor(), QColor(Qt::black));
    QCOMPARE(fill->strokeWidth(), 0.2);
    QCOMPARE(fill->strokeWidthUnit(), Qgis::RenderUnit::Millimeters);
    QVERIFY(layer->labelsEnabled() && layer->labeling());
    QCOMPARE(layer->labeling()->settings().fieldName, QStringLiteral("JIBUN"));
    QVERIFY(CadastralImport::applyStyle(layer, QColor(QStringLiteral("#d02030")), false));
    QVERIFY(!layer->labelsEnabled());
    renderer = dynamic_cast<QgsSingleSymbolRenderer*>(layer->renderer());
    QVERIFY(renderer && renderer->symbol());
    fill = dynamic_cast<QgsSimpleFillSymbolLayer*>(renderer->symbol()->symbolLayer(0));
    QVERIFY(fill);
    QCOMPARE(fill->strokeColor(), QColor(QStringLiteral("#d02030")));
    QCOMPARE(fill->brushStyle(), Qt::NoBrush);
    const QString savedLayerId = layer->id();
    const QString projectPath = output.filePath(QStringLiteral("cadastral-style.qgz"));
    QVERIFY(project.write(projectPath));
    QgsProject reopened;
    QVERIFY(reopened.read(projectPath));
    auto* restored = qobject_cast<QgsVectorLayer*>(reopened.mapLayer(savedLayerId));
    QVERIFY(restored && restored->isValid());
    QVERIFY(LayerOps::isReferenceLayer(restored));
    QVERIFY(!restored->labelsEnabled());
    QVERIFY(restored->labeling());
    QCOMPARE(restored->labeling()->settings().fieldName, QStringLiteral("JIBUN"));
    auto* restoredGroup = reopened.layerTreeRoot()->findGroup(QStringLiteral("참조 지도"));
    QVERIFY(restoredGroup && restoredGroup->findLayer(savedLayerId));
    renderer = dynamic_cast<QgsSingleSymbolRenderer*>(restored->renderer());
    QVERIFY(renderer && renderer->symbol());
    fill = dynamic_cast<QgsSimpleFillSymbolLayer*>(renderer->symbol()->symbolLayer(0));
    QVERIFY(fill);
    QCOMPARE(fill->strokeColor(), QColor(QStringLiteral("#d02030")));
    QCOMPARE(fill->strokeWidth(), 0.2);
    QCOMPARE(fill->brushStyle(), Qt::NoBrush);
    QVERIFY(reopened.mapLayer(surveyId));
    QCOMPARE(reopened.crs(), project.crs());
  }

  void parsesObservedPortalRowsWithoutCrossPairing() {
    // Minimal public markup from 2026-09-16 search-gumi/search-gangseo pages.
    const QByteArray html = QStringLiteral(R"HTML(<ul>
      <li><div class="tit min">LSMD_CONT_LDREG_경북_구미시.zip</div>
       <span>용량<em>38</em>MB</span><button onClick="listFnc.download('30563', '64', '39394' );">다운로드</button></li>
      <li><div class="tit min">LSMD_CONT_LDREG_서울_강서구.zip</div>
       <button onClick="listFnc.download('30563', '215', '4649' );">다운로드</button></li>
      <li><div class="tit min">LSMD_CONT_LDREG_부산_강서구.zip</div>
       <button onClick="listFnc.download('30563', '216', '12982' );">다운로드</button></li>
      <li><div class="tit min">LSMD_CONT_LDREG_경북_없는군.zip</div></li>
      <li><button onClick="listFnc.download('30563', '999', '1');">다른 자료</button></li>
      </ul>)HTML").toUtf8();
    const auto parsed = CadastralPortal::parseResources(html);
    QCOMPARE(parsed.size(), 3);
    QMap<QString, CadastralPortal::Resource> byName;
    for (const auto& resource : parsed) byName.insert(resource.name, resource);
    const auto gumi = byName.value(QStringLiteral("LSMD_CONT_LDREG_경북_구미시.zip"));
    QCOMPARE(gumi.fileNo, QStringLiteral("64"));
    QCOMPARE(gumi.sizeKb, 39394LL);
    QCOMPARE(byName.value(QStringLiteral("LSMD_CONT_LDREG_서울_강서구.zip")).fileNo, QStringLiteral("215"));
    QCOMPARE(byName.value(QStringLiteral("LSMD_CONT_LDREG_부산_강서구.zip")).fileNo, QStringLiteral("216"));
  }

  void selectsExactProvinceAndNestedDistrict() {
    const QList<CadastralPortal::Resource> resources{
        {QStringLiteral("215"), QStringLiteral("LSMD_CONT_LDREG_서울_강서구.zip"), 4649},
        {QStringLiteral("216"), QStringLiteral("LSMD_CONT_LDREG_부산_강서구.zip"), 12982},
        {QStringLiteral("300"), QStringLiteral("LSMD_CONT_LDREG_경기_수원시_영통구.zip"), 100},
        {QStringLiteral("301"), QStringLiteral("LSMD_CONT_LDREG_경기_수원시_팔달구.zip"), 100}};
    QString error;
    auto selected = CadastralPortal::selectResources(resources,
        {{QStringLiteral("11500"), QStringLiteral("서울특별시 강서구")},
         {QStringLiteral("41117"), QStringLiteral("경기도 수원시 영통구")}}, &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(selected.size(), 2);
    QSet<QString> ids;
    for (const auto& resource : selected) ids.insert(resource.fileNo);
    QCOMPARE(ids, QSet<QString>({QStringLiteral("215"), QStringLiteral("300")}));
    selected = CadastralPortal::selectResources(resources,
        {{QStringLiteral("26440"), QStringLiteral("부산광역시 강서구")}}, &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(selected.size(), 1);
    QCOMPARE(selected.first().fileNo, QStringLiteral("216"));
  }

  void refusesMissingAndAmbiguousResources() {
    const QList<CadastralPortal::Resource> resources{
        {QStringLiteral("64"), QStringLiteral("LSMD_CONT_LDREG_경북_구미시.zip"), 39394}};
    QString error;
    auto selected = CadastralPortal::selectResources(resources,
        {{QStringLiteral("47190"), QStringLiteral("경상북도 구미시")},
         {QStringLiteral("11500"), QStringLiteral("서울특별시 강서구")}}, &error);
    QVERIFY(selected.isEmpty());
    QVERIFY(!error.isEmpty());
    auto ambiguous = resources;
    ambiguous.append({QStringLiteral("65"), resources.first().name, 39394});
    error.clear();
    selected = CadastralPortal::selectResources(ambiguous,
        {{QStringLiteral("47190"), QStringLiteral("경상북도 구미시")}}, &error);
    QVERIFY(selected.isEmpty());
    QVERIFY(!error.isEmpty());
  }

  void selectsSejongMunicipalityInsteadOfProvinceArchive() {
    const QList<CadastralPortal::Resource> resources{
        {QStringLiteral("306"), QStringLiteral("LSMD_CONT_LDREG_세종시.zip"), 100},
        {QStringLiteral("312"), QStringLiteral("LSMD_CONT_LDREG_세종.zip"), 200}};
    for (const QString& name : {QStringLiteral("세종특별자치시"), QStringLiteral("세종특별자치시 세종시")}) {
      QString error;
      const auto selected = CadastralPortal::selectResources(resources,
          {{QStringLiteral("36110"), name}}, &error);
      QVERIFY2(error.isEmpty(), qPrintable(error));
      QCOMPARE(selected.size(), 1);
      QCOMPARE(selected.first().fileNo, QStringLiteral("306"));
    }
  }

  void livePortalDownloadsFiveKilometerSurveyScope() {
    const QString directory = qEnvironmentVariable("KA_CADASTRAL_LIVE_QA");
    if (directory.isEmpty()) QSKIP("Live portal QA requires an explicitly supplied output directory");
    QVERIFY2(QFileInfo(directory).isAbsolute(), "Live QA output directory must be absolute");
    QCoreApplication::setOrganizationName(QStringLiteral("ka-hgis"));
    QCoreApplication::setApplicationName(QStringLiteral("ka-hgis"));
    CadastralPortal::Request request;
    request.directory = directory;
    request.credentials = CadastralPortal::credentials();
    request.apiKey = VworldSettings::loadApiKey();
    QVERIFY2(!request.credentials.username.isEmpty() && !request.credentials.password.isEmpty(),
             "Live QA requires locally configured VWorld credentials");
    QVERIFY2(!request.apiKey.isEmpty(), "Live QA requires a locally configured VWorld API key");
    request.surveyCrs = QgsCoordinateReferenceSystem(QStringLiteral("EPSG:4326"));
    request.workCrs = QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5187"));
    request.survey = box(128.3499, 36.1199, 0.0002, 0.0002);
    request.bufferMeters = 5000.;
    int lastProgress = -10;
    QString lastStage;
    QElapsedTimer elapsed;
    elapsed.start();
    const auto prepared = CadastralPortal::prepare(request, {}, [&](int progress, const QString& stage) {
      if (stage != lastStage || progress >= lastProgress + 5) {
        qInfo().noquote() << "CADASTRAL_LIVE stage=" << stage << "progress=" << progress;
        lastStage = stage;
        lastProgress = progress;
      }
    });
    qInfo() << "CADASTRAL_LIVE prepare_elapsed_ms=" << elapsed.elapsed();
    QVERIFY2(prepared.isReady(), qPrintable(prepared.error));
    QgsVectorLayer layer(outputUri(prepared), QStringLiteral("live QA result"), QStringLiteral("ogr"));
    QVERIFY(layer.isValid());
    QCOMPARE(layer.crs(), request.workCrs);
    QVERIFY(layer.featureCount() > 0);
    QCOMPARE(layer.hasSpatialIndex(), Qgis::SpatialIndexPresence::Present);
    qint64 originalFeatures = 0;
    QDirIterator originals(QDir(directory).filePath(QStringLiteral("원본")), QDir::Files,
                           QDirIterator::Subdirectories);
    while (originals.hasNext()) {
      const QString path = originals.next();
      if (!path.endsWith(QLatin1String(".shp"), Qt::CaseInsensitive)) continue;
      QgsVectorLayer original(path, QStringLiteral("QA source"), QStringLiteral("ogr"));
      QVERIFY(original.isValid());
      originalFeatures += original.featureCount();
    }
    qInfo() << "CADASTRAL_LIVE original_count=" << originalFeatures
            << "prepared_count=" << layer.featureCount();
    QVERIFY(originalFeatures >= layer.featureCount());
    QgsGeometry scope = request.survey;
    scope.transform(QgsCoordinateTransform(request.surveyCrs, request.workCrs, request.context));
    scope = scope.buffer(request.bufferMeters, 24);
    QgsFeature feature;
    auto iterator = layer.getFeatures();
    while (iterator.nextFeature(feature)) {
      QVERIFY(feature.geometry().intersects(scope));
      QVERIFY(!feature.attribute(QStringLiteral("JIBUN")).toString().trimmed().isEmpty());
    }
    QVERIFY(CadastralImport::applyStyle(&layer));
    QgsMapSettings settings;
    settings.setLayers({&layer});
    settings.setDestinationCrs(request.workCrs);
    settings.setTransformContext(request.context);
    settings.setOutputSize(QSize(1200, 1200));
    settings.setOutputDpi(96.);
    settings.setBackgroundColor(Qt::white);
    settings.setExtent(scope.boundingBox());
    elapsed.restart();
    QgsMapRendererParallelJob job(settings);
    job.start();
    job.waitForFinished();
    const auto image = job.renderedImage();
    QVERIFY(!image.isNull());
    const QString imagePath = QDir(directory).filePath(QStringLiteral("cadastral-5km.png"));
    QVERIFY(image.save(imagePath));
    qInfo().noquote() << "CADASTRAL_LIVE render_elapsed_ms=" << elapsed.elapsed() << "path=" << imagePath;
    // Check actual rendered ink, not only successful PNG serialization.
    int darkPixels = 0;
    for (int y = 0; y < image.height(); ++y)
      for (int x = 0; x < image.width(); ++x)
        if (qGray(image.pixel(x, y)) < 200) ++darkPixels;
    QVERIFY(darkPixels > 100);
    QgsGeometry surveyInWorkCrs = request.survey;
    surveyInWorkCrs.transform(QgsCoordinateTransform(request.surveyCrs, request.workCrs, request.context));
    QgsPointXY detailCenter = surveyInWorkCrs.centroid().asPoint();
    QImage detailImage;
    int placedLabels = 0;
    elapsed.restart();
    auto renderDetail = [&](const QgsPointXY& center, double halfWidth) {
      settings.setExtent(QgsRectangle(center.x() - halfWidth, center.y() - halfWidth,
                                      center.x() + halfWidth, center.y() + halfWidth));
      QgsMapRendererParallelJob detailJob(settings);
      detailJob.start();
      detailJob.waitForFinished();
      detailImage = detailJob.renderedImage();
      std::unique_ptr<QgsLabelingResults> labels(detailJob.takeLabelingResults());
      placedLabels = labels ? labels->allLabels().size() : 0;
    };
    renderDetail(detailCenter, 300.);
    if (!placedLabels) {
      // A survey may lie in a river parcel. Use a real small parcel for the
      // optional 300 m detail image without printing its identifiers or label.
      auto parcels = layer.getFeatures();
      QgsFeature parcel;
      while (parcels.nextFeature(parcel)) {
        if (!parcel.geometry().isEmpty() && parcel.geometry().area() < 10000.) {
          detailCenter = parcel.geometry().centroid().asPoint();
          break;
        }
      }
      renderDetail(detailCenter, 150.);
    }
    QVERIFY(!detailImage.isNull());
    const QString detailPath = QDir(directory).filePath(QStringLiteral("cadastral-jibun-detail.png"));
    QVERIFY(detailImage.save(detailPath));
    qInfo().noquote() << "CADASTRAL_LIVE detail_render_elapsed_ms=" << elapsed.elapsed()
                     << "placed_labels=" << placedLabels << "path=" << detailPath;
    QVERIFY(placedLabels > 0);
    qInfo().noquote() << "CADASTRAL_LIVE count=" << layer.featureCount()
                     << "CRS=" << layer.crs().authid() << "path=" << prepared.gpkgPath;
  }

  void prepareBreakdown_clipDedupFasterThanWkbHash() {
    QTemporaryDir input;
    QTemporaryDir output;
    QVERIFY(input.isValid() && output.isValid());
    QList<Parcel> parcels;
    constexpr int cols = 120;
    constexpr int rows = 80;
    parcels.reserve(cols * rows);
    for (int r = 0; r < rows; ++r) {
      for (int c = 0; c < cols; ++c) {
        parcels.append({r * cols + c + 1, QString::number(r * cols + c + 1),
                        box(200000. + c * 10., 550000. + r * 10., 8., 8.)});
      }
    }
    const QString source = writeParcels(input.filePath(QStringLiteral("grid.gpkg")), parcels);
    QVERIFY(!source.isEmpty());
    const QgsGeometry scope = box(200000., 550000., 800., 400.);

    QElapsedTimer oldTimer;
    oldTimer.start();
    QSet<QString> oldKeys;
    for (const Parcel& parcel : parcels) {
      QgsGeometry canonical = parcel.geometry;
      canonical.convertToMultiType();
      canonical.normalize();
      oldKeys.insert(QString::fromLatin1(
          QCryptographicHash::hash(canonical.asWkb(), QCryptographicHash::Sha256).toHex()));
    }
    const qint64 oldMs = qMax<qint64>(1, oldTimer.elapsed());

    QElapsedTimer newTimer;
    newTimer.start();
    QSet<QString> newKeys;
    for (const Parcel& parcel : parcels) {
      const QgsRectangle box = parcel.geometry.boundingBox();
      const int points =
          parcel.geometry.constGet() ? static_cast<int>(parcel.geometry.constGet()->nCoordinates()) : 0;
      newKeys.insert(QStringLiteral("%1:%2:%3:%4:%5:%6:%7")
                         .arg(pnu(parcel.id))
                         .arg(qRound64(box.xMinimum() * 1000.0))
                         .arg(qRound64(box.yMinimum() * 1000.0))
                         .arg(qRound64(box.xMaximum() * 1000.0))
                         .arg(qRound64(box.yMaximum() * 1000.0))
                         .arg(qRound64(parcel.geometry.area() * 1000.0))
                         .arg(points));
    }
    const qint64 newMs = newTimer.elapsed();
    qInfo() << "CADASTRAL_BREAKDOWN old_wkb_hash_ms=" << oldMs << "cheap_key_ms=" << newMs
            << "keys=" << oldKeys.size();
    QVERIFY2(newMs * 10 <= oldMs * 7,
             qPrintable(QStringLiteral("cheap key %1ms is not 30% faster than WKB hash %2ms")
                            .arg(newMs)
                            .arg(oldMs)));

    CadastralImport::PrepareBreakdown timing;
    QElapsedTimer prepareTimer;
    prepareTimer.start();
    const auto result =
        CadastralImport::prepare({source}, scope, sourceCrs(), {}, output.path(), {}, {}, &timing);
    const qint64 prepareMs = prepareTimer.elapsed();
    QVERIFY2(result.isReady(), qPrintable(result.error));
    QVERIFY2(timing.written >= 3200 && timing.written <= 3600,
             qPrintable(QStringLiteral("written=%1").arg(timing.written)));
    QVERIFY(timing.candidates >= timing.written);
    QVERIFY(timing.hashMs >= 0);
    QVERIFY(timing.clipMs >= 0);
    QVERIFY(timing.indexMs >= 0);

    QgsVectorLayer layer(outputUri(result), QStringLiteral("timed"), QStringLiteral("ogr"));
    QVERIFY(layer.isValid());
    QCOMPARE(layer.featureCount(), timing.written);
    QCOMPARE(layer.hasSpatialIndex(), Qgis::SpatialIndexPresence::Present);

    QElapsedTimer fontTimer;
    fontTimer.start();
    QVERIFY(CadastralImport::applyStyle(&layer));
    QgsMapSettings settings;
    settings.setLayers({&layer});
    settings.setDestinationCrs(sourceCrs());
    settings.setOutputSize(QSize(800, 600));
    settings.setOutputDpi(96.);
    settings.setExtent(scope.boundingBox());
    QgsMapRendererSequentialJob job(settings);
    job.start();
    job.waitForFinished();
    QVERIFY(!job.renderedImage().isNull());
    const qint64 fontMs = fontTimer.elapsed();
    QCOMPARE(layer.labeling()->settings().format().font().family(), QStringLiteral("Malgun Gothic"));
    qInfo() << "CADASTRAL_BREAKDOWN prepare_ms=" << prepareMs << "hash_ms=" << timing.hashMs
            << "clip_ms=" << timing.clipMs << "index_ms=" << timing.indexMs << "font_ms=" << fontMs
            << "candidates=" << timing.candidates << "written=" << timing.written;
  }
};

int main(int argc, char** argv) {
  QgsApplication app(argc, argv, false);
  QgsApplication::setPrefixPath(
      qEnvironmentVariable("QGIS_PREFIX_PATH", "A:/OSGeo4W/apps/qgis-dev"), true);
  QgsApplication::initQgis();
  CadastralTest test;
  const int result = QTest::qExec(&test, argc, argv);
  QgsApplication::exitQgis();
  return result;
}
#include "test_cadastral.moc"
