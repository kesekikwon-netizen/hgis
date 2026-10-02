// DXF 한 장을 모형 공간 도형만 선·면·점·글자로 읽는 CadDrawingReader.
// 사용자 DWG 를 LibreDWG 로 바꾼 DXF 에서 잰 사실을 시험한다: 머리글이 ANSI_949 여도 UTF-8 인 글자, 종이 공간 도형,
// 정렬점 때문에 생기는 엉뚱한 글자 dx·dy, 본체에서 60~85 km 떨어진 몇 개의 도형.
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>

#include "cad_fixture.h"
#include "core/CadDrawingReader.h"

#include <gdal.h>
#include <qgswkbtypes.h>

namespace {

const QByteArray kGasuriUtf8 = QStringLiteral("가수리 374-1전").toUtf8();
// 같은 글자의 CP949 바이트: 가 B0A1, 수 BCF6, 리 B8AE, 전 C0FC.
const QByteArray kGasuriCp949 = QByteArray::fromHex("B0A1BCF6B8AE20") + "374-1" + QByteArray::fromHex("C0FC");

struct Read {
  bool ok = false;
  CadDrawing drawing;
  QString error;
  QString details;
};

Read readBytes(const QTemporaryDir& dir, const QByteArray& dxf, const std::function<bool()>& canceled = {}) {
  const QString path = dir.filePath(QStringLiteral("도면.dxf"));
  Read r;
  if (!CadFixture::write(path, dxf)) return r;
  r.ok = CadDrawingReader::read(path, &r.drawing, &r.error, &r.details, canceled);
  return r;
}

int countKind(const CadDrawing& drawing, CadKind kind) {
  int n = 0;
  for (const CadEntity& e : drawing.entities) n += e.kind == kind ? 1 : 0;
  return n;
}

const CadEntity* firstText(const CadDrawing& drawing) {
  for (const CadEntity& e : drawing.entities)
    if (e.kind == CadKind::Text) return &e;
  return nullptr;
}

}  // namespace

class TestCadReader : public QObject {
  Q_OBJECT
 private slots:
  void initTestCase() { GDALAllRegister(); }

  void looksUtf8_tellsUtf8FromCp949() {
    QVERIFY(CadDrawingReader::looksUtf8(QStringLiteral("가수리").toUtf8()));
    QVERIFY(!CadDrawingReader::looksUtf8(QByteArray::fromHex("B0A1BCF6B8AE")));
    QVERIFY(!CadDrawingReader::looksUtf8("ABC"));
  }

  void colorFromStyle_ignoresAlphaAndDarkensWhite() {
    const QColor nearlyBlack = CadDrawingReader::colorFromStyle(QStringLiteral("PEN(c:#00000e00)"));
    QCOMPARE(nearlyBlack, QColor(0, 0, 14));
    QCOMPARE(nearlyBlack.alpha(), 255);
    QCOMPARE(CadDrawingReader::colorFromStyle(QStringLiteral("PEN(c:#ffffff)")), QColor(QStringLiteral("#3c3c3c")));
    QCOMPARE(CadDrawingReader::colorFromStyle(QStringLiteral("LABEL(f:\"Arial\",t:\"x\",c:#00ff00)")),
             QColor(QStringLiteral("#00ff00")));
    QCOMPARE(CadDrawingReader::colorFromStyle(QString()), QColor(QStringLiteral("#000000")));
  }

