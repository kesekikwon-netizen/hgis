// P2 icons: the outline glyph style. Every ribbon chip and UI id has its own outline drawing,
// every id the app names renders in this style without a "?" box, the normal state is one ink
// on a transparent square, checked / disabled / save_unsaved follow the tokens, small direct
// renders work for chips, and the legacy tile style stays reachable behind the switch.
#include <QtTest>
#include <QApplication>
#include <QDir>
#include <QImage>
#include <QPainter>
#include "app/KaIconMetrics.h"
#include "app/KaIcons.h"
#include "app/KaIconsOutline.h"
#include "app/KaTheme.h"
#include "icon_pixels.h"

using namespace IconPixels;

namespace {

// §4.2: the 27 ribbon chips.
const char* const ribbonIds[] = {
    "new",     "open",  "save",     "save_as", "select",         "measure", "draw_poly", "trench_grid", "buffer",
    "cadastral", "topo_download", "heritage", "contour", "dem",  "survey_contour", "soil", "paleo", "geology",
    "river",   "old_map", "georef", "pdf",    "print",   "section", "geotiff", "export_convert", "more",
};
// §4.3: tabs, home, panels, chips, map overlay.
const char* const uiIds[] = {
    "home",  "map",  "search", "undo",   "redo",         "check",         "warn",  "missing", "survey_thumb",
    "clock", "folder", "lock", "note",   "pencil",       "chevron_left",  "chevron_right", "snap", "layer",
    "satellite", "list", "brush", "photo",
};
// Used by the app but drawn only in the legacy table: they take the flat ink fallback.
const char* const fallbackIds[] = {"crs", "web", "old_topo", "terrain_3d", "layout_legend"};

QStringList names(const char* const* ids, int count) {
  QStringList list;
  for (int i = 0; i < count; ++i) list << QString::fromLatin1(ids[i]);
  return list;
}

}  // namespace

class TestIconsOutline : public QObject {
  Q_OBJECT
private slots:
  void initTestCase();
  void everyRibbonIdHasOutlineGlyph();
  void everyUsedIdRendersInOutlineStyle();
  void outlineNormalIsMonochromeInk();
  void outlineHasNoTile();
  void outlineVisibleAt32px();
  void checkedUsesAccentWashAndAccentGlyph();
  void disabledIsNeutralGrey();
  void fallbackIdsAreMonochrome();
  void saveUnsavedHasAccentTileAndWarnDot();
  void envVarSelectsTile();
  void outlineIdsAreDistinct();
  void glyphPixmapSmallSizes();
  void contactSheet();
};

void TestIconsOutline::initTestCase() {
  // The app default is the Mockup style; this file tests the Outline style behind the switch.
  KaIcons::setGlyphStyle(KaIcons::GlyphStyle::Outline);
  QCOMPARE(KaIcons::glyphStyle(), KaIcons::GlyphStyle::Outline);
  QCOMPARE(KaIconMetrics::kOutlineInset, 12);
}

void TestIconsOutline::everyRibbonIdHasOutlineGlyph() {
  for (const char* id : ribbonIds) QVERIFY2(KaIconsOutline::hasOutlineGlyph(QString::fromLatin1(id)), id);
  for (const char* id : uiIds) QVERIFY2(KaIconsOutline::hasOutlineGlyph(QString::fromLatin1(id)), id);
  QVERIFY(KaIconsOutline::hasOutlineGlyph(QStringLiteral("save_unsaved")));
  QVERIFY(KaIcons::hasIcon(QStringLiteral("home")));
}

void TestIconsOutline::everyUsedIdRendersInOutlineStyle() {
  const QSet<QString> ids = usedIconIds();
  QVERIFY2(ids.size() >= 30, qPrintable(QStringLiteral("found %1 ids; run from the source tree").arg(ids.size())));
  QTest::ignoreMessage(QtWarningMsg, "KaIcons: no glyph for icon id definitely_not_an_icon");
  const QImage missingBox = render(QStringLiteral("definitely_not_an_icon"));
  for (const QString& id : ids) {
    QVERIFY2(KaIcons::hasIcon(id), qPrintable(id));
    const QImage image = render(id);
    QVERIFY2(covered(image) >= 40, qPrintable(id));
    QVERIFY2(differingShare(image, missingBox) > 0.05, qPrintable(id + QStringLiteral(" renders as the ? box")));
  }
}

