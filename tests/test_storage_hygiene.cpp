// Stale staging cleanup, crash-dump pruning, recovery revisions and bundle copies
// (Strata storage review F114, F116, F119, F133).
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>

#include "core/SurveyBundle.h"
#include "core/SurveyFileHygiene.h"
#include "core/SurveyRecovery.h"
#include <qgsapplication.h>
#include <qgsfeature.h>
#include <qgsgeometry.h>
#include <qgsproject.h>
#include <qgsvectorlayer.h>
#include <qgsvectorlayereditbuffer.h>

namespace {

bool writeFile(const QString& path, const QByteArray& bytes, const QDateTime& modified = {}) {
  QDir().mkpath(QFileInfo(path).absolutePath());
  QFile file(path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate) || file.write(bytes) != bytes.size()) return false;
  // Set the time after the bytes reached the OS so the close does not stamp it again.
  if (modified.isValid() && !(file.flush() && file.setFileTime(modified, QFileDevice::FileModificationTime)))
    return false;
  return true;
}

QByteArray readFile(const QString& path) {
  QFile file(path);
  return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

QStringList names(const QList<SurveyFileHygiene::StaleItem>& items) {
  QStringList out;
  for (const auto& item : items) out << QFileInfo(item.path).fileName();
  out.sort();
  return out;
}

}  // namespace

