// 한 번도 작업공간에 저장된 적 없는 도면 변환본만 지운다(CadPendingCopies). 사용자 「테스트」 조사에 저장 없이
// 다시 불러온 test1 (2)~(7) 이 쌓이던 일(2026-10-03, 사용자 결정 「저장 안 된 것만 지우기」). 저장·다른 이름으로
// 저장처럼 작업공간을 쓰는 모든 길이 그 작업공간이 쓰는 변환본을 「저장됨」으로 바꾼다.
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>

#include <memory>

#include "core/CadDrawingStore.h"
#include "core/CadPendingCopies.h"
#include "core/KaSafeQgis.h"
#include "core/SurveyBundle.h"
#include "core/SurveyStorage.h"

#include <qgsapplication.h>
#include <qgsproject.h>
#include <qgsvectorlayer.h>

namespace {

// 앱이 불러오기 때 만드는 것과 같은 변환본 한 장.
bool writeCopy(const QString& path) {
  CadEntity e;
  e.kind = CadKind::Line;
  e.geometry = QgsGeometry::fromPolylineXY({QgsPointXY(0, 0), QgsPointXY(10, 10)});
  e.cadLayer = QStringLiteral("JIJUK");
  e.color = QColor(QStringLiteral("#7f0000"));
  CadDrawing drawing;
  drawing.entities = {e};
  const CadStoreInfo info{QStringLiteral("C:/x/test1.dwg"), QStringLiteral("ab12"), QString(), QString()};
  QString error;
  return QDir().mkpath(QFileInfo(path).absolutePath()) &&
         CadDrawingStore::write(drawing, info, QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5187")),
                                QgsProject::instance()->transformContext(), path, &error);
}

std::unique_ptr<QgsVectorLayer> lines(const QString& gpkg) {
  return std::make_unique<QgsVectorLayer>(gpkg + QStringLiteral("|layername=lines"), QStringLiteral("선"),
                                          QStringLiteral("ogr"));
}

}  // namespace

class TestCadPendingCopies : public QObject {
  Q_OBJECT

  QTemporaryDir m_tmp;
  QString survey() const { return m_tmp.filePath(QStringLiteral("조사/테스트.gpkg")); }
  QString copy(const QString& name, const QString& folder = QStringLiteral("조사")) const {
    return m_tmp.filePath(folder + QStringLiteral("/") + SurveyBundle::collectedFolderName() + QStringLiteral("/도면/") +
                          name);
  }

