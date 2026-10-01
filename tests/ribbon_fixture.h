#pragma once
// The production ribbon as test fixture (test_ribbon_overflow): the groups and chips of
// MainWindow::buildMenus read from MainWindowRibbon.cpp, a window that shows them like the app does, and
// the checks every width has to pass (no fold button, every chip and group name shown, nothing clipped).
#include <QtTest>
#include <algorithm>
#include <QFile>
#include <QFontMetrics>
#include <QFrame>
#include <QLabel>
#include <QMainWindow>
#include <QPair>
#include <QPixmap>
#include <QRegularExpression>
#include <QToolBar>
#include <QToolButton>
#include "app/KaAppBar.h"
#include "app/KaBeginnerRibbon.h"

namespace RibbonFixture {

struct Chip {
  qsizetype at = 0;
  QString group;
  QString text;
};

struct RibbonSource {
  QList<QPair<QString, QString>> groups;  // id, caption
  QList<Chip> chips;
};

// Groups and every chip of MainWindow::buildMenus, in source order. Run from the source tree.
inline RibbonSource readProductionRibbon() {
  RibbonSource out;
  QFile file(QStringLiteral("src/app/MainWindowRibbon.cpp"));
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return out;
  const QString src = QString::fromUtf8(file.readAll());
  const qsizetype start = src.indexOf(QStringLiteral("void MainWindow::buildMenus"));
  if (start < 0) return out;
  const qsizetype end = src.indexOf(QStringLiteral("\nvoid MainWindow::"), start + 10);
  const QString body = src.mid(start, end > start ? end - start : -1);
  static const QRegularExpression group(
      QStringLiteral("ribbon->addGroup\\(QStringLiteral\\(\"(\\w+)\"\\),\\s*QStringLiteral\\(\"([^\"]+)\"\\)\\)"));
  for (auto it = group.globalMatch(body); it.hasNext();) {
    const auto m = it.next();
    out.groups.append({m.captured(1), m.captured(2)});
  }
  static const QRegularExpression icon(QStringLiteral(
      "addIcon\\(\\s*QStringLiteral\\(\"(\\w+)\"\\),\\s*QStringLiteral\\(\"\\w+\"\\),\\s*QStringLiteral\\(\"([^\"]+)\"\\)"));
  for (auto it = icon.globalMatch(body); it.hasNext();) {
    const auto m = it.next();
    out.chips.append({m.capturedStart(), m.captured(1), m.captured(2)});
  }
  static const QRegularExpression widget(
      QStringLiteral("ribbon->addWidget\\(QStringLiteral\\(\"(\\w+)\"\\),\\s*(\\w+)\\)"));
  for (auto it = widget.globalMatch(body); it.hasNext();) {
    const auto m = it.next();
    const QRegularExpression setText(QRegularExpression::escape(m.captured(2)) +
                                     QStringLiteral("->setText\\(QStringLiteral\\(\"([^\"]+)\"\\)\\)"));
    QString text;
    for (auto t = setText.globalMatch(body.left(m.capturedStart())); t.hasNext();) text = t.next().captured(1);
    out.chips.append({m.capturedStart(), m.captured(1), text.isEmpty() ? m.captured(2) : text});
  }
  std::sort(out.chips.begin(), out.chips.end(), [](const Chip& a, const Chip& b) { return a.at < b.at; });
  return out;
}

// The production groups and chips in `ribbon` (each chip has a picture; its size does not depend on which).
inline void fillRibbon(KaBeginnerRibbon* ribbon, const RibbonSource& source) {
  QPixmap picture(64, 64);
  picture.fill(Qt::gray);
  for (const auto& group : source.groups) ribbon->addGroup(group.first, group.second);
  for (const Chip& chip : source.chips) {
    auto* button = new QToolButton(ribbon);
    button->setText(chip.text);
    button->setIcon(QIcon(picture));
    button->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    ribbon->addWidget(chip.group, button);
  }
}

// A layout settles over several rounds of posted events (chip size, group, ribbon, toolbar), and a window
// resize reaches the offscreen platform window a little later than the call.
inline void settle() {
  QTest::qWait(20);
  QCoreApplication::processEvents();
}

// The production ribbon and the app bar on the main toolbar of a window of the given width, shown and
// laid out. The ribbon is owned by `window`.
inline KaBeginnerRibbon* showProductionRibbon(QMainWindow& window, const RibbonSource& source, int windowWidth) {
  window.setAttribute(Qt::WA_DontShowOnScreen);
  auto* toolbar = new QToolBar(&window);
  toolbar->setObjectName(QStringLiteral("mainToolbar"));
  toolbar->setIconSize(QSize(20, 20));
  toolbar->setMovable(false);
  auto* appBar = new KaAppBar(toolbar);
  appBar->setRegionWidget(new QWidget);
  auto* ribbon = new KaBeginnerRibbon(toolbar);
  fillRibbon(ribbon, source);
  toolbar->addWidget(ribbon);
  toolbar->addWidget(appBar);
  window.addToolBar(toolbar);
  window.setCentralWidget(new QWidget(&window));
  window.resize(windowWidth, 768);
  window.show();
  settle();
  return ribbon;
}

inline QString describe(KaBeginnerRibbon* ribbon, const RibbonSource& source) {
  const RibbonLook look = ribbon->look();
  QFrame* last = ribbon->group(source.groups.last().first);
  return QStringLiteral("ribbon %1 px: tile %2 labels %3 · right gap %4 px")
      .arg(ribbon->width()).arg(look.tile).arg(look.labels).arg(ribbon->width() - last->x() - last->width());
}

// Whatever the width: no fold button, every chip and group name shown, nothing clipped or overlapping.
// A failure leaves QTest::currentTestFailed() true; callers return on it.
inline void verifyWhole(KaBeginnerRibbon* ribbon, const RibbonSource& source) {
  const QString where = describe(ribbon, source);
  QVERIFY2(!ribbon->findChild<QToolButton*>(QStringLiteral("ribbonOverflow")), "no 「더 많은 작업」 button");
  const RibbonLook look = ribbon->look();
  const QList<QToolButton*> chips = ribbon->tabButtons();
  QCOMPARE(chips.size(), source.chips.size());
  for (int i = 0; i < chips.size(); ++i) {
    QToolButton* chip = chips.at(i);
    const QString name = chip->text();
    const QRect box(chip->mapTo(ribbon, QPoint()), chip->size());
    const int line = QFontMetrics(chip->font()).height();
    QVERIFY2(chip->isVisible() && ribbon->rect().contains(box), qPrintable(name + QStringLiteral(" is clipped, ") + where));
    QVERIFY2(chip->toolTip().contains(name), qPrintable(name + QStringLiteral(": the tooltip keeps the full name")));
    QCOMPARE(chip->iconSize(), QSize(look.tile, look.tile));
    QCOMPARE(chip->toolButtonStyle(), look.labels ? Qt::ToolButtonTextUnderIcon : Qt::ToolButtonIconOnly);
    QCOMPARE(chip->height(), look.tile + 12 + (look.labels ? line : 0));  // tile + 6 + label line + 6, or tile + 12
    QVERIFY2(chip->width() >= (look.labels ? QFontMetrics(chip->font()).horizontalAdvance(name) + 4 : look.tile + 2),
             qPrintable(name + QStringLiteral(": label or tile does not fit the chip, ") + where));
    for (int j = i + 1; j < chips.size(); ++j)
      QVERIFY2(!box.intersects(QRect(chips.at(j)->mapTo(ribbon, QPoint()), chips.at(j)->size())), qPrintable(where));
  }
  const auto captions = ribbon->findChildren<QLabel*>(QStringLiteral("ribbonGroupCaption"));
  QCOMPARE(captions.size(), source.groups.size());
  for (QLabel* caption : captions)
    QVERIFY2(caption->isVisible() && caption->width() >= caption->sizeHint().width(), qPrintable(caption->text() + where));
  for (const auto& group : source.groups) {
    QFrame* frame = ribbon->group(group.first);
    QVERIFY2(frame && frame->geometry().right() < ribbon->width() && frame->height() >= frame->sizeHint().height(),
             qPrintable(group.first + QStringLiteral(" is clipped, ") + where));
  }
}

}  // namespace RibbonFixture