class TestStorageHygiene : public QObject {
  Q_OBJECT
 private slots:
  // F114/F133: only app staging next to the survey is offered; recovery copies, preserved
  // generations, session temp folders and anything the open project uses are never listed.
  void staleStaging_listsOnlyAppLeftovers() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString survey = dir.filePath(QStringLiteral("조사.gpkg"));
    QVERIFY(writeFile(survey, "gpkg"));
    QVERIFY(writeFile(dir.filePath(QStringLiteral(".ka-survey-gen-AAAAAA/survey.gpkg")), "orphan"));
    QVERIFY(writeFile(dir.filePath(QStringLiteral(".ka-survey-gen-KEEP01/survey.gpkg")), "kept"));
    QVERIFY(SurveyFileHygiene::markPreservedGeneration(dir.filePath(QStringLiteral(".ka-survey-gen-KEEP01")),
                                                       QStringLiteral("시험")));
    QVERIFY(writeFile(dir.filePath(QStringLiteral(".ka-survey-gen-PROT01/survey.gpkg")), "in use"));
    QVERIFY(writeFile(dir.filePath(QStringLiteral(".ka-survey-copy-BBBBBB/survey.gpkg")), "copy"));
    QVERIFY(writeFile(dir.filePath(QStringLiteral("조사.gpkg.ka-new")), "staged"));
    QVERIFY(writeFile(dir.filePath(QStringLiteral("조사.ka-writing-CCCCCC.qgz")), "qgz"));
    QVERIFY(writeFile(dir.filePath(QStringLiteral("복구사본/조사복구_20260101_000000_x/복구조사.gpkg")), "rec"));
    QVERIFY(writeFile(dir.filePath(QStringLiteral("ka-hgis-ABCDEF/raster.tif")), "session"));
    QVERIFY(writeFile(dir.filePath(QStringLiteral("가져온자료/주변유적/a.gpkg")), "data"));
    const QStringList protect{dir.filePath(QStringLiteral(".ka-survey-gen-PROT01/survey.gpkg")) +
                              QStringLiteral("|layername=survey_area")};
    QVERIFY(SurveyFileHygiene::staleStagingNear(survey, protect).isEmpty());  // all fresh
    const auto items = SurveyFileHygiene::staleStagingNear(survey, protect, 0);
    QCOMPARE(names(items), (QStringList{QStringLiteral(".ka-survey-copy-BBBBBB"),
                                        QStringLiteral(".ka-survey-gen-AAAAAA"),
                                        QStringLiteral("조사.gpkg.ka-new"),
                                        QStringLiteral("조사.ka-writing-CCCCCC.qgz")}));
    // A folder marked as preserved after the list was shown is skipped at deletion time.
    QVERIFY(SurveyFileHygiene::markPreservedGeneration(dir.filePath(QStringLiteral(".ka-survey-gen-AAAAAA")),
                                                       QStringLiteral("방금 실패")));
    QStringList failed;
    QCOMPARE(SurveyFileHygiene::removeStaleStaging(items, protect, &failed, 0), 3);
    QCOMPARE(failed.size(), 1);
    QVERIFY(failed.first().endsWith(QStringLiteral(".ka-survey-gen-AAAAAA")));
    for (const QString& kept : {QStringLiteral(".ka-survey-gen-AAAAAA"), QStringLiteral(".ka-survey-gen-KEEP01"),
                                QStringLiteral(".ka-survey-gen-PROT01"), QStringLiteral("복구사본"),
                                QStringLiteral("ka-hgis-ABCDEF"), QStringLiteral("가져온자료"),
                                QStringLiteral("조사.gpkg")})
      QVERIFY2(QFileInfo::exists(dir.filePath(kept)), qPrintable(kept));
    QVERIFY(!QFileInfo::exists(dir.filePath(QStringLiteral(".ka-survey-copy-BBBBBB"))));
    QVERIFY(!QFileInfo::exists(dir.filePath(QStringLiteral("조사.gpkg.ka-new"))));
  }

  // F116: the 2-minute copy is only needed when the pending edits changed.
  void editSignature_changesOnlyWithEdits() {
    QgsProject project;
    auto* layer = new QgsVectorLayer(QStringLiteral("Point?crs=EPSG:5186&field=note:string"),
                                     QStringLiteral("점"), QStringLiteral("memory"));
    QVERIFY(layer->isValid());
    project.addMapLayer(layer);
    const QStringList ids{layer->id()};
    const QByteArray clean = SurveyRecovery::editSignature(&project, ids);
    QVERIFY(!clean.isEmpty());
    QVERIFY(layer->startEditing());
    QgsFeature feature(layer->fields());
    feature.setAttribute(0, QStringLiteral("첫"));
    feature.setGeometry(QgsGeometry::fromWkt(QStringLiteral("Point (1 2)")));
    QVERIFY(layer->addFeature(feature));
    const QByteArray edited = SurveyRecovery::editSignature(&project, ids);
    QVERIFY(edited != clean);
    QCOMPARE(SurveyRecovery::editSignature(&project, ids), edited);  // nothing new: same revision
    QVERIFY(!SurveyRecovery::snapshotNeeded(edited, SurveyRecovery::editSignature(&project, ids)));
    const QgsFeatureId id = *layer->editBuffer()->addedFeatures().keyBegin();
    QVERIFY(layer->changeAttributeValue(id, 0, QStringLiteral("둘")));
    const QByteArray changed = SurveyRecovery::editSignature(&project, ids);
    QVERIFY(changed != edited);
    QVERIFY(SurveyRecovery::snapshotNeeded(edited, changed));
    QVERIFY(SurveyRecovery::snapshotNeeded(QByteArray(), changed));
    layer->rollBack();
  }

  // F119: a re-downloaded file with the same name and size but other content is copied again;
  // an unchanged file is reused, not duplicated.
  void collect_sameSizeDifferentContentIsCopiedAgain() {
    QTemporaryDir source;  // under the temp folder: app-managed data
    QTemporaryDir surveyDir;
    QVERIFY(source.isValid() && surveyDir.isValid());
    const QString csv = source.filePath(QStringLiteral("points.csv"));
    QVERIFY(writeFile(csv, "x,y,n\n1,2,a\n", QDateTime::currentDateTime().addSecs(-3600)));
    QgsProject project;
    auto* first = new QgsVectorLayer(csv, QStringLiteral("지점"), QStringLiteral("ogr"));
    QVERIFY(first->isValid());
    project.addMapLayer(first);
    QCOMPARE(SurveyBundle::collectIntoSurvey(&project, surveyDir.path()).copied, QStringList{QStringLiteral("지점")});
    const QString firstCopy = SurveyBundle::sourceFile(first->source());
    QVERIFY(firstCopy.startsWith(QDir::fromNativeSeparators(surveyDir.path()), Qt::CaseInsensitive));
    QVERIFY(writeFile(csv, "x,y,n\n3,4,b\n"));  // same size, new content, new time
    auto* second = new QgsVectorLayer(csv, QStringLiteral("지점"), QStringLiteral("ogr"));
    QVERIFY(second->isValid());
    project.addMapLayer(second);
    SurveyBundle::collectIntoSurvey(&project, surveyDir.path());
    const QString secondCopy = SurveyBundle::sourceFile(second->source());
    QVERIFY2(secondCopy.compare(firstCopy, Qt::CaseInsensitive) != 0, qPrintable(secondCopy));
    QCOMPARE(readFile(secondCopy), QByteArray("x,y,n\n3,4,b\n"));
    QCOMPARE(readFile(firstCopy), QByteArray("x,y,n\n1,2,a\n"));
    auto* third = new QgsVectorLayer(csv, QStringLiteral("지점"), QStringLiteral("ogr"));
    QVERIFY(third->isValid());
    project.addMapLayer(third);
    SurveyBundle::collectIntoSurvey(&project, surveyDir.path());
    QCOMPARE(SurveyBundle::sourceFile(third->source()).toLower(), secondCopy.toLower());
  }
};

int main(int argc, char** argv) {
  QStandardPaths::setTestModeEnabled(true);
  QTemporaryDir isolated;
  if (!isolated.isValid()) return 1;
  QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, isolated.filePath(QStringLiteral("settings")));
  qputenv("KA_HGIS_LOG_DIR", QDir::toNativeSeparators(isolated.filePath(QStringLiteral("logs"))).toLocal8Bit());
  QgsApplication app(argc, argv, true);
  QgsApplication::setPrefixPath(qEnvironmentVariable("QGIS_PREFIX_PATH"), true);
  QgsApplication::initQgis();
  TestStorageHygiene tests;
  const int result = QTest::qExec(&tests, argc, argv);
  QgsApplication::exitQgis();
  return result;
}

#include "test_storage_hygiene.moc"
