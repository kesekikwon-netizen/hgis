#include <QtTest>
#include <QDir>
#include <QFile>
#include <QSet>
#include <QFileInfo>
#include <QScopeGuard>
#include <QTemporaryDir>
#include <cpl_conv.h>
#include <optional>
#include <qgsapplication.h>
#include <qgscategorizedsymbolrenderer.h>
#include <qgssinglesymbolrenderer.h>
#include <qgsvectorlayerlabeling.h>
#include <qgslayertree.h>
#include <qgslayertreegroup.h>
#include <qgslayertreelayer.h>
#include <qgslayertreemodellegendnode.h>
#include <qgsproject.h>
#include <qgssymbol.h>
#include <qgsvectorlayer.h>
#include <qgsvectordataprovider.h>
#include <qgsvectorfilewriter.h>
#include <qgsfeature.h>
#include <qgsgeometry.h>
#include <qgscoordinatereferencesystem.h>
#include <qgsrectangle.h>

#include "core/HeritageImport.h"
#include "core/HeritageLayoutNumbers.h"
#include "core/HeritagePledge.h"
#include "core/HeritageSiteLegend.h"
#include "core/HeritageStyle.h"
#include "core/LayerOps.h"

namespace {
bool writeZip(const QString& path, const QList<QPair<QString, QByteArray>>& entries) {
  void* archive = CPLCreateZip(path.toUtf8().constData(), nullptr);
  if (!archive) return false;
  bool ok = true;
  for (const auto& entry : entries) {
    if (CPLCreateFileInZip(archive, entry.first.toUtf8().constData(), nullptr) != CE_None) {
      ok = false; break;
    }
    ok = CPLWriteFileInZip(archive, entry.second.constData(), static_cast<int>(entry.second.size())) == CE_None;
    ok = CPLCloseFileInZip(archive) == CE_None && ok;
    if (!ok) break;
  }
  return CPLCloseZip(archive) == CE_None && ok;
}
QByteArray readFile(const QString& path) {
  QFile file(path);
  return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}
}

// 실제로 인트라넷에서 받은 ZIP 으로 **적재까지** 되는지 본다.
// 사이트에 붙지 않고 확인할 수 있어야 한다 — 받는 쪽이 막혀도 올리는 쪽은 굳어 있어야 하기 때문이다.
//
// 표본은 build/qa/heritage-sample/지정유산.zip (실제 내려받은 파일).
// 없으면 건너뛴다. 리포에 자료를 넣지 않는다(배포 금지).
class HeritageImportTest : public QObject {
  Q_OBJECT