  void read_skipsPaperSpaceAndSortsKinds() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QByteArray entities =
        CadFixture::line("WALL", 1, 0, 0, 10, 0) + CadFixture::line("0", 7, 0, 0, 420, 297, true) +
        CadFixture::closedPolyline("WALL", 7, {{0, 0}, {5, 0}, {5, 5}, {0, 5}}) +
        CadFixture::text("JIBUN", 3, 1, 1, 2.5, 0, "T") + CadFixture::point("PT", 2, 3, 3) +
        CadFixture::solid("FILL", 4, {{0, 0}, {2, 0}, {2, 2}, {0, 2}});
    const Read r = readBytes(dir, CadFixture::document("ANSI_1252", entities));
    QVERIFY2(r.ok, qUtf8Printable(r.error + QLatin1Char(' ') + r.details));
    QCOMPARE(countKind(r.drawing, CadKind::Line), 2);
    QCOMPARE(countKind(r.drawing, CadKind::Text), 1);
    QCOMPARE(countKind(r.drawing, CadKind::Point), 1);
    QCOMPARE(countKind(r.drawing, CadKind::Fill), 1);
    QCOMPARE(r.drawing.paperSpaceSkipped, 1);
    for (const CadEntity& e : r.drawing.entities) QVERIFY(e.cadLayer != QLatin1String("0"));
  }

  void read_utf8TextUnderAnAnsi949Header() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const Read r = readBytes(dir, CadFixture::document("ANSI_949", CadFixture::text("JIBUN", 3, 1, 1, 2, 0, kGasuriUtf8)));
    QVERIFY2(r.ok, qUtf8Printable(r.error + QLatin1Char(' ') + r.details));
    const CadEntity* t = firstText(r.drawing);
    QVERIFY(t);
    QCOMPARE(t->text, QStringLiteral("가수리 374-1전"));
    QVERIFY(r.drawing.readAsUtf8);
  }

  void read_cp949TextUnderAnAnsi949Header() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const Read r = readBytes(dir, CadFixture::document("ANSI_949", CadFixture::text("JIBUN", 3, 1, 1, 2, 0, kGasuriCp949)));
    QVERIFY2(r.ok, qUtf8Printable(r.error + QLatin1Char(' ') + r.details));
    const CadEntity* t = firstText(r.drawing);
    QVERIFY(t);
    QCOMPARE(t->text, QStringLiteral("가수리 374-1전"));
    QVERIFY(!r.drawing.readAsUtf8);
  }

  // 정렬점 0,0 때문에 GDAL 스타일에는 dx:-387050g dy:-280025g 가 붙는다. 글자는 점 위치에 그대로 둔다.
  void read_textHeightAngleAnchorIgnoringOffsets() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const Read r =
        readBytes(dir, CadFixture::document("ANSI_1252", CadFixture::text("JIBUN", 3, 387050, 280025, 2.5, 30, "T1")));
    QVERIFY2(r.ok, qUtf8Printable(r.error + QLatin1Char(' ') + r.details));
    const CadEntity* t = firstText(r.drawing);
    QVERIFY(t);
    QCOMPARE(t->textHeight, 2.5);
    QCOMPARE(t->textAngle, 30.0);
    QCOMPARE(t->textAnchor, 1);
    QCOMPARE(t->geometry.asPoint(), QgsPointXY(387050, 280025));
    QCOMPARE(t->color, QColor(0, 255, 0));
  }

  // 블록 안의 블록(겹친 INSERT)도 한 조각씩 빠짐없이 읽는다. GDAL 은 그것을 모음 안의 모음(선·점이 섞인 블록)이나
  // 여러선(선만 있는 블록)으로 주는데, 변환본 표는 한 조각 선·점만 받는다(2026-10-02 ogrinfo 로 잰 모양).
  void read_nestedBlocksGiveSingleParts() {
    QTemporaryDir dir;
    const QByteArray blocks =
        CadFixture::block("MIXED", CadFixture::line("A", 1, 0, 0, 10, 0) + CadFixture::point("A", 1, 3, 3)) +
        CadFixture::block("LINES", CadFixture::line("A", 1, 0, 0, 10, 0) + CadFixture::line("A", 1, 0, 0, 0, 5)) +
        CadFixture::block("OUTER", CadFixture::line("A", 1, 0, 0, 0, 10) + CadFixture::insert("A", "MIXED", 100, 0) +
                                       CadFixture::insert("A", "LINES", 200, 0));
    const Read r =
        readBytes(dir, CadFixture::document("ANSI_1252", CadFixture::insert("A", "OUTER", 1000, 0), blocks));
    QVERIFY2(r.ok, qUtf8Printable(r.error + QLatin1Char(' ') + r.details));
    QCOMPARE(countKind(r.drawing, CadKind::Line), 4);
    QCOMPARE(countKind(r.drawing, CadKind::Point), 1);
    for (const CadEntity& e : r.drawing.entities)
      QVERIFY2(!QgsWkbTypes::isMultiType(e.geometry.wkbType()), qUtf8Printable(e.geometry.asWkt(0)));
  }

  void read_dropsZ() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const Read r = readBytes(dir, CadFixture::document("ANSI_1252", CadFixture::line("WALL", 1, 0, 0, 10, 0, false, 12)));
    QVERIFY2(r.ok, qUtf8Printable(r.error + QLatin1Char(' ') + r.details));
    QCOMPARE(r.drawing.entities.size(), 1);
    QVERIFY(!QgsWkbTypes::hasZ(r.drawing.entities.first().geometry.wkbType()));
  }

  void read_robustExtentIgnoresFarAway() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QByteArray entities;
    for (int i = 0; i < 40; ++i) {
      const double x = 387000 + (i % 10) * 10;
      const double y = 280000 + (i / 10) * 25;
      entities += CadFixture::line("JIJUK", 7, x, y, x + 5, y + 5);
    }
    entities += CadFixture::text("JIBUN", 7, 445796, 195312, 2, 0, "far");
    const Read r = readBytes(dir, CadFixture::document("ANSI_1252", entities));
    QVERIFY2(r.ok, qUtf8Printable(r.error + QLatin1Char(' ') + r.details));
    const QgsRectangle inside(386990, 279990, 387110, 280110);
    QVERIFY2(inside.contains(r.drawing.robustExtent), qUtf8Printable(r.drawing.robustExtent.toString()));
  }

  // 2026-10-03 「…_5187.dxf」: 지적선은 제자리, 지번 글자 1,043개(거의 절반)는 파일 안에서 수백 km 떨어진 곳.
  // 글자까지 섞어 범위를 잡으면 388 km 가 되어 「좌표 없는 도면」으로 판단하고 정합을 열었다. 선·면이 있으면 그것으로 잡는다.
  void read_robustExtentFollowsLinesWhenTextsSitApart() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QByteArray entities;
    for (int i = 0; i < 40; ++i) {
      const double x = 207000 + (i % 10) * 10;
      const double y = 378000 + (i / 10) * 25;
      entities += CadFixture::line("JIJUK", 7, x, y, x + 5, y + 5);
      entities += CadFixture::text("JIBUN", 7, -180270 + i, 98300 + i, 2, 0, "454");
    }
    const Read r = readBytes(dir, CadFixture::document("ANSI_1252", entities));
    QVERIFY2(r.ok, qUtf8Printable(r.error + QLatin1Char(' ') + r.details));
    const QgsRectangle inside(206990, 377990, 207110, 378110);
    QVERIFY2(inside.contains(r.drawing.robustExtent), qUtf8Printable(r.drawing.robustExtent.toString()));
  }

  void read_onlyPaperSpaceSaysSo() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QByteArray paperOnly = CadFixture::line("0", 7, 0, 0, 420, 0, true) + CadFixture::line("0", 7, 0, 0, 0, 297, true);
    const Read r = readBytes(dir, CadFixture::document("ANSI_1252", paperOnly));
    QVERIFY(!r.ok);
    QCOMPARE(r.error, QStringLiteral("도면에 모형 공간 도형이 없습니다."));
    QCOMPARE(r.details, QStringLiteral("종이 공간 도형 2개만 있습니다."));
  }

  void read_leavesTheFileUntouched() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QByteArray bytes = CadFixture::document("ANSI_949", CadFixture::text("JIBUN", 3, 1, 1, 2, 0, kGasuriCp949));
    const QString path = dir.filePath(QStringLiteral("원본.dxf"));
    QVERIFY(CadFixture::write(path, bytes));
    CadDrawing drawing;
    QString error, details;
    QVERIFY(CadDrawingReader::read(path, &drawing, &error, &details));
    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadOnly));
    QCOMPARE(file.readAll(), bytes);
  }

  void read_cancelStops() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const Read r = readBytes(dir, CadFixture::document("ANSI_1252", CadFixture::line("WALL", 1, 0, 0, 10, 0)),
                             [] { return true; });
    QVERIFY(!r.ok);
    QCOMPARE(r.error, QStringLiteral("도면 읽기를 취소했습니다."));
  }
};

int main(int argc, char** argv) {
  QStandardPaths::setTestModeEnabled(true);
  QCoreApplication app(argc, argv);
  TestCadReader tests;
  return QTest::qExec(&tests, argc, argv);
}

#include "test_cad_reader.moc"
