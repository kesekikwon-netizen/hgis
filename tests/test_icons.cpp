// F090/F091: every icon id the app asks for has its own glyph, an unknown id is
// loud instead of a silent 「새 조사」, menu globes and arrows no longer repeat,
// glyph stroke widths are live, the map/output group hues sit apart, and the Mockup glyph
// style (grey tile + Lucide line icon) is the default and paints the measured colours.
#include <QtTest>
#include <QApplication>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QImage>
#include <QPainter>
#include <QRegularExpression>
#include <QSet>
#include "app/KaIconMetrics.h"
#include "app/KaIcons.h"
#include "app/KaIconsMockup.h"
#include "app/KaTheme.h"
#include "icon_pixels.h"

namespace {

// The 27 ribbon chips (MainWindowRibbon.cpp), then the tab / map-control / panel ids.
const char* const mappedIds[] = {
    "new", "open", "save", "save_as", "select", "measure", "draw_poly", "trench_grid", "buffer", "cadastral",
    "topo_download", "heritage", "contour", "dem", "survey_contour", "soil", "paleo", "geology", "river", "old_map",
    "georef", "pdf", "print", "section", "geotiff", "export_convert", "more", "home", "map", "region", "search",
    "undo", "redo", "zoom_in", "zoom_out", "zoom_fit", "folder", "note", "layers", "warn", "snap", "chevron_left",
    "chevron_right", "satellite", "save_unsaved",
};
const QColor kTile(0xE4, 0xEA, 0xED);
const QColor kInk(0x2B, 0x48, 0x58);
const QColor kOrange(0xF2, 0xA3, 0x3A);

// The pixel at (x, y) of `image` is within `tol` per channel of `want`.
#define VERIFY_NEAR(image, x, y, want, tol) \
  QVERIFY2(IconPixels::near((image).pixelColor(x, y), (want), (tol)), qPrintable((image).pixelColor(x, y).name()))

// A mockup icon rendered at px device pixels in one mode/state.
QImage mockupImage(const QString& id, int px, QIcon::Mode mode, QIcon::State state, bool strong = false) {
  const QPixmap pm = KaIconsMockup::mockupIcon(id, strong).pixmap(QSize(px, px), 1.0, mode, state);
  return pm.toImage().convertToFormat(QImage::Format_ARGB32);
}

// The pixel of the 18 px glyph box closest to `target`: a stroke centre is fully covered, so it is exact.
QColor closestPixel(const QImage& image, const QColor& target) {
  const auto gap = [&](const QColor& c) {
    return qAbs(c.red() - target.red()) + qAbs(c.green() - target.green()) + qAbs(c.blue() - target.blue());
  };
  QColor best = image.pixelColor(16, 16);
  for (int y = 7; y < 25; ++y)
    for (int x = 7; x < 25; ++x)
      if (gap(image.pixelColor(x, y)) < gap(best)) best = image.pixelColor(x, y);
  return best;
}

// Clears the style variable and restores the glyph style even when a check returns early.
struct StyleGuard {
  KaIcons::GlyphStyle previous = KaIcons::glyphStyle();
  ~StyleGuard() {
    qunsetenv("KA_HGIS_ICON_STYLE");
    KaIcons::setGlyphStyle(previous);
  }
};

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
  void mockupIsDefault();
  void environmentSelectsLegacyStyles();
  void everyMappedIdHasSvg();
  void normalTileColours();
  void checkedTileHasBorder();
  void unsavedSaveHasDot();
  void disabledIsFaded();
  void hoverTileIsDarker();
  void mockupIconIsCrispAtDpr150();
  void kaIconsUsesMockupAndFallsBackToOutline();
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

void TestIcons::mockupIsDefault() {
  StyleGuard guard;
  qunsetenv("KA_HGIS_ICON_STYLE");
  QCOMPARE(KaIcons::glyphStyleFromEnvironment(), KaIcons::GlyphStyle::Mockup);
  QCOMPARE(KaIcons::glyphStyle(), KaIcons::GlyphStyle::Mockup);
}

void TestIcons::environmentSelectsLegacyStyles() {
  StyleGuard guard;
  qputenv("KA_HGIS_ICON_STYLE", "outline");
  QCOMPARE(KaIcons::glyphStyleFromEnvironment(), KaIcons::GlyphStyle::Outline);
  qputenv("KA_HGIS_ICON_STYLE", "tile");
  QCOMPARE(KaIcons::glyphStyleFromEnvironment(), KaIcons::GlyphStyle::Tile);
  qputenv("KA_HGIS_ICON_STYLE", "something else");
  QCOMPARE(KaIcons::glyphStyleFromEnvironment(), KaIcons::GlyphStyle::Mockup);
}

void TestIcons::everyMappedIdHasSvg() {
  for (const char* name : mappedIds) {
    const QString path = KaIconsMockup::svgPathFor(QString::fromLatin1(name));
    QVERIFY2(!path.isEmpty(), name);
    QVERIFY2(QFile::exists(path), qPrintable(path));
  }
  QVERIFY(KaIconsMockup::svgPathFor(QStringLiteral("definitely_not_an_icon")).isEmpty());
  QVERIFY(KaIconsMockup::mockupIcon(QStringLiteral("definitely_not_an_icon")).isNull());
  // Every bundled SVG (37 Lucide + 3 Strata drawings) recolours through currentColor.
  int svgFiles = 0;
  for (QDirIterator it(QStringLiteral(":/ka-hgis/icons"), {QStringLiteral("*.svg")}, QDir::Files,
                       QDirIterator::Subdirectories);
       it.hasNext(); ++svgFiles) {
    QFile file(it.next());
    QVERIFY2(file.open(QIODevice::ReadOnly) && file.readAll().contains("currentColor"), qPrintable(file.fileName()));
  }
  QCOMPARE(svgFiles, 40);
}

void TestIcons::normalTileColours() {
  const QImage image = mockupImage(QStringLiteral("save"), 32, QIcon::Normal, QIcon::Off);
  QCOMPARE(image.size(), QSize(32, 32));
  // 12 px from the centre is tile, outside the 18 px glyph box.
  VERIFY_NEAR(image, 28, 16, kTile, 6);
  VERIFY_NEAR(image, 4, 16, kTile, 6);
  VERIFY_NEAR(image, 16, 28, kTile, 6);
  QVERIFY(image.pixelColor(0, 0).alpha() < 20);
  QVERIFY2(IconPixels::near(closestPixel(image, kInk), kInk, 12), qPrintable(closestPixel(image, kInk).name()));
}

void TestIcons::checkedTileHasBorder() {
  const QColor border(0x1A, 0x68, 0xB0);
  const QColor fill(0xE0, 0xEC, 0xF8);
  const QImage image = mockupImage(QStringLiteral("save"), 32, QIcon::Normal, QIcon::On);
  VERIFY_NEAR(image, 1, 16, border, 10);
  VERIFY_NEAR(image, 16, 1, border, 10);
  VERIFY_NEAR(image, 4, 16, fill, 6);
  const QColor navy(0x10, 0x50, 0x88);
  QVERIFY2(IconPixels::near(closestPixel(image, navy), navy, 12), qPrintable(closestPixel(image, navy).name()));
  // The unchecked tile has no border: the same edge pixel is the tile grey.
  VERIFY_NEAR(mockupImage(QStringLiteral("save"), 32, QIcon::Normal, QIcon::Off), 1, 16, kTile, 6);
}

void TestIcons::unsavedSaveHasDot() {
  const QImage image = mockupImage(QStringLiteral("save_unsaved"), 32, QIcon::Normal, QIcon::Off, true);
  VERIFY_NEAR(image, 4, 16, QColor(0x20, 0x6C, 0xB0), 6);
  // The dot is centred at (26, 6): the pixels around that corner are the warn orange.
  VERIFY_NEAR(image, 25, 5, kOrange, 10);
  VERIFY_NEAR(image, 26, 6, kOrange, 10);
  // A plain save has neither the blue tile nor the dot.
  const QImage plain = mockupImage(QStringLiteral("save"), 32, QIcon::Normal, QIcon::Off);
  QVERIFY(!IconPixels::near(plain.pixelColor(25, 5), kOrange, 40));
  VERIFY_NEAR(plain, 4, 16, kTile, 6);
}

void TestIcons::disabledIsFaded() {
  const QImage image = mockupImage(QStringLiteral("save"), 32, QIcon::Disabled, QIcon::Off);
  VERIFY_NEAR(image, 28, 16, QColor(0xEE, 0xF1, 0xF3), 6);
  // The darkest glyph pixel is at most half way from the tile to the ink (45 % opacity).
  const qreal share = (238.0 - closestPixel(image, kInk).red()) / (238.0 - kInk.red());
  QVERIFY2(share <= 0.5 && share >= 0.2, qPrintable(QString::number(share)));
  QCOMPARE(KaIconsMockup::lookFor(QIcon::Disabled, QIcon::Off, false).glyphOpacity, 0.45);
  QVERIFY(!KaIconsMockup::lookFor(QIcon::Disabled, QIcon::Off, true).dot);  // disabled wins over unsaved
}

void TestIcons::hoverTileIsDarker() {
  VERIFY_NEAR(mockupImage(QStringLiteral("save"), 32, QIcon::Active, QIcon::Off), 28, 16, QColor(0xD9, 0xE2, 0xE7), 6);
  QCOMPARE(KaIconsMockup::lookFor(QIcon::Active, QIcon::On, false).tile, QColor(0xE0, 0xEC, 0xF8));
}

void TestIcons::mockupIconIsCrispAtDpr150() {
  // A 32 px button on a 150 % screen: the engine paints 48 device pixels, drawn 1:1.
  QImage image(48, 48, QImage::Format_ARGB32);
  image.setDevicePixelRatio(1.5);
  image.fill(Qt::transparent);
  QPainter painter(&image);
  KaIconsMockup::mockupIcon(QStringLiteral("save")).paint(&painter, QRect(0, 0, 32, 32), Qt::AlignCenter, QIcon::Normal, QIcon::On);
  painter.end();
  // Walking in from the top and the left edge: 3 border pixels, then fill, at most two blended in between.
  for (const bool horizontal : {true, false}) {
    int blended = 0;
    int borderRun = 0;
    for (int i = 0; i < 8; ++i) {
      const QColor c = horizontal ? image.pixelColor(24, i) : image.pixelColor(i, 24);
      if (IconPixels::near(c, QColor(0x1A, 0x68, 0xB0), 10)) ++borderRun;
      else if (!IconPixels::near(c, QColor(0xE0, 0xEC, 0xF8), 6)) ++blended;
    }
    QVERIFY2(blended <= 2 && borderRun >= 2, qPrintable(QStringLiteral("%1 blended, %2 border").arg(blended).arg(borderRun)));
  }
}

void TestIcons::kaIconsUsesMockupAndFallsBackToOutline() {
  StyleGuard guard;
  KaIcons::setGlyphStyle(KaIcons::GlyphStyle::Mockup);
  // 8,32 is tile and not glyph at 64 px: a mapped id gets the grey tile, save_unsaved the blue one...
  VERIFY_NEAR(render(QStringLiteral("new")), 8, 32, kTile, 6);
  VERIFY_NEAR(render(QStringLiteral("save_unsaved")), 8, 32, QColor(0x20, 0x6C, 0xB0), 6);
  // ...an id without an SVG is drawn in the outline style (no tile), and so is a monochrome request.
  QVERIFY(KaIconsMockup::svgPathFor(QStringLiteral("crs")).isEmpty());
  QCOMPARE(render(QStringLiteral("crs")).pixelColor(8, 32).alpha(), 0);
  const QPixmap inked = KaIcons::icon(QStringLiteral("new"), Qt::red).pixmap(QSize(64, 64), 1.0);
  QCOMPARE(inked.toImage().pixelColor(8, 32).alpha(), 0);
  // The legacy style keeps its look behind the switch.
  KaIcons::setGlyphStyle(KaIcons::GlyphStyle::Outline);
  QCOMPARE(render(QStringLiteral("new")).pixelColor(8, 32).alpha(), 0);
}

QTEST_MAIN(TestIcons)
#include "test_icons.moc"
