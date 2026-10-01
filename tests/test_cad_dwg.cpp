// DWG를 DXF로 바꾸는 CadDwgConverter. 별도 프로그램(LibreDWG dwg2dxf)을 작업 폴더에서 ASCII 이름으로 돌리므로
// 한글·괄호·빈칸이 든 이름도 되고, 원본은 읽기만 한다. 망가진 파일·도구 없음·취소·시간 초과도 한국어 한 줄로 끝나고
// 찌꺼기가 남지 않아야 한다.
// tests/data/cad/r2000-small.dwg 는 같은 폴더의 r2000-small.dxf(층 WALL 에 LINE 하나, TEXT 「ABC」 하나)를
// `dxf2dwg --as r2000 -y -o r2000-small.dwg r2000-small.dxf` 로 만든 것이다.
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>

#include <random>

#include "core/CadDwgConverter.h"

#include <gdal.h>
#include <ogr_api.h>
#include <qgsapplication.h>

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

QByteArray sha256Of(const QString& path) {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) return {};
  return QCryptographicHash::hash(file.readAll(), QCryptographicHash::Sha256).toHex();
}

// GDAL DXF 드라이버로 열어 Layer 가 WALL 인 도형을 종류별 한 줄씩 돌려준다: 선은 "선", 점은 "글자:<내용>".
// DXF 를 열지 못하면 "GDAL 이 DXF 를 열지 못함" 한 줄만 돌려준다.
QStringList wallShapes(const QString& dxfPath) {
  const char* drivers[] = {"DXF", nullptr};
  GDALDatasetH ds =
      GDALOpenEx(dxfPath.toUtf8().constData(), GDAL_OF_VECTOR | GDAL_OF_READONLY, drivers, nullptr, nullptr);
  if (!ds) return {QStringLiteral("GDAL 이 DXF 를 열지 못함")};
  QStringList shapes;
  if (OGRLayerH layer = GDALDatasetGetLayerByName(ds, "entities")) {
    OGR_L_ResetReading(layer);
    while (OGRFeatureH feature = OGR_L_GetNextFeature(layer)) {
      if (QByteArray(OGR_F_GetFieldAsString(feature, OGR_F_GetFieldIndex(feature, "Layer"))) == "WALL") {
        OGRGeometryH geometry = OGR_F_GetGeometryRef(feature);
        const OGRwkbGeometryType type = geometry ? wkbFlatten(OGR_G_GetGeometryType(geometry)) : wkbUnknown;
        if (type == wkbLineString)
          shapes << QStringLiteral("선");
        else if (type == wkbPoint)
          shapes << QStringLiteral("글자:") +
                        QString::fromUtf8(OGR_F_GetFieldAsString(feature, OGR_F_GetFieldIndex(feature, "Text")));
        else
          shapes << QStringLiteral("기타");
      }
      OGR_F_Destroy(feature);
    }
  }
  GDALClose(ds);
  return shapes;
}

