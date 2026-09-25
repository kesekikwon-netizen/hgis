#include <QtTest>
#include <cmath>
#include <limits>
#include <QAction>
#include <QApplication>
#include <QColor>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFontDatabase>
#include <QFontMetrics>
#include <QFrame>
#include <QGridLayout>
#include <QImage>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QMenu>
#include <QMainWindow>
#include <QPushButton>
#include <QRegularExpression>
#include <QSet>
#include <QSignalSpy>
#include <QStatusBar>
#include <QTabWidget>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>
#include "app/KaAppBar.h"
#include "app/KaBeginnerRibbon.h"
#include "app/KaIcons.h"
#include "app/KaTheme.h"

class TestTheme : public QObject {
  Q_OBJECT
private slots:
  void initTestCase();
  void ribbonButtons_renderAtIntendedSize();
  void ribbonOverflow_preservesControlsAndKeyboard();
  void ribbonOverflow_keepsAlignAtFieldWidth();
  void ribbon_tabEnterNewSurveyToSave();
  void domainIcons_useDistinctColors();
  void iconStates_preserveMeaningAndDisableColor();
  void explicitIconTint_remainsMonochrome();
  void flatIcons_keepTransparentCornersAndHighDpi();
  void appIcon_usesNavyTileWithGoldTrowel();
  void chromeSurfaces_renderFlatAndReadableText();
  void strataPalette_matchesSpec();
  void regionChip_remainsCompactAndTextOnly();
  void tokensMatchSpec();
  void embeddedNotEmpty();
  void diskMatchesEmbedded();
  void noUrl();
  void versionAndLaunchScripts_exist();
  void gisExcludePresent();
  void requiredSelectorsPresent();
  void toolbarCheckedHasDistinctTreatment();
  void primaryToolbarIconsRemainReadable();
  void noCheapSpinArrowBlock();
  void noCatchAllWidgetRules();
  void chromeFontIsFieldKorean();
  void beginnerChrome_questionLabels();
  void appBar_routesLotSearches();
};

void TestTheme::initTestCase() {
#ifdef Q_OS_WIN
  if (QGuiApplication::platformName() == QLatin1String("offscreen")) {
    // The offscreen platform does not discover Windows system fonts. Use the
    // same installed Korean face as the field app for label and pixel checks.
    const QDir windows(qEnvironmentVariable("WINDIR"));
    for (const QString& file : {QStringLiteral("malgun.ttf"), QStringLiteral("malgunbd.ttf")}) {
      const QString path = windows.filePath(QStringLiteral("Fonts/") + file);
      QVERIFY2(QFontDatabase::addApplicationFont(path) >= 0, qPrintable(path));
    }
  }
#endif
  KaTheme::apply(qApp);
  const QRegularExpression token(QStringLiteral("@[A-Za-z][A-Za-z0-9_]*@"));
  QVERIFY2(!token.match(qApp->styleSheet()).hasMatch(), "applied QSS must resolve metric tokens");
  QVERIFY2(!token.match(KaTheme::resolvedStyleSheet(KaTheme::embeddedStyleSheet())).hasMatch(),
           "embedded fallback must resolve the same metric tokens");
}

namespace {
QRect changedPixels(const QImage& first, const QImage& second) {
  if (first.size() != second.size())
    return {};
  QRect bounds;
  for (int y = 0; y < first.height(); ++y) {
    for (int x = 0; x < first.width(); ++x) {
      const QColor a = first.pixelColor(x, y);
      const QColor b = second.pixelColor(x, y);
      if (qAbs(a.red() - b.red()) > 16 || qAbs(a.green() - b.green()) > 16 ||
          qAbs(a.blue() - b.blue()) > 16)
        bounds = bounds.united(QRect(x, y, 1, 1));
    }
  }
  return bounds;
}

template <typename Predicate>
int opaquePixelsMatching(const QImage& image, Predicate matches) {
  int count = 0;
  for (int y = 0; y < image.height(); ++y) {
    for (int x = 0; x < image.width(); ++x) {
      const QColor color = image.pixelColor(x, y);
      if (color.alpha() >= 200 && matches(color))
        ++count;
    }
  }
  return count;
}

bool closeColor(const QColor& a, const QColor& b) {
  return qAbs(a.red() - b.red()) <= 8 && qAbs(a.green() - b.green()) <= 8 &&
         qAbs(a.blue() - b.blue()) <= 8;
}

double relativeLuminance(const QColor& color) {
  const auto linear = [](double value) {
    return value <= 0.04045 ? value / 12.92 : std::pow((value + 0.055) / 1.055, 2.4);
  };
  return 0.2126 * linear(color.redF()) + 0.7152 * linear(color.greenF()) +
         0.0722 * linear(color.blueF());
}

double contrastRatio(const QColor& first, const QColor& second) {
  const double a = relativeLuminance(first);
  const double b = relativeLuminance(second);
  return (qMax(a, b) + 0.05) / (qMin(a, b) + 0.05);
}

QColor logicalPixel(const QImage& image, int x, int y) {
  return image.pixelColor(qRound(x * image.devicePixelRatio()), qRound(y * image.devicePixelRatio()));
}

double worstVerticalContrast(const QImage& image, int x, int fromY, int toY, const QColor& ink) {
  double worst = std::numeric_limits<double>::max();
  for (int y = fromY; y <= toY; ++y)
    worst = qMin(worst, contrastRatio(ink, logicalPixel(image, x, y)));
  return worst;
}

const char* const themedIconIds[] = {
    "new", "open", "save", "save_as", "select", "measure", "draw_poly", "trench_grid",
    "contour", "dem", "soil", "paleo", "geology", "river", "georef", "buffer",
    "check", "pdf", "export", "more",
};
}

void TestTheme::ribbon_tabEnterNewSurveyToSave() {
  KaBeginnerRibbon ribbon;
  ribbon.addGroup(QStringLiteral("survey"), QStringLiteral("조사파일"));
  ribbon.addGroup(QStringLiteral("record"), QStringLiteral("기록"));
  ribbon.addGroup(QStringLiteral("out"), QStringLiteral("내보내기"));
  struct Cmd {
    const char* group;
    const char* text;
    QKeySequence key;
  };
  const Cmd cmds[] = {
      {"survey", "신규", QKeySequence::New},
      {"survey", "열기", QKeySequence::Open},
      {"survey", "저장", QKeySequence::Save},
      {"survey", "다른이름", QKeySequence::SaveAs},
      {"record", "그리기", QKeySequence(QStringLiteral("Ctrl+D"))},
      {"record", "선택", QKeySequence(QStringLiteral("Ctrl+1"))},
      {"out", "도면", QKeySequence(QStringLiteral("Ctrl+L"))},
      {"out", "5179", QKeySequence(QStringLiteral("Ctrl+E"))},
  };
  QList<QAction*> actions;
  QList<QToolButton*> buttons;
  for (const auto& cmd : cmds) {
    auto* action = new QAction(QString::fromUtf8(cmd.text), &ribbon);
    action->setShortcut(cmd.key);
    QVERIFY2(!action->shortcut().isEmpty(), cmd.text);
    auto* button = ribbon.addAction(QString::fromLatin1(cmd.group), action);
    QVERIFY(button);
    actions.append(action);
    buttons.append(button);
  }
  ribbon.applyTabOrder();
  ribbon.resize(1100, ribbon.sizeHint().height());
  ribbon.show();
  QVERIFY(QTest::qWaitForWindowExposed(&ribbon));
  QCOMPARE(ribbon.tabButtons().size(), 8);
  QCOMPARE(ribbon.tabButtons().at(0), buttons.at(0));
  QCOMPARE(ribbon.tabButtons().at(2), buttons.at(2));
  QSignalSpy newSpy(actions.at(0), &QAction::triggered);
  QSignalSpy saveSpy(actions.at(2), &QAction::triggered);
  buttons.at(0)->setFocus(Qt::TabFocusReason);
  QCOMPARE(QApplication::focusWidget(), static_cast<QWidget*>(buttons.at(0)));
  QTest::keyClick(buttons.at(0), Qt::Key_Enter);
  QTRY_COMPARE(newSpy.count(), 1);
  QTest::keyClick(QApplication::focusWidget(), Qt::Key_Tab);
  QTest::keyClick(QApplication::focusWidget(), Qt::Key_Tab);
  QCOMPARE(QApplication::focusWidget(), static_cast<QWidget*>(buttons.at(2)));
  QTest::keyClick(QApplication::focusWidget(), Qt::Key_Enter);
  QTRY_COMPARE(saveSpy.count(), 1);
}

