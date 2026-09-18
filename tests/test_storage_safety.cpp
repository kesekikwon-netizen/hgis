#include <stdexcept>
#include <QCryptographicHash>
#include <QDir>
#include <QDomDocument>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QTemporaryDir>
#include <QtTest>

#include "core/KaSafeQgis.h"
#include "core/LayerOps.h"
#include "core/SurveyProjectFactory.h"
#include "core/SurveyStorage.h"
#include <qgsapplication.h>
#include <qgsproject.h>
#include <qgsfeature.h>
#include <qgsgeometry.h>
#include <qgslayertree.h>
#include <qgsrasterlayer.h>
#include <qgsvectorlayer.h>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

class ReadLock {
 public:
  explicit ReadLock(const QString& path)
      : m_handle(CreateFileW(reinterpret_cast<LPCWSTR>(path.utf16()), GENERIC_READ, FILE_SHARE_READ, nullptr,
                             OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr)) {}
  ~ReadLock() {
    if (valid()) CloseHandle(m_handle);
  }
  bool valid() const { return m_handle != INVALID_HANDLE_VALUE; }
  ReadLock(const ReadLock&) = delete;
  ReadLock& operator=(const ReadLock&) = delete;

 private:
  HANDLE m_handle;
};
#endif

static QByteArray fileHash(const QString& path) {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) return {};
  QCryptographicHash hash(QCryptographicHash::Sha256);
  if (!hash.addData(&file)) return {};
  return hash.result();
}

static bool writeBytes(const QString& path, const QByteArray& bytes) {
  QFile file(path);
  return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size();
}

static QStringList temporaryArtifacts(const QString& path) {
  return QDir(path).entryList({QStringLiteral(".ka-new-survey-*"), QStringLiteral("*ka-writing*")},
                              QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot);
}

class TestStorageSafety : public QObject {
  Q_OBJECT
 private slots:
  void recoverySnapshot_preservesPendingEditsAndSource_data() {
    QTest::addColumn<QString>("crs");
    QTest::newRow("central") << QStringLiteral("EPSG:5186");
    QTest::newRow("east") << QStringLiteral("EPSG:5187");
  }