void TestIconsOutline::outlineNormalIsMonochromeInk() {
  const QColor ink = KaTheme::iconPalette().ink;
  for (const QString& id : names(ribbonIds, 27) + names(uiIds, 22)) {
    const QImage image = render(id);
    const int opaque = covered(image, 200);
    const int inked = countPixels(image, 200, [&](const QColor& c) { return near(c, ink, 24); });
    QVERIFY2(opaque >= 40 && inked == opaque,
             qPrintable(QStringLiteral("%1: %2 of %3 opaque pixels are ink").arg(id).arg(inked).arg(opaque)));
  }
}

void TestIconsOutline::outlineHasNoTile() {
  for (const char* id : ribbonIds) {
    const QImage image = render(QString::fromLatin1(id));
    QCOMPARE(image.pixelColor(4, 4).alpha(), 0);
    QCOMPARE(image.pixelColor(60, 60).alpha(), 0);
    QVERIFY2(bandIsClear(image, 8), id);
    QVERIFY2(bandIsClear(render(QString::fromLatin1(id), 32), 4), id);
  }
}

void TestIconsOutline::outlineVisibleAt32px() {
  // 3.2 units on the 64 grid is 1.6 px at 32 px, floored to 2 device px; 1.5 px below 24 px.
  QVERIFY(KaIconMetrics::outlineStrokeWidth(KaIconMetrics::kOutlineStroke, 0.5) * 0.5 >= 1.5);
  QCOMPARE(KaIconMetrics::outlineStrokeWidth(KaIconMetrics::kOutlineStroke, 0.5) * 0.5, 2.0);
  QCOMPARE(KaIconMetrics::outlineStrokeWidth(KaIconMetrics::kOutlineStroke, 14.0 / 64.0) * 14.0 / 64.0, 1.5);
  QCOMPARE(KaIconMetrics::outlineStrokeWidth(KaIconMetrics::kOutlineStroke, 1.0), 3.2);
  for (const char* id : ribbonIds) QVERIFY2(covered(render(QString::fromLatin1(id), 32)) >= 40, id);
}

void TestIconsOutline::checkedUsesAccentWashAndAccentGlyph() {
  const auto& tokens = KaTheme::tokens();
  for (const char* id : {"draw_poly", "contour", "select", "snap", "trench_grid"}) {
    const QImage on = render(QString::fromLatin1(id), 64, QIcon::Normal, QIcon::On);
    QVERIFY2(near(on.pixelColor(8, 32), tokens.accentWash, 6) && on.pixelColor(8, 32).alpha() > 240, id);
    QVERIFY2(near(on.pixelColor(32, 8), tokens.accentWash, 6), id);
    QCOMPARE(on.pixelColor(0, 0).alpha(), 0);
    QVERIFY2(countPixels(on, 200, [&](const QColor& c) { return near(c, tokens.accent, 24); }) >= 30, id);
    // No checkmark bubble: the lower right corner is still wash.
    QVERIFY2(near(on.pixelColor(51, 51), tokens.accentWash, 6), id);
  }
}

void TestIconsOutline::disabledIsNeutralGrey() {
  for (const char* id : ribbonIds) {
    for (QIcon::State state : {QIcon::Off, QIcon::On}) {
      const QImage image = render(QString::fromLatin1(id), 64, QIcon::Disabled, state);
      QVERIFY2(covered(image, 32) >= 20, id);
      QCOMPARE(countPixels(image, 32, [](const QColor& c) { return !isGrey(c); }), 0);
    }
  }
}