void TestTheme::ribbonOverflow_preservesControlsAndKeyboard() {
  KaBeginnerRibbon ribbon;
  ribbon.addGroup(QStringLiteral("survey"), QStringLiteral("조사파일"));
  ribbon.addGroup(QStringLiteral("record"), QStringLiteral("기록"));
  auto* survey = new QAction(QStringLiteral("열기"), &ribbon);
  auto* open = ribbon.addAction(QStringLiteral("survey"), survey);
  auto* draw = new QToolButton(&ribbon);
  draw->setText(QStringLiteral("그리기"));
  draw->setCheckable(true);
  ribbon.addWidget(QStringLiteral("record"), draw);
  QPointer<QToolButton> retainedDraw(draw);
  QSignalSpy activated(draw, &QToolButton::clicked);
  ribbon.resize(700, ribbon.sizeHint().height());
  ribbon.show();
  QCoreApplication::processEvents();
  auto* overflow = ribbon.findChild<QToolButton*>(QStringLiteral("ribbonOverflow"));
  QVERIFY(overflow);
  QVERIFY(!overflow->isVisible());
  ribbon.resize(80, ribbon.height());
  QTRY_VERIFY(overflow->isVisible());
  QVERIFY(overflow->width() >= overflow->fontMetrics().horizontalAdvance(overflow->text()));
  auto* menu = ribbon.findChild<QMenu*>(QStringLiteral("ribbonOverflowMenu"));
  auto* record = ribbon.findChild<QMenu*>(QStringLiteral("ribbonOverflowGroup_record"));
  QVERIFY(menu && record && record->menuAction()->isVisible());
  menu->popup(overflow->mapToGlobal(QPoint(0, overflow->height())));
  menu->setActiveAction(record->menuAction());
  QTest::keyClick(menu, Qt::Key_Right);
  QTRY_VERIFY(record->isVisible());
  QTRY_VERIFY(draw->isVisible());
  draw->setFocus();
  QTest::keyClick(draw, Qt::Key_Space);
  QCOMPARE(activated.count(), 1);
  QVERIFY(draw->isChecked());
  menu->close();
  for (int i = 0; i < 3; ++i) {
    ribbon.resize(700, ribbon.height());
    QTRY_VERIFY(!overflow->isVisible());
    QVERIFY(retainedDraw && retainedDraw == draw);
    QCOMPARE(open->defaultAction(), survey);
    QVERIFY(draw->isVisible());
    QCOMPARE(draw->font().pixelSize(), 12);
    ribbon.resize(80, ribbon.height());
    QTRY_VERIFY(overflow->isVisible());
  }
}

void TestTheme::ribbonOverflow_keepsAlignAtFieldWidth() {
  KaBeginnerRibbon ribbon;
  ribbon.setAttribute(Qt::WA_DontShowOnScreen);
  ribbon.addGroup(QStringLiteral("survey"), QStringLiteral("조사파일"));
  ribbon.addGroup(QStringLiteral("record"), QStringLiteral("기록"));
  ribbon.addGroup(QStringLiteral("basemap"), QStringLiteral("배경 지도"));
  ribbon.addGroup(QStringLiteral("align"), QStringLiteral("좌표 정합"));
  ribbon.addGroup(QStringLiteral("out"), QStringLiteral("내보내기"));
  ribbon.addGroup(QStringLiteral("find"), QStringLiteral("찾기"));
  const struct { const char* group; const char* text; } chips[] = {
      {"survey", "신규"}, {"survey", "열기"}, {"survey", "저장"}, {"survey", "다른이름"},
      {"record", "선택"}, {"record", "측거"}, {"record", "그리기"}, {"record", "시굴격자"},
      {"basemap", "지형"}, {"basemap", "수치"}, {"basemap", "DEM"}, {"basemap", "토양"},
      {"basemap", "고지형"}, {"basemap", "지적"}, {"basemap", "대동여지"}, {"basemap", "1919지형"},
      {"basemap", "지질"}, {"basemap", "수계"},
      {"align", "정합"}, {"align", "버퍼"}, {"align", "유산"},
      {"out", "도면"}, {"out", "단면"}, {"out", "GeoTIFF"}, {"out", "5179"},
      {"find", "웹"}, {"find", "더보기"},
  };
  for (const auto& chip : chips) {
    auto* button = new QToolButton(&ribbon);
    button->setText(QString::fromUtf8(chip.text));
    if (QString::fromLatin1(chip.group) == QLatin1String("align"))
      button->setObjectName(QStringLiteral("fieldAlign_") + QString::fromUtf8(chip.text));
    ribbon.addWidget(QString::fromLatin1(chip.group), button);
  }
  auto* locator = new QWidget(&ribbon);
  locator->setObjectName(QStringLiteral("fieldRegionLocator"));
  locator->setMinimumWidth(230);
  locator->setFixedHeight(48);
  ribbon.addWidget(QStringLiteral("find"), locator);
  ribbon.resize(1366, ribbon.sizeHint().height());
  ribbon.show();
  QCoreApplication::processEvents();
  auto* overflow = ribbon.findChild<QToolButton*>(QStringLiteral("ribbonOverflow"));
  QVERIFY(overflow);
  auto* align = ribbon.group(QStringLiteral("align"));
  QVERIFY(align);
  QVERIFY2(align->parentWidget() == &ribbon,
           "좌표 정합 must stay on the ribbon at 1366, not inside 더 많은 작업");
  auto* georef = ribbon.findChild<QToolButton*>(QStringLiteral("fieldAlign_정합"));
  QVERIFY(georef);
  QVERIFY(georef->isVisible());
  QCOMPARE(KaTheme::buttonMetrics().ribbonChipGap, 0);
  QCOMPARE(KaTheme::buttonMetrics().ribbonChipWidth, 56);
  QCOMPARE(KaTheme::buttonMetrics().ribbonMinWidth, 56);
  QCOMPARE(KaTheme::buttonMetrics().ribbonFontSize, 12);
  QVERIFY(georef->width() >= KaTheme::buttonMetrics().ribbonChipWidth);
  QVERIFY(georef->height() >= KaTheme::buttonMetrics().ribbonHeight);
  ribbon.resize(2200, ribbon.height());
  QTRY_VERIFY(ribbon.group(QStringLiteral("find"))->parentWidget() == &ribbon);
  QVERIFY(georef->width() >= KaTheme::buttonMetrics().ribbonChipWidth);
  auto* find = ribbon.group(QStringLiteral("find"));
  QVERIFY(find);
  const int packedRight = find->mapTo(&ribbon, QPoint(find->width(), 0)).x();
  QVERIFY2(ribbon.width() - packedRight >= 80,
           "leftover window width must stay empty on the right, not on chips");
}

