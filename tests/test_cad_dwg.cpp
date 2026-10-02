// DWG를 DXF로 바꾸는 CadDwgConverter. 별도 프로그램(LibreDWG dwg2dxf)을 작업 폴더에서 ASCII 이름으로 돌리므로
// 한글·괄호·빈칸이 든 이름도 되고, 원본은 읽기만 한다. 망가진 파일·도구 없음·취소·시간 초과도 한국어 한 줄로 끝나고
// 찌꺼기가 남지 않아야 한다.
// tests/data/cad/r2000-small.dwg 는 같은 폴더의 r2000-small.dxf(층 WALL 에 LINE 하나, TEXT 「ABC」 하나)를
// `dxf2dwg --as r2000 -y -o r2000-small.dwg r2000-small.dxf` 로 만든 것이다.
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>

#include "cad_dwg_fixture.h"
#include "core/CadDwgConverter.h"

namespace {

QString toolPath() {
  return QString::fromUtf8(KA_TEST_LIBREDWG);
}

QString fixturePath() {
  return QDir(QString::fromUtf8(KA_TEST_CAD_DATA)).filePath(QStringLiteral("r2000-small.dwg"));
}

// 고정 도면을 임시 폴더 안에 name 으로 복사한다. 시험은 저장소의 원본 파일을 직접 쓰지 않는다.
QString copyFixtureTo(const QTemporaryDir& dir, const QString& name) {
  const QString target = dir.filePath(name);
  return QFile::copy(fixturePath(), target) ? target : QString();
}

}  // namespace

using namespace CadDwgFixture;

class TestCadDwg : public QObject {
  Q_OBJECT
 private slots:
  void initTestCase() { GDALAllRegister(); }