  static QString samplePath() {
    return qEnvironmentVariable("KA_HERITAGE_SAMPLE_ZIP", "build/qa/heritage-sample/지정유산.zip");
  }

private slots:
  void cp949NamesSurviveAutomaticImport_data() {
    QTest::addColumn<bool>("withCpg");
    QTest::addColumn<QString>("inputEncoding");
    QTest::newRow("declared-cp949") << true << QStringLiteral("CP949");
    QTest::newRow("missing-cpg") << false << QStringLiteral("CP949");
    QTest::newRow("already-utf8") << true << QStringLiteral("UTF-8");
  }
  void cp949NamesSurviveAutomaticImport() {
    QFETCH(bool, withCpg);
    QFETCH(QString, inputEncoding);
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const bool cp949 = inputEncoding == QLatin1String("CP949");
    const QString koreanField = cp949 ? QStringLiteral("국가유산명") : QStringLiteral("명칭");
    QgsVectorLayer source(QStringLiteral("Polygon?crs=EPSG:5187&field=NAME:string(100)&field=%1:string(100)&field=NOTE:string(240)").arg(koreanField),
                          QStringLiteral("fixture"), QStringLiteral("memory"));
    QVERIFY(source.startEditing());
    QgsFeature feature(source.fields());
    const QString name = QStringLiteral("안동 하회리 유적");
    feature.setAttribute(0, name);
    feature.setAttribute(1, name);
    const QString longText = QStringLiteral("유적").repeated(cp949 ? 50 : 20);
    feature.setAttribute(2, longText);
    feature.setGeometry(QgsGeometry::fromWkt(QStringLiteral(
        "POLYGON((100000 400000,100010 400000,100010 400010,100000 400010,100000 400000))")));
    QVERIFY(source.addFeature(feature));
    QVERIFY(source.commitChanges());
    QgsVectorFileWriter::SaveVectorOptions options;
    options.driverName = QStringLiteral("ESRI Shapefile");
    options.fileEncoding = inputEncoding;
    const QString shp = temp.filePath(QStringLiteral("문화유적.shp"));
    QCOMPARE(QgsVectorFileWriter::writeAsVectorFormatV3(&source, shp, QgsCoordinateTransformContext(), options),
             QgsVectorFileWriter::NoError);
    const QString base = temp.filePath(QStringLiteral("문화유적"));
    if (!withCpg) QVERIFY(QFile::remove(base + QStringLiteral(".cpg")));
    QMap<QString, QByteArray> originals;
    for (const QString& suffix : {QStringLiteral(".shp"), QStringLiteral(".shx"), QStringLiteral(".dbf"),
                                 QStringLiteral(".prj"), QStringLiteral(".cpg")}) {
      if (QFileInfo::exists(base + suffix)) originals.insert(suffix, readFile(base + suffix));
    }
    const char* oldGlobal = CPLGetConfigOption("SHAPE_ENCODING", nullptr);
    const auto savedGlobal = oldGlobal ? std::optional<QByteArray>(oldGlobal) : std::nullopt;
    const char* oldLocal = CPLGetThreadLocalConfigOption("SHAPE_ENCODING", nullptr);
    const auto savedLocal = oldLocal ? std::optional<QByteArray>(oldLocal) : std::nullopt;
    const auto restoreEncoding = qScopeGuard([savedGlobal, savedLocal]() {
      CPLSetConfigOption("SHAPE_ENCODING", savedGlobal ? savedGlobal->constData() : nullptr);
      CPLSetThreadLocalConfigOption("SHAPE_ENCODING", savedLocal ? savedLocal->constData() : nullptr);
    });
    CPLSetConfigOption("SHAPE_ENCODING", "UTF-8");
    CPLSetThreadLocalConfigOption("SHAPE_ENCODING", "CP1252");
    QgsProject project;
    const auto imported = HeritageImport::loadDataset(&project, HeritageDataset::DesignatedHeritage,
                                                      {shp}, temp.filePath(QStringLiteral("cache")));
    QVERIFY2(imported.ok(), qPrintable(imported.error));
    QCOMPARE(QByteArray(CPLGetThreadLocalConfigOption("SHAPE_ENCODING", nullptr)), QByteArray("CP1252"));
    CPLSetThreadLocalConfigOption("SHAPE_ENCODING", nullptr);
    QCOMPARE(QByteArray(CPLGetConfigOption("SHAPE_ENCODING", nullptr)), QByteArray("UTF-8"));
    QCOMPARE(imported.layers.size(), 1);
    const QString workingFile = imported.layers.first()->source().section(QLatin1Char('|'), 0, 0);
    QCOMPARE(QFileInfo(workingFile).suffix(), QStringLiteral("gpkg"));
    QVERIFY(workingFile.startsWith(temp.filePath(QStringLiteral("cache"))));
    QVERIFY(readFile(workingFile).contains(name.toUtf8()));
    QCOMPARE(imported.layers.first()->crs(), source.crs());
    QCOMPARE(imported.layers.first()->featureCount(), source.featureCount());
    auto* reference = project.layerTreeRoot()->findGroup(HeritageImport::referenceGroupName());
    QVERIFY(reference);
    auto* kind = reference->findGroup(HeritageStyle::layerName(HeritageDataset::DesignatedHeritage));
    QVERIFY(kind);
    QVERIFY2(!kind->isExpanded(), "added heritage groups must open collapsed");
    for (QgsLayerTreeNode* child : kind->children())
      QVERIFY2(!child->isExpanded(), "added child layers must open collapsed");
    for (auto it = originals.cbegin(); it != originals.cend(); ++it)
      QCOMPARE(readFile(base + it.key()), it.value());
    QCOMPARE(QFileInfo::exists(base + QStringLiteral(".cpg")), withCpg);
    QgsFeature read;
    QVERIFY(imported.layers.first()->getFeatures().nextFeature(read));
    QCOMPARE(read.attribute(QStringLiteral("NAME")).toString(), name);
    QCOMPARE(read.attribute(koreanField).toString(), name);
    QCOMPARE(read.attribute(QStringLiteral("NOTE")).toString(), longText);
    QVERIFY(read.geometry().isTopologicallyEqual(feature.geometry()));
    const auto* renderer = dynamic_cast<const QgsSingleSymbolRenderer*>(imported.layers.first()->renderer());
    QVERIFY(renderer);
    QVERIFY(imported.layers.first()->labelsEnabled());
    const QString labelField = imported.layers.first()->labeling()->settings().fieldName;
    QVERIFY(labelField.contains(koreanField) || labelField.contains(QLatin1String("NAME")));
    const QString projectPath = temp.filePath(QStringLiteral("saved.qgs"));
    QVERIFY(project.write(projectPath));
    project.clear();
    QVERIFY(project.read(projectPath));
    QCOMPARE(project.mapLayers().size(), 1);
    auto* reopened = qobject_cast<QgsVectorLayer*>(project.mapLayers().first());
    QVERIFY(reopened && reopened->isValid());
    QVERIFY(reopened->getFeatures().nextFeature(read));
    QCOMPARE(read.attribute(koreanField).toString(), name);
    QCOMPARE(read.attribute(QStringLiteral("NOTE")).toString(), longText);
    reopened->dataProvider()->reloadData();
    project.clear();
  }