void TestTheme::ribbonButtons_renderAtIntendedSize() {
  // Reproduce the real nesting: toolbar -> ribbon -> group frame -> button.
  // This is deliberately not QToolBar::addAction, which propagates iconSize.
  QToolBar toolbar;
  toolbar.setAttribute(Qt::WA_DontShowOnScreen);
  toolbar.setObjectName(QStringLiteral("mainToolbar"));
  toolbar.setIconSize(QSize(20, 20));
  auto* ribbon = new KaBeginnerRibbon(&toolbar);
  ribbon->addGroup(QStringLiteral("survey"), QStringLiteral("조사파일"));
  ribbon->addGroup(QStringLiteral("record"), QStringLiteral("기록"));
  ribbon->addGroup(QStringLiteral("basemap"), QStringLiteral("배경 지도"));
  ribbon->addGroup(QStringLiteral("align"), QStringLiteral("좌표 정합"));
  ribbon->addGroup(QStringLiteral("out"), QStringLiteral("내보내기"));
  ribbon->addGroup(QStringLiteral("find"), QStringLiteral("찾기"));
  struct ButtonSpec { const char* group; const char* icon; const char* text; bool custom; };
  const ButtonSpec specs[] = {
      {"survey", "new", "신규", false}, {"survey", "open", "열기", false},
      {"survey", "save", "저장", false}, {"survey", "save_as", "다른이름", false},
      {"record", "select", "선택", false},
      {"record", "measure", "측거", false}, {"record", "draw_poly", "그리기", true},
      {"record", "trench_grid", "시굴격자", false},
      {"basemap", "contour", "지형", true}, {"basemap", "dem", "DEM", true},
      {"basemap", "soil", "토양", true}, {"basemap", "paleo", "고지형", true},
      {"basemap", "geology", "지질", false}, {"basemap", "river", "수계", false},
      {"align", "georef", "정합", false}, {"align", "buffer", "버퍼", true},
      {"out", "check", "검수", false}, {"out", "pdf", "도면", false},
      {"out", "export", "내보내기", false},
      {"find", "more", "더보기", true},
  };
  QList<QToolButton*> buttons;
  for (const auto& spec : specs) {
    QToolButton* button = nullptr;
    const QIcon icon = KaIcons::icon(QString::fromLatin1(spec.icon));
    if (spec.custom) {
      button = new QToolButton(ribbon);
      button->setText(QString::fromUtf8(spec.text));
      button->setIcon(icon);
      if (QString::fromLatin1(spec.icon) == QLatin1String("more")) {
        auto* menu = new QMenu(button);
        menu->addAction(QStringLiteral("작업 목록"));
        button->setMenu(menu);
        button->setPopupMode(QToolButton::InstantPopup);
      }
      ribbon->addWidget(QString::fromLatin1(spec.group), button);
    } else {
      auto* action = new QAction(icon, QString::fromUtf8(spec.text), ribbon);
      button = ribbon->addAction(QString::fromLatin1(spec.group), action);
    }
    QVERIFY(button);
    button->setObjectName(QString::fromLatin1(spec.icon));
    buttons.append(button);
  }
  toolbar.addWidget(ribbon);
  toolbar.resize(qMax(1800, toolbar.sizeHint().width()), toolbar.sizeHint().height());
  toolbar.show();
  QCoreApplication::processEvents();

  const int commonWidth = buttons.front()->width();
  const int commonHeight = buttons.front()->height();
  QVERIFY(commonWidth >= KaTheme::buttonMetrics().ribbonChipWidth);
  QVERIFY(commonHeight >= KaTheme::buttonMetrics().ribbonHeight);
  for (int i = 0; i < buttons.size(); ++i) {
    QToolButton* button = buttons.at(i);
    QVERIFY(qobject_cast<QFrame*>(button->parentWidget()));
    QCOMPARE(button->iconSize(), QSize(32, 32));
    QCOMPARE(button->font().pixelSize(), 12);
    QCOMPARE(button->width(), commonWidth);
    QCOMPARE(button->height(), commonHeight);
    QCOMPARE(button->toolButtonStyle(), Qt::ToolButtonTextUnderIcon);
    if (i > 0 && buttons.at(i - 1)->parentWidget() == button->parentWidget()) {
      const int gap = button->x() - (buttons.at(i - 1)->x() + buttons.at(i - 1)->width());
      QCOMPARE(gap, KaTheme::buttonMetrics().ribbonChipGap);
    }
    QVERIFY2(toolbar.rect().contains(QRect(button->mapTo(&toolbar, QPoint()), button->size())),
             "all fixture buttons must fit without toolbar overflow");
    const QFontMetrics fm(button->font());
    const QSize label = fm.size(Qt::TextShowMnemonic, button->text());
    QVERIFY2(button->height() >= 32 + 4 + label.height() + 6,
             qPrintable(button->text() + QStringLiteral(": icon and label must fit")));
    QVERIFY(button->width() >= label.width());
    QVERIFY2(!button->text().contains(QLatin1Char('\n')),
             qPrintable(button->text() + QStringLiteral(": ribbon caption must stay one line")));

    // Compare actual widget pixels against the same widget with a transparent
    // icon. A non-null transparent icon keeps the layout and label unchanged.
    const QImage rendered = button->grab().toImage();
    const QIcon original = button->icon();
    QPixmap transparent(64, 64);
    transparent.fill(Qt::transparent);
    button->setIcon(QIcon(transparent));
    const QImage withoutIcon = button->grab().toImage();
    button->setIcon(original);
    QCOMPARE(rendered.size(), withoutIcon.size());
    const QRect ink = changedPixels(rendered, withoutIcon);
    const qreal dpr = rendered.devicePixelRatio();
    QVERIFY2(qMax(ink.width(), ink.height()) / dpr >= 18.0,
             qPrintable(button->text() + QStringLiteral(": actual icon ink must exceed the old 16px box")));
    QVERIFY2(ink.width() / dpr <= 34.0 && ink.height() / dpr <= 34.0,
             "pixel difference must be confined to the icon, not label/layout movement");
    qInfo().noquote() << button->text().replace(QLatin1Char('\n'), QLatin1Char('/'))
                     << "button" << button->size() << "icon ink (physical px)" << ink.size()
                     << "DPR" << dpr;
  }
  QVERIFY2(!KaBeginnerRibbon::twoLine(QStringLiteral("조사 열기")).contains(QLatin1Char('\n')),
           "twoLine keeps spaced Korean on one line");
  QVERIFY2(KaBeginnerRibbon::twoLine(QStringLiteral("1919 조선지형도\n1:5만")) ==
               QStringLiteral("1919 조선지형도 1:5만"),
           "twoLine flattens explicit newlines");
  // A toolbar size change must not shrink nested ribbon icons again.
  toolbar.setIconSize(QSize(24, 24));
  QCoreApplication::processEvents();
  for (QToolButton* button : buttons) {
    QCOMPARE(button->iconSize(), QSize(32, 32));
    QCOMPARE(button->height(), commonHeight);
  }
  const QString output = qEnvironmentVariable("KA_HGIS_QA_OUTPUT_DIR");
  if (!output.isEmpty() && QDir(output).exists()) {
    const QString path = QDir(output).filePath(QStringLiteral("theme-ribbon-widget-render.png"));
    QVERIFY2(toolbar.grab().save(path), qPrintable(path));
    qInfo().noquote() << "Automatic Qt widget render; not a portable field screenshot:" << path;
  }
}