  // 한글·괄호·빈칸이 든 도면 이름과 작업 폴더 이름. dwg2dxf 에는 ASCII 이름만 간다.
  void convert_readsTheBundledFixture() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString dwg = copyFixtureTo(dir, QStringLiteral("[동국] 시험 도면(2010v).dwg"));
    QVERIFY(!dwg.isEmpty());
    const QString out = dir.filePath(QStringLiteral("작업 폴더 (변환)"));
    const CadDwgConverter::Result r = CadDwgConverter::convert(dwg, out, toolPath());
    QVERIFY2(r.ok, qUtf8Printable(r.error + QLatin1Char(' ') + r.details));
    QVERIFY(r.error.isEmpty());
    QCOMPARE(r.dxfPath, QDir(out).filePath(QStringLiteral("source.dxf")));
    QVERIFY(QFileInfo(r.dxfPath).isFile());
    // 성공하면 작업 사본 source.dwg 만 지운다.
    QVERIFY(!QFile::exists(QDir(out).filePath(QStringLiteral("source.dwg"))));
    const QStringList shapes = wallShapes(r.dxfPath);
    QVERIFY2(shapes.size() >= 2, qUtf8Printable(shapes.join(QLatin1Char(','))));
    QVERIFY2(shapes.contains(QStringLiteral("선")) && shapes.contains(QStringLiteral("글자:ABC")),
             qUtf8Printable(shapes.join(QLatin1Char(','))));
  }

  // 줄표(–)와 ✓ 는 한국어 ANSI 코드 페이지(cp949)에 없다. dwg2dxf 에 그런 이름을 넘기면 파일을 못 찾는다(실측).
  void convert_acceptsNamesTheAnsiCodePageCannotHold() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString dwg = copyFixtureTo(dir, QStringLiteral("소나무재선충 – 지적도 ✓.dwg"));
    QVERIFY(!dwg.isEmpty());
    const CadDwgConverter::Result r = CadDwgConverter::convert(dwg, dir.filePath(QStringLiteral("out")), toolPath());
    QVERIFY2(r.ok, qUtf8Printable(r.error + QLatin1Char(' ') + r.details));
    QVERIFY2(wallShapes(r.dxfPath).size() >= 2, qUtf8Printable(wallShapes(r.dxfPath).join(QLatin1Char(','))));
  }

  void convert_leavesTheOriginalUntouched() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString dwg = copyFixtureTo(dir, QStringLiteral("원본 도면.dwg"));
    QVERIFY(!dwg.isEmpty());
    const QByteArray before = sha256Of(dwg);
    QVERIFY(!before.isEmpty());
    const CadDwgConverter::Result r = CadDwgConverter::convert(dwg, dir.filePath(QStringLiteral("out")), toolPath());
    QVERIFY2(r.ok, qUtf8Printable(r.error + QLatin1Char(' ') + r.details));
    QVERIFY(QFile::exists(dwg));
    QCOMPARE(sha256Of(dwg), before);
  }

  void convert_missingToolSaysSoInKorean() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString dwg = copyFixtureTo(dir, QStringLiteral("도구 없음.dwg"));
    QVERIFY(!dwg.isEmpty());
    const CadDwgConverter::Result r =
        CadDwgConverter::convert(dwg, dir.filePath(QStringLiteral("out")), dir.filePath(QStringLiteral("없음.exe")));
    QVERIFY(!r.ok);
    QCOMPARE(r.error, QStringLiteral("DWG 변환 도구(LibreDWG)를 찾지 못했습니다."));
    // 어디서 찾았는지가 details 에 남는다.
    QVERIFY2(r.details.contains(QStringLiteral("없음.exe")), qUtf8Printable(r.details));
    QVERIFY(r.dxfPath.isEmpty());
  }

  void convert_brokenDwgFailsCleanly() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString broken = writeBrokenDwg(dir);
    QVERIFY(!broken.isEmpty());
    const QString out = dir.filePath(QStringLiteral("out"));
    const CadDwgConverter::Result r = CadDwgConverter::convert(broken, out, toolPath());
    QVERIFY(!r.ok);
    QCOMPARE(r.error, QStringLiteral("이 DWG를 읽지 못했습니다."));
    QVERIFY(r.dxfPath.isEmpty());
    // details 는 dwg2dxf 가 마지막으로 쓴 비어 있지 않은 줄이다(앞 줄에는 깨진 글자가 섞인다).
    QCOMPARE(r.details, QStringLiteral("READ ERROR 0x800"));
    QVERIFY(leavesNothingBehind(out));
    QVERIFY(QFile::exists(broken));
  }

  // 도구가 stderr 에 한 줄도 쓰지 않고 실패하면 details 는 「종료 코드 N」이다(가짜 도구: 아무것도 안 쓰고 3 으로 끝난다).
  void convert_silentFailureReportsTheExitCode() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString dwg = copyFixtureTo(dir, QStringLiteral("조용한 실패.dwg"));
    QVERIFY(!dwg.isEmpty());
    const QString out = dir.filePath(QStringLiteral("out"));
    FakeToolScope exitsWithThree(3);
    const CadDwgConverter::Result r = CadDwgConverter::convert(dwg, out, fakeToolPath());
    QVERIFY(!r.ok);
    QCOMPARE(r.error, QStringLiteral("이 DWG를 읽지 못했습니다."));
    QCOMPARE(r.details, QStringLiteral("종료 코드 3"));
    QVERIFY(r.dxfPath.isEmpty());
    QVERIFY(leavesNothingBehind(out));
  }

  // 작업 폴더에 묵은 source.dxf 가 있어도 망가진 도면은 실패이고, 그 DXF 는 남지 않는다.
  void convert_staleDxfDoesNotSurviveABrokenDwg() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString broken = writeBrokenDwg(dir);
    QVERIFY(!broken.isEmpty());
    const QString out = dir.filePath(QStringLiteral("out"));
    QVERIFY(writeFile(QDir(out).filePath(QStringLiteral("source.dxf")), QByteArrayLiteral("stale result")));
    const CadDwgConverter::Result r = CadDwgConverter::convert(broken, out, toolPath());
    QVERIFY(!r.ok);
    QCOMPARE(r.error, QStringLiteral("이 DWG를 읽지 못했습니다."));
    QVERIFY(r.dxfPath.isEmpty());
    QVERIFY(leavesNothingBehind(out));
  }

  // 도구가 아무것도 쓰지 않고 0 으로 끝나도(가짜 도구) 묵은 source.dxf 를 이번 결과로 받아들이면 안 된다.
  void convert_staleDxfIsNotMistakenForTheOutput() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString dwg = copyFixtureTo(dir, QStringLiteral("묵은 결과.dwg"));
    QVERIFY(!dwg.isEmpty());
    const QString out = dir.filePath(QStringLiteral("out"));
    QVERIFY(writeFile(QDir(out).filePath(QStringLiteral("source.dxf")), QByteArrayLiteral("stale result")));
    FakeToolScope exitsWithZeroWritingNothing(0);
    const CadDwgConverter::Result r = CadDwgConverter::convert(dwg, out, fakeToolPath());
    QVERIFY2(!r.ok, "the DXF left by an earlier run was taken for this run's output");
    QCOMPARE(r.error, QStringLiteral("이 DWG를 읽지 못했습니다."));
    QVERIFY(r.dxfPath.isEmpty());
    QVERIFY(leavesNothingBehind(out));
  }

  // 복사가 실패하면 아무것도 지우지 않는다. 작업 폴더에 남의 source.dwg 가 있어도 그대로 남는다.
  void convert_failedCopyLeavesAnExistingSourceDwgAlone() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString out = dir.filePath(QStringLiteral("out"));
    const QString existing = QDir(out).filePath(QStringLiteral("source.dwg"));
    QVERIFY(writeFile(existing, QByteArrayLiteral("not ours")));
    const QString missing = dir.filePath(QStringLiteral("없는 도면.dwg"));
    const CadDwgConverter::Result r = CadDwgConverter::convert(missing, out, toolPath());
    QVERIFY(!r.ok);
    QCOMPARE(r.error, QStringLiteral("이 DWG를 읽지 못했습니다."));
    QVERIFY2(r.details.contains(QStringLiteral("없는 도면.dwg")), qUtf8Printable(r.details));
    QCOMPARE(readFile(existing), QByteArrayLiteral("not ours"));
  }

  void convert_cancelKillsTheProcess() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString dwg = copyFixtureTo(dir, QStringLiteral("취소 도면.dwg"));
    QVERIFY(!dwg.isEmpty());
    const QString out = dir.filePath(QStringLiteral("out"));
    int asked = 0;
    const CadDwgConverter::Result r = CadDwgConverter::convert(dwg, out, toolPath(), 120000, [&asked] {
      ++asked;
      return true;
    });
    QVERIFY(!r.ok);
    QCOMPARE(r.error, QStringLiteral("DWG 변환을 취소했습니다."));
    QVERIFY(asked >= 1);
    QVERIFY(r.dxfPath.isEmpty());
    QVERIFY(leavesNothingBehind(out));
    // 프로세스가 정말 끝났다면 잠시 뒤에도 DXF 가 새로 생기지 않는다.
    QTest::qWait(500);
    QVERIFY(leavesNothingBehind(out));
  }

  void convert_timeoutKillsTheProcess() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString dwg = copyFixtureTo(dir, QStringLiteral("시간 초과.dwg"));
    QVERIFY(!dwg.isEmpty());
    const QString out = dir.filePath(QStringLiteral("out"));
    // 0 ms 는 시작하자마자 시간이 다 된 것이다.
    const CadDwgConverter::Result r = CadDwgConverter::convert(dwg, out, toolPath(), 0);
    QVERIFY(!r.ok);
    QCOMPARE(r.error, QStringLiteral("DWG 변환이 너무 오래 걸려 멈췄습니다."));
    QVERIFY(r.dxfPath.isEmpty());
    QVERIFY(leavesNothingBehind(out));
    QTest::qWait(500);
    QVERIFY(leavesNothingBehind(out));
  }

  // CD·공유 폴더에서 온 도면은 읽기 전용 속성이 붙어 있다. 작업 사본에 그 속성이 옮겨 가면 지워지지 않는다.
  void convert_readOnlyOriginalLeavesNoWorkCopy() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString dwg = copyFixtureTo(dir, QStringLiteral("읽기 전용.dwg"));
    QVERIFY(!dwg.isEmpty());
    QVERIFY(QFile::setPermissions(dwg, QFileDevice::ReadOwner));
    const QByteArray before = sha256Of(dwg);
    const QString out = dir.filePath(QStringLiteral("out"));
    const CadDwgConverter::Result r = CadDwgConverter::convert(dwg, out, toolPath());
    // 임시 폴더를 지울 수 있도록 먼저 되돌린다.
    QFile::setPermissions(dwg, QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    QVERIFY2(r.ok, qUtf8Printable(r.error + QLatin1Char(' ') + r.details));
    QVERIFY(!QFile::exists(QDir(out).filePath(QStringLiteral("source.dwg"))));
    QCOMPARE(sha256Of(dwg), before);
  }

  void bundledToolIn_findsOnlyAnExistingTool() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QCOMPARE(CadDwgConverter::bundledToolIn(dir.path()), QString());
    // tools/libredwg 폴더만 있고 도구가 없으면 여전히 없다.
    QVERIFY(QDir(dir.path()).mkpath(QStringLiteral("tools/libredwg")));
    QCOMPARE(CadDwgConverter::bundledToolIn(dir.path()), QString());
    const QString tool = dir.filePath(QStringLiteral("tools/libredwg/dwg2dxf.exe"));
    QVERIFY(writeFile(tool, QByteArrayLiteral("x")));
    QCOMPARE(CadDwgConverter::bundledToolIn(dir.path()), tool);
    // 앱 쪽 길은 앱 실행 파일 폴더를 같은 규칙으로 본다.
    QCOMPARE(CadDwgConverter::bundledTool(), CadDwgConverter::bundledToolIn(QCoreApplication::applicationDirPath()));
  }
};

int main(int argc, char** argv) {
  if (actsAsFakeTool(argc, argv)) return qEnvironmentVariableIntValue("KA_FAKE_DWG2DXF");
  QStandardPaths::setTestModeEnabled(true);
  QCoreApplication app(argc, argv);
  TestCadDwg tests;
  return QTest::qExec(&tests, argc, argv);
}

#include "test_cad_dwg.moc"
