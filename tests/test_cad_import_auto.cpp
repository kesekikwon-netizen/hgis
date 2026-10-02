// 좌표가 있는 도면은 묻지 않고 제자리에 올리고, 로컬 좌표 도면만 정합으로 보낸다(2026-10-03 사용자 목표:
// 「사용자는 이도면의 위치 좌표계를 모른다는 가정」, 「좌표계가있는 도면이라면 이것은 자동으로 계산이 되게 해서 들어가야한다」).
// 단서: 도면이 밝힌 좌표계 → 같은 파일 기억 → 조사 위치 → 지도 화면 → 지명 → 최근 좌표계 → 기본 순서.
// 단서로 정하지 못한 자리는 알림이 「가장 그럴듯한 자리」라고 밝히고 「다른 위치로 바꾸기」를 둔다.
#include <QDir>
#include <QFile>
#include <QMainWindow>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QToolButton>
#include <QtTest>

#include "app/KaCadImport.h"
#include "cad_fixture.h"
#include "core/HeritageImport.h"

#include <qgsapplication.h>
#include <qgslayertree.h>
#include <qgsmapcanvas.h>
#include <qgsmessagebar.h>
#include <qgsmessagebaritem.h>
#include <qgsproject.h>
#include <qgsvectorlayer.h>

namespace {

const QgsPointXY kExpected(207440.79, 378546.00);                // 가수리 중심을 5174 → 5187
const QgsRectangle kKoreaWide(0, 200000, 400000, 600000);        // 한반도 전체: 자리를 정하지 못한다
const QgsRectangle kYeongcheon(177000, 348000, 237000, 408000);  // 영천 둘레 60 km: 가수리 자리 하나만 든다

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

// 조사를 새로 연 것처럼 프로젝트를 비우고 지도는 view 를 본다. 조사구역은 없다.
struct Fixture {
  QMainWindow window;
  QgsMapCanvas* canvas = new QgsMapCanvas();
  QgsMessageBar* bar = new QgsMessageBar(&window);
  int alignCalls = 0;
  explicit Fixture(const QgsRectangle& view) {
    QgsProject::instance()->clear();
    QgsProject::instance()->setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5187")));
    window.setAttribute(Qt::WA_DontShowOnScreen);
    window.setCentralWidget(canvas);
    window.resize(1000, 700);
    canvas->setRenderFlag(false);
    canvas->setDestinationCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5187")));
    canvas->setExtent(view);
    window.show();
  }
  bool run(const QString& file, const QString& forced = QString()) {
    const KaCadImport::Hooks hooks{&window, canvas, bar, QString(), QStringLiteral("EPSG:5187"),
                                   [this](QgsMapLayer*) { ++alignCalls; },
                                   [](const QString&, const QString&) { return true; }};
    return KaCadImport::run(hooks, file, forced);
  }
  QString notice() const { return bar->currentItem() ? bar->currentItem()->text() : QString(); }
};

bool landsAtGasuri(const QString& file) {
  QgsVectorLayer* lines = drawingLines(QFileInfo(file).completeBaseName() + QStringLiteral(" (도면)"));
  return lines && lines->extent().center().distance(kExpected) < 30;
}

}  // namespace