void TestTheme::domainIcons_useDistinctColors() {
  const auto& palette = KaTheme::iconPalette();
  const struct { const char* id; QColor color; } groups[] = {
      {"new", palette.file}, {"draw_poly", palette.record}, {"dem", palette.map},
      {"measure", palette.record}, {"tape", palette.record},
      {"georef", palette.align}, {"pdf", palette.output}, {"section", palette.output},
  };
  for (const auto& group : groups) {
    const QImage image = KaIcons::icon(QString::fromLatin1(group.id)).pixmap(64, 64).toImage();
    QVERIFY2(opaquePixelsMatching(image, [&](const QColor& c) {
      const int distance = qAbs(c.hsvHue() - group.color.hsvHue());
      return c.hsvSaturation() >= 30 && qMin(distance, 360 - distance) <= 15;
    }) >= 20,
             group.id);
  }
  const QImage water = KaIcons::icon(QStringLiteral("river")).pixmap(64, 64).toImage();
  QVERIFY2(opaquePixelsMatching(water, [](const QColor& c) {
    return c.blue() > c.red() + 25 && c.blue() > c.green() + 10;
  }) >= 20, "river must visibly contain blue water, not only neutral outlines");
  const QImage soil = KaIcons::icon(QStringLiteral("soil")).pixmap(64, 64).toImage();
  QVERIFY2(opaquePixelsMatching(soil, [](const QColor& c) {
    return c.red() > c.green() + 15 && c.green() > c.blue() + 10;
  }) >= 20, "soil must visibly contain earth brown");
  QVERIFY2(opaquePixelsMatching(soil, [](const QColor& c) {
    return qMax(c.red(), qMax(c.green(), c.blue())) < 100;
  }) >= 20, "soil needs a dark outline as well as the earth fill");
}

void TestTheme::flatIcons_keepTransparentCornersAndHighDpi() {
  for (const char* id : {"new", "map", "layer", "gps", "georef", "pdf"}) {
    const auto icon = KaIcons::icon(QString::fromLatin1(id));
    const auto normal = icon.pixmap(QSize(64, 64), 1.0).toImage();
    QCOMPARE(normal.pixelColor(0, 0).alpha(), 0);
    QCOMPARE(normal.pixelColor(63, 63).alpha(), 0);
    QVERIFY(normal.pixelColor(8, 18).alpha() > 240);
    QVERIFY(normal.pixelColor(8, 46).alpha() > 240);
    // Flat tile: no reflection band, the fill is the same from top to bottom.
    QVERIFY(qAbs(normal.pixelColor(8, 18).lightness() - normal.pixelColor(8, 46).lightness()) <= 3);
    const auto hi = icon.pixmap(QSize(40, 40), 2.0);
    QCOMPARE(hi.size(), QSize(80, 80));
    QCOMPARE(hi.devicePixelRatio(), 2.0);
  }
}

void TestTheme::appIcon_usesNavyTileWithGoldTrowel() {
  QFile bundled(QStringLiteral(":/ka-hgis/app-icon.png"));
  QFile source(QStringLiteral("data/theme/ka-hgis-app.png"));
  QVERIFY(bundled.open(QIODevice::ReadOnly));
  QVERIFY(source.open(QIODevice::ReadOnly));
  QCOMPARE(QCryptographicHash::hash(bundled.readAll(), QCryptographicHash::Sha256),
           QCryptographicHash::hash(source.readAll(), QCryptographicHash::Sha256));
  const QImage master(QStringLiteral(":/ka-hgis/app-icon.png"));
  QVERIFY(!master.isNull());
  QVERIFY(master.hasAlphaChannel());
  QVERIFY(master.width() >= 256 && master.height() >= 256);
  const auto icon = KaIcons::appIcon();
  QVERIFY(!icon.isNull());
  for (int size : {16, 32, 48, 64, 128, 256}) {
    const auto image = icon.pixmap(QSize(size, size), 1.0).toImage();
    QCOMPARE(image.size(), QSize(size, size));
    QCOMPARE(image.pixelColor(0, 0).alpha(), 0);
    QCOMPARE(image.pixelColor(size - 1, size - 1).alpha(), 0);
    QVERIFY(image.pixelColor(size / 2, size / 2).alpha() > 240);
    // Strata icon: a navy tile with a gold trowel on a contour mound.
    QVERIFY(opaquePixelsMatching(image, [](const QColor& c) {
      return c.hsvHue() >= 20 && c.hsvHue() <= 55 && c.hsvSaturation() > 60;
    }) > size * size / 25);
    QVERIFY(opaquePixelsMatching(image, [](const QColor& c) {
      return c.hsvHue() >= 180 && c.hsvHue() <= 230 && c.hsvSaturation() > 60;
    }) > size * size / 2);
  }
  const QString output = qEnvironmentVariable("KA_HGIS_QA_OUTPUT_DIR");
  if (!output.isEmpty() && QDir(output).exists())
    QVERIFY(icon.pixmap(256, 256).save(QDir(output).filePath(QStringLiteral("app-icon-preview.png"))));
}

void TestTheme::iconStates_preserveMeaningAndDisableColor() {
  QWidget grid;
  grid.setAttribute(Qt::WA_DontShowOnScreen);
  auto* layout = new QGridLayout(&grid);
  const struct { const char* label; QIcon::Mode mode; QIcon::State state; } states[] = {
      {"Normal", QIcon::Normal, QIcon::Off}, {"Checked", QIcon::Normal, QIcon::On},
      {"Active", QIcon::Active, QIcon::Off}, {"Selected", QIcon::Selected, QIcon::Off},
      {"Disabled", QIcon::Disabled, QIcon::Off},
  };
  for (int column = 0; column < 5; ++column)
    layout->addWidget(new QLabel(QString::fromLatin1(states[column].label), &grid), 0, column + 1);
  int row = 1;
  for (const char* id : themedIconIds) {
    const QIcon icon = KaIcons::icon(QString::fromLatin1(id));
    const QImage normal = icon.pixmap(64, 64, QIcon::Normal, QIcon::Off).toImage();
    const QImage checked = icon.pixmap(64, 64, QIcon::Normal, QIcon::On).toImage();
    const QImage disabled = icon.pixmap(64, 64, QIcon::Disabled, QIcon::Off).toImage();
    QVERIFY2(!normal.isNull() && !checked.isNull() && !disabled.isNull(), id);
    QVERIFY2(!changedPixels(normal, checked).isEmpty(), id);
    int sharedColoredPixels = 0;
    int huePreservedPixels = 0;
    int brightnessChanges = 0;
    for (int y = 0; y < qMin(normal.height(), checked.height()); ++y) {
      for (int x = 0; x < qMin(normal.width(), checked.width()); ++x) {
        const QColor a = normal.pixelColor(x, y);
        const QColor b = checked.pixelColor(x, y);
        if (a.alpha() < 200 || b.alpha() < 200 || a.hsvSaturation() < 60 || b.hsvSaturation() < 60)
          continue;
        ++sharedColoredPixels;
        const int hueDistance = qAbs(a.hsvHue() - b.hsvHue());
        if (qMin(hueDistance, 360 - hueDistance) <= 15)
          ++huePreservedPixels;
        if (qAbs(a.value() - b.value()) >= 3)
          ++brightnessChanges;
      }
    }
    // Neutral utility icons may have no saturated group fill. Their checked
    // marker is covered by the pixel difference check above.
    const int normalColored = opaquePixelsMatching(normal, [](const QColor& c) {
      return c.hsvSaturation() >= 60;
    });
    if (normalColored >= 20) {
      QVERIFY2(sharedColoredPixels * 2 >= normalColored, id);
      QVERIFY2(huePreservedPixels * 100 >= sharedColoredPixels * 70, id);
      QVERIFY2(brightnessChanges >= 8, id);
    }
    int visibleDisabled = 0;
    for (int y = 0; y < disabled.height(); ++y) {
      for (int x = 0; x < disabled.width(); ++x) {
        const QColor c = disabled.pixelColor(x, y);
        if (c.alpha() < 32)
          continue;
        ++visibleDisabled;
        QVERIFY2(qAbs(c.red() - c.green()) <= 2 && qAbs(c.green() - c.blue()) <= 2, id);
      }
    }
    QVERIFY2(visibleDisabled >= 20, id);
    layout->addWidget(new QLabel(QString::fromLatin1(id), &grid), row, 0);
    for (int column = 0; column < 5; ++column) {
      auto* preview = new QLabel(&grid);
      preview->setFixedSize(72, 72);
      preview->setAlignment(Qt::AlignCenter);
      preview->setPixmap(icon.pixmap(64, 64, states[column].mode, states[column].state));
      layout->addWidget(preview, row, column + 1);
    }
    ++row;
  }
  grid.resize(grid.sizeHint());
  const QString output = qEnvironmentVariable("KA_HGIS_QA_OUTPUT_DIR");
  if (!output.isEmpty() && QDir(output).exists()) {
    const QString path = QDir(output).filePath(QStringLiteral("theme-icon-state-widget-render.png"));
    QVERIFY2(grid.grab().save(path), qPrintable(path));
    qInfo().noquote() << "Automatic Qt widget render; not a portable field screenshot:" << path;
  }
}