 private slots:
  void init() {
    QFile::remove(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation) + QStringLiteral("/cad-copies.ini"));
    QDir(m_tmp.path()).removeRecursively();
    QVERIFY(QDir().mkpath(m_tmp.path()));
  }

  // 저장된 적 없고 지금 지도도 쓰지 않는 것만 지운다. 지금 지도가 쓰는 것, 사용자 파일, 다른 조사의 것은 남긴다.
  void removeUnsaved_removesOnlyCopiesNoWorkspaceEverUsed() {
    for (const QString& name : {QStringLiteral("test1.gpkg"), QStringLiteral("test1 (2).gpkg")}) {
      QVERIFY(writeCopy(copy(name)));
      CadPendingCopies::add(copy(name));
    }
    QVERIFY(writeCopy(copy(QStringLiteral("사용자 자료.gpkg"))));
    QVERIFY(writeCopy(copy(QStringLiteral("남의 것.gpkg"), QStringLiteral("다른 조사"))));
    CadPendingCopies::add(copy(QStringLiteral("남의 것.gpkg"), QStringLiteral("다른 조사")));
    QgsProject project;
    project.addMapLayer(lines(copy(QStringLiteral("test1 (2).gpkg"))).release());
    QCOMPARE(CadPendingCopies::removeUnsaved(&project, survey()), 1);
    QVERIFY(!QFile::exists(copy(QStringLiteral("test1.gpkg"))));
    QVERIFY(QFile::exists(copy(QStringLiteral("test1 (2).gpkg"))));
    // 연 조사가 쓰는 것은 저장된 작업공간에서 읽은 것이다: 이제 저장된 것으로 친다(코드 검토).
    QVERIFY(!CadPendingCopies::isPending(copy(QStringLiteral("test1 (2).gpkg"))));
    QVERIFY(QFile::exists(copy(QStringLiteral("사용자 자료.gpkg"))));
    QVERIFY(QFile::exists(copy(QStringLiteral("남의 것.gpkg"), QStringLiteral("다른 조사"))));
    QCOMPARE(CadPendingCopies::removeUnsaved(&project, QString()), 0);  // 조사가 없으면 아무것도 지우지 않는다
  }

  // .qgz(저장·다른 이름으로 저장의 동반 작업공간)나 GPKG 안(조사 파일)에 한 번 쓰인 변환본은
  // 지금 지도가 쓰지 않아도 남긴다.
  void writingAWorkspaceKeepsTheCopiesItUses() {
    for (const QString& name : {QStringLiteral("test1.gpkg"), QStringLiteral("test1 (2).gpkg")}) {
      QVERIFY(writeCopy(copy(name)));
      CadPendingCopies::add(copy(name));
    }
    {
      QgsProject saved;
      saved.addMapLayer(lines(copy(QStringLiteral("test1.gpkg"))).release());
      QString error;
      QVERIFY2(kaWriteQgisProjectAtomic(&saved, m_tmp.filePath(QStringLiteral("조사/테스트_복사본.qgz")), &error),
               qUtf8Printable(error));
    }
    {
      const QString surveyCopy = m_tmp.filePath(QStringLiteral("조사/테스트 - 복사본.gpkg"));
      QVERIFY(writeCopy(surveyCopy));  // 작업공간을 품는 GPKG 자리
      QgsProject saved;
      saved.addMapLayer(lines(copy(QStringLiteral("test1 (2).gpkg"))).release());
      QString error;
      QVERIFY2(SurveyStorage::writeEmbedded(&saved, surveyCopy, &error), qUtf8Printable(error));
    }
    QVERIFY(!CadPendingCopies::isPending(copy(QStringLiteral("test1.gpkg"))));
    QVERIFY(!CadPendingCopies::isPending(copy(QStringLiteral("test1 (2).gpkg"))));
    QTest::qWait(200);  // 저장한 프로젝트의 레이어가 파일을 놓을 때까지
    QgsProject project;
    QCOMPARE(CadPendingCopies::removeUnsaved(&project, survey()), 0);
    QVERIFY(QFile::exists(copy(QStringLiteral("test1.gpkg"))));
    QVERIFY(QFile::exists(copy(QStringLiteral("test1 (2).gpkg"))));
  }

  // 아직 열려 있어 못 지운 것은 저장 안 됨으로 남아 다음 조사 열기 때 지운다.
  void aCopyStillOpenIsRemovedNextTime() {
    QVERIFY(writeCopy(copy(QStringLiteral("test1.gpkg"))));
    CadPendingCopies::add(copy(QStringLiteral("test1.gpkg")));
    auto held = lines(copy(QStringLiteral("test1.gpkg")));  // 지운 묶음을 되돌리기(Ctrl+Z)가 붙든 것처럼
    QCOMPARE(held->featureCount(), 1);
    QgsProject project;
    QCOMPARE(CadPendingCopies::removeUnsaved(&project, survey()), 0);
    QVERIFY(CadPendingCopies::isPending(copy(QStringLiteral("test1.gpkg"))));
    held.reset();
    QTest::qWait(200);
    QCOMPARE(CadPendingCopies::removeUnsaved(&project, survey()), 1);
    QVERIFY(!QFile::exists(copy(QStringLiteral("test1.gpkg"))));
  }

  // 지운 변환본 자리에 나중에 들어온 다른 파일(다른 조사에서 복사한 같은 이름 변환본 등)은 지우지 않는다(코드 검토).
  void aFileThatTookADeletedCopysPlaceIsKept() {
    QVERIFY(writeCopy(copy(QStringLiteral("test1.gpkg"))));
    CadPendingCopies::add(copy(QStringLiteral("test1.gpkg")));
    QVERIFY(QFile::remove(copy(QStringLiteral("test1.gpkg"))));  // 다시 불러오기가 지운 것처럼
    QVERIFY(writeCopy(copy(QStringLiteral("test1.gpkg"))));
    QFile later(copy(QStringLiteral("test1.gpkg")));  // 나중에 만든 파일이라 만든 시각이 다르다
    QVERIFY(later.open(QIODevice::ReadWrite) &&
            later.setFileTime(QDateTime::currentDateTime().addDays(1), QFileDevice::FileBirthTime));
    later.close();
    QgsProject project;
    QCOMPARE(CadPendingCopies::removeUnsaved(&project, survey()), 0);
    QVERIFY(QFile::exists(copy(QStringLiteral("test1.gpkg"))));
    QVERIFY(!CadPendingCopies::isPending(copy(QStringLiteral("test1.gpkg"))));
  }

  // 「저장됨」을 목록에 적지 못하면(디스크가 가득 참 등) 목록을 버려 다음 실행에서 아무것도 지우지 않는다(코드 검토).
  void aLedgerThatCannotBeWrittenIsDropped() {
    QVERIFY(writeCopy(copy(QStringLiteral("test1.gpkg"))));
    CadPendingCopies::add(copy(QStringLiteral("test1.gpkg")));
    const QString ledger =
        QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation) + QStringLiteral("/cad-copies.ini");
    QVERIFY(QFile::exists(ledger));
    QVERIFY(QFile::setPermissions(ledger, QFileDevice::ReadOwner));
    QgsProject saved;
    saved.addMapLayer(lines(copy(QStringLiteral("test1.gpkg"))).release());
    CadPendingCopies::markSaved(&saved);
    const bool dropped = !QFile::exists(ledger);
    QFile::setPermissions(ledger, QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    QVERIFY(dropped);
  }
};

int main(int argc, char** argv) {
  QStandardPaths::setTestModeEnabled(true);
  QgsApplication app(argc, argv, false);
  QgsApplication::setPrefixPath(qEnvironmentVariable("QGIS_PREFIX_PATH"), true);
  QgsApplication::initQgis();
  int result = 0;
  {
    TestCadPendingCopies tests;
    result = QTest::qExec(&tests, argc, argv);
  }
  QgsApplication::exitQgis();
  return result;
}

#include "test_cad_pending_copies.moc"
