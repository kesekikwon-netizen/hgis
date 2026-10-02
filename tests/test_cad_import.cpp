// 도면(DXF·DWG)을 좌표계를 알아내 작업 좌표계 변환본으로 참조 지도 제자리에 올리는 KaCadImport.
// 가수리 숫자: 도면 중심 (387699.70, 280239.90)을 5174 로 읽어 5187 로 바꾸면 (207440.79, 378546.00)(2026-10-02 잰 값).
// MainWindow 의 파일함·끌어놓기·맞추기·「벡터·도면 불러오기」는 모두 addVectorFromPath 를 거쳐 이 흐름으로 온다.
#include <QDir>
#include <QFile>
#include <QMainWindow>
#include <QPushButton>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QToolButton>
#include <QtTest>

#include "app/KaCadCrsDialog.h"
#include "app/KaCadImport.h"
#include "cad_fixture.h"
#include "core/CadDwgConverter.h"
#include "core/HeritageImport.h"
#include "core/LayerOps.h"
#include "core/SurveyBundle.h"

#include <qgsapplication.h>
#include <qgsfeature.h>
#include <qgslayertree.h>
#include <qgsmapcanvas.h>
#include <qgsmessagebar.h>
#include <qgsmessagebaritem.h>
#include <qgsproject.h>
#include <qgsproviderregistry.h>
#include <qgsvectorlayer.h>

namespace {

const QgsPointXY kExpected(207440.79, 378546.00);  // 가수리 중심을 5174 → 5187
const QgsRectangle kKoreaWide(0, 200000, 400000, 600000);  // 폭 400 km: 화면 중심을 조사 위치로 쓰지 않는다

// 가수리 DWG 를 LibreDWG 로 바꾼 DXF 처럼: 머리글 ANSI_949, 바이트는 UTF-8, 층 JIJUK 의 선 30개와 지번 글자 하나.
QString writeGasuriDxf(const QString& dir, const QString& name) {
  QByteArray entities;
  for (int i = 0; i < 30; ++i) {
    const double x = 387699.70 - 300 + i * 20;
    entities += CadFixture::line("JIJUK", 1, x, 280239.90 - 300, x + 10, 280239.90 + 300);
  }
  entities += CadFixture::text("JIBUN", 7, 387699.70, 280239.90, 2, 0, QStringLiteral("374-1전").toUtf8());
  const QString path = QDir(dir).filePath(name + QStringLiteral(".dxf"));
  return CadFixture::write(path, CadFixture::document("ANSI_949", entities)) ? path : QString();
}

QByteArray bytes(const QString& path) {
  QFile file(path);
  return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

void addYeongcheonSurveyArea() {
  auto* area = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5187"), QStringLiteral("조사구역"),
                                  QStringLiteral("memory"));
  QgsFeature feature(area->fields());
  feature.setGeometry(QgsGeometry::fromRect(QgsRectangle(207390.79, 378496.00, 207490.79, 378596.00)));
  area->dataProvider()->addFeature(feature);
  area->updateExtents();
  LayerOps::markSurveyLayer(area, QStringLiteral("survey_area"));
  QgsProject::instance()->addMapLayer(area);
}

QgsLayerTreeGroup* drawingGroup(const QString& title) {
  QgsLayerTreeGroup* reference =
      QgsProject::instance()->layerTreeRoot()->findGroup(HeritageImport::referenceGroupName());
  return reference ? reference->findGroup(title) : nullptr;
}

int groupCount(const QString& title) {
  QgsLayerTreeGroup* reference =
      QgsProject::instance()->layerTreeRoot()->findGroup(HeritageImport::referenceGroupName());
  int count = 0;
  if (reference)
    for (QgsLayerTreeNode* node : reference->children()) count += QgsLayerTree::isGroup(node) && node->name() == title;
  return count;
}

QStringList childNames(QgsLayerTreeGroup* group) {
  QStringList names;
  for (QgsLayerTreeNode* node : group->children()) names << node->name();
  return names;
}

QgsVectorLayer* layerIn(QgsLayerTreeGroup* group, const QString& name) {
  if (!group) return nullptr;
  for (QgsLayerTreeLayer* node : group->findLayers())
    if (node->name() == name) return qobject_cast<QgsVectorLayer*>(node->layer());
  return nullptr;
}

QString sourceFile(const QgsMapLayer* layer) {
  return QgsProviderRegistry::instance()
      ->decodeUri(QStringLiteral("ogr"), layer->source())
      .value(QStringLiteral("path"))
      .toString();
}

// 앱 창 대신 창·지도·알림 막대를 따로 만들고, 정합 시작과 다시 불러오기는 기록만 한다.
struct Fixture {
  QMainWindow window;
  QgsMapCanvas* canvas = new QgsMapCanvas();
  QgsMessageBar* bar = new QgsMessageBar(&window);
  int alignCalls = 0;
  QString alignedLayer;

