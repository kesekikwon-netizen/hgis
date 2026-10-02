// Generation save contracts added by the Strata storage review (F025, F026, F108, F109, F132, F165).
#include <stdexcept>

#include <QCryptographicHash>
#include <QDir>
#include <QDomDocument>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>

#include "core/EditHistory.h"
#include "core/LayerOps.h"
#include "core/SurveyDurability.h"
#include "core/SurveyFileFingerprint.h"
#include "core/SurveyProjectFactory.h"
#include "core/SurveySession.h"
#include "core/SurveyStorage.h"
#include <qgsapplication.h>
#include <qgscoordinatereferencesystem.h>
#include <qgsfeature.h>
#include <qgsgeometry.h>
#include <qgsproject.h>
#include <qgsrectangle.h>
#include <qgsvectorlayer.h>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace {

QByteArray fileHash(const QString& path) {
  QFile file(path);
  QCryptographicHash hash(QCryptographicHash::Sha256);
  return file.open(QIODevice::ReadOnly) && hash.addData(&file) ? hash.result() : QByteArray();
}

bool addArea(QgsVectorLayer* layer, const QString& note, double x) {
  if (!layer->isEditable() && !layer->startEditing()) return false;
  QgsFeature feature(layer->fields());
  feature.setAttribute(QStringLiteral("note"), note);
  feature.setGeometry(layer->geometryType() == Qgis::GeometryType::Line
      ? QgsGeometry::fromWkt(QStringLiteral("LineString (%1 450000, %2 450010)").arg(x).arg(x + 10))
      : QgsGeometry::fromRect(QgsRectangle(x, 450000, x + 10, 450010)));
  return layer->addFeature(feature);
}

}  // namespace

class TestStorageGeneration : public QObject {
  Q_OBJECT
 private slots:
  // F025/F132: a target that refuses the rename but allows writes must never be truncated in
  // place. The verified copy goes next to it as -저장.gpkg and the original bytes stay.
  void copySurvey_renameRefused_neverTruncatesOriginal() {
#ifdef Q_OS_WIN
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QString error;
    const QString source = SurveyProjectFactory::createNewSurvey(dir.path(), QStringLiteral("원본"), &error,
                                                                 QStringLiteral("EPSG:5186"));
    const QString target = SurveyProjectFactory::createNewSurvey(dir.path(), QStringLiteral("대상"), &error,
                                                                 QStringLiteral("EPSG:5186"));
    QVERIFY2(!source.isEmpty() && !target.isEmpty(), qPrintable(error));
    const QByteArray before = fileHash(target);
    HANDLE lock = CreateFileW(reinterpret_cast<LPCWSTR>(QDir::toNativeSeparators(target).utf16()), GENERIC_READ,
                              FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
                              nullptr);
    QVERIFY(lock != INVALID_HANDLE_VALUE);
    QString written;
    const bool copied = SurveyStorage::copySurvey(source, target, &error, &written);
    CloseHandle(lock);
    QVERIFY2(copied, qPrintable(error));
    QCOMPARE(fileHash(target), before);
    QVERIFY(written.contains(QStringLiteral("-저장")));
    QVERIFY2(SurveyStorage::validateForOpen(written, &error), qPrintable(error));
    QVERIFY(!QFileInfo::exists(target + QStringLiteral(".ka-new")));
#else
    QSKIP("Windows sharing-lock contract");
#endif
  }

