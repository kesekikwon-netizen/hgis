// 도면 좌표계 고르기 창은 그대로 두되, 맞으면 그 도면은 다음부터 창을 끌 수 있다(2026-10-03 사용자:
// 「한번된것은 이런것으로 안막았으면한다」, 「잘되고있던게 왜 또이상해진것인가」, 「창을 유지하되 맞으면 창을 끌수있게하라」).
// 창의 「다음부터 묻지 않기」를 켜면 고른 좌표계를 원본 파일 내용(SHA256)으로 기억한다. 이름만 다른 복사본도 같은 도면이다.
// 04:01 에는 창을 없앤 앱이 첫 후보 5186 을 골라 부산 앞바다에 놓았다.
#include <QDir>
#include <QFile>
#include <QMainWindow>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>

#include "app/KaCadCrsDialog.h"
#include "app/KaCadImport.h"
#include "cad_fixture.h"
#include "core/HeritageImport.h"

#include <qgsapplication.h>
#include <qgslayertree.h>
#include <qgsmapcanvas.h>
#include <qgsmessagebar.h>
#include <qgsproject.h>
#include <qgsvectorlayer.h>

namespace {

const QgsPointXY kExpected(207440.79, 378546.00);  // 가수리 중심을 5174 → 5187
const QgsRectangle kKoreaWide(0, 200000, 400000, 600000);  // 폭 400 km: 화면 중심을 조사 위치로 쓰지 않는다

// 가수리 지적선 30개(베셀 중부원점 보정 EPSG:5174 숫자). 5186 으로 읽으면 부산 쪽, 5174 면 경북 가수리다.
QString writeGasuriDxf(const QString& dir, const QString& name) {
  QByteArray entities;
  for (int i = 0; i < 30; ++i) {
    const double x = 387699.70 - 300 + i * 20;
    entities += CadFixture::line("JIJUK", 1, x, 280239.90 - 300, x + 10, 280239.90 + 300);
  }
  const QString path = QDir(dir).filePath(name + QStringLiteral(".dxf"));
  return CadFixture::write(path, CadFixture::document("ANSI_949", entities)) ? path : QString();
}

QgsVectorLayer* drawingLines(const QString& title) {
  QgsLayerTreeGroup* reference =
      QgsProject::instance()->layerTreeRoot()->findGroup(HeritageImport::referenceGroupName());
  QgsLayerTreeGroup* group = reference ? reference->findGroup(title) : nullptr;
  if (!group) return nullptr;
  for (QgsLayerTreeLayer* node : group->findLayers())
    if (node->name() == QStringLiteral("선")) return qobject_cast<QgsVectorLayer*>(node->layer());
  return nullptr;
}

// 조사를 새로 연 것처럼: 프로젝트를 비우고 지도는 전국을 본다(위치 단서 없음).
struct Fixture {
  QMainWindow window;
  QgsMapCanvas* canvas = new QgsMapCanvas();
  QgsMessageBar* bar = new QgsMessageBar(&window);
  Fixture() {
    QgsProject::instance()->clear();
    QgsProject::instance()->setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5187")));
    window.setAttribute(Qt::WA_DontShowOnScreen);
    window.setCentralWidget(canvas);
    window.resize(1000, 700);
    canvas->setRenderFlag(false);
    canvas->setDestinationCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5187")));
    canvas->setExtent(kKoreaWide);
    window.show();
  }
  KaCadImport::Hooks hooks(const QString& surveyPath) {
    return {&window, canvas, bar, surveyPath, QStringLiteral("EPSG:5187"), [](QgsMapLayer*) {},
            [](const QString&, const QString&) { return true; }};
  }
};

}  // namespace