void TestTheme::explicitIconTint_remainsMonochrome() {
  for (const QColor& ink : {QColor(157, 38, 181), KaTheme::iconPalette().ink}) {
    for (const char* id : {"river", "soil", "georef", "pdf"}) {
      const QIcon icon = KaIcons::icon(QString::fromLatin1(id), ink);
      for (QIcon::Mode mode : {QIcon::Normal, QIcon::Active, QIcon::Selected, QIcon::Disabled}) {
        for (QIcon::State state : {QIcon::Off, QIcon::On}) {
          const QImage image = icon.pixmap(64, 64, mode, state).toImage();
          int visible = 0;
          int maxAlpha = 0;
          for (int y = 0; y < image.height(); ++y) {
            for (int x = 0; x < image.width(); ++x) {
              const QColor c = image.pixelColor(x, y);
              maxAlpha = qMax(maxAlpha, c.alpha());
              if (c.alpha() < 64)
                continue;
              ++visible;
              QVERIFY2(closeColor(c, ink), id);
            }
          }
          QVERIFY2(visible >= 20, qPrintable(QStringLiteral("%1 mode=%2 state=%3 visible=%4 maxAlpha=%5")
                                                .arg(QString::fromLatin1(id)).arg(int(mode)).arg(int(state))
                                                .arg(visible).arg(maxAlpha)));
        }
      }
    }
  }
}

void TestTheme::chromeSurfaces_renderFlatAndReadableText() {
  const auto& tokens = KaTheme::tokens();
  const QColor backgrounds[] = {
      tokens.surface, tokens.glossMiddle, tokens.glossBottom, tokens.hoverTop, tokens.hoverBottom,
      tokens.pressedTop, tokens.pressedBottom, tokens.selectedTop, tokens.selectedBottom, tokens.desk,
      tokens.glossReflection, tokens.glossShoulder,
  };
  for (const QColor& background : backgrounds) {
    QVERIFY2(contrastRatio(tokens.ink, background) >= 4.5, "body text against every surface");
    QVERIFY2(contrastRatio(tokens.inkMuted, background) >= 4.5, "secondary text against every surface");
  }
  QVERIFY(contrastRatio(tokens.inkDisabled, tokens.disabledSurface) >= 4.5);
  for (const QColor& background : {tokens.accentReflection, tokens.sky1, tokens.sky2})
    QVERIFY(contrastRatio(tokens.surface, background) >= 4.5);

  QMainWindow window;
  window.setAttribute(Qt::WA_DontShowOnScreen);
  auto* toolbar = new QToolBar(&window);
  toolbar->setObjectName(QStringLiteral("mainToolbar"));
  auto* ribbon = new KaBeginnerRibbon(toolbar);
  ribbon->addGroup(QStringLiteral("survey"), QStringLiteral("조사파일"));
  ribbon->addGroup(QStringLiteral("record"), QStringLiteral("기록"));
  ribbon->addAction(QStringLiteral("survey"), new QAction(KaIcons::icon(QStringLiteral("new")),
                                                           QStringLiteral("새 조사"), ribbon));
  ribbon->addAction(QStringLiteral("survey"), new QAction(KaIcons::icon(QStringLiteral("open")),
                                                           QStringLiteral("조사 열기"), ribbon));
  ribbon->addAction(QStringLiteral("record"), new QAction(KaIcons::icon(QStringLiteral("draw_poly")),
                                                           QStringLiteral("그리기"), ribbon));
  toolbar->addWidget(ribbon);
  window.addToolBar(toolbar);
  auto* central = new QWidget(&window);
  auto* columns = new QHBoxLayout(central);
  auto* panel = new QFrame(central);
  panel->setObjectName(QStringLiteral("layersCard"));
  panel->setFixedWidth(210);
  auto* panelRows = new QVBoxLayout(panel);
  panelRows->setContentsMargins(16, 16, 16, 16);
  auto* caption = new QLabel(QStringLiteral("조사 데이터"), panel);
  caption->setObjectName(QStringLiteral("cardCaption"));
  auto* body = new QLabel(QStringLiteral("조사구역\n유구 기록\n기준점"), panel);
  panelRows->addWidget(caption);
  panelRows->addWidget(body);
  panelRows->addStretch();
  columns->addWidget(panel);
  auto* tabs = new QTabWidget(central);
  tabs->setObjectName(QStringLiteral("viewTabs"));
  auto* page = new QWidget(tabs);
  auto* pageRows = new QVBoxLayout(page);
  auto* map = new QFrame(page);
  map->setObjectName(QStringLiteral("mapCard"));
  auto* mapRows = new QVBoxLayout(map);
  mapRows->addWidget(new QLabel(QStringLiteral("지도 작업 영역 · EPSG:5187"), map));
  mapRows->addStretch();
  pageRows->addWidget(map, 1);
  auto* stateRows = new QGridLayout;
  const QStringList names = {QStringLiteral("기본"), QStringLiteral("선택"),
                             QStringLiteral("누름"), QStringLiteral("비활성")};
  QList<QPushButton*> states;
  for (int column = 0; column < names.size(); ++column) {
    // Empty text makes the sampled center column pure background in every state.
    auto* button = new QPushButton(page);
    button->setFixedSize(124, 60);
    if (column == 1) {
      button->setCheckable(true);
      button->setChecked(true);
    } else if (column == 2) {
      button->setDown(true);
    } else if (column == 3) {
      button->setEnabled(false);
    }
    states.append(button);
    stateRows->addWidget(button, 0, column);
    auto* label = new QLabel(names[column], page);
    label->setAlignment(Qt::AlignCenter);
    stateRows->addWidget(label, 1, column);
  }
  pageRows->addLayout(stateRows);
  tabs->addTab(page, QStringLiteral("지도"));
  tabs->addTab(new QWidget(tabs), QStringLiteral("검수"));
  columns->addWidget(tabs, 1);
  window.setCentralWidget(central);
  auto* status = new QStatusBar(&window);
  status->setObjectName(QStringLiteral("kaStatusBar"));
  auto* statusText = new QLabel(QStringLiteral("저장 완료 · 다음: 도면 만들기"), status);
  status->addWidget(statusText);
  window.setStatusBar(status);
  window.resize(1000, 540);
  window.show();
  QCoreApplication::processEvents();

  // Flat faces: every state is one colour from top to bottom.
  const QColor face[] = {tokens.surface, tokens.selectedTop, tokens.pressedTop, tokens.disabledSurface};
  for (int index = 0; index < states.size(); ++index) {
    QPushButton* button = states[index];
    const QImage rendered = button->grab().toImage();
    const QColor top = logicalPixel(rendered, button->width() / 2, 8);
    const QColor bottom = logicalPixel(rendered, button->width() / 2, button->height() - 9);
    QVERIFY2(closeColor(top, bottom), qPrintable(names[index] + QStringLiteral(": flat face ") + top.name() + QLatin1Char('/') + bottom.name()));
    QVERIFY2(closeColor(top, face[index]), qPrintable(names[index] + QStringLiteral(": ") + top.name()));
    const QPalette::ColorGroup group = button->isEnabled() ? QPalette::Active : QPalette::Disabled;
    const QColor foreground = button->palette().color(group, QPalette::ButtonText);
    const double ratio = worstVerticalContrast(rendered, button->width() / 2, 8,
                                               button->height() - 9, foreground);
    QVERIFY2(ratio >= 4.5, qPrintable(names[index] + QStringLiteral(": contrast %1").arg(ratio)));
    qInfo().noquote() << "button-state" << index << "top" << top.name() << "bottom" << bottom.name()
                     << "worst rendered contrast" << ratio;
  }
  const QImage panelImage = panel->grab().toImage();
  for (QLabel* text : {caption, body}) {
    const QColor foreground = text->palette().color(text->foregroundRole());
    QVERIFY(worstVerticalContrast(panelImage, panel->width() - 10, 16,
                                  panel->height() - 17, foreground) >= 4.5);
  }
  const QImage statusImage = status->grab().toImage();
  QVERIFY(worstVerticalContrast(statusImage, status->width() - 60, 6, status->height() - 7,
                                statusText->palette().color(statusText->foregroundRole())) >= 4.5);
  const QString output = qEnvironmentVariable("KA_HGIS_QA_OUTPUT_DIR");
  if (!output.isEmpty() && QDir(output).exists()) {
    const QString path = QDir(output).filePath(QStringLiteral("theme-chrome-states-widget-render.png"));
    QVERIFY2(window.grab().save(path), qPrintable(path));
    qInfo().noquote() << "Automatic Qt widget render; not a portable field screenshot:" << path;
  }
}