  Fixture() {
    window.setAttribute(Qt::WA_DontShowOnScreen);
    window.setCentralWidget(canvas);
    window.resize(1000, 700);
    canvas->setRenderFlag(false);
    canvas->setDestinationCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5187")));
    window.show();
  }

  KaCadImport::Hooks hooks(const QString& surveyPath) {
    return {&window, canvas, bar, surveyPath, QStringLiteral("EPSG:5187"),
            [this](QgsMapLayer* layer) {
              ++alignCalls;
              alignedLayer = layer ? layer->name() : QString();
            },
            [](const QString&, const QString&) { return true; }};
  }
};

}  // namespace

class TestCadImport : public QObject {
  Q_OBJECT
 private slots:
  void initTestCase() {
    QDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + QStringLiteral("/cad-drawings"))
        .removeRecursively();
  }

  void init() {
    QgsProject::instance()->clear();
    QgsProject::instance()->setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5187")));
  }

  void run_dxfNearTheSurveyLandsInPlace() {
    QTemporaryDir tmp;
    addYeongcheonSurveyArea();
    const QString dxf = writeGasuriDxf(tmp.filePath(QStringLiteral("원본")), QStringLiteral("가수리"));
    QVERIFY(!dxf.isEmpty());
    const QByteArray before = bytes(dxf);
    Fixture fx;
    QVERIFY(KaCadImport::run(fx.hooks(tmp.filePath(QStringLiteral("조사/영천 조사.gpkg"))), dxf));
    QgsLayerTreeGroup* group = drawingGroup(QStringLiteral("가수리 (도면)"));
    QVERIFY(group);
    QCOMPARE(childNames(group), QStringList({"글자", "선"}));
    QgsVectorLayer* lines = layerIn(group, QStringLiteral("선"));
    QVERIFY(lines);
    QCOMPARE(QFileInfo(sourceFile(lines)).absoluteFilePath(),
             QFileInfo(tmp.filePath(QStringLiteral("조사/가져온자료/도면/가수리.gpkg"))).absoluteFilePath());
    const QgsPointXY centre = lines->extent().center();
    QVERIFY2(centre.distance(kExpected) < 30, qUtf8Printable(centre.toString(2)));
    QVERIFY(QgsProject::instance()->mapLayersByName(QStringLiteral("entities")).isEmpty());
    QCOMPARE(bytes(dxf), before);
    QgsMessageBarItem* item = fx.bar->currentItem();
    QVERIFY(item);
    QVERIFY(item->findChild<QToolButton*>(QStringLiteral("cadOtherCrs")));
    QVERIFY(item->findChild<QPushButton*>(QStringLiteral("cadAlignNow")));
    QCOMPARE(fx.alignCalls, 0);
  }

  void run_withoutSurveyAreaAsksWhichCrs() {
    QTemporaryDir tmp;
    const QString dxf = writeGasuriDxf(tmp.filePath(QStringLiteral("원본")), QStringLiteral("가수리"));
    Fixture fx;
    fx.canvas->setExtent(kKoreaWide);
    int calls = 0;
    qsizetype offered = 0;
    KaCadCrsDialog::setChooserForTests([&](const CadCrsResult& guess) -> std::optional<int> {
      ++calls;
      offered = guess.candidates.size();
      for (int i = 0; i < guess.candidates.size(); ++i)
        if (guess.candidates[i].authId == QStringLiteral("EPSG:5174")) return i;
      return std::nullopt;
    });
    const bool ok = KaCadImport::run(fx.hooks(tmp.filePath(QStringLiteral("조사/빈 조사.gpkg"))), dxf);
    KaCadCrsDialog::setChooserForTests({});
    QVERIFY(ok);
    QCOMPARE(calls, 1);
    QVERIFY(offered >= 2);
    QgsVectorLayer* lines = layerIn(drawingGroup(QStringLiteral("가수리 (도면)")), QStringLiteral("선"));
    QVERIFY(lines);
    QVERIFY2(lines->extent().center().distance(kExpected) < 30, qUtf8Printable(lines->extent().center().toString(2)));
  }

  void run_dwgUsesTheBundledConverter() {
    QVERIFY2(QFileInfo::exists(CadDwgConverter::bundledTool()), "ka-hgis를 먼저 빌드해 tools/libredwg를 복사하세요");
    QTemporaryDir tmp;
    const QString dwg = tmp.filePath(QStringLiteral("[동국] 작은 도면(2010v).dwg"));
    QVERIFY(QFile::copy(QDir(QString::fromUtf8(KA_TEST_CAD_DATA)).filePath(QStringLiteral("r2000-small.dwg")), dwg));
    Fixture fx;
    QVERIFY(KaCadImport::run(fx.hooks(tmp.filePath(QStringLiteral("조사/작은 조사.gpkg"))), dwg));
    QVERIFY(drawingGroup(QStringLiteral("[동국] 작은 도면(2010v) (도면)")));
    // 시험 도면은 로컬 숫자라 좌표가 없다: 그 도면의 「선」으로 정합을 바로 시작한다.
    QCOMPARE(fx.alignCalls, 1);
    QCOMPARE(fx.alignedLayer, QStringLiteral("선"));
  }

  void run_sameDrawingTwiceKeepsOneGroup() {
    QTemporaryDir tmp;
    addYeongcheonSurveyArea();
    const QString dxf = writeGasuriDxf(tmp.filePath(QStringLiteral("원본")), QStringLiteral("가수리"));
    Fixture fx;
    const KaCadImport::Hooks hooks = fx.hooks(tmp.filePath(QStringLiteral("조사/영천 조사.gpkg")));
    QVERIFY(KaCadImport::run(hooks, dxf));
    QVERIFY(KaCadImport::run(hooks, dxf));
    QCOMPARE(groupCount(QStringLiteral("가수리 (도면)")), 1);
    QCOMPARE(drawingGroup(QStringLiteral("가수리 (도면)"))->findLayers().size(), 2);
  }

  // 다른 폴더의 같은 이름 도면은 바꾸지 않고 「(도면 2)」로 둘 다 둔다. 먼저 올린 변환본도 그대로다.
  void run_sameNameFromAnotherFolderKeepsBoth() {
    QTemporaryDir tmp;
    addYeongcheonSurveyArea();
    const QString first = writeGasuriDxf(tmp.filePath(QStringLiteral("A")), QStringLiteral("가수리"));
    const QString second = writeGasuriDxf(tmp.filePath(QStringLiteral("B")), QStringLiteral("가수리"));
    Fixture fx;
    const KaCadImport::Hooks hooks = fx.hooks(tmp.filePath(QStringLiteral("조사/영천 조사.gpkg")));
    QVERIFY(KaCadImport::run(hooks, first) && KaCadImport::run(hooks, second));
    QVERIFY(drawingGroup(QStringLiteral("가수리 (도면)")) && drawingGroup(QStringLiteral("가수리 (도면 2)")));
    QVERIFY(QFileInfo::exists(tmp.filePath(QStringLiteral("조사/가져온자료/도면/가수리.gpkg"))));
  }

  // 좌표 없는 도면은 정합을 바로 시작하므로 화면을 도면 숫자(바다)로 옮기지 않는다.
  void run_localDrawingKeepsTheView() {
    QTemporaryDir tmp;
    const QString dxf = tmp.filePath(QStringLiteral("로컬.dxf"));
    QVERIFY(CadFixture::write(dxf, CadFixture::document("ANSI_949", CadFixture::line("A", 1, 0, 0, 100, 80))));
    Fixture fx;
    const QgsRectangle view(207000, 378000, 208000, 379000);
    fx.canvas->setExtent(view);
    const QgsRectangle before = fx.canvas->extent();
    QVERIFY(KaCadImport::run(fx.hooks(tmp.filePath(QStringLiteral("조사/조사.gpkg"))), dxf));
    QCOMPARE(fx.alignCalls, 1);
    QVERIFY2(fx.canvas->extent().center().distance(before.center()) < 1, qUtf8Printable(fx.canvas->extent().toString()));
  }

  // 다른 좌표계로 다시 불러오면 그 도면의 지난 알림은 내리고 새 알림 하나만 둔다.
  void run_reimportKeepsOneNotice() {
    QTemporaryDir tmp;
    addYeongcheonSurveyArea();
    const QString dxf = writeGasuriDxf(tmp.filePath(QStringLiteral("원본")), QStringLiteral("가수리"));
    Fixture fx;
    const KaCadImport::Hooks hooks = fx.hooks(tmp.filePath(QStringLiteral("조사/영천 조사.gpkg")));
    QVERIFY(KaCadImport::run(hooks, dxf));
    QVERIFY(KaCadImport::run(hooks, dxf, QStringLiteral("EPSG:5181")));
    QCOMPARE(fx.bar->items().size(), 1);
    QVERIFY(fx.bar->currentItem()->text().contains(QStringLiteral("EPSG:5181")));
  }

  void run_withoutSurveyWritesIntoAppData() {
    QTemporaryDir tmp;
    const QString dxf = writeGasuriDxf(tmp.filePath(QStringLiteral("원본")), QStringLiteral("가수리 앱"));
    Fixture fx;
    fx.canvas->setExtent(kKoreaWide);
    KaCadCrsDialog::setChooserForTests([](const CadCrsResult&) { return std::optional<int>(0); });
    const bool ok = KaCadImport::run(fx.hooks(QString()), dxf);
    KaCadCrsDialog::setChooserForTests({});
    QVERIFY(ok);
    QgsVectorLayer* lines = layerIn(drawingGroup(QStringLiteral("가수리 앱 (도면)")), QStringLiteral("선"));
    QVERIFY(lines);
    const QString file = sourceFile(lines);
    QVERIFY2(file.startsWith(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) +
                             QStringLiteral("/cad-drawings/")),
             qUtf8Printable(file));
    QVERIFY(SurveyBundle::isAppManagedPath(file));
  }
};

int main(int argc, char** argv) {
  QStandardPaths::setTestModeEnabled(true);
  QgsApplication app(argc, argv, true);
  QgsApplication::setPrefixPath(qEnvironmentVariable("QGIS_PREFIX_PATH"), true);
  QgsApplication::initQgis();
  int result = 0;
  {
    TestCadImport tests;
    result = QTest::qExec(&tests, argc, argv);
  }
  QgsProject::instance()->clear();
  QgsApplication::exitQgis();
  return result;
}

#include "test_cad_import.moc"
