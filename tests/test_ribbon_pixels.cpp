// Rendered ribbon chips at every size the ribbon can take (tile 56 down to 20, labels shown or hidden):
// the tile is drawn whole, and while the labels show the label hangs about 6 px under the tile and is not
// cut off by the chip's bottom or sides. Pixels come from real chips under the application sheet.
#include <QtTest>
#include <QAction>
#include <QApplication>
#include <QDir>
#include <QFontDatabase>
#include <QImage>
#include <QMenu>
#include <QToolBar>
#include <QToolButton>
#include "app/KaBeginnerRibbon.h"
#include "app/KaIcons.h"
#include "app/KaTheme.h"

namespace {

bool closeColor(const QColor& a, const QColor& b) {
  return qAbs(a.red() - b.red()) <= 8 && qAbs(a.green() - b.green()) <= 8 && qAbs(a.blue() - b.blue()) <= 8;
}

// The twenty-chip ribbon of the size checks: six groups with the mockup icons, built the way the app
// does it. The list keeps the order below; each chip's object name is its icon id.
QList<QToolButton*> addIntendedChips(KaBeginnerRibbon* ribbon) {
  ribbon->addGroup(QStringLiteral("survey"), QStringLiteral("조사파일"));
  ribbon->addGroup(QStringLiteral("record"), QStringLiteral("기록"));
  ribbon->addGroup(QStringLiteral("basemap"), QStringLiteral("배경 지도"));
  ribbon->addGroup(QStringLiteral("align"), QStringLiteral("좌표 정합"));
  ribbon->addGroup(QStringLiteral("out"), QStringLiteral("내보내기"));
  ribbon->addGroup(QStringLiteral("find"), QStringLiteral("찾기"));
  struct ButtonSpec { const char* group; const char* icon; const char* text; bool custom; };
  const ButtonSpec specs[] = {
      {"survey", "new", "신규", false}, {"survey", "open", "열기", false},
      {"survey", "save", "저장", false}, {"survey", "save_as", "다른 이름", false},
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
      button = ribbon->addAction(QString::fromLatin1(spec.group),
                                 new QAction(icon, QString::fromUtf8(spec.text), ribbon));
    }
    button->setObjectName(QString::fromLatin1(spec.icon));
    buttons.append(button);
  }
  return buttons;
}

}  // namespace

class TestRibbonPixels : public QObject {
  Q_OBJECT
private slots:
  void initTestCase();
  void chips_stayWholeAtEverySize();
};

void TestRibbonPixels::initTestCase() {
#ifdef Q_OS_WIN
  const QDir windows(qEnvironmentVariable("WINDIR", QStringLiteral("C:/Windows")));
  for (const QString& file : {QStringLiteral("malgun.ttf"), QStringLiteral("malgunbd.ttf")})
    QFontDatabase::addApplicationFont(windows.filePath(QStringLiteral("Fonts/") + file));
#endif
  KaTheme::apply(qApp);
}

void TestRibbonPixels::chips_stayWholeAtEverySize() {
  QToolBar toolbar;
  toolbar.setAttribute(Qt::WA_DontShowOnScreen);
  toolbar.setObjectName(QStringLiteral("mainToolbar"));
  auto* ribbon = new KaBeginnerRibbon(&toolbar);
  const QList<QToolButton*> buttons = addIntendedChips(ribbon);
  toolbar.addWidget(ribbon);
  toolbar.show();
  QCoreApplication::processEvents();
  const QList<int> widths = ribbon->lookWidths();  // after the first show: the group frames are polished
  const QColor tileFill(0xE4, 0xEA, 0xED);
  QSet<int> tilesSeen;
  const QString output = qEnvironmentVariable("KA_HGIS_QA_OUTPUT_DIR");  // one picture per size, to look at
  for (int i = 0; i < widths.size(); ++i) {
    ribbon->setFixedWidth(widths.at(i));  // exactly the width this size needs
    toolbar.resize(widths.at(i) + 40, 200);
    QTest::qWait(20);  // the groups take their new places over a few layout rounds
    const RibbonLook look = ribbon->look();
    tilesSeen.insert(look.tile * (look.labels ? 1 : -1));
    if (!output.isEmpty() && QDir(output).exists())
      toolbar.grab().save(QDir(output).filePath(QStringLiteral("ribbon-size-%1-%2.png").arg(i, 2, 10, QLatin1Char('0')).arg(look.tile)));
    for (QToolButton* chip : {buttons.at(0), buttons.at(3), buttons.at(17)}) {  // 신규, 다른 이름, 도면
      const QString where = QStringLiteral("%1 at tile %2, labels %3").arg(chip->text()).arg(look.tile).arg(look.labels);
      const QImage image = chip->grab().toImage();
      const qreal dpr = image.devicePixelRatio();
      const auto boundsOf = [&](int fromY, int toY, auto&& match) {
        QRect box;
        for (int y = fromY; y < qMin(toY, image.height()); ++y)
          for (int x = 0; x < image.width(); ++x)
            if (match(image.pixelColor(x, y))) box = box.united(QRect(x, y, 1, 1));
        return box;
      };
      // The tile sits in the top tile + 8 px of the chip (4 px of edge and icon box above it); the label
      // (anti-aliased text has pale pixels the tile colour would match) starts below that.
      const int below = qRound((look.tile + 8) * dpr);
      const QRect tile = boundsOf(0, below, [&](const QColor& c) { return closeColor(c, tileFill); });
      QVERIFY2(!tile.isNull(), qPrintable(where + QStringLiteral(": no tile")));
      QVERIFY2(qAbs(tile.width() / dpr - look.tile) <= 2 && qAbs(tile.height() / dpr - look.tile) <= 2,
               qPrintable(where + QStringLiteral(": tile drawn %1 x %2").arg(tile.width() / dpr).arg(tile.height() / dpr)));
      QVERIFY2(tile.left() >= dpr && tile.top() >= dpr && tile.right() <= image.width() - 2 * dpr, qPrintable(where));
      const QRect label =
          boundsOf(below, image.height(), [](const QColor& c) { return c.alpha() >= 200 && c.lightness() < 200; });
      if (!look.labels) {
        QVERIFY2(label.isNull(), qPrintable(where + QStringLiteral(": ink under the tile")));
        continue;
      }
      QVERIFY2(!label.isNull(), qPrintable(where + QStringLiteral(": no label")));
      const qreal gap = (label.top() - tile.bottom() - 1) / dpr;
      QVERIFY2(gap >= 5 && gap <= 7, qPrintable(where + QStringLiteral(": label ink starts %1 px under the tile").arg(gap)));
      QVERIFY2(label.bottom() <= image.height() - 2 * dpr && label.left() >= dpr && label.right() <= image.width() - 2 * dpr,
               qPrintable(where + QStringLiteral(": label cut off, ink %1,%2 to %3,%4 in %5 x %6")
                              .arg(label.left()).arg(label.top()).arg(label.right()).arg(label.bottom())
                              .arg(image.width()).arg(image.height())));
    }
  }
  QVERIFY2(tilesSeen.size() >= 14, qPrintable(QStringLiteral("only %1 different sizes were drawn").arg(tilesSeen.size())));
}

QTEST_MAIN(TestRibbonPixels)
#include "test_ribbon_pixels.moc"