  void recoverySnapshot_preservesPendingEditsAndSource() {
    QFETCH(QString, crs);
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QString error;
    const QString source = SurveyProjectFactory::createNewSurvey(
        dir.path(), QStringLiteral("원본조사"), &error, crs);
    QVERIFY2(!source.isEmpty(), qPrintable(error));
    QgsProject project;
    project.setCrs(QgsCoordinateReferenceSystem(crs));
    project.setTitle(QStringLiteral("복구 전 현재 작업"));
    auto* area = LayerOps::ensureDomainLayer(&project, source, QStringLiteral("survey_area"),
                                             QStringLiteral("조사구역"), &error);
    auto* line = LayerOps::ensureDomainLayer(&project, source, QStringLiteral("feature_line"),
                                             QStringLiteral("미저장 선"), &error);
    QVERIFY2(area && line, qPrintable(error));
    for (auto* layer : {area, line}) {
      QVERIFY(layer->startEditing());
      QgsFeature feature(layer->fields());
      feature.setAttribute(QStringLiteral("note"), QStringLiteral("기존"));
      feature.setGeometry(layer == area
          ? QgsGeometry::fromRect(QgsRectangle(190000, 560000, 190010, 560010))
          : QgsGeometry::fromWkt(QStringLiteral("LineString (190000 560000, 190010 560010)")));
      QVERIFY(layer->addFeature(feature));
      QVERIFY(layer->commitChanges(false));
    }
    const QgsFeatureId areaId = *area->allFeatureIds().constBegin();
    QVERIFY(area->changeAttributeValue(areaId, area->fields().indexOf(QStringLiteral("note")),
                                       QStringLiteral("수정된 면")));
    QgsGeometry changedArea = QgsGeometry::fromRect(QgsRectangle(190020, 560020, 190040, 560040));
    QVERIFY(area->changeGeometry(areaId, changedArea));
    QgsFeature added(area->fields());
    added.setAttribute(QStringLiteral("note"), QStringLiteral("새로 추가한 면"));
    added.setGeometry(QgsGeometry::fromRect(QgsRectangle(190050, 560050, 190060, 560060)));
    QVERIFY(area->addFeature(added)); // NULL fid + existing fid must both survive writer.
    const QgsFeatureId lineId = *line->allFeatureIds().constBegin();
    QVERIFY(line->deleteFeature(lineId));
    QgsFeature replacement(line->fields());
    replacement.setAttribute(QStringLiteral("note"), QStringLiteral("삭제 뒤 대체한 선"));
    replacement.setGeometry(QgsGeometry::fromWkt(QStringLiteral("LineString (190030 560030, 190080 560080)")));
    QVERIFY(line->addFeature(replacement));
    auto* reference = new QgsVectorLayer(
        QStringLiteral("Point?crs=%1&field=note:string").arg(crs), QStringLiteral("참조점"), QStringLiteral("memory"));
    QVERIFY(reference->isValid());
    LayerOps::markReferenceLayer(reference);
    project.addMapLayer(reference);
    QVERIFY(reference->startEditing());
    QgsFeature point(reference->fields());
    point.setAttribute(QStringLiteral("note"), QStringLiteral("메모리에만 있는 점"));
    point.setGeometry(QgsGeometry::fromWkt(QStringLiteral("Point (190070 560070)")));
    QVERIFY(reference->addFeature(point));
    project.layerTreeRoot()->findLayer(reference->id())->setItemVisibilityChecked(false);
    auto* missingRaster = new QgsRasterLayer(dir.filePath(QStringLiteral("없는사진.tif")),
                                             QStringLiteral("원본이 없는 사진"), QStringLiteral("gdal"));
    QVERIFY(!missingRaster->isValid());
    LayerOps::markReferenceLayer(missingRaster);
    project.addMapLayer(missingRaster);
    project.setFileName(dir.filePath(QStringLiteral("현재작업.qgz")));
    project.setPresetHomePath(dir.path());
    project.setDirty(true);
    area->setAllowCommit(false);
    QVERIFY(!area->commitChanges(false));
    const QByteArray before = fileHash(source);
    const QString originalFile = project.fileName();
    const QString originalHome = project.presetHomePath();
    QMap<QString, QString> sources;
    QMap<QString, QMap<QString, QgsFeature>> expected;
    for (auto* layer : {area, line, reference}) {
      sources.insert(layer->name(), layer->source());
      auto iterator = layer->getFeatures();
      QgsFeature feature;
      while (iterator.nextFeature(feature))
        expected[layer->name()].insert(feature.attribute(QStringLiteral("note")).toString(), feature);
    }
    const QString folder = dir.filePath(QStringLiteral("복구 사본"));
    const QString first = SurveyStorage::writeRecoverySnapshot(&project, folder, &error);
    QVERIFY2(!first.isEmpty(), qPrintable(error));
    QVERIFY(QFileInfo::exists(first));
    QCOMPARE(fileHash(source), before);
    QCOMPARE(project.fileName(), originalFile);
    QCOMPARE(project.presetHomePath(), originalHome);
    QVERIFY(project.isDirty());
    for (auto* layer : {area, line, reference}) {
      QCOMPARE(layer->source(), sources.value(layer->name()));
      QVERIFY(layer->isEditable() && layer->isModified());
    }
    QgsProject restored;
    QVERIFY2(restored.read(SurveyStorage::projectUri(first)), qPrintable(restored.error()));
    QCOMPARE(restored.crs().authid(), crs);
    QCOMPARE(restored.mapLayers().size(), 4);
    const auto missing = restored.mapLayersByName(QStringLiteral("원본이 없는 사진"));
    QCOMPARE(missing.size(), 1);
    QVERIFY(!missing.first()->isValid());
    const QStringList external = restored.readListEntry(QStringLiteral("ka_hgis"),
                                                        QStringLiteral("recovery_external_references"));
    QVERIFY(external.join(QLatin1Char('\n')).contains(QStringLiteral("원본이 없는 사진")));
    for (auto* original : {area, line, reference}) {
      const auto matches = restored.mapLayersByName(original->name());
      QCOMPARE(matches.size(), 1);
      auto* snapshot = qobject_cast<QgsVectorLayer*>(matches.first());
      QVERIFY(snapshot && snapshot->isValid());
      QCOMPARE(snapshot->crs().authid(), original->crs().authid());
      QCOMPARE(LayerOps::layerKeyOf(snapshot), LayerOps::layerKeyOf(original));
      QCOMPARE(LayerOps::isReferenceLayer(snapshot), LayerOps::isReferenceLayer(original));
      QCOMPARE(snapshot->featureCount(), qint64(expected.value(original->name()).size()));
      auto* node = restored.layerTreeRoot()->findLayer(snapshot->id());
      QVERIFY(node);
      QCOMPARE(node->itemVisibilityChecked(),
               project.layerTreeRoot()->findLayer(original->id())->itemVisibilityChecked());
      auto iterator = snapshot->getFeatures();
      QgsFeature feature;
      while (iterator.nextFeature(feature)) {
        const QString key = feature.attribute(QStringLiteral("note")).toString();
        QVERIFY(expected.value(original->name()).contains(key));
        const QgsFeature prior = expected.value(original->name()).value(key);
        QVERIFY(feature.geometry().equals(prior.geometry()));
        for (const QgsField& field : original->fields()) {
          QVERIFY(snapshot->fields().lookupField(field.name()) >= 0);
          // 저장 왕복에서 생기는 차이는 데이터 손실이 아니다.
          // - 아직 저장되지 않은 도형의 fid 는 비어 있고, GPKG 가 기록할 때 새로 매긴다.
          // - 설정하지 않은 값은 NULL 로 두어도 빈 값으로 돌아온다.
          // 값이 있던 항목은 그대로 유지되어야 하며, 그것만 엄격히 비교한다.
          const QVariant actual = feature.attribute(field.name());
          const QVariant expectedValue = prior.attribute(field.name());
          if (expectedValue.isNull()) {
            const bool isFid = field.name().compare(QLatin1String("fid"), Qt::CaseInsensitive) == 0;
            if (isFid || actual.isNull() || actual.toString().isEmpty()) continue;
          }
          QCOMPARE(actual, expectedValue);
        }
      }
    }
    restored.clear();
    const QByteArray firstHash = fileHash(first);
    const QString second = SurveyStorage::writeRecoverySnapshot(&project, folder, &error);
    QVERIFY2(!second.isEmpty(), qPrintable(error));
    QVERIFY(second != first);
    QCOMPARE(fileHash(first), firstHash);
    QCOMPARE(fileHash(source), before);
    const QString blocked = dir.filePath(QStringLiteral("파일이라 폴더가 아님"));
    QVERIFY(writeBytes(blocked, QByteArray("keep")));
    QVERIFY(SurveyStorage::writeRecoverySnapshot(&project, blocked, &error).isEmpty());
    QVERIFY(!error.isEmpty());
    QCOMPARE(fileHash(first), firstHash);
    QCOMPARE(fileHash(source), before);
    QVERIFY(project.isDirty());
    for (auto* layer : {area, line, reference}) QVERIFY(layer->isEditable() && layer->isModified());
    area->setAllowCommit(true);
    for (auto* layer : {area, line, reference}) layer->rollBack();
  }