class TestCadImportAuto : public QObject {
  Q_OBJECT
 private slots:
  void init() { QFile::remove(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation) + QStringLiteral("/cad-crs.ini")); }
  void cleanup() { KaCadCrsDialog::setChooserForTests({}); }

  void rememberedChoiceIsNotAskedAgainEvenForACopy() { QCOMPARE(importThreeTimes(true), 1); }
  void withoutRememberTheWindowStaysEveryTime() { QCOMPARE(importThreeTimes(false), 3); }

  // 「좌표 없음」은 기억하지 않는다: 기억하면 창도 알림도 없이 정합으로만 가서 좌표계를 다시 고를 길이 없다.
  void noCrsIsNeverRemembered() {
    QTemporaryDir tmp;
    const QString dxf = writeGasuriDxf(tmp.path(), QStringLiteral("test1"));
    const QString survey = tmp.filePath(QStringLiteral("조사/테스트.gpkg"));
    int asked = 0;
    KaCadCrsDialog::setChooserForTests([&](const CadCrsResult&) -> std::optional<int> { ++asked; return -1; }, true);
    for (int i = 0; i < 2; ++i) {
      Fixture fx;
      QVERIFY(KaCadImport::run(fx.hooks(survey), dxf));
    }
    QCOMPARE(asked, 2);
  }

  // 기억한 도면을 알림에서 「좌표 없는 도면으로 보기」로 바꾸면 기억을 지워 다음에 다시 묻는다.
  void choosingNoCrsInTheNoticeForgetsTheDrawing() {
    QTemporaryDir tmp;
    const QString dxf = writeGasuriDxf(tmp.path(), QStringLiteral("test1"));
    const QString survey = tmp.filePath(QStringLiteral("조사/테스트.gpkg"));
    QCOMPARE(importThreeTimes(true), 1);
    int asked = 0;
    KaCadCrsDialog::setChooserForTests([&](const CadCrsResult&) -> std::optional<int> { ++asked; return -1; });
    {
      Fixture fx;
      QVERIFY(KaCadImport::run(fx.hooks(survey), dxf, QString::fromLatin1(KaCadImport::kNoCrs)));
    }
    {
      Fixture fx;
      QVERIFY(KaCadImport::run(fx.hooks(survey), dxf));
    }
    QCOMPARE(asked, 1);
  }

 private:
  // 처음, 조사를 다시 연 뒤 같은 파일, 이름만 다른 복사본을 차례로 불러온다. 창이 뜬 횟수를 돌려준다.
  int importThreeTimes(bool remember) {
    QTemporaryDir tmp;
    const QString dxf = writeGasuriDxf(tmp.path(), QStringLiteral("test1"));
    const QString copy = QDir(tmp.path()).filePath(QStringLiteral("test1 - 복사본.dxf"));
    if (!QFile::copy(dxf, copy)) return -1;
    const QString survey = tmp.filePath(QStringLiteral("조사/테스트.gpkg"));
    int asked = 0;
    KaCadCrsDialog::setChooserForTests(
        [&](const CadCrsResult& guess) -> std::optional<int> {
          ++asked;
          for (int i = 0; i < guess.candidates.size(); ++i)
            if (guess.candidates[i].authId == QStringLiteral("EPSG:5174")) return i;
          return std::nullopt;
        },
        remember);
    for (const QString& file : {dxf, dxf, copy}) {
      Fixture fx;
      if (!KaCadImport::run(fx.hooks(survey), file)) return -1;
      QgsVectorLayer* lines = drawingLines(QFileInfo(file).completeBaseName() + QStringLiteral(" (도면)"));
      if (!lines || lines->extent().center().distance(kExpected) >= 30) return -1;
    }
    return asked;
  }
};

int main(int argc, char** argv) {
  QStandardPaths::setTestModeEnabled(true);
  QgsApplication app(argc, argv, true);
  QgsApplication::setPrefixPath(qEnvironmentVariable("QGIS_PREFIX_PATH"), true);
  QgsApplication::initQgis();
  int result = 0;
  {
    TestCadImportAuto tests;
    result = QTest::qExec(&tests, argc, argv);
  }
  QgsProject::instance()->clear();
  QgsApplication::exitQgis();
  return result;
}

#include "test_cad_import_auto.moc"
