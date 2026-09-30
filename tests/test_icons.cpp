// F090/F091: every icon id the app asks for has its own glyph, an unknown id is
// loud instead of a silent 「새 조사」, menu globes and arrows no longer repeat,
// glyph stroke widths are live, and the map/output group hues sit apart.
#include <QtTest>
#include <QApplication>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QImage>
#include <QRegularExpression>
#include <QSet>
#include "app/KaIconMetrics.h"
#include "app/KaIcons.h"
#include "app/KaTheme.h"

namespace {

QImage render(const QString& id) {
  return KaIcons::icon(id).pixmap(QSize(64, 64), 1.0).toImage().convertToFormat(QImage::Format_ARGB32);
}

// Share of opaque pixels whose colour differs clearly between two renders.
double differingShare(const QImage& a, const QImage& b) {
  int opaque = 0;
  int differing = 0;
  for (int y = 0; y < a.height(); ++y) {
    for (int x = 0; x < a.width(); ++x) {
      const QColor p = a.pixelColor(x, y);
      const QColor q = b.pixelColor(x, y);
      if (p.alpha() < 128 && q.alpha() < 128) continue;
      ++opaque;
      if (qAbs(p.red() - q.red()) + qAbs(p.green() - q.green()) + qAbs(p.blue() - q.blue()) > 60) ++differing;
    }
  }
  return opaque ? double(differing) / opaque : 0.0;
}

// Icon ids named as string literals in the app sources.
QSet<QString> usedIconIds() {
  QSet<QString> ids;
  const QRegularExpression patterns[] = {
      QRegularExpression(QStringLiteral(R"re(KaIcons::(?:icon|strongIcon)\(QStringLiteral\("(\w+)"\))re")),
      QRegularExpression(QStringLiteral(R"re(addIcon\(\s*QStringLiteral\("\w+"\),\s*QStringLiteral\("(\w+)"\))re")),
      QRegularExpression(QStringLiteral(R"re((?:addWeb|makeOutput|addBottom)\(QStringLiteral\("(\w+)"\))re")),
      QRegularExpression(QStringLiteral(R"re(paintPrimary\(\w+,\s*QStringLiteral\("(\w+)"\))re")),
  };
  QDirIterator it(QStringLiteral("src/app"), {QStringLiteral("*.cpp")}, QDir::Files);
  while (it.hasNext()) {
    QFile file(it.next());
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) continue;
    const QString text = QString::fromUtf8(file.readAll());
    for (const auto& pattern : patterns)
      for (auto m = pattern.globalMatch(text); m.hasNext();) ids.insert(m.next().captured(1));
  }
  return ids;
}

}  // namespace

class TestIcons : public QObject {
  Q_OBJECT
private slots:
  void everyUsedIdHasItsOwnGlyph();
  void unknownIdIsLoudAndDistinct();
  void menuGlobesAndArrowsAreDistinct();
  void strokeWidthsAreLive();
  void mapAndOutputHuesSitApart();
};

void TestIcons::everyUsedIdHasItsOwnGlyph() {
  const QSet<QString> ids = usedIconIds();
  QVERIFY2(ids.size() >= 30, qPrintable(QStringLiteral("found %1 ids; run from the source tree").arg(ids.size())));
  for (const QString& id : ids)
    QVERIFY2(KaIcons::hasIcon(id), qPrintable(QStringLiteral("no glyph for icon id \"%1\"").arg(id)));
}

void TestIcons::unknownIdIsLoudAndDistinct() {
  const QString id = QStringLiteral("definitely_not_an_icon");
  QVERIFY(!KaIcons::hasIcon(id));
  QTest::ignoreMessage(QtWarningMsg, "KaIcons: no glyph for icon id definitely_not_an_icon");
  const QImage missing = render(id);
  QVERIFY(differingShare(missing, render(QStringLiteral("new"))) > 0.05);
  // One warning per id, not one per call.
  render(id);
}

void TestIcons::menuGlobesAndArrowsAreDistinct() {
  const QStringList globes = {QStringLiteral("satellite"), QStringLiteral("crs"), QStringLiteral("web")};
  for (int i = 0; i < globes.size(); ++i)
    for (int j = i + 1; j < globes.size(); ++j)
      QVERIFY2(differingShare(render(globes[i]), render(globes[j])) > 0.05,
               qPrintable(globes[i] + QStringLiteral(" vs ") + globes[j]));
  QVERIFY2(differingShare(render(QStringLiteral("export")), render(QStringLiteral("upload"))) > 0.05,
           "export vs upload");
}

void TestIcons::strokeWidthsAreLive() {
  // 64 px bake: 0.84 device px per glyph unit inside the tile.
  QCOMPARE(KaIconMetrics::strokeWidth(2.4, 0.84), 2.4);
  QCOMPARE(KaIconMetrics::strokeWidth(3.2, 0.84), 3.2);
  QVERIFY(KaIconMetrics::strokeWidth(2.4, 0.84) < KaIconMetrics::strokeWidth(3.0, 0.84));
  // A too-thin request is floored at 2 device px (1 px at a 32 px ribbon icon).
  QVERIFY(qFuzzyCompare(KaIconMetrics::strokeWidth(1.0, 0.84) * 0.84, KaIconMetrics::kMinDeviceStroke));
  QCOMPARE(KaIconMetrics::strokeWidth(3.0, 1.68), 3.0);
}

// IconPalette values only: the tile style paints them, the outline style keeps them for
// fallback tinting, so this holds in either glyph style.
void TestIcons::mapAndOutputHuesSitApart() {
  const auto& palette = KaTheme::iconPalette();
  const int distance = qAbs(palette.map.hslHue() - palette.output.hslHue());
  QVERIFY2(qMin(distance, 360 - distance) >= 30,
           qPrintable(QStringLiteral("배경 지도 %1 vs 내보내기 %2").arg(palette.map.name(), palette.output.name())));
  QVERIFY(palette.map.green() > palette.map.blue());       // green
  QVERIFY(palette.output.blue() >= palette.output.green());  // teal
}

QTEST_MAIN(TestIcons)
#include "test_icons.moc"