void TestTheme::strataPalette_matchesSpec() {
  // Strata chrome (2026-09-23): navy frame, one blue accent, flat surfaces.
  const auto& tokens = KaTheme::tokens();
  QCOMPARE(tokens.sky1, QColor(0x1F, 0x6F, 0xB2));
  QCOMPARE(tokens.sky3, QColor(0x12, 0x55, 0x8D));
  QCOMPARE(tokens.rail, QColor(0x0B, 0x3A, 0x63));
  QCOMPARE(tokens.desk, QColor(0xF4, 0xF6, 0xF8));
  QCOMPARE(tokens.ink, QColor(0x1D, 0x27, 0x33));
  QCOMPARE(tokens.inkMuted, QColor(0x5B, 0x68, 0x75));
  QCOMPARE(tokens.inkDisabled, QColor(0x59, 0x68, 0x74));
  QCOMPARE(tokens.border, QColor(0xDC, 0xE3, 0xEA));
  // The gloss stops remain as flat aliases so older selectors paint plainly.
  QCOMPARE(tokens.glossReflection, tokens.surface);
  QCOMPARE(tokens.glossShoulder, tokens.surface);
  QCOMPARE(tokens.glossBottom, tokens.surface);
  QCOMPARE(tokens.hoverTop, tokens.hoverBottom);
  QCOMPARE(tokens.pressedTop, tokens.pressedBottom);
  QCOMPARE(tokens.selectedTop, tokens.selectedBottom);
  QVERIFY(contrastRatio(tokens.railText, tokens.rail) >= 4.5);
  QVERIFY(contrastRatio(tokens.railMuted, tokens.rail) >= 4.5);
  QVERIFY(contrastRatio(tokens.railMuted, tokens.sky3) >= 4.5);
  // Icon fills keep their softened pre-2026-09-10 intensity.
  const auto& icons = KaTheme::iconPalette();
  const struct { QColor before; QColor after; } fills[] = {
      {QColor(0x32, 0x6B, 0x9B), icons.file}, {QColor(0x95, 0x60, 0x29), icons.record},
      {QColor(0x39, 0x73, 0x68), icons.map}, {QColor(0x6B, 0x59, 0x96), icons.align},
      {QColor(0x24, 0x78, 0x6C), icons.output}, {QColor(0x1D, 0x6E, 0xB8), icons.water},
      {QColor(0x93, 0x60, 0x39), icons.earth},
  };
  for (const auto& fill : fills) {
    QVERIFY(qAbs(fill.after.hslSaturationF() / fill.before.hslSaturationF() - 0.8) < 0.02);
    QVERIFY(qAbs(fill.after.hslHueF() - fill.before.hslHueF()) < 0.01);
    QVERIFY(qAbs(fill.after.lightnessF() - fill.before.lightnessF()) < 0.005);
    QCOMPARE(fill.after.alpha(), fill.before.alpha());
  }
  QCOMPARE(icons.ink, QColor(0x23, 0x29, 0x30));
}

void TestTheme::regionChip_remainsCompactAndTextOnly() {
  QWidget locator;
  locator.setAttribute(Qt::WA_DontShowOnScreen);
  locator.setObjectName(QStringLiteral("regionLocator"));
  auto* chip = new QToolButton(&locator);
  chip->setObjectName(QStringLiteral("regionChip"));
  chip->setText(QStringLiteral("경북"));
  chip->setToolButtonStyle(Qt::ToolButtonTextOnly);
  auto* menu = new QMenu(chip);
  QAction* action = menu->addAction(QStringLiteral("안동시"));
  chip->setMenu(menu);
  chip->setPopupMode(QToolButton::InstantPopup);
  chip->ensurePolished();
  chip->resize(chip->sizeHint());
  QCOMPARE(chip->font().pixelSize(), 11);
  QCOMPARE(chip->toolButtonStyle(), Qt::ToolButtonTextOnly);
  QVERIFY(chip->icon().isNull());
  QVERIFY(action->icon().isNull());
  QVERIFY(chip->height() <= 30);
  QVERIFY(chip->width() <= 48);
  QVERIFY(!chip->grab().isNull());
}

void TestTheme::tokensMatchSpec() {
  const auto& tokens = KaTheme::tokens();
  QCOMPARE(tokens.canvasNeutral, QColor(Qt::white));
  QVERIFY(tokens.ink.red() < 60 && tokens.ink.green() < 60 && tokens.ink.blue() < 60);
  QVERIFY(tokens.sky1.green() > tokens.sky1.red() + 20 && tokens.sky1.blue() > tokens.sky1.red() + 20);
  QCOMPARE(tokens.sky1, tokens.sky5);
  QVERIFY(tokens.desk.blue() >= tokens.desk.red());
  QVERIFY(tokens.desk.blue() - tokens.desk.red() <= 16);
  // Flat chrome still shows hover and selection as their own tints.
  QVERIFY(tokens.hoverBottom != tokens.surface);
  QVERIFY(tokens.selectedTop != tokens.surface);
  QVERIFY(tokens.pressedTop != tokens.selectedTop);
}

static QString normalize(QString s) { return s.replace(QLatin1String("\r\n"), QLatin1String("\n")); }