  void newSurvey_existingFilesRemainByteIdentical() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QString error;
    const QString gpkg = SurveyProjectFactory::createNewSurvey(dir.path(), QStringLiteral("보존할조사"), &error,
                                                               QStringLiteral("EPSG:5187"));
    QVERIFY2(!gpkg.isEmpty(), qPrintable(error));
    const QString qgz = dir.filePath(QStringLiteral("보존할조사.qgz"));
    const auto gpkgBefore = fileHash(gpkg);
    const auto qgzBefore = fileHash(qgz);
    QVERIFY(!gpkgBefore.isEmpty() && !qgzBefore.isEmpty());
    const QString duplicate = SurveyProjectFactory::createNewSurvey(
        dir.path(), QStringLiteral("보존할조사"), &error, QStringLiteral("EPSG:5186"));
    QCOMPARE(fileHash(gpkg), gpkgBefore);
    QCOMPARE(fileHash(qgz), qgzBefore);
    QVERIFY(duplicate.isEmpty());
    QVERIFY(!error.isEmpty());
    QVERIFY(temporaryArtifacts(dir.path()).isEmpty());
  }

  void newSurvey_orphanWorkspaceIsNotOverwritten() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString qgz = dir.filePath(QStringLiteral("보존할작업.qgz"));
    QVERIFY(writeBytes(qgz, QByteArray("existing workspace bytes")));
    const auto before = fileHash(qgz);
    QString error;
    const QString duplicate = SurveyProjectFactory::createNewSurvey(
        dir.path(), QStringLiteral("보존할작업"), &error, QStringLiteral("EPSG:5187"));
    QCOMPARE(fileHash(qgz), before);
    QVERIFY(duplicate.isEmpty());
    QVERIFY(!error.isEmpty());
    QVERIFY(!QFileInfo::exists(dir.filePath(QStringLiteral("보존할작업.gpkg"))));
    QVERIFY(temporaryArtifacts(dir.path()).isEmpty());
  }

  void newSurvey_invalidCrsLeavesNoPartialFiles() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QString error;
    QVERIFY(SurveyProjectFactory::createNewSurvey(dir.path(), QStringLiteral("실패한조사"), &error,
                                                  QStringLiteral("EPSG:999999999"))
                .isEmpty());
    QVERIFY(!error.isEmpty());
    QVERIFY(QDir(dir.path()).entryList(QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot).isEmpty());
  }

  void newSurvey_workspaceCollisionLeavesNoPartialGpkg() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString qgz = dir.filePath(QStringLiteral("막힌조사.qgz"));
    QVERIFY(QDir().mkpath(qgz));
    QVERIFY(writeBytes(QDir(qgz).filePath(QStringLiteral("keep.txt")), QByteArray("keep")));
    QString error;
    QVERIFY(SurveyProjectFactory::createNewSurvey(dir.path(), QStringLiteral("막힌조사"), &error,
                                                  QStringLiteral("EPSG:5187"))
                .isEmpty());
    QVERIFY(!error.isEmpty());
    QVERIFY(QFileInfo::exists(QDir(qgz).filePath(QStringLiteral("keep.txt"))));
    QVERIFY(!QFileInfo::exists(dir.filePath(QStringLiteral("막힌조사.gpkg"))));
    QVERIFY(temporaryArtifacts(dir.path()).isEmpty());
  }

  void newSurvey_schemaExistsButWorkspaceIsEmpty_data() {
    QTest::addColumn<QString>("crs");
    QTest::newRow("central") << QStringLiteral("EPSG:5186");
    QTest::newRow("east") << QStringLiteral("EPSG:5187");
  }

  void newSurvey_schemaExistsButWorkspaceIsEmpty() {
    QFETCH(QString, crs);
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QString error;
    const QString gpkg = SurveyProjectFactory::createNewSurvey(dir.path(), QStringLiteral("빈조사"), &error, crs);
    QVERIFY2(!gpkg.isEmpty(), qPrintable(error));
    QVERIFY(error.isEmpty());
    QVERIFY2(SurveyStorage::validateForOpen(gpkg, &error), qPrintable(error));
    QgsVectorLayer survey(gpkg + QStringLiteral("|layername=survey_area"), QStringLiteral("schema"),
                          QStringLiteral("ogr"));
    QVERIFY(survey.isValid());
    QCOMPARE(survey.featureCount(), 0LL);
    QCOMPARE(survey.crs().authid(), crs);
    QgsProject workspace;
    QVERIFY(workspace.read(dir.filePath(QStringLiteral("빈조사.qgz"))));
    QCOMPARE(workspace.crs().authid(), crs);
    QCOMPARE(workspace.mapLayers().size(), 0);
    QVERIFY(temporaryArtifacts(dir.path()).isEmpty());
  }

  void atomicWorkspace_preservesRelativeSourceAndPreviousGeneration() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QString error;
    const QString gpkg =
        SurveyProjectFactory::createNewSurvey(dir.path(), QStringLiteral("왕복"), &error, QStringLiteral("EPSG:5187"));
    QVERIFY2(!gpkg.isEmpty(), qPrintable(error));
    QgsProject project;
    project.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5187")));
    auto* vector = new QgsVectorLayer(gpkg + QStringLiteral("|layername=survey_area"), QStringLiteral("구역"),
                                      QStringLiteral("ogr"));
    QVERIFY(vector->isValid());
    project.addMapLayer(vector);
    const QString path = dir.filePath(QStringLiteral("작업 화면.qgz"));
    project.setTitle(QStringLiteral("첫 저장"));
    QVERIFY2(kaWriteQgisProjectAtomic(&project, path, &error), qPrintable(error));
    const auto before = fileHash(path);
    project.setTitle(QStringLiteral("두 번째 저장"));
    QVERIFY2(kaWriteQgisProjectAtomic(&project, path, &error), qPrintable(error));
    QCOMPARE(fileHash(kaProjectBackupPath(path)), before);
    QCOMPARE(project.fileName(), QFileInfo(path).absoluteFilePath());
    QgsProject reopened;
    QVERIFY(reopened.read(path));
    QCOMPARE(reopened.title(), QStringLiteral("두 번째 저장"));
    QCOMPARE(reopened.mapLayers().size(), 1);
    QVERIFY(reopened.mapLayers().first()->isValid());
    QCOMPARE(QFileInfo(reopened.mapLayers().first()->source().section(QLatin1Char('|'), 0, 0)).canonicalFilePath(),
             QFileInfo(gpkg).canonicalFilePath());
    QVERIFY(temporaryArtifacts(dir.path()).isEmpty());
  }

  void atomicWorkspace_lockedTargetPreservesFileAndUnsavedState() {
#ifdef Q_OS_WIN
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("잠긴작업.qgz"));
    QgsProject project;
    project.setTitle(QStringLiteral("이전 내용"));
    QString error;
    QVERIFY2(kaWriteQgisProjectAtomic(&project, path, &error), qPrintable(error));
    const auto before = fileHash(path);
    project.setFileName(QStringLiteral("original-session.qgz"));
    project.setTitle(QStringLiteral("아직 저장하지 못한 내용"));
    project.setDirty(true);
    ReadLock lock(path);
    QVERIFY(lock.valid());
    QVERIFY(!kaWriteQgisProjectAtomic(&project, path, &error));
    QVERIFY(!error.isEmpty());
    QCOMPARE(fileHash(path), before);
    QCOMPARE(project.fileName(), QStringLiteral("original-session.qgz"));
    QVERIFY(project.isDirty());
    QVERIFY(temporaryArtifacts(dir.path()).isEmpty());
