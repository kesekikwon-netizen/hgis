// 도면 좌표계 판단에 쓰는 단서(CadCrsHints): 도면이 밝힌 좌표계, 지명이 가리키는 시·도, 파일마다의 기억.
// 사용자는 도면 좌표계를 모른다고 보고, 도면이 스스로 밝힌 것을 먼저 쓴다(2026-10-03 사용자 목표).
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>

#include "core/CadCrsHints.h"

#include <qgsapplication.h>
#include <qgscoordinatereferencesystem.h>

class TestCadHints : public QObject {
  Q_OBJECT
 private slots:
  void init() {
    QFile::remove(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation) + QStringLiteral("/cad-crs.ini"));
  }

  // 사용자 DXF 「…(고경면 가수리)면적산출_5187.dxf」처럼 파일 이름 끝에 붙인 번호.
  void stated_readsEpsgNumberInFileName() {
    QCOMPARE(CadCrsHints::stated(QStringLiteral("C:/a/(고경면 가수리)면적산출_5187.dxf"), {}),
             QStringList({"EPSG:5187"}));
    QCOMPARE(CadCrsHints::stated(QStringLiteral("C:/a/EPSG5174 도면.dxf"), {}), QStringList({"EPSG:5174"}));
    QCOMPARE(CadCrsHints::stated(QStringLiteral("C:/a/test1.dwg"), {}), QStringList());
    QCOMPARE(CadCrsHints::stated(QStringLiteral("C:/a/도면51860.dxf"), {}), QStringList());  // 더 긴 숫자의 일부
    // 지번은 좌표계가 아니다(검토 2026-10-03).
    QCOMPARE(CadCrsHints::stated(QStringLiteral("C:/a/산5174.dxf"), {}), QStringList());
    QCOMPARE(CadCrsHints::stated(QStringLiteral("C:/a/5186-1번지 도면.dxf"), {}), QStringList());
  }

  void stated_readsPrjNextToTheDrawing() {
    QTemporaryDir tmp;
    // 의뢰처 파일 이름의 대괄호는 와일드카드가 아니다(검토 2026-10-03).
    const QString dxf = tmp.filePath(QStringLiteral("[동국] 도면.dxf"));
    QFile prj(tmp.filePath(QStringLiteral("[동국] 도면.prj")));
    QVERIFY(prj.open(QIODevice::WriteOnly));
    prj.write(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5174")).toWkt(Qgis::CrsWktVariant::Wkt1Esri).toUtf8());
    prj.close();
    QCOMPARE(CadCrsHints::stated(dxf, {}), QStringList({"EPSG:5174"}));
  }

  void stated_readsCrsNamesInTexts() {
    const QString dxf = QStringLiteral("C:/a/도면.dxf");
    QCOMPARE(CadCrsHints::stated(dxf, {QStringLiteral("좌표계: EPSG:5186")}), QStringList({"EPSG:5186"}));
    // GRS80 원점은 2010(FN 600000)과 2002(FN 500000)가 숫자로 100 km 다르다: 둘 다 내고 다음 단서가 고른다.
    QCOMPARE(CadCrsHints::stated(dxf, {QStringLiteral("측량 기준 GRS80 동부원점")}), QStringList({"EPSG:5187", "EPSG:5183"}));
    QCOMPARE(CadCrsHints::stated(dxf, {QStringLiteral("세계측지계 중부 원점")}), QStringList({"EPSG:5186", "EPSG:5181"}));
    QCOMPARE(CadCrsHints::stated(dxf, {QStringLiteral("Bessel 중부원점")}), QStringList({"EPSG:5174"}));
    QCOMPARE(CadCrsHints::stated(dxf, {QStringLiteral("중부원점")}), QStringList({"EPSG:5186", "EPSG:5181", "EPSG:5174"}));
    QCOMPARE(CadCrsHints::stated(dxf, {QStringLiteral("UTM-K")}), QStringList({"EPSG:5179"}));
    // 지번·면적 숫자는 좌표계가 아니다.
    QCOMPARE(CadCrsHints::stated(dxf, {QStringLiteral("산5174"), QStringLiteral("374-1답")}), QStringList());
  }

  void provinces_comeFromPlaceNames() {
    QCOMPARE(CadCrsHints::provinces({QStringLiteral("[동국]소나무재선충 방제 벌채사업 지표(고경면 가수리)면적산출_5187")}),
             QStringList({"경상북도"}));
    QCOMPARE(CadCrsHints::provinces({QStringLiteral("영천시 측량도")}), QStringList({"경상북도"}));
    QCOMPARE(CadCrsHints::provinces({QStringLiteral("제주특별자치도 조사")}), QStringList({"제주특별자치도"}));
    QCOMPARE(CadCrsHints::provinces({QStringLiteral("test1"), QStringLiteral("374-1답")}), QStringList());
    QCOMPARE(CadCrsHints::provinces({QStringLiteral("중구")}), QStringList());  // 여러 시·도에 있는 짧은 이름
    // 지번 글자가 수만 개여도 지명 하나를 찾는다(검토 2026-10-03: 글자마다 이름 3,500개를 훑지 않는다).
    QStringList many;
    for (int i = 0; i < 50000; ++i) many << QStringLiteral("%1-%2답").arg(i).arg(i % 7);
    many << QStringLiteral("영천시고경면가수리");
    QCOMPARE(CadCrsHints::provinces(many), QStringList({"경상북도"}));
  }

  // 같은 파일은 원본 내용(SHA256)으로 기억하고, 맞았던 좌표계는 최근 목록 앞에 둔다.
  void memory_remembersPerFileAndRecent() {
    CadCrsHints::remember(QStringLiteral("aa"), QStringLiteral("EPSG:5174"));
    CadCrsHints::remember(QStringLiteral("bb"), QStringLiteral("EPSG:5186"));
    QCOMPARE(CadCrsHints::remembered(QStringLiteral("aa")), QStringLiteral("EPSG:5174"));
    QCOMPARE(CadCrsHints::recent(), QStringList({"EPSG:5186", "EPSG:5174"}));
    CadCrsHints::remember(QStringLiteral("aa"), QStringLiteral("EPSG:5174"));
    QCOMPARE(CadCrsHints::recent(), QStringList({"EPSG:5174", "EPSG:5186"}));
    CadCrsHints::remember(QStringLiteral("aa"), QString());  // 「좌표 없는 도면으로 보기」를 고르면 잊는다
    QCOMPARE(CadCrsHints::remembered(QStringLiteral("aa")), QString());
    QCOMPARE(CadCrsHints::recent(), QStringList({"EPSG:5174", "EPSG:5186"}));
    QCOMPARE(CadCrsHints::remembered(QString()), QString());
  }
};

int main(int argc, char** argv) {
  QStandardPaths::setTestModeEnabled(true);
  QgsApplication app(argc, argv, false);
  QgsApplication::setPrefixPath(qEnvironmentVariable("QGIS_PREFIX_PATH"), true);
  QgsApplication::initQgis();
  int result = 0;
  {
    TestCadHints tests;
    result = QTest::qExec(&tests, argc, argv);
  }
  QgsApplication::exitQgis();
  return result;
}

#include "test_cad_hints.moc"