void TestTheme::embeddedNotEmpty() {
  QVERIFY2(!KaTheme::embeddedStyleSheet().trimmed().isEmpty(), "embedded QSS must compile in");
}

void TestTheme::diskMatchesEmbedded() {
  QFile f(QStringLiteral("data/theme/ka-hgis.qss"));
  QVERIFY2(f.open(QIODevice::ReadOnly | QIODevice::Text),
           "run theme_qss with WORKING_DIRECTORY = source tree");
  const QString disk = normalize(QString::fromUtf8(f.readAll()));
  const QString emb = normalize(KaTheme::embeddedStyleSheet());
  QCOMPARE(emb, disk);
}

void TestTheme::noUrl() {
  QVERIFY2(!KaTheme::embeddedStyleSheet().contains(QLatin1String("url(")),
           "QSS must not use url()");
}

void TestTheme::versionAndLaunchScripts_exist() {
  QFile ver(QStringLiteral("VERSION"));
  QVERIFY2(ver.exists() && ver.open(QIODevice::ReadOnly | QIODevice::Text),
           "CTest WORKING_DIRECTORY must be the repo root");
  const QString text = QString::fromUtf8(ver.readAll()).trimmed();
  QVERIFY2(QRegularExpression(QStringLiteral(R"(^\d+\.\d+\.\d+$)")).match(text).hasMatch(),
           qPrintable(text));
  QVERIFY2(QFile::exists(QStringLiteral("scripts/start-ka-hgis.vbs")), "start-ka-hgis.vbs");
  QVERIFY2(QFile::exists(QStringLiteral("scripts/run-ka-hgis.ps1")), "run-ka-hgis.ps1");
  QVERIFY2(QFile::exists(QStringLiteral("VERSION_QGIS_PIN.txt")), "VERSION_QGIS_PIN.txt");
  QVERIFY2(QFile::exists(QStringLiteral("THIRD_PARTY_NOTICES.md")), "THIRD_PARTY_NOTICES.md");
}

void TestTheme::gisExcludePresent() {
  const QString qss = KaTheme::embeddedStyleSheet();
  QVERIFY(qss.contains(QLatin1String("QgsMapCanvas")));
  QVERIFY(qss.contains(QLatin1String("QWidget#mapCanvas")));
  QVERIFY(qss.contains(QLatin1String("QgsLayoutView")));
  QVERIFY(qss.contains(QLatin1String("QWidget#layoutView")));
}

void TestTheme::requiredSelectorsPresent() {
  const QString qss = KaTheme::embeddedStyleSheet();
  const char* need[] = {
      "QToolButton",
      "QToolBar#subToolbar",
      "QDockWidget",
      "QSplitter::handle",
      "QgsLayerTreeView",
      "QAbstractSpinBox::up-button",
      "QCheckBox",
      "QToolTip",
      "QPushButton#btnAdjustDone",
      "QToolButton.sampleTile",
      "QLabel#emptyState",
      "QToolBar#mainToolbar::separator",
      "QTableWidget",
      "QDialogButtonBox",
      "QWidget#startPage",
      "QWidget#beginnerRibbon",
      "QFrame#ribbonGroup",
      "QLabel#ribbonGroupCaption",
      "QWidget#regionLocator QToolButton#regionChip",
      "QFrame#layerOpacityRail",
      "QSplitter#studioLeftSplit",
      "QSplitter#studioMainSplit",
  };
  for (const char* sel : need) {
    QVERIFY2(qss.contains(QLatin1String(sel)), sel);
  }
  for (const QString& type : {QStringLiteral("QPushButton"), QStringLiteral("QToolButton.sampleTile")}) {
    const QRegularExpression checked(QRegularExpression::escape(type) +
                                    QStringLiteral(R"((?::enabled)?:checked(?::enabled)?(?=\s*[,\{]))"));
    QVERIFY2(checked.match(qss).hasMatch(), qPrintable(type + QStringLiteral(":checked rule")));
  }
}

void TestTheme::toolbarCheckedHasDistinctTreatment() {
  QToolBar toolbar;
  toolbar.setAttribute(Qt::WA_DontShowOnScreen);
  toolbar.setObjectName(QStringLiteral("mainToolbar"));
  auto* ribbon = new KaBeginnerRibbon(&toolbar);
  ribbon->addGroup(QStringLiteral("record"), QStringLiteral("기록"));
  auto* normalAction = new QAction(KaIcons::icon(QStringLiteral("draw_poly")), QStringLiteral("그리기"), ribbon);
  auto* checkedAction = new QAction(normalAction->icon(), normalAction->text(), ribbon);
  checkedAction->setCheckable(true);
  checkedAction->setChecked(true);
  QToolButton* normal = ribbon->addAction(QStringLiteral("record"), normalAction);
  QToolButton* checked = ribbon->addAction(QStringLiteral("record"), checkedAction);
  toolbar.addWidget(ribbon);
  toolbar.resize(400, toolbar.sizeHint().height());
  toolbar.show();
  QCoreApplication::processEvents();
  const QImage off = normal->grab().toImage();
  const QImage on = checked->grab().toImage();
  // Compare empty face pixels, not the separately tested icon/check marker.
  const int faceX = qMax(4, normal->width() / 2);
  const int faceY = qBound(normal->iconSize().height() + 4, normal->height() - 8,
                           normal->height() - 4);
  const QColor normalFace = logicalPixel(off, faceX, faceY);
  const QColor checkedFace = logicalPixel(on, faceX, faceY);
  QVERIFY2(normalFace != checkedFace, "checked ribbon needs a visible face treatment");
  const QColor foreground = checked->palette().color(QPalette::ButtonText);
  QVERIFY(contrastRatio(foreground, checkedFace) >= 4.5);
}

void TestTheme::primaryToolbarIconsRemainReadable() {
  QFile f(QStringLiteral("src/app/MainWindow.cpp"));
  QVERIFY2(f.open(QIODevice::ReadOnly | QIODevice::Text), "MainWindow.cpp");
  const QString src = QString::fromUtf8(f.readAll());
  QVERIFY2(!src.contains(QLatin1String("KaIcons::icon(iconId, QColor(255, 255, 255))")),
           "outlined 새조사/열기/저장 chips must not use white icons");
  QVERIFY2(!src.contains(QLatin1String("b->setIcon(KaIcons::icon(iconId, QColor(")),
           "primary ribbon buttons must not flatten group-colored icons to a hardcoded tint");
}

void TestTheme::noCheapSpinArrowBlock() {
  const QString qss = KaTheme::embeddedStyleSheet();
  QVERIFY2(!qss.contains(QLatin1String("QAbstractSpinBox::up-arrow")),
           "spin arrows must be drawn by ChromeStyle, not a QSS black square");
}

void TestTheme::noCatchAllWidgetRules() {
  const QString qss = KaTheme::embeddedStyleSheet();
  const QRegularExpression banned(
      QStringLiteral(R"((^|[\n{;])\s*(QWidget|QGraphicsView|QFrame)\s*\{)"));
  QVERIFY2(!banned.match(qss).hasMatch(),
           "no ID-less QWidget / QGraphicsView / QFrame rules");
}

void TestTheme::chromeFontIsFieldKorean() {
  const QString qss = KaTheme::embeddedStyleSheet();
  const QRegularExpression face(
      QStringLiteral(R"(QMainWindow,[\s\S]*?font-family:\s*"Malgun Gothic")"));
  QVERIFY2(face.match(qss).hasMatch(),
           "window face must lead with Malgun Gothic (field Korean UI)");
  QVERIFY2(!qss.contains(QLatin1String("font-family: \"Pretendard GOV\"")),
           "do not lead chrome with an unshipped Pretendard family");
  QVERIFY2(!qss.contains(QLatin1String("QToolBar#studioToolRail QToolButton")) ||
               !QRegularExpression(QStringLiteral(
                    R"(QToolBar#studioToolRail QToolButton[^\{]*\{[^\}]*font-size:\s*10px)"))
                    .match(qss)
                    .hasMatch(),
           "studio tool rail must not use 10px type");
}

