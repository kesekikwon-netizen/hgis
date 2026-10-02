// 도면 한 장을 작업 좌표계 GPKG 변환본으로 쓰고, 정합 결과를 변환본의 모든 표에 적용하는 CadDrawingStore.
// 기준 숫자: 가수리 도면 중심 (387699.70, 280239.90)을 5174 로 읽어 5187 로 바꾸면 (207440.79, 378546.00)(2026-10-02 잰 값).
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>

#include <cmath>
#include <memory>

#include "core/CadDrawingStore.h"
#include "core/SurveyBundle.h"

#include <qgsapplication.h>
#include <qgsfeature.h>
#include <qgsfeatureiterator.h>
#include <qgsgeometry.h>
#include <qgsproject.h>
#include <qgsvectorlayer.h>

namespace {

QgsCoordinateReferenceSystem crs5187() { return QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5187")); }

CadEntity line(double x0, double y0, double x1, double y1) {
  CadEntity e;
  e.kind = CadKind::Line;
  e.geometry = QgsGeometry::fromPolylineXY({QgsPointXY(x0, y0), QgsPointXY(x1, y1)});
  e.cadLayer = QStringLiteral("JIJUK");
  e.color = QColor(QStringLiteral("#7f0000"));
  return e;
}

CadEntity text(double x, double y, double angle, double height) {
  CadEntity e;
  e.kind = CadKind::Text;
  e.geometry = QgsGeometry::fromPointXY(QgsPointXY(x, y));
  e.cadLayer = QStringLiteral("JIBUN");
  e.color = QColor(QStringLiteral("#3c3c3c"));
  e.text = QStringLiteral("374-1전");
  e.textHeight = height;
  e.textAngle = angle;
  e.textAnchor = 7;
  return e;
}

bool writeDrawing(const QString& outPath, const QVector<CadEntity>& entities, const QString& sourceCrs,
                  QString* error = nullptr, const std::function<bool()>& canceled = {}) {
  CadDrawing drawing;
  drawing.entities = entities;
  const CadStoreInfo info{QStringLiteral("C:/x/가수리.dwg"), QStringLiteral("ab12"), sourceCrs,
                          QStringLiteral("LibreDWG 0.14")};
  QString ignored;
  return CadDrawingStore::write(drawing, info, crs5187(), QgsProject::instance()->transformContext(), outPath,
                                error ? error : &ignored, canceled);
}

std::unique_ptr<QgsVectorLayer> table(const QString& gpkg, const QString& name) {
  return std::make_unique<QgsVectorLayer>(gpkg + QStringLiteral("|layername=") + name, name, QStringLiteral("ogr"));
}

QgsFeature first(QgsVectorLayer* layer) {
  QgsFeature f;
  layer->getFeatures().nextFeature(f);
  return f;
}

QgsPointXY firstVertex(QgsVectorLayer* layer) { return QgsPointXY(first(layer).geometry().vertexAt(0)); }

bool near(const QgsPointXY& p, double x, double y, double tolerance) {
  return std::abs(p.x() - x) <= tolerance && std::abs(p.y() - y) <= tolerance;
}

// 90° 회전 · 2배 · (10, 20) 이동.
GeorefService::Affine turnDoubleAndShift() {
  GeorefService::Affine a;
  a.a = 0;
  a.b = -2;
  a.c = 10;
  a.d = 2;
  a.e = 0;
  a.f = 20;
  a.valid = true;
  return a;
}

}  // namespace

class TestCadStore : public QObject {
  Q_OBJECT
 private slots:
  void outputPathFor_usesTheSurveyFolderAndNumbers() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString source = QStringLiteral("C:/x/[동국] 시험 도면(2010v).dwg");
    const QString folder = tmp.path() + QStringLiteral("/가져온자료/도면/");
    const QString path = CadDrawingStore::outputPathFor(source, tmp.path());
    QCOMPARE(path, folder + QStringLiteral("[동국] 시험 도면(2010v).gpkg"));
    QVERIFY(QDir().mkpath(folder));
    QFile taken(path);
    QVERIFY(taken.open(QIODevice::WriteOnly));
    taken.close();
    QCOMPARE(CadDrawingStore::outputPathFor(source, tmp.path()),
             folder + QStringLiteral("[동국] 시험 도면(2010v) (2).gpkg"));
  }