void TestIconsOutline::fallbackIdsAreMonochrome() {
  const QColor ink = KaTheme::iconPalette().ink;
  for (const char* id : fallbackIds) {
    const QString name = QString::fromLatin1(id);
    QVERIFY2(!KaIconsOutline::hasOutlineGlyph(name) && KaIcons::hasIcon(name), id);
    const QImage image = render(name);
    QVERIFY2(covered(image, 64) >= 20, id);
    QCOMPARE(countPixels(image, 64, [&](const QColor& c) { return !near(c, ink, 24); }), 0);
    QCOMPARE(image.pixelColor(0, 0).alpha(), 0);
    const QImage disabled = render(name, 64, QIcon::Disabled, QIcon::Off);
    QCOMPARE(countPixels(disabled, 32, [](const QColor& c) { return !isGrey(c); }), 0);
  }
}

void TestIconsOutline::saveUnsavedHasAccentTileAndWarnDot() {
  const auto& tokens = KaTheme::tokens();
  const QImage unsaved = KaIcons::strongIcon(QStringLiteral("save_unsaved")).pixmap(QSize(64, 64), 1.0).toImage();
  QVERIFY(near(unsaved.pixelColor(8, 32), tokens.accent, 8) && unsaved.pixelColor(8, 32).alpha() > 240);
  QVERIFY(near(unsaved.pixelColor(50, 14), tokens.warnSurface, 12));
  int warnRing = 0;
  for (int y = 7; y <= 21; ++y)
    for (int x = 43; x <= 57; ++x)
      if (near(unsaved.pixelColor(x, y), tokens.warn, 40)) ++warnRing;
  QVERIFY2(warnRing >= 8, qPrintable(QStringLiteral("warn ring pixels %1").arg(warnRing)));
  QVERIFY(countPixels(unsaved, 200, [](const QColor& c) { return near(c, Qt::white, 12); }) >= 40);
  QCOMPARE(unsaved.pixelColor(0, 0).alpha(), 0);
  // Plain save keeps no tile in this style, strong or not.
  const QImage save = render(QStringLiteral("save"));
  QCOMPARE(save.pixelColor(8, 32).alpha(), 0);
  QVERIFY(differingShare(save, KaIcons::strongIcon(QStringLiteral("save")).pixmap(QSize(64, 64), 1.0).toImage()) < 0.01);
  QVERIFY(differingShare(unsaved, render(QStringLiteral("save_unsaved"))) < 0.01);
}

void TestIconsOutline::envVarSelectsTile() {
  qputenv("KA_HGIS_ICON_STYLE", "tile");
  QCOMPARE(KaIcons::glyphStyleFromEnvironment(), KaIcons::GlyphStyle::Tile);
  qputenv("KA_HGIS_ICON_STYLE", "outline");
  QCOMPARE(KaIcons::glyphStyleFromEnvironment(), KaIcons::GlyphStyle::Outline);
  qunsetenv("KA_HGIS_ICON_STYLE");
  QCOMPARE(KaIcons::glyphStyleFromEnvironment(), KaIcons::GlyphStyle::Mockup);
  KaIcons::setGlyphStyle(KaIcons::GlyphStyle::Tile);
  const QImage tile = render(QStringLiteral("new"));
  // The tile style borrows an outline-only glyph instead of showing a ? box.
  const QImage home = render(QStringLiteral("home"));
  KaIcons::setGlyphStyle(KaIcons::GlyphStyle::Outline);
  const QImage outline = render(QStringLiteral("new"));
  QVERIFY(tile.pixelColor(8, 32).alpha() > 240);
  QCOMPARE(outline.pixelColor(8, 32).alpha(), 0);
  QVERIFY(countPixels(home, 200, [](const QColor& c) { return c.lightness() < 120; }) >= 40);
}

void TestIconsOutline::outlineIdsAreDistinct() {
  const QList<QStringList> groups = {
      {QStringLiteral("new"), QStringLiteral("open"), QStringLiteral("save"), QStringLiteral("save_as")},
      {QStringLiteral("contour"), QStringLiteral("survey_contour"), QStringLiteral("dem")},
      {QStringLiteral("soil"), QStringLiteral("geology"), QStringLiteral("paleo")},
      {QStringLiteral("pdf"), QStringLiteral("print")},
      {QStringLiteral("undo"), QStringLiteral("redo")},
      {QStringLiteral("check"), QStringLiteral("warn"), QStringLiteral("missing")},
      {QStringLiteral("map"), QStringLiteral("old_map")},
      {QStringLiteral("open"), QStringLiteral("folder")},
  };
  for (const QStringList& group : groups)
    for (int i = 0; i < group.size(); ++i)
      for (int j = i + 1; j < group.size(); ++j)
        QVERIFY2(differingShare(render(group[i]), render(group[j])) > 0.05,
                 qPrintable(group[i] + QStringLiteral(" vs ") + group[j]));
}