#else
    QSKIP("Windows sharing-lock failure contract");
#endif
  }

  void atomicWorkspace_lockedBackupPreservesBothFiles() {
#ifdef Q_OS_WIN
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("백업실패.qgz"));
    const QString backup = kaProjectBackupPath(path);
    QgsProject project;
    QString error;
    project.setTitle(QStringLiteral("첫 저장"));
    QVERIFY2(kaWriteQgisProjectAtomic(&project, path, &error), qPrintable(error));
    project.setTitle(QStringLiteral("두 번째 저장"));
    QVERIFY2(kaWriteQgisProjectAtomic(&project, path, &error), qPrintable(error));
    const auto before = fileHash(path), backupBefore = fileHash(backup);
    project.setTitle(QStringLiteral("저장되지 않을 세 번째"));
    project.setDirty(true);
    ReadLock lock(backup);
    QVERIFY(lock.valid());
    QVERIFY(!kaWriteQgisProjectAtomic(&project, path, &error));
    QCOMPARE(fileHash(path), before);
    QCOMPARE(fileHash(backup), backupBefore);
    QVERIFY(project.isDirty());
    QVERIFY(temporaryArtifacts(dir.path()).isEmpty());
#else
    QSKIP("Windows sharing-lock failure contract");
#endif
  }

  void atomicWorkspace_serializationExceptionKeepsFileAndState() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("예외복구.qgz"));
    QgsProject project;
    QString error;
    QVERIFY2(kaWriteQgisProjectAtomic(&project, path, &error), qPrintable(error));
    const auto before = fileHash(path);
    project.setFileName(QStringLiteral("preserve-session.qgz"));
    project.setDirty(true);
    connect(
        &project, &QgsProject::writeProject, &project,
        [](QDomDocument&) { throw std::runtime_error("synthetic serialization failure"); }, Qt::DirectConnection);
    QVERIFY(!kaWriteQgisProjectAtomic(&project, path, &error));
    QVERIFY(!error.isEmpty());
    QCOMPARE(fileHash(path), before);
    QCOMPARE(project.fileName(), QStringLiteral("preserve-session.qgz"));
    QVERIFY(project.isDirty());
    QVERIFY(temporaryArtifacts(dir.path()).isEmpty());
  }
};

int main(int argc, char** argv) {
  QgsApplication app(argc, argv, true);
  QTemporaryDir settings;
  QgsApplication::setPrefixPath(qEnvironmentVariable("QGIS_PREFIX_PATH"), true);
  QgsApplication::initQgis();
  QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settings.path());
  TestStorageSafety tests;
  const int result = QTest::qExec(&tests, argc, argv);
  QgsApplication::exitQgis();
  return result;
}

#include "test_storage_safety.moc"