  void outputPathFor_withoutSurveyUsesAppData() {
    const QString path = CadDrawingStore::outputPathFor(QStringLiteral("C:/x/도면.dxf"), QString());
    QCOMPARE(path, QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) +
                       QStringLiteral("/cad-drawings/도면.gpkg"));
    QVERIFY(SurveyBundle::isAppManagedPath(path));
  }

  void write_moves5174NumbersInto5187() {
    QTemporaryDir tmp;
    const QString out = tmp.filePath(QStringLiteral("가수리.gpkg"));
    QString error;
    QVERIFY2(writeDrawing(out, {line(387699.70, 280239.90, 387709.70, 280239.90)}, QStringLiteral("EPSG:5174"), &error),
             qUtf8Printable(error));
    auto lines = table(out, QStringLiteral("lines"));
    QVERIFY(lines->isValid());
    QCOMPARE(lines->crs().authid(), QStringLiteral("EPSG:5187"));
    const QgsPointXY p = firstVertex(lines.get());
    QVERIFY2(near(p, 207440.79, 378546.00, 0.05), qUtf8Printable(p.toString(3)));
  }

  void write_keepsNumbersWithoutCrs() {
    QTemporaryDir tmp;
    const QString out = tmp.filePath(QStringLiteral("로컬.gpkg"));
    QVERIFY(writeDrawing(out, {line(12.5, 34.25, 100, 80)}, QString()));
    auto lines = table(out, QStringLiteral("lines"));
    QCOMPARE(firstVertex(lines.get()), QgsPointXY(12.5, 34.25));
  }

  void write_onlyNonEmptyTablesWithFieldsAndInfo() {
    QTemporaryDir tmp;
    const QString out = tmp.filePath(QStringLiteral("가수리.gpkg"));
    QVERIFY(writeDrawing(out, {line(387699.70, 280239.90, 387709.70, 280239.90), text(387700, 280240, 30, 2.5)},
                         QStringLiteral("EPSG:5174")));
    QCOMPARE(CadDrawingStore::tableNames(out), QStringList({"lines", "texts"}));
    auto texts = table(out, QStringLiteral("texts"));
    QStringList names = texts->fields().names();
    names.removeAll(QStringLiteral("fid"));
    QCOMPARE(names, QStringList({"cad_layer", "color", "text", "text_height", "text_angle", "text_anchor"}));
    const QgsFeature f = first(texts.get());
    QCOMPARE(f.attribute(QStringLiteral("cad_layer")).toString(), QStringLiteral("JIBUN"));
    QCOMPARE(f.attribute(QStringLiteral("color")).toString(), QStringLiteral("#3c3c3c"));
    QCOMPARE(f.attribute(QStringLiteral("text")).toString(), QStringLiteral("374-1전"));
    QCOMPARE(f.attribute(QStringLiteral("text_height")).toDouble(), 2.5);
    QCOMPARE(f.attribute(QStringLiteral("text_angle")).toDouble(), 30.0);
    QCOMPARE(f.attribute(QStringLiteral("text_anchor")).toInt(), 7);
    const CadStoreInfo info = CadDrawingStore::readInfo(out);
    QCOMPARE(info.sourcePath, QStringLiteral("C:/x/가수리.dwg"));
    QCOMPARE(info.sourceSha256, QStringLiteral("ab12"));
    QCOMPARE(info.sourceCrs, QStringLiteral("EPSG:5174"));
    QCOMPARE(info.converter, QStringLiteral("LibreDWG 0.14"));
  }

  void write_neverOverwrites() {
    QTemporaryDir tmp;
    const QString out = tmp.filePath(QStringLiteral("있음.gpkg"));
    QFile existing(out);
    QVERIFY(existing.open(QIODevice::WriteOnly));
    existing.write("keep");
    existing.close();
    QString error;
    QVERIFY(!writeDrawing(out, {line(0, 0, 1, 1)}, QString(), &error));
    QCOMPARE(error, QStringLiteral("같은 이름의 변환본이 이미 있습니다."));
    QFile check(out);
    QVERIFY(check.open(QIODevice::ReadOnly));
    QCOMPARE(check.readAll(), QByteArray("keep"));
  }

  // 첫 확인은 넘기고 다음 확인에서 취소한다: 이미 쓰기 시작한 임시 파일까지 지워야 한다.
  void write_cancelLeavesNothing() {
    QTemporaryDir tmp;
    const QString out = tmp.filePath(QStringLiteral("취소.gpkg"));
    int checks = 0;
    QString error;
    QVERIFY(!writeDrawing(out, {line(0, 0, 1, 1), text(0, 0, 0, 1)}, QString(), &error, [&] { return ++checks > 1; }));
    QCOMPARE(error, QStringLiteral("도면 변환을 취소했습니다."));
    QVERIFY(checks > 1);
    QVERIFY(!QFileInfo::exists(out));
    QCOMPARE(QDir(tmp.path()).entryList(QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot),
             QStringList());
  }

  void applyAffine_movesEveryTableAndTurnsTexts() {
    QTemporaryDir tmp;
    const QString out = tmp.filePath(QStringLiteral("로컬.gpkg"));
    QVERIFY(writeDrawing(out, {line(1, 0, 2, 0), text(1, 0, 30, 2.5)}, QString()));
    {
      auto lines = table(out, QStringLiteral("lines"));
      auto texts = table(out, QStringLiteral("texts"));
      QString error;
      QVERIFY2(CadDrawingStore::applyAffine({lines.get(), texts.get()}, turnDoubleAndShift(), &error),
               qUtf8Printable(error));
    }
    auto lines = table(out, QStringLiteral("lines"));
    const QgsPointXY moved = firstVertex(lines.get());
    QVERIFY2(near(moved, 10, 22, 1e-6), qUtf8Printable(moved.toString(6)));
    auto texts = table(out, QStringLiteral("texts"));
    const QgsFeature t = first(texts.get());
    QVERIFY2(near(t.geometry().asPoint(), 10, 22, 1e-6), qUtf8Printable(t.geometry().asWkt()));
    QVERIFY(std::abs(t.attribute(QStringLiteral("text_angle")).toDouble() - 120.0) < 1e-9);
    QVERIFY(std::abs(t.attribute(QStringLiteral("text_height")).toDouble() - 5.0) < 1e-9);
  }

  // 둘째 레이어를 읽기 전용 사본에서 열어 실패시킨다. 이미 옮긴 첫 레이어는 원래 자리로 돌아와야 한다.
  void applyAffine_restoresEarlierTablesWhenOneFails() {
    QTemporaryDir tmp;
    const QString out = tmp.filePath(QStringLiteral("로컬.gpkg"));
    QVERIFY(writeDrawing(out, {line(1, 0, 2, 0), text(1, 0, 30, 2.5)}, QString()));
    const QString copy = tmp.filePath(QStringLiteral("읽기전용.gpkg"));
    QVERIFY(QFile::copy(out, copy));
    QVERIFY(QFile::setPermissions(copy, QFileDevice::ReadOwner | QFileDevice::ReadUser | QFileDevice::ReadGroup |
                                            QFileDevice::ReadOther));
    {
      auto lines = table(out, QStringLiteral("lines"));
      auto locked = table(copy, QStringLiteral("texts"));
      QVERIFY(lines->isValid() && locked->isValid());
      QString error;
      QVERIFY(!CadDrawingStore::applyAffine({lines.get(), locked.get()}, turnDoubleAndShift(), &error));
      QCOMPARE(error, QStringLiteral("맞춘 결과를 도면 전체에 적용하지 못했습니다."));
    }
    auto lines = table(out, QStringLiteral("lines"));
    QCOMPARE(firstVertex(lines.get()), QgsPointXY(1, 0));
    QFile::setPermissions(copy, QFileDevice::ReadOwner | QFileDevice::WriteOwner);  // 임시 폴더가 지울 수 있게
  }
};

int main(int argc, char** argv) {
  QStandardPaths::setTestModeEnabled(true);
  QgsApplication app(argc, argv, false);
  QgsApplication::setPrefixPath(qEnvironmentVariable("QGIS_PREFIX_PATH"), true);
  QgsApplication::initQgis();
  int result = 0;
  {
    TestCadStore tests;
    result = QTest::qExec(&tests, argc, argv);
  }
  QgsApplication::exitQgis();
  return result;
}

#include "test_cad_store.moc"