  // F165: a failure after the edits reached the generation (here the embedded workspace write)
  // leaves every byte of the original and every edit buffer, for all layers.
  void persistWorkspace_lateFailure_neverCommitsOriginal() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QString error;
    const QString gpkg = SurveyProjectFactory::createNewSurvey(dir.path(), QStringLiteral("늦은실패"), &error,
                                                               QStringLiteral("EPSG:5186"));
    QVERIFY2(!gpkg.isEmpty(), qPrintable(error));
    QgsProject project;
    project.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")));
    auto* area = LayerOps::ensureDomainLayer(&project, gpkg, QStringLiteral("survey_area"), QStringLiteral("조사구역"), &error);
    auto* line = LayerOps::ensureDomainLayer(&project, gpkg, QStringLiteral("feature_line"), QStringLiteral("유구선"), &error);
    QVERIFY2(area && line, qPrintable(error));
    QVERIFY(addArea(area, QStringLiteral("면"), 200000));
    QVERIFY(addArea(line, QStringLiteral("선"), 200100));
    const QByteArray before = fileHash(gpkg);
    connect(&project, &QgsProject::writeProject, &project,
            [](QDomDocument&) { throw std::runtime_error("synthetic workspace failure"); }, Qt::DirectConnection);
    const auto attempt = SurveyStorage::persistWorkspace(&project, gpkg, dir.filePath(QStringLiteral("복구사본")));
    QVERIFY(!attempt.saved);
    QVERIFY(attempt.originalCommittedLayers.isEmpty());
    QCOMPARE(fileHash(gpkg), before);
    QVERIFY(area->isModified() && line->isModified());
    QVERIFY(!attempt.recoveryPath.isEmpty());
    QVERIFY(SurveySession::originalStateNote(attempt).contains(QStringLiteral("덮어쓰지 않았습니다")));
    area->rollBack();
    line->rollBack();
  }

  // User decision kept: a blocked layer still lets the other layers commit to the original, but
  // the failure now says so (F165).
  void persistWorkspace_blockedLayer_reportsPartialOriginalUpdate() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QString error;
    const QString gpkg = SurveyProjectFactory::createNewSurvey(dir.path(), QStringLiteral("막힘"), &error,
                                                               QStringLiteral("EPSG:5186"));
    QVERIFY2(!gpkg.isEmpty(), qPrintable(error));
    QgsProject project;
    project.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")));
    auto* area = LayerOps::ensureDomainLayer(&project, gpkg, QStringLiteral("survey_area"), QStringLiteral("조사구역"), &error);
    auto* line = LayerOps::ensureDomainLayer(&project, gpkg, QStringLiteral("feature_line"), QStringLiteral("유구선"), &error);
    QVERIFY2(area && line, qPrintable(error));
    QVERIFY(addArea(area, QStringLiteral("면"), 200000));
    QVERIFY(addArea(line, QStringLiteral("선"), 200100));
    line->setAllowCommit(false);
    const auto attempt = SurveyStorage::persistWorkspace(&project, gpkg, dir.filePath(QStringLiteral("복구사본")));
    line->setAllowCommit(true);
    QVERIFY(!attempt.saved);
    QCOMPARE(attempt.originalCommittedLayers, QStringList{QStringLiteral("조사구역")});
    QVERIFY2(attempt.error.contains(QStringLiteral("원본 조사 파일에 먼저 저장")), qPrintable(attempt.error));
    QVERIFY(SurveySession::originalStateNote(attempt).contains(QStringLiteral("조사구역")));
    QVERIFY(!area->isModified());
    QVERIFY(line->isModified());
    line->rollBack();
  }

  // F165 follow-up: the save drops the edit buffer the way a commit does. EditHistory must see
  // the saved commands leave the layer stack while applied (Committed); a stack that is undone
  // first and cleared afterwards reads as thrown away (Discarded), and the app then prunes its
  // own 되돌리기 entries right after every 저장.
  void persistWorkspace_saveLeavesCommandsCommittedNotDiscarded() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QString error;
    const QString gpkg = SurveyProjectFactory::createNewSurvey(dir.path(), QStringLiteral("저장기록"), &error,
                                                               QStringLiteral("EPSG:5186"));
    QVERIFY2(!gpkg.isEmpty(), qPrintable(error));
    QgsProject project;
    project.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")));
    auto* area = LayerOps::ensureDomainLayer(&project, gpkg, QStringLiteral("survey_area"), QStringLiteral("조사구역"), &error);
    QVERIFY2(area, qPrintable(error));
    EditHistory history;
    history.watch(area);
    QVERIFY(addArea(area, QStringLiteral("면"), 200000));
    const quint64 command = history.undoTop(area).id;
    QVERIFY(command != 0);
    QCOMPARE(history.state(command), EditHistory::State::Applied);
    const auto attempt = SurveyStorage::persistWorkspace(&project, gpkg, dir.filePath(QStringLiteral("복구사본")));
    QVERIFY2(attempt.saved, qPrintable(attempt.error));
    QCOMPARE(history.state(command), EditHistory::State::Committed);
    QVERIFY(area->isEditable() && !area->isModified());
    QCOMPARE(area->undoStack()->count(), 0);
    QCOMPARE(area->featureCount(), 1LL);
    QVERIFY(addArea(area, QStringLiteral("면2"), 200050));  // editing goes on after the save
    QVERIFY(area->isModified());
    area->rollBack();
  }

  // F108: in-place commits keep the file identity; a save elsewhere swaps the file and is seen.
  void fingerprint_seesReplacementButNotInPlaceWrites() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("조사.gpkg"));
    {
      QFile file(path);
      QVERIFY(file.open(QIODevice::WriteOnly));
      file.write("first");
    }
    SurveyFileFingerprint::remember(path);
    QVERIFY(SurveyFileFingerprint::isRemembered(path));
    {
      QFile file(path);
      QVERIFY(file.open(QIODevice::Append));
      file.write(" and in-place commit");
    }
    QVERIFY(!SurveyFileFingerprint::replacedElsewhere(path));
    if (SurveyFileFingerprint::capture(path).fileId.isEmpty()) QSKIP("파일 식별자를 얻을 수 없는 파일 시스템");
    const QString other = dir.filePath(QStringLiteral("다른PC저장.gpkg"));
    {
      QFile file(other);
      QVERIFY(file.open(QIODevice::WriteOnly));
      file.write("saved on another PC");
    }
    QVERIFY(SurveyDurability::replaceFileDurably(other, path));
    QString detail;
    QVERIFY(SurveyFileFingerprint::replacedElsewhere(path, &detail));
    QVERIFY(detail.contains(QStringLiteral("다른 이름으로 저장")));
    SurveyFileFingerprint::remember(path);
    QVERIFY(!SurveyFileFingerprint::replacedElsewhere(path));
    QVERIFY(QFile::remove(path));
    QVERIFY(SurveyFileFingerprint::replacedElsewhere(path));
    SurveyFileFingerprint::forget(path);
    QVERIFY(!SurveyFileFingerprint::replacedElsewhere(path));
  }

  // F108: our own publish updates the remembered state, so the next save does not warn.
  void persistWorkspace_ownSaveIsNotSeenAsElsewhere() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QString error;
    const QString gpkg = SurveyProjectFactory::createNewSurvey(dir.path(), QStringLiteral("내저장"), &error,
                                                               QStringLiteral("EPSG:5186"));
    QVERIFY2(!gpkg.isEmpty(), qPrintable(error));
    SurveyFileFingerprint::remember(gpkg);
    QgsProject project;
    project.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")));
    auto* area = LayerOps::ensureDomainLayer(&project, gpkg, QStringLiteral("survey_area"), QStringLiteral("조사구역"), &error);
    QVERIFY2(area, qPrintable(error));
    QVERIFY(addArea(area, QStringLiteral("면"), 200000));
    const auto attempt = SurveyStorage::persistWorkspace(&project, gpkg, dir.filePath(QStringLiteral("복구사본")));
    QVERIFY2(attempt.saved, qPrintable(attempt.error));
    QVERIFY(SurveyFileFingerprint::isRemembered(attempt.surveyPath));
    QVERIFY(!SurveyFileFingerprint::replacedElsewhere(attempt.surveyPath));
  }

  // F026: a companion .qgz that could not be written is marked, and the fallback notice says so.
  void persistWork_companionFailureMarksStaleWorkspace() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QString error;
    const QString gpkg = SurveyProjectFactory::createNewSurvey(dir.path(), QStringLiteral("동반"), &error,
                                                               QStringLiteral("EPSG:5186"));
    QVERIFY2(!gpkg.isEmpty(), qPrintable(error));
    const QString qgz = SurveySession::companionQgzPathFor(gpkg);
    QVERIFY(!SurveySession::companionMayBeStale(gpkg));
    QgsProject project;
    project.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")));
    auto* area = LayerOps::ensureDomainLayer(&project, gpkg, QStringLiteral("survey_area"), QStringLiteral("조사구역"), &error);
    QVERIFY2(area, qPrintable(error));
    QVERIFY(addArea(area, QStringLiteral("면"), 200000));
    QFile::remove(qgz);
    QVERIFY(QDir().mkpath(qgz));  // a folder with the companion name: the .qgz write must fail
    SurveySession::PersistInput in;
    in.surveyPath = gpkg;
    auto result = SurveySession::persistWork(&project, in);
    QVERIFY2(result.saved, qPrintable(result.workspace.error));
    QVERIFY(!result.companionSaved);
    QVERIFY(SurveySession::companionMayBeStale(gpkg));
    QVERIFY(SurveySession::embeddedFallbackNotice(gpkg, qgz).contains(QStringLiteral("오래된")));
    QVERIFY(QDir(qgz).removeRecursively());
    QVERIFY(addArea(area, QStringLiteral("면2"), 200050));
    result = SurveySession::persistWork(&project, in);
    QVERIFY2(result.saved && result.companionSaved, qPrintable(result.companionError));
    QVERIFY(!SurveySession::companionMayBeStale(gpkg));
    QVERIFY(!SurveySession::embeddedFallbackNotice(gpkg, qgz).contains(QStringLiteral("오래된")));
  }
};

int main(int argc, char** argv) {
  QStandardPaths::setTestModeEnabled(true);
  QTemporaryDir isolated;
  if (!isolated.isValid()) return 1;
  QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, isolated.filePath(QStringLiteral("settings")));
  qputenv("KA_HGIS_LOG_DIR", QDir::toNativeSeparators(isolated.filePath(QStringLiteral("logs"))).toLocal8Bit());
  qputenv("LOCALAPPDATA", QDir::toNativeSeparators(isolated.filePath(QStringLiteral("local"))).toLocal8Bit());
  QgsApplication app(argc, argv, true);
  QgsApplication::setPrefixPath(qEnvironmentVariable("QGIS_PREFIX_PATH"), true);
  QgsApplication::initQgis();
  TestStorageGeneration tests;
  const int result = QTest::qExec(&tests, argc, argv);
  QgsApplication::exitQgis();
  return result;
}

#include "test_storage_generation.moc"