// 실패한 변환 뒤에는 작업 폴더에 DXF 도 source.dwg 도 없어야 한다.
bool leavesNothingBehind(const QString& outDir) {
  return QDir(outDir).entryList({QStringLiteral("*.dxf")}, QDir::Files).isEmpty() &&
         !QFile::exists(QDir(outDir).filePath(QStringLiteral("source.dwg")));
}

}  // namespace

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
    QVERIFY2(r.ok, qPrintable(r.error + QLatin1Char(' ') + r.details));
    QVERIFY(r.error.isEmpty());
    QCOMPARE(r.dxfPath, QDir(out).filePath(QStringLiteral("source.dxf")));
    QVERIFY(QFileInfo(r.dxfPath).isFile());
    // 성공하면 작업 사본 source.dwg 만 지운다.
    QVERIFY(!QFile::exists(QDir(out).filePath(QStringLiteral("source.dwg"))));
    const QStringList shapes = wallShapes(r.dxfPath);
    QVERIFY2(shapes.size() >= 2, qPrintable(shapes.join(QLatin1Char(','))));
    QVERIFY2(shapes.contains(QStringLiteral("선")) && shapes.contains(QStringLiteral("글자:ABC")),
             qPrintable(shapes.join(QLatin1Char(','))));
  }

  // 줄표(–)와 ✓ 는 한국어 ANSI 코드 페이지(cp949)에 없다. dwg2dxf 에 그런 이름을 넘기면 파일을 못 찾는다(실측).
  void convert_acceptsNamesTheAnsiCodePageCannotHold() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString dwg = copyFixtureTo(dir, QStringLiteral("소나무재선충 – 지적도 ✓.dwg"));
    QVERIFY(!dwg.isEmpty());
    const CadDwgConverter::Result r = CadDwgConverter::convert(dwg, dir.filePath(QStringLiteral("out")), toolPath());
    QVERIFY2(r.ok, qPrintable(r.error + QLatin1Char(' ') + r.details));
    QVERIFY2(wallShapes(r.dxfPath).size() >= 2, qPrintable(wallShapes(r.dxfPath).join(QLatin1Char(','))));
  }

  void convert_leavesTheOriginalUntouched() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString dwg = copyFixtureTo(dir, QStringLiteral("원본 도면.dwg"));
    QVERIFY(!dwg.isEmpty());
    const QByteArray before = sha256Of(dwg);
    QVERIFY(!before.isEmpty());
    const CadDwgConverter::Result r = CadDwgConverter::convert(dwg, dir.filePath(QStringLiteral("out")), toolPath());
    QVERIFY2(r.ok, qPrintable(r.error + QLatin1Char(' ') + r.details));
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
    QVERIFY2(r.details.contains(QStringLiteral("없음.exe")), qPrintable(r.details));
    QVERIFY(r.dxfPath.isEmpty());
  }

  void convert_brokenDwgFailsCleanly() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    // 4096 바이트 의사난수. 씨앗이 고정이라 늘 같은 바이트다.
    std::mt19937 random(12345);
    QByteArray junk(4096, 0);
    for (char& byte : junk)
      byte = static_cast<char>(random() & 0xFF);
    const QString broken = dir.filePath(QStringLiteral("broken.dwg"));
    {
      QFile file(broken);
      QVERIFY(file.open(QIODevice::WriteOnly));
      QCOMPARE(file.write(junk), junk.size());
    }
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
    QVERIFY2(r.ok, qPrintable(r.error + QLatin1Char(' ') + r.details));
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
    {
      QFile file(tool);
      QVERIFY(file.open(QIODevice::WriteOnly));
      QCOMPARE(file.write("x"), qint64(1));
    }
    QCOMPARE(CadDwgConverter::bundledToolIn(dir.path()), tool);
    // 앱 쪽 길은 앱 실행 파일 폴더를 같은 규칙으로 본다.
    QCOMPARE(CadDwgConverter::bundledTool(), CadDwgConverter::bundledToolIn(QCoreApplication::applicationDirPath()));
  }

  void converterLabel_namesTheBundledVersion() {
    QCOMPARE(CadDwgConverter::converterLabel(), QStringLiteral("LibreDWG 0.14"));
  }
};

int main(int argc, char** argv) {
  QStandardPaths::setTestModeEnabled(true);
  QTemporaryDir isolated;
  if (!isolated.isValid()) return 1;
  QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, isolated.filePath(QStringLiteral("settings")));
  QgsApplication app(argc, argv, true);
  QgsApplication::setPrefixPath(qEnvironmentVariable("QGIS_PREFIX_PATH"), true);
  QgsApplication::initQgis();
  TestCadDwg tests;
  const int result = QTest::qExec(&tests, argc, argv);
  QgsApplication::exitQgis();
  return result;
}

#include "test_cad_dwg.moc"