class TestCadImportAuto : public QObject {
  Q_OBJECT
 private slots:
  void init() {
    QFile::remove(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation) + QStringLiteral("/cad-crs.ini"));
  }

  // 단서가 없어도 창 없이 올린다. 알림이 가장 그럴듯한 자리라고 밝히고 다른 자리로 바꿀 길을 둔다.
  void withoutCluesTheLikeliestPlaceIsUsedAndOthersOffered() {
    QTemporaryDir tmp;
    const QString dxf = writeGasuriDxf(tmp.path(), QStringLiteral("test1"));
    Fixture fx(kKoreaWide);
    QVERIFY(fx.run(dxf));
    QVERIFY(drawingLines(QStringLiteral("test1 (도면)")));
    QCOMPARE(fx.alignCalls, 0);
    QVERIFY2(fx.notice().contains(QStringLiteral("가장 그럴듯한")), qUtf8Printable(fx.notice()));
    QToolButton* other = fx.bar->currentItem()->findChild<QToolButton*>(QStringLiteral("cadOtherCrs"));
    QVERIFY(other);
    QCOMPARE(other->text(), QStringLiteral("다른 위치로 바꾸기"));
  }

  // 가장 그럴듯한 자리에 둔 도면(화면도 거기로 간다)이 다음 도면의 단서가 되지 않는다(검토 2026-10-03).
  void anUnsurePlacementIsNotAClueForTheNextDrawing() {
    QTemporaryDir tmp;
    const QString first = writeGasuriDxf(tmp.path(), QStringLiteral("test1"));
    const QString second = writeGasuriDxf(tmp.path(), QStringLiteral("test2 다른 도면"));
    QFile extra(second);
    QVERIFY(extra.open(QIODevice::Append));
    extra.write("\n");  // 내용이 달라야 같은 원본 기억을 쓰지 않는다
    extra.close();
    Fixture fx(kKoreaWide);
    QVERIFY(fx.run(first));
    QVERIFY(fx.run(second));
    QVERIFY2(fx.notice().contains(QStringLiteral("가장 그럴듯한")), qUtf8Printable(fx.notice()));
  }

  // 조사구역이 없어도 지도 화면이 조사 지역을 보고 있으면 그 자리다.
  void theMapViewPlacesTheDrawing() {
    QTemporaryDir tmp;
    const QString dxf = writeGasuriDxf(tmp.path(), QStringLiteral("test1"));
    Fixture fx(kYeongcheon);
    QVERIFY(fx.run(dxf));
    QVERIFY(landsAtGasuri(dxf));
    QVERIFY2(!fx.notice().contains(QStringLiteral("가장 그럴듯한")), qUtf8Printable(fx.notice()));
  }

  // 한 번 제자리에 올린 도면은 지도가 어디를 보든, 이름만 다른 복사본이어도 같은 자리에 올린다.
  void aPlacedDrawingIsRememberedEvenForACopy() {
    QTemporaryDir tmp;
    const QString dxf = writeGasuriDxf(tmp.path(), QStringLiteral("test1"));
    const QString copy = tmp.filePath(QStringLiteral("test1 - 복사본.dxf"));
    QVERIFY(QFile::copy(dxf, copy));
    {
      Fixture fx(kYeongcheon);
      QVERIFY(fx.run(dxf));
    }
    for (const QString& file : {dxf, copy}) {
      Fixture fx(kKoreaWide);
      QVERIFY(fx.run(file));
      QVERIFY2(landsAtGasuri(file), qUtf8Printable(file));
      QVERIFY2(!fx.notice().contains(QStringLiteral("가장 그럴듯한")), qUtf8Printable(fx.notice()));
    }
  }

  // 도면이 밝힌 좌표계(파일 이름의 번호)는 지도가 어디를 보든 쓴다.
  void aCrsInTheFileNameIsUsedAnywhere() {
    QTemporaryDir tmp;
    const QString dxf = writeGasuriDxf(tmp.path(), QStringLiteral("가수리 면적산출_5174"));
    Fixture fx(kKoreaWide);
    QVERIFY(fx.run(dxf));
    QVERIFY(landsAtGasuri(dxf));
  }

  // 기억한 도면을 알림에서 「좌표 없는 도면으로 보기」로 바꾸면 잊는다: 다음에는 다시 단서로 정한다.
  void choosingNoCrsForgetsTheDrawing() {
    QTemporaryDir tmp;
    const QString dxf = writeGasuriDxf(tmp.path(), QStringLiteral("test1"));
    {
      Fixture fx(kYeongcheon);
      QVERIFY(fx.run(dxf));
    }
    {
      Fixture fx(kKoreaWide);
      QVERIFY(fx.run(dxf, QString::fromLatin1(KaCadImport::kNoCrs)));
      QCOMPARE(fx.alignCalls, 1);
    }
    Fixture fx(kKoreaWide);
    QVERIFY(fx.run(dxf));
    QVERIFY2(fx.notice().contains(QStringLiteral("가장 그럴듯한")), qUtf8Printable(fx.notice()));
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