  void receivedIncompleteZipRequestsRetry() {
    const QString path = qEnvironmentVariable("KA_HERITAGE_INCOMPLETE_ZIP");
    if (path.isEmpty()) QSKIP("Local received ZIP inspection is opt-in.");
    QVERIFY(QFileInfo(path).isFile());
    const QByteArray before = readFile(path);
    QVERIFY(!before.isEmpty());
    QTemporaryDir temp;
    QgsProject project;
    const auto result = HeritageImport::loadDataset(
        &project, HeritageDataset::DesignatedHeritage, {path}, temp.path());
    QVERIFY(!result.ok());
    QVERIFY2(result.retryableDownload, qPrintable(result.error));
    QVERIFY(project.mapLayers().isEmpty());
    QCOMPARE(readFile(path), before);
  }
  void onlyIncompleteDownloadsRequestRetry_data() {
    QTest::addColumn<QString>("fixture");
    QTest::addColumn<bool>("retryable");
    QTest::newRow("missing-central-directory") << QStringLiteral("truncated") << true;
    QTest::newRow("zip-without-shp") << QStringLiteral("no-shp") << true;
    QTest::newRow("zip-missing-shp-companions") << QStringLiteral("missing-pairs") << true;
    QTest::newRow("raw-shp-missing-companions") << QStringLiteral("raw") << false;
    QTest::newRow("library-is-a-file") << QStringLiteral("storage") << false;
    QTest::newRow("provider-cannot-open-layer") << QStringLiteral("invalid-layer") << false;
  }
  void onlyIncompleteDownloadsRequestRetry() {
    QFETCH(QString, fixture);
    QFETCH(bool, retryable);
    QTemporaryDir temp; QVERIFY(temp.isValid());
    const auto source = temp.filePath(fixture == QLatin1String("raw")
        ? QStringLiteral("source.shp") : QStringLiteral("source.zip"));
    if (fixture == QLatin1String("raw")) {
      QFile file(source); QVERIFY(file.open(QIODevice::WriteOnly));
      QCOMPARE(file.write("incomplete shape"), qint64(16)); file.close();
    } else {
      QList<QPair<QString, QByteArray>> entries;
      entries.append({fixture == QLatin1String("no-shp") ? QStringLiteral("notice.txt")
                                                         : QStringLiteral("site.shp"), QByteArray("fixture")});
      if (fixture == QLatin1String("invalid-layer")) {
        for (const auto& suffix : {"shx", "dbf", "prj"})
          entries.append({QStringLiteral("site.%1").arg(QString::fromLatin1(suffix)), QByteArray("invalid")});
      }
      QVERIFY(writeZip(source, entries));
      if (fixture == QLatin1String("truncated")) {
        auto bytes = readFile(source);
        const auto central = bytes.indexOf(QByteArray::fromHex("504b0102")); QVERIFY(central >= 0);
        bytes.truncate(central);
        QFile file(source); QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write(bytes), bytes.size()); file.close();
      }
    }
    const auto original = readFile(source);
    const auto library = temp.filePath(QStringLiteral("library"));
    if (fixture == QLatin1String("storage")) {
      QFile blocker(library); QVERIFY(blocker.open(QIODevice::WriteOnly));
      QCOMPARE(blocker.write("keep"), qint64(4)); blocker.close();
    }
    QgsProject project;
    auto* existing = new QgsVectorLayer(QStringLiteral("Point?crs=EPSG:5179"),
                                        QStringLiteral("existing"), QStringLiteral("memory"));
    QVERIFY(existing->isValid()); project.addMapLayer(existing);
    const auto result = HeritageImport::loadDataset(
        &project, HeritageDataset::DesignatedHeritage, {source}, library);
    QVERIFY(!result.ok()); QVERIFY(!result.error.isEmpty()); QVERIFY(result.layers.isEmpty());
    QCOMPARE(result.retryableDownload, retryable);
    QCOMPARE(project.mapLayers().size(), 1); QCOMPARE(project.mapLayer(existing->id()), existing);
    QCOMPARE(readFile(source), original);
    if (fixture == QLatin1String("storage")) QCOMPARE(readFile(library), QByteArray("keep"));
  }
  void zipEncodingRestoresThreadLocalSetting_data() {
    QTest::addColumn<bool>("defined");
    QTest::addColumn<QByteArray>("value");
    QTest::newRow("unset") << false << QByteArray();
    QTest::newRow("empty-value") << true << QByteArray("");
    QTest::newRow("different-encoding") << true << QByteArray("UTF-8");
  }
  void zipEncodingRestoresThreadLocalSetting() {
    QFETCH(bool, defined);
    QFETCH(QByteArray, value);
    const char* old = CPLGetThreadLocalConfigOption("CPL_ZIP_ENCODING", nullptr);
    const auto saved = old ? std::optional<QByteArray>(old) : std::nullopt;
    const auto restore = qScopeGuard([saved]() {
      CPLSetThreadLocalConfigOption("CPL_ZIP_ENCODING", saved ? saved->constData() : nullptr);
    });
    CPLSetThreadLocalConfigOption("CPL_ZIP_ENCODING", defined ? value.constData() : nullptr);
    QTemporaryDir temp; QVERIFY(temp.isValid());
    const auto source = temp.filePath(QStringLiteral("empty-data.zip"));
    QVERIFY(writeZip(source, {{QStringLiteral("notice.txt"), QByteArray("fixture")}}));
    QgsProject project;
    const auto result = HeritageImport::loadDataset(
        &project, HeritageDataset::DesignatedHeritage, {source}, temp.filePath(QStringLiteral("library")));
    QVERIFY(result.retryableDownload); QVERIFY(project.mapLayers().isEmpty());
    const char* after = CPLGetThreadLocalConfigOption("CPL_ZIP_ENCODING", nullptr);
    QCOMPARE(after != nullptr, defined);
    if (defined) QCOMPARE(QByteArray(after), value);
  }
  void realZipBecomesStyledReferenceLayers() {
    if (!QFileInfo::exists(samplePath()))
      QSKIP("표본 ZIP 이 없습니다(build/qa/heritage-sample/지정유산.zip).");

    QTemporaryDir work;
    QVERIFY(work.isValid());
    QgsProject project;

    const HeritageImport::Result result = HeritageImport::loadDataset(
        &project, HeritageDataset::DesignatedHeritage, {samplePath()}, work.path());

    QVERIFY2(result.error.isEmpty(), qPrintable(result.error));
    QVERIFY2(!result.layers.isEmpty(), "레이어가 하나도 만들어지지 않았습니다.");
    QVERIFY(!result.retryableDownload);

    // 한 ZIP 에 여러 SHP 가 들어온다. 이름은 파일 이름을 살려야 레이어창에서 구분된다.
    QStringList names;
    for (QgsVectorLayer* layer : result.layers) names << layer->name();
    QVERIFY2(names.contains(QStringLiteral("국가지정유산")), qPrintable(names.join(QLatin1Char(','))));
    QVERIFY(names.contains(QStringLiteral("시도지정유산")));

    QSet<QString> layerColors;
    for (QgsVectorLayer* layer : result.layers) {
      QVERIFY2(layer->isValid(), qPrintable(layer->name()));
      QVERIFY2(layer->featureCount() > 0, qPrintable(layer->name()));

      // 좌표계는 5179 로 온다(사이트 안내: EPSG 5179).
      QVERIFY2(layer->crs().isValid(), qPrintable(layer->name()));

      // 조사 데이터와 섞이지 않게 참조 레이어로 표시되고 읽기 전용이어야 한다.
      QVERIFY(layer->property("readOnly").toBool());

      // 한 레이어 안에서는 색이 하나다(유적마다 갈리지 않는다).
      // 레이어끼리는 서로 다르다 — 아래에서 따로 확인한다.
      if (auto* single = dynamic_cast<QgsSingleSymbolRenderer*>(layer->renderer())) {
        QVERIFY(single->symbol());
        layerColors.insert(single->symbol()->color().name());
      } else if (auto* cat = dynamic_cast<QgsCategorizedSymbolRenderer*>(layer->renderer())) {
        QString one;
        for (const QgsRendererCategory& c : cat->categories()) {
          QVERIFY(c.symbol());
          if (one.isEmpty()) one = c.symbol()->color().name();
          QCOMPARE(c.symbol()->color().name(), one);
        }
        if (!one.isEmpty()) layerColors.insert(one);
      }

      // 레이어창에는 종류 한 줄만, 도면 범례에는 유적명 전부.
      QVERIFY(HeritageSiteLegend::isInstalled(layer));
      QgsLayerTreeLayer* panelNode = project.layerTreeRoot()->findLayer(layer->id());
      QVERIFY(panelNode);
      QList<QgsLayerTreeModelLegendNode*> panel =
          layer->legend()->createLayerTreeModelLegendNodes(panelNode);
      QCOMPARE(panel.size(), 1);
      qDeleteAll(panel);
    }

    // 지정유산 안의 6종은 **같은 색**이다. 구분은 색이 아니라 그룹으로 한다
    // (2026-09-13 사용자 확인: 6종은 전체 종류가 아니라 지정유산의 갈래다).
    QCOMPARE(layerColors.size(), 1);
    QCOMPARE(*layerColors.constBegin(),
             HeritageStyle::color(HeritageDataset::DesignatedHeritage).name());

    // 레이어창 구조: 참조 지도 › 지정유산 › 국가지정유산 …
    QgsLayerTreeGroup* reference =
        project.layerTreeRoot()->findGroup(HeritageImport::referenceGroupName());
    QVERIFY2(reference, "「참조 지도」 그룹이 없습니다.");
    QgsLayerTreeGroup* kind =
        reference->findGroup(HeritageStyle::layerName(HeritageDataset::DesignatedHeritage));
    QVERIFY2(kind, "「지정유산」 그룹이 없습니다.");
    QVERIFY2(!kind->isExpanded(), "지정유산 그룹은 접힌 채 올라와야 한다");
    QVERIFY2(reference->isVisible(), "참조 지도가 꺼져 있으면 유적이 안 보인다");
    QVERIFY2(kind->isVisible(), "지정유산 그룹이 꺼져 있으면 유적이 안 보인다");
    for (QgsVectorLayer* layer : result.layers)
      QVERIFY2(kind->findLayer(layer->id()), qPrintable(layer->name()));
    for (QgsVectorLayer* layer : result.layers) {
      QgsLayerTreeLayer* node = kind->findLayer(layer->id());
      QVERIFY(node);
      QVERIFY2(node->isVisible(), qPrintable(layer->name() + QStringLiteral(" 이 그룹에 가려 있다")));
    }
  }

  void siteNameFieldIsPickedFromRealAttributes() {
    if (!QFileInfo::exists(samplePath()))
      QSKIP("표본 ZIP 이 없습니다.");

    QTemporaryDir work;
    QgsProject project;
    const HeritageImport::Result result = HeritageImport::loadDataset(
        &project, HeritageDataset::DesignatedHeritage, {samplePath()}, work.path());
    QVERIFY(!result.layers.isEmpty());

    // 유적명 컬럼은 실제 필드에서 고른다. 이름을 지어내지 않는다.
    for (QgsVectorLayer* layer : result.layers) {
      const QString field = HeritageImport::chooseNameField(layer);
      if (field.isEmpty()) continue;  // 못 고르면 호출자가 알린다(그 자체는 실패가 아니다)
      QVERIFY2(layer->fields().indexOf(field) >= 0, qPrintable(field));
    }
  }

  void loadKeepsOnlySurveyBuffer() {
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    QgsVectorLayer source(QStringLiteral("Polygon?crs=EPSG:5186&field=nm:string(40)"),
                          QStringLiteral("fixture"), QStringLiteral("memory"));
    QVERIFY(source.startEditing());
    QgsFeature near(source.fields());
    near.setAttribute(0, QStringLiteral("근처"));
    near.setGeometry(QgsGeometry::fromWkt(QStringLiteral(
        "POLYGON((200000 450000,200020 450000,200020 450020,200000 450020,200000 450000))")));
    QVERIFY(source.addFeature(near));
    QgsFeature far(source.fields());
    far.setAttribute(0, QStringLiteral("멀리"));
    far.setGeometry(QgsGeometry::fromWkt(QStringLiteral(
        "POLYGON((230000 480000,230020 480000,230020 480020,230000 480020,230000 480000))")));
    QVERIFY(source.addFeature(far));
    QVERIFY(source.commitChanges());
    QgsVectorFileWriter::SaveVectorOptions options;
    options.driverName = QStringLiteral("ESRI Shapefile");
    options.fileEncoding = QStringLiteral("UTF-8");
    const QString shp = temp.filePath(QStringLiteral("유적.shp"));
    QCOMPARE(QgsVectorFileWriter::writeAsVectorFormatV3(&source, shp, QgsCoordinateTransformContext(), options),
             QgsVectorFileWriter::NoError);

    QgsProject project;
    project.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")));
    auto* survey = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5186"),
                                      QStringLiteral("조사구역"), QStringLiteral("memory"));
    LayerOps::markSurveyLayer(survey, QStringLiteral("survey_area"));
    QVERIFY(survey->startEditing());
    QgsFeature sa(survey->fields());
    sa.setGeometry(QgsGeometry::fromRect(QgsRectangle(200000., 450000., 200010., 450010.)));
    QVERIFY(survey->addFeature(sa));
    QVERIFY(survey->commitChanges());
    project.addMapLayer(survey);

    const auto imported = HeritageImport::loadDataset(&project, HeritageDataset::DesignatedHeritage,
                                                      {shp}, temp.filePath(QStringLiteral("cache")));
    QVERIFY2(imported.ok(), qPrintable(imported.error));
    QCOMPARE(imported.featureCount, 1);
    QgsFeature kept;
    QVERIFY(imported.layers.first()->getFeatures().nextFeature(kept));
    QCOMPARE(kept.attribute(HeritageImport::chooseNameField(imported.layers.first())).toString(),
             QStringLiteral("근처"));
    QVERIFY(QFileInfo::exists(shp));
  }

  // F120: a neighbouring 시·군 often has no site inside the 5 km scope. That is a valid
  // result (nothing to put on the map), not a failed download that stops the run.
  void nothingInsideScopeIsNotAFailure() {
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString shp = writeFixture(temp.filePath(QStringLiteral("이웃.shp")),
                                     {{QStringLiteral("멀리"), 230000., 480000.}});
    QVERIFY(!shp.isEmpty());
    QgsProject project;
    addSurveyArea(project);
    const auto imported = HeritageImport::loadDataset(&project, HeritageDataset::DesignatedHeritage,
                                                      {shp}, temp.filePath(QStringLiteral("cache")),
                                                      QStringLiteral("예천군"));
    QVERIFY2(imported.ok(), qPrintable(imported.error));
    QVERIFY(imported.emptyInScope);
    QVERIFY(imported.layers.isEmpty());
    QCOMPARE(imported.featureCount, 0);
    QVERIFY(!imported.messages.isEmpty());
    QVERIFY(imported.messages.first().contains(QStringLiteral("5km")));
    QVERIFY(imported.messages.first().contains(QStringLiteral("예천군")));
    QVERIFY(!project.layerTreeRoot()->findGroup(HeritageImport::referenceGroupName()));
  }

  // F120: layers of different 시·군 of one run are told apart; F169: intranet layers are
  // marked so the submit checklist can add its guidance.
  void regionLabelNamesLayersAndMarksIntranetData() {
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString shp = writeFixture(temp.filePath(QStringLiteral("국가지정유산.shp")),
                                     {{QStringLiteral("근처"), 200000., 450000.}});
    QVERIFY(!shp.isEmpty());
    QgsProject project;
    addSurveyArea(project);
    QVERIFY(!HeritagePledge::projectHasIntranetLayers(&project));
    const auto imported = HeritageImport::loadDataset(&project, HeritageDataset::DesignatedHeritage,
                                                      {shp}, temp.filePath(QStringLiteral("cache")),
                                                      QStringLiteral("예천군"));
    QVERIFY2(imported.ok(), qPrintable(imported.error));
    QVERIFY(!imported.emptyInScope);
    QCOMPARE(imported.layers.size(), 1);
    QCOMPARE(imported.layers.first()->name(), QStringLiteral("국가지정유산 · 예천군"));
    QVERIFY(HeritagePledge::isIntranetLayer(imported.layers.first()));
    QVERIFY(HeritagePledge::projectHasIntranetLayers(&project));
    QVERIFY(!HeritagePledge::checklistPasses(&project));
    // The intranet mark must not replace the logical dataset tag that layout numbers
    // read (ka_hgis/heritage_dataset stays the key, never the display name).
    QVERIFY(HeritageLayoutNumbers::taggedDataset(imported.layers.first()) ==
            HeritageDataset::DesignatedHeritage);
    QCOMPARE(imported.layers.first()->customProperty(QStringLiteral("ka_hgis/heritage_region")).toString(),
             QStringLiteral("예천군"));
  }