void TestTheme::beginnerChrome_questionLabels() {
  QFile mw(QStringLiteral("src/app/MainWindow.cpp"));
  QVERIFY2(mw.open(QIODevice::ReadOnly | QIODevice::Text), "MainWindow.cpp");
  const QString main = QString::fromUtf8(mw.readAll());
  QVERIFY2(main.contains(QLatin1String("beginnerRibbon")) ||
               main.contains(QLatin1String("KaBeginnerRibbon")),
           "메인에 초보자 리본");
  QVERIFY2(main.contains(QString::fromUtf8("조사파일")), "조사파일 그룹");
  QVERIFY2(main.contains(QString::fromUtf8("기록")), "기록 그룹");
  QVERIFY2(main.contains(QString::fromUtf8("배경 지도")), "배경 그룹");
  QVERIFY2(main.contains(QString::fromUtf8("좌표 정합")) ||
               main.contains(QString::fromUtf8("정합·분석")),
           "정합 그룹");
  QVERIFY2(main.contains(QString::fromUtf8("산출")) ||
               main.contains(QString::fromUtf8("내보내기")),
           "내보내기 그룹");
  QVERIFY2(main.contains(QString::fromUtf8("찾기")), "찾기 그룹");
  QVERIFY2(main.contains(QString::fromUtf8("새 조사")), "새 조사");
  QVERIFY2(main.contains(QString::fromUtf8("도면 만들기")), "도면 만들기");
  QVERIFY2(!main.contains(QLatin1String("addDockWidget(Qt::RightDockWidgetArea, checkDock)")),
           "도면 검수 칸을 지도에 붙이지 않음");
  QVERIFY2(main.contains(QString::fromUtf8("5179")), "제출은 5179");
  QVERIFY2(main.contains(QString::fromUtf8("그리기")), "그리기");
  QFile fb(QStringLiteral("src/app/KaFileBrowserPanel.cpp"));
  const QString panelCode = fb.open(QIODevice::ReadOnly | QIODevice::Text) ? QString::fromUtf8(fb.readAll()) : QString();
  QVERIFY2(main.contains(QString::fromUtf8("파일을 지도에 끌어 넣으면 레이어가 됩니다.")) ||
               panelCode.contains(QString::fromUtf8("파일을 지도에 끌어 넣으면 레이어가 됩니다.")),
           "파일함 끌어넣기 안내");
  QVERIFY2(panelCode.contains(QLatin1String("fitShortcutFonts")),
           "파일함 좁으면 글자 크기 축소");
  QVERIFY2(panelCode.contains(QLatin1String("horizontalAdvance")),
           "파일함 글자 폭은 QFontMetrics");
  QVERIFY2(!main.contains(QLatin1String("addIcon(QStringLiteral(\"out\"), QStringLiteral(\"terrain_3d\")")),
           "입체지형 리본 삭제");

  QFile rb(QStringLiteral("src/app/KaBeginnerRibbon.cpp"));
  QVERIFY2(rb.open(QIODevice::ReadOnly | QIODevice::Text), "KaBeginnerRibbon.cpp");
  const QString ribbon = QString::fromUtf8(rb.readAll());
  QVERIFY2(ribbon.contains(QLatin1String("ribbonGroupCaption")), "그룹 제목 라벨");
  QVERIFY2(ribbon.contains(QLatin1String("twoLine")), "리본 글자 한 줄 정규화");
  QVERIFY2(ribbon.contains(QLatin1String("replace(QLatin1Char('\\n')")),
           "리본은 줄바꿈을 한 칸으로 붙인다");

  QFile ds(QStringLiteral("src/app/KaDrawingStudio.cpp"));
  QVERIFY2(ds.open(QIODevice::ReadOnly | QIODevice::Text), "KaDrawingStudio.cpp");
  const QString studio = QString::fromUtf8(ds.readAll());
  QVERIFY2(studio.contains(QString::fromUtf8("PDF 내보내기")), "조판 PDF");
  QVERIFY2(studio.contains(QString::fromUtf8("조판 항목")), "조판 오른쪽 안내");
  QVERIFY2(!studio.contains(QLatin1String("studioToolbar")), "조판 위 보기 툴바 없음 — 휠·드래그");
  QVERIFY2(!studio.contains(QString::fromUtf8("용지 전체를 볼까?")), "용지 맞춤 버튼 없음");
  QVERIFY2(!studio.contains(QString::fromUtf8("화면을 움직여볼까?")), "화면 이동 버튼 없음");
  QVERIFY2(!studio.contains(QLatin1String("addGroup(QStringLiteral(\"view\")")),
           "보기 그룹 없음");
  QVERIFY2(studio.contains(QLatin1String("savePdf")), "PDF 슬롯은 그대로");
  QVERIFY2(studio.contains(QLatin1String("&KaDrawingStudio::savePdf")),
           "조판 「PDF 내보내기」가 savePdf에 연결되어 있어야 한다");
  QVERIFY2(studio.contains(QLatin1String("KaLayerOpacityRail")),
           "도면만들기 맵 안 세로 투명도");
  QVERIFY2(studio.contains(QLatin1String("studioMainSplit")),
           "도면만들기 레이어·파일함 폭을 끌어서 조절");
  QVERIFY2(main.contains(QLatin1String("KaLayerOpacityRail")),
           "지도 창 안 세로 투명도");
  QVERIFY2(!main.contains(QLatin1String("layersLay->addWidget(m_layerOpacityWidget)")),
           "투명도는 레이어 카드가 아니라 맵 안");
  QVERIFY2(!studio.contains(QLatin1String("addStudio(QStringLiteral(\"out\")")),
           "위 리본 PDF는 범례창과 중복이라 뺌");
}

void TestTheme::appBar_routesLotSearches() {
  QVERIFY(KaAppBar::looksLikeLot(QStringLiteral("제주시 애월읍 광령리 1615")));
  QVERIFY(KaAppBar::looksLikeLot(QStringLiteral("광령리 1615-3")));
  QVERIFY(KaAppBar::looksLikeLot(QStringLiteral("광령리 산12")));
  QVERIFY(!KaAppBar::looksLikeLot(QStringLiteral("제주시 애월읍 광령리")));
  QVERIFY(!KaAppBar::looksLikeLot(QStringLiteral("1615")));
  KaAppBar bar;
  bar.setAttribute(Qt::WA_DontShowOnScreen);
  bar.resize(600, bar.sizeHint().height());
  bar.show();
  QCoreApplication::processEvents();
  QSignalSpy spy(&bar, &KaAppBar::searchRequested);
  bar.searchField()->setText(QStringLiteral("  광령리   1615 "));
  QTest::keyClick(bar.searchField(), Qt::Key_Return);
  QCOMPARE(spy.count(), 1);
  QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("광령리 1615"));
  QCOMPARE(spy.at(0).at(1).toBool(), true);
  // 「지역」 appears only once the province chips are handed over.
  auto* region = bar.findChild<QToolButton*>(QStringLiteral("appBarRegion"));
  QVERIFY(region && !region->isVisible());
  bar.setRegionWidget(new QWidget);
  QVERIFY(region->isVisible() && region->menu());
}

QTEST_MAIN(TestTheme)
#include "test_theme.moc"