void TestIconsOutline::glyphPixmapSmallSizes() {
  const QColor ink = KaTheme::tokens().warn;
  for (int px : {14, 16, 20}) {
    for (const char* id : {"check", "warn", "missing", "search", "folder", "crs"}) {
      const QPixmap pm = KaIcons::glyphPixmap(QString::fromLatin1(id), ink, px);
      QCOMPARE(pm.size(), QSize(px, px));
      const QImage image = pm.toImage().convertToFormat(QImage::Format_ARGB32);
      QVERIFY2(countPixels(image, 128, [&](const QColor& c) { return near(c, ink, 24); }) >= 6,
               qPrintable(QStringLiteral("%1 at %2 px").arg(QString::fromLatin1(id)).arg(px)));
    }
  }
  const QPixmap hi = KaIcons::glyphPixmap(QStringLiteral("check"), ink, 14, 2.0);
  QCOMPARE(hi.size(), QSize(28, 28));
  QCOMPARE(hi.devicePixelRatio(), 2.0);
  // The chip path renders the same glyph through icon(id, ink) at 14 px without a tile.
  const QImage chip = KaIcons::icon(QStringLiteral("warn"), ink).pixmap(QSize(14, 14), 1.0).toImage();
  QVERIFY(countPixels(chip.convertToFormat(QImage::Format_ARGB32), 128,
                      [&](const QColor& c) { return near(c, ink, 24); }) >= 6);
}

// KA_HGIS_QA_OUTPUT_DIR: one sheet of every ribbon glyph at 32 px (normal / checked / disabled)
// and every UI glyph at 20 and 14 px, for the eye check against the mock-up.
void TestIconsOutline::contactSheet() {
  const QString output = qEnvironmentVariable("KA_HGIS_QA_OUTPUT_DIR");
  if (output.isEmpty() || !QDir(output).exists()) QSKIP("KA_HGIS_QA_OUTPUT_DIR not set");
  const int cell = 44;
  const int columns = 14;
  // Five blocks of two rows, each followed by a blank row: 14 rows.
  QImage sheet(cell * columns + 16, cell * 14 + 16, QImage::Format_ARGB32);
  sheet.fill(Qt::white);
  QPainter p(&sheet);
  int row = 0;
  auto place = [&](const QStringList& ids, int px, QIcon::Mode mode, QIcon::State state) {
    int column = 0;
    for (const QString& id : ids) {
      const QRect box(8 + column * cell, 8 + row * cell, cell, cell);
      p.drawPixmap(box.center().x() - px / 2, box.center().y() - px / 2,
                   KaIcons::icon(id).pixmap(QSize(px, px), 1.0, mode, state));
      if (++column == columns) { column = 0; ++row; }
    }
    ++row;
  };
  const QStringList ribbon = names(ribbonIds, 27) + QStringList{QStringLiteral("save_unsaved")};
  place(ribbon, 32, QIcon::Normal, QIcon::Off);
  place(ribbon, 32, QIcon::Normal, QIcon::On);
  place(ribbon, 32, QIcon::Disabled, QIcon::Off);
  place(names(uiIds, 22), 20, QIcon::Normal, QIcon::Off);
  place(names(uiIds, 22), 14, QIcon::Normal, QIcon::Off);
  p.end();
  const QString path = QDir(output).filePath(QStringLiteral("icon-outline-sheet.png"));
  QVERIFY2(sheet.save(path), qPrintable(path));
  qInfo().noquote() << "Automatic glyph render; not a portable field screenshot:" << path;
}

QTEST_MAIN(TestIconsOutline)
#include "test_icons_outline.moc"