private:
  struct Site { QString name; double x; double y; };
  static QString writeFixture(const QString& shp, const QList<Site>& sites) {
    QgsVectorLayer source(QStringLiteral("Polygon?crs=EPSG:5186&field=nm:string(40)"),
                          QStringLiteral("fixture"), QStringLiteral("memory"));
    if (!source.startEditing()) return {};
    for (const Site& site : sites) {
      QgsFeature f(source.fields());
      f.setAttribute(0, site.name);
      f.setGeometry(QgsGeometry::fromRect(QgsRectangle(site.x, site.y, site.x + 20., site.y + 20.)));
      if (!source.addFeature(f)) return {};
    }
    if (!source.commitChanges()) return {};
    QgsVectorFileWriter::SaveVectorOptions options;
    options.driverName = QStringLiteral("ESRI Shapefile");
    options.fileEncoding = QStringLiteral("UTF-8");
    return QgsVectorFileWriter::writeAsVectorFormatV3(&source, shp, QgsCoordinateTransformContext(),
                                                     options) == QgsVectorFileWriter::NoError
               ? shp
               : QString();
  }
  static void addSurveyArea(QgsProject& project) {
    project.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")));
    auto* survey = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5186"),
                                      QStringLiteral("조사구역"), QStringLiteral("memory"));
    LayerOps::markSurveyLayer(survey, QStringLiteral("survey_area"));
    survey->startEditing();
    QgsFeature sa(survey->fields());
    sa.setGeometry(QgsGeometry::fromRect(QgsRectangle(200000., 450000., 200010., 450010.)));
    survey->addFeature(sa);
    survey->commitChanges();
    project.addMapLayer(survey);
  }
};

int main(int argc, char** argv) {
  QgsApplication app(argc, argv, false);
  QgsApplication::setPrefixPath(
      qEnvironmentVariable("QGIS_PREFIX_PATH", "D:/OSGeo4W/apps/qgis-dev"), true);
  QgsApplication::initQgis();
  HeritageImportTest test;
  const int result = QTest::qExec(&test, argc, argv);
  QgsApplication::exitQgis();
  return result;
}
#include "test_heritage_import.moc"
