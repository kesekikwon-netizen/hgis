// The Mockup glyph style (grey tile + Lucide line icon): it is the default, the env switch keeps
// the older styles, every mapped id has its SVG, and the measured colours of the mockup are
// what the tile, the border, the unsaved dot and the disabled fade actually paint.
#include <QtTest>
#include <QApplication>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QImage>
#include <QPainter>
#include "app/KaIcons.h"
#include "app/KaIconsMockup.h"
#include "app/KaTheme.h"
#include "icon_pixels.h"

namespace {

// The 27 ribbon chips (MainWindowRibbon.cpp), then the tab / map-control / panel ids the app uses.
const char* const mappedIds[] = {
    "new", "open", "save", "save_as", "select", "measure", "draw_poly", "trench_grid", "buffer", "cadastral",
    "topo_download", "heritage", "contour", "dem", "survey_contour", "soil", "paleo", "geology", "river", "old_map",
    "georef", "pdf", "print", "section", "geotiff", "export_convert", "more", "home", "map", "region", "search",
    "undo", "redo", "zoom_in", "zoom_out", "zoom_fit", "folder", "note", "layer", "warn", "snap", "chevron_left",
    "chevron_right", "satellite", "save_unsaved",
    // The drawing sub-toolbar (MainWindowRibbon.cpp, KaDrawSketchTools.cpp).
    "check", "stop", "draw_area", "draw_line", "easy_draw", "artifact", "transform",
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

// Walking in from the top and the left edge of a checked 150 % icon (48 device pixels): 3 border
// pixels, then fill, at most two blended pixels in between (a straight edge, no stair-step).
void verifyStraightEdges(const QImage& image) {
  QCOMPARE(image.size(), QSize(48, 48));
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

// Clears the style variable and restores the glyph style even when a check returns early.
struct StyleGuard {
  KaIcons::GlyphStyle previous = KaIcons::glyphStyle();
  ~StyleGuard() {
    qunsetenv("KA_HGIS_ICON_STYLE");
    KaIcons::setGlyphStyle(previous);
  }
};

}  // namespace

class TestIconsMockup : public QObject {
  Q_OBJECT
private slots:
  void mockupIsDefault();
  void environmentSelectsLegacyStyles();
  void everyMappedIdHasSvg();
  void normalTileColours();
  void checkedTileHasBorder();
  void unsavedSaveHasDot();
  void disabledIsFaded();
  void hoverTileIsDarker();
  void pixmapHonoursDevicePixelRatio();
  void paperLook_tilesFollowTokens();
  void mockupIconIsCrispAtDpr150();
  void paintIsCrispAtDpr150();
  void kaIconsUsesMockupAndFallsBackToOutline();
};

void TestIconsMockup::mockupIsDefault() {
  StyleGuard guard;
  qunsetenv("KA_HGIS_ICON_STYLE");
  QCOMPARE(KaIcons::glyphStyleFromEnvironment(), KaIcons::GlyphStyle::Mockup);
  QCOMPARE(KaIcons::glyphStyle(), KaIcons::GlyphStyle::Mockup);
}

void TestIconsMockup::environmentSelectsLegacyStyles() {
  StyleGuard guard;
  qputenv("KA_HGIS_ICON_STYLE", "outline");
  QCOMPARE(KaIcons::glyphStyleFromEnvironment(), KaIcons::GlyphStyle::Outline);
  qputenv("KA_HGIS_ICON_STYLE", "tile");
  QCOMPARE(KaIcons::glyphStyleFromEnvironment(), KaIcons::GlyphStyle::Tile);
  qputenv("KA_HGIS_ICON_STYLE", "something else");
  QCOMPARE(KaIcons::glyphStyleFromEnvironment(), KaIcons::GlyphStyle::Mockup);
}

void TestIconsMockup::everyMappedIdHasSvg() {
  for (const char* name : mappedIds) {
    const QString path = KaIconsMockup::svgPathFor(QString::fromLatin1(name));
    QVERIFY2(!path.isEmpty(), name);
    QVERIFY2(QFile::exists(path), qPrintable(path));
  }
  QVERIFY(KaIconsMockup::svgPathFor(QStringLiteral("definitely_not_an_icon")).isEmpty());
  QVERIFY(KaIconsMockup::mockupIcon(QStringLiteral("definitely_not_an_icon")).isNull());
  // Every bundled SVG (44 Lucide + 3 Strata drawings) recolours through currentColor.
  int svgFiles = 0;
  for (QDirIterator it(QStringLiteral(":/ka-hgis/icons"), {QStringLiteral("*.svg")}, QDir::Files,
                       QDirIterator::Subdirectories);
       it.hasNext(); ++svgFiles) {
    QFile file(it.next());
    QVERIFY2(file.open(QIODevice::ReadOnly) && file.readAll().contains("currentColor"), qPrintable(file.fileName()));
  }
  QCOMPARE(svgFiles, 47);
}

void TestIconsMockup::normalTileColours() {
  const QImage image = mockupImage(QStringLiteral("save"), 32, QIcon::Normal, QIcon::Off);
  QCOMPARE(image.size(), QSize(32, 32));
  // 12 px from the centre is tile, outside the 18 px glyph box.
  VERIFY_NEAR(image, 28, 16, kTile, 6);
  VERIFY_NEAR(image, 4, 16, kTile, 6);
  VERIFY_NEAR(image, 16, 28, kTile, 6);
  VERIFY_NEAR(image, 16, 4, kTile, 6);
  QVERIFY(image.pixelColor(0, 0).alpha() < 20);
  QVERIFY2(IconPixels::near(closestPixel(image, kInk), kInk, 12), qPrintable(closestPixel(image, kInk).name()));
}

void TestIconsMockup::checkedTileHasBorder() {
  const QColor border(0x1A, 0x68, 0xB0);
  const QColor fill(0xE0, 0xEC, 0xF8);
  const QImage image = mockupImage(QStringLiteral("save"), 32, QIcon::Normal, QIcon::On);
  VERIFY_NEAR(image, 1, 16, border, 10);
  VERIFY_NEAR(image, 16, 1, border, 10);
  VERIFY_NEAR(image, 4, 16, fill, 6);
  VERIFY_NEAR(image, 28, 16, fill, 6);
  const QColor navy(0x10, 0x50, 0x88);
  QVERIFY2(IconPixels::near(closestPixel(image, navy), navy, 12), qPrintable(closestPixel(image, navy).name()));
  // The unchecked tile has no border: the same edge pixel is the tile grey.
  VERIFY_NEAR(mockupImage(QStringLiteral("save"), 32, QIcon::Normal, QIcon::Off), 1, 16, kTile, 6);
}

void TestIconsMockup::unsavedSaveHasDot() {
  const QImage image = mockupImage(QStringLiteral("save_unsaved"), 32, QIcon::Normal, QIcon::Off, true);
  VERIFY_NEAR(image, 4, 16, QColor(0x20, 0x6C, 0xB0), 6);
  // The dot is centred at (26, 6): its four neighbouring pixels are the warn orange.
  VERIFY_NEAR(image, 25, 5, kOrange, 10);
  VERIFY_NEAR(image, 26, 5, kOrange, 10);
  VERIFY_NEAR(image, 25, 6, kOrange, 10);
  VERIFY_NEAR(image, 26, 6, kOrange, 10);
  QVERIFY2(IconPixels::near(closestPixel(image, Qt::white), Qt::white, 12), "white glyph");
  // A plain save has neither the blue tile nor the dot.
  const QImage plain = mockupImage(QStringLiteral("save"), 32, QIcon::Normal, QIcon::Off);
  QVERIFY(!IconPixels::near(plain.pixelColor(25, 5), kOrange, 40));
  VERIFY_NEAR(plain, 4, 16, kTile, 6);
}

void TestIconsMockup::disabledIsFaded() {
  const QImage image = mockupImage(QStringLiteral("save"), 32, QIcon::Disabled, QIcon::Off);
  VERIFY_NEAR(image, 28, 16, QColor(0xEE, 0xF1, 0xF3), 6);
  // The darkest glyph pixel is at most half way from the tile to the ink (45 % opacity).
  const qreal share = (238.0 - closestPixel(image, kInk).red()) / (238.0 - kInk.red());
  QVERIFY2(share <= 0.5 && share >= 0.2, qPrintable(QString::number(share)));
  QCOMPARE(KaIconsMockup::lookFor(QIcon::Disabled, QIcon::Off, false).glyphOpacity, 0.45);
  // Disabled wins over the unsaved look and over checked.
  QVERIFY(!KaIconsMockup::lookFor(QIcon::Disabled, QIcon::Off, true).dot);
  QVERIFY(!KaIconsMockup::lookFor(QIcon::Disabled, QIcon::On, false).tileBorder.isValid());
}

void TestIconsMockup::hoverTileIsDarker() {
  VERIFY_NEAR(mockupImage(QStringLiteral("save"), 32, QIcon::Active, QIcon::Off), 28, 16, QColor(0xD9, 0xE2, 0xE7), 6);
  // A checked button stays checked under the mouse.
  QCOMPARE(KaIconsMockup::lookFor(QIcon::Active, QIcon::On, false).tile, QColor(0xE0, 0xEC, 0xF8));
}

// QIcon::pixmap(size, dpr) hands the engine the logical size and the scale: the pixmap must have
// size * dpr pixels and carry that ratio, or a high-dpi screen shows an upscaled 1x bake.
void TestIconsMockup::pixmapHonoursDevicePixelRatio() {
  const QIcon icon = KaIconsMockup::mockupIcon(QStringLiteral("save"));
  for (const qreal dpr : {1.0, 1.5, 2.0}) {
    const QPixmap pm = icon.pixmap(QSize(32, 32), dpr, QIcon::Normal, QIcon::On);
    QCOMPARE(pm.size(), QSize(qRound(32 * dpr), qRound(32 * dpr)));
    QCOMPARE(pm.devicePixelRatio(), dpr);
  }
}

void TestIconsMockup::mockupIconIsCrispAtDpr150() {
  const QIcon icon = KaIconsMockup::mockupIcon(QStringLiteral("save"));
  const QPixmap pm = icon.pixmap(QSize(32, 32), 1.5, QIcon::Normal, QIcon::On);
  QCOMPARE(pm.size(), QSize(48, 48));
  QCOMPARE(pm.devicePixelRatio(), 1.5);
  verifyStraightEdges(pm.toImage().convertToFormat(QImage::Format_ARGB32));
}

// The path a widget paints through: QIcon::paint on a device with ratio 1.5, drawn 1:1.
void TestIconsMockup::paintIsCrispAtDpr150() {
  QImage image(48, 48, QImage::Format_ARGB32);
  image.setDevicePixelRatio(1.5);
  image.fill(Qt::transparent);
  QPainter painter(&image);
  KaIconsMockup::mockupIcon(QStringLiteral("save")).paint(&painter, QRect(0, 0, 32, 32), Qt::AlignCenter, QIcon::Normal, QIcon::On);
  painter.end();
  verifyStraightEdges(image);
}

void TestIconsMockup::kaIconsUsesMockupAndFallsBackToOutline() {
  StyleGuard guard;
  KaIcons::setGlyphStyle(KaIcons::GlyphStyle::Mockup);
  // 8,32 is tile and not glyph at 64 px: a mapped id gets the grey tile, save_unsaved the blue one...
  VERIFY_NEAR(IconPixels::render(QStringLiteral("new")), 8, 32, kTile, 6);
  VERIFY_NEAR(IconPixels::render(QStringLiteral("save_unsaved")), 8, 32, QColor(0x20, 0x6C, 0xB0), 6);
  // ...an id without an SVG is drawn in the outline style (no tile), and so is a monochrome request.
  QVERIFY(KaIconsMockup::svgPathFor(QStringLiteral("crs")).isEmpty());
  QCOMPARE(IconPixels::render(QStringLiteral("crs")).pixelColor(8, 32).alpha(), 0);
  const QPixmap inked = KaIcons::icon(QStringLiteral("new"), Qt::red).pixmap(QSize(64, 64), 1.0);
  QCOMPARE(inked.toImage().pixelColor(8, 32).alpha(), 0);
  // The legacy style keeps its look behind the switch.
  KaIcons::setGlyphStyle(KaIcons::GlyphStyle::Outline);
  QCOMPARE(IconPixels::render(QStringLiteral("new")).pixelColor(8, 32).alpha(), 0);
}

// 새 모양 changes the tile and glyph colours through the theme tokens, and an icon made before the
// switch repaints in the new colours (the engine's cache is keyed by them).
void TestIconsMockup::paperLook_tilesFollowTokens() {
  KaIcons::setGlyphStyle(KaIcons::GlyphStyle::Mockup);
  const QIcon icon = KaIconsMockup::mockupIcon(QStringLiteral("save"), false);
  const auto tileOf = [&icon](QIcon::State state) {
    return icon.pixmap(QSize(32, 32), 1.0, QIcon::Normal, state).toImage().convertToFormat(QImage::Format_ARGB32);
  };
  VERIFY_NEAR(tileOf(QIcon::Off), 4, 16, kTile, 6);
  KaTheme::DisplayOptions paper;
  paper.paperLook = true;
  KaTheme::setDisplayOptions(nullptr, paper);
  const QImage off = tileOf(QIcon::Off);
  const QImage on = tileOf(QIcon::On);
  KaTheme::setDisplayOptions(nullptr, KaTheme::DisplayOptions());  // restore before a check can return early
  VERIFY_NEAR(off, 4, 16, QColor(0xF0, 0xEE, 0xE6), 6);
  VERIFY_NEAR(on, 1, 16, QColor(0xB5, 0x57, 0x3A), 10);
  VERIFY_NEAR(on, 4, 16, QColor(0xF4, 0xE3, 0xDA), 6);
  VERIFY_NEAR(tileOf(QIcon::Off), 4, 16, kTile, 6);
}

QTEST_MAIN(TestIconsMockup)
#include "test_icons_mockup.moc"
