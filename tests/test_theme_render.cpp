// Rendered theme contracts from the Strata evaluation: keyboard focus rings
// (F040/F081), the selected-row accent bar (F092), flat download windows and the
// shared progress bar (F007/F095) and the flat radio indicator. Pixels are
// sampled from real widgets under the application sheet.
#include <QtTest>
#include <cmath>
#include <QAction>
#include <QApplication>
#include <QDir>
#include <QFontDatabase>
#include <QFrame>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QRadioButton>
#include <QToolBar>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>
#include "app/KaBeginnerRibbon.h"
#include "app/KaDownloadUi.h"
#include "app/KaTheme.h"

namespace {

double luminance(const QColor& c) {
  const auto lin = [](double v) { return v <= 0.04045 ? v / 12.92 : std::pow((v + 0.055) / 1.055, 2.4); };
  return 0.2126 * lin(c.redF()) + 0.7152 * lin(c.greenF()) + 0.0722 * lin(c.blueF());
}

double contrast(const QColor& a, const QColor& b) {
  const double x = luminance(a), y = luminance(b);
  return (qMax(x, y) + 0.05) / (qMin(x, y) + 0.05);
}

bool near(const QColor& a, const QColor& b, int tolerance = 10) {
  return qAbs(a.red() - b.red()) <= tolerance && qAbs(a.green() - b.green()) <= tolerance &&
         qAbs(a.blue() - b.blue()) <= tolerance;
}

QColor at(const QImage& image, int x, int y) {
  const qreal dpr = image.devicePixelRatio();
  return image.pixelColor(qRound(x * dpr), qRound(y * dpr));
}

}  // namespace

class TestThemeRender : public QObject {
  Q_OBJECT
private slots:
  void initTestCase();
  void keyboardFocusShowsAccentRing();
  void selectedRowHasAccentBar();
  void downloadWindowIsFlatAndReadable();
  void progressBarTextReadableOnChunk();
  void radioIndicatorUsesAccent();
  void paperLook_mainButtonIsSlateAndBandIsPaper();
  void paperLook_drawingRowToolsAreButtons();
};

void TestThemeRender::initTestCase() {
#ifdef Q_OS_WIN
  const QDir windows(qEnvironmentVariable("WINDIR", QStringLiteral("C:/Windows")));
  for (const QString& file : {QStringLiteral("malgun.ttf"), QStringLiteral("malgunbd.ttf")})
    QFontDatabase::addApplicationFont(windows.filePath(QStringLiteral("Fonts/") + file));
#endif
  qunsetenv("KA_HGIS_QSS_FROM_DISK");
  KaTheme::apply(qApp);
}

void TestThemeRender::keyboardFocusShowsAccentRing() {
  QWidget host;
  auto* rows = new QVBoxLayout(&host);
  auto* first = new QPushButton(QStringLiteral("취소"), &host);
  auto* second = new QPushButton(QStringLiteral("적용"), &host);
  rows->addWidget(first);
  rows->addWidget(second);
  auto* ribbon = new KaBeginnerRibbon(&host);
  ribbon->addGroup(QStringLiteral("survey"), QStringLiteral("조사"));
  auto* chip = ribbon->addAction(QStringLiteral("survey"), new QAction(QStringLiteral("열기"), ribbon));
  rows->addWidget(ribbon);
  host.resize(320, 260);
  host.show();
  QVERIFY(QTest::qWaitForWindowExposed(&host));
  host.activateWindow();
  const auto& t = KaTheme::tokens();
  second->setFocus(Qt::TabFocusReason);
  QTRY_VERIFY(second->hasFocus());
  QCoreApplication::processEvents();
  const QImage focused = second->grab().toImage();
  const QImage idle = first->grab().toImage();
  QVERIFY2(near(at(focused, 0, second->height() / 2), t.focusRing),
           qPrintable(at(focused, 0, second->height() / 2).name()));
  QVERIFY(near(at(idle, 0, first->height() / 2), t.border));
  chip->setFocus(Qt::TabFocusReason);
  QTRY_VERIFY(chip->hasFocus());
  QCoreApplication::processEvents();
  const QImage ring = chip->grab().toImage();
  for (int x : {0, 1})
    QVERIFY2(near(at(ring, x, chip->height() / 2), t.focusRing), qPrintable(at(ring, x, chip->height() / 2).name()));
  QVERIFY(chip->width() >= KaTheme::buttonMetrics().ribbonChipWidth);  // the ring never resizes the chip
}

void TestThemeRender::selectedRowHasAccentBar() {
  QTreeWidget tree;
  tree.setRootIsDecorated(false);
  tree.setHeaderHidden(true);
  auto* picked = new QTreeWidgetItem(&tree, {QStringLiteral("조사구역")});
  auto* other = new QTreeWidgetItem(&tree, {QStringLiteral("유구")});
  tree.resize(240, 120);
  tree.show();
  QVERIFY(QTest::qWaitForWindowExposed(&tree));
  tree.setCurrentItem(picked);
  QCoreApplication::processEvents();
  const QImage view = tree.viewport()->grab().toImage();
  const QRect row = tree.visualItemRect(picked);
  const QRect plain = tree.visualItemRect(other);
  const auto& t = KaTheme::tokens();
  QVERIFY2(near(at(view, row.left(), row.center().y()), t.accent), "selected row: accent bar at its left edge");
  QVERIFY(near(at(view, row.center().x(), row.center().y()), t.selected));
  QVERIFY(!near(at(view, plain.left(), plain.center().y()), t.accent));
}

void TestThemeRender::downloadWindowIsFlatAndReadable() {
  KaDownloadProgressDialog dialog(QStringLiteral("지적도 다운로드"), QStringLiteral("조사 주변 5 km의 지적도 자료를 받습니다."));
  dialog.show();
  QVERIFY(QTest::qWaitForWindowExposed(&dialog));
  QVERIFY(dialog.styleSheet().isEmpty());  // no inline palette island any more
  auto* header = dialog.findChild<QFrame*>(QStringLiteral("downloadHeader"));
  auto* title = dialog.findChild<QLabel*>(QStringLiteral("downloadTitle"));
  auto* subtitle = dialog.findChild<QLabel*>(QStringLiteral("downloadSubtitle"));
  QVERIFY(header && title && subtitle);
  const QImage face = header->grab().toImage();
  const QColor top = at(face, header->width() / 2, 3);
  const QColor bottom = at(face, header->width() / 2, header->height() - 4);
  QVERIFY2(near(top, bottom, 3), "flat header, no gradient");
  QVERIFY(near(top, KaTheme::tokens().accentDeep));
  QVERIFY(contrast(title->palette().color(title->foregroundRole()), top) >= 4.5);
  QVERIFY(contrast(subtitle->palette().color(subtitle->foregroundRole()), top) >= 4.5);
  QCOMPARE(title->font().weight(), QFont::Bold);
}

void TestThemeRender::progressBarTextReadableOnChunk() {
  QProgressBar bar;
  bar.setRange(0, 100);
  bar.setValue(80);
  bar.setTextVisible(true);
  bar.resize(240, bar.sizeHint().height());
  bar.show();
  QVERIFY(QTest::qWaitForWindowExposed(&bar));
  const QImage image = bar.grab().toImage();
  const QColor chunk = at(image, 20, bar.height() / 2);
  QVERIFY2(near(chunk, KaTheme::tokens().progressFill), qPrintable(chunk.name()));
  QVERIFY(contrast(bar.palette().color(QPalette::Text), chunk) >= 4.5);
}

void TestThemeRender::radioIndicatorUsesAccent() {
  QRadioButton radio(QStringLiteral("A4"));
  radio.setChecked(true);
  radio.show();
  QVERIFY(QTest::qWaitForWindowExposed(&radio));
  const QImage image = radio.grab().toImage();
  int accent = 0;
  for (int y = 0; y < image.height(); ++y)
    for (int x = 0; x < qMin(image.width(), 24); ++x)
      if (near(image.pixelColor(x, y), KaTheme::tokens().accent, 24)) ++accent;
  QVERIFY2(accent >= 8, qPrintable(QStringLiteral("accent pixels %1").arg(accent)));
}

// 새 모양: the main button turns slate with paper text, and the download header becomes a paper band
// with slate text. The stock look keeps its accent button and blue header.
void TestThemeRender::paperLook_mainButtonIsSlateAndBandIsPaper() {
  QPushButton main(QStringLiteral("등고선 만들기"));
  main.setDefault(true);
  main.resize(180, 40);
  main.show();
  QVERIFY(QTest::qWaitForWindowExposed(&main));
  const auto face = [&main]() { return at(main.grab().toImage(), 6, main.height() / 2); };
  QVERIFY2(near(face(), KaTheme::tokens().accent), qPrintable(face().name()));
  KaTheme::DisplayOptions paper;
  paper.paperLook = true;
  KaTheme::setDisplayOptions(qApp, paper);
  const KaTheme::Tokens t = KaTheme::tokens();
  QCoreApplication::processEvents();
  const QColor slate = face();
  KaDownloadProgressDialog dialog(QStringLiteral("지적도 다운로드"), QStringLiteral("조사 주변 5 km의 지적도 자료를 받습니다."));
  dialog.show();
  const bool shown = QTest::qWaitForWindowExposed(&dialog);
  auto* header = dialog.findChild<QFrame*>(QStringLiteral("downloadHeader"));
  auto* title = dialog.findChild<QLabel*>(QStringLiteral("downloadTitle"));
  const QColor band = header ? at(header->grab().toImage(), header->width() / 2, 3) : QColor();
  const QColor titleInk = title ? title->palette().color(title->foregroundRole()) : QColor();
  KaTheme::setDisplayOptions(qApp, KaTheme::DisplayOptions());  // restore before any check can return early
  QVERIFY(shown && header && title);
  QVERIFY2(near(slate, t.primary), qPrintable(slate.name()));
  QVERIFY2(near(band, t.rail), qPrintable(band.name()));
  QVERIFY2(contrast(titleInk, band) >= 4.5, qPrintable(titleInk.name()));
}

// 새 모양: the tools of the drawing row (#subToolbar) are buttons — a face with a thin rounded edge,
// and the chosen tool in the clay wash with a clay edge (user 2026-10-04 「이것들도 버튼 형태로
// 엔트로픽스타일로」). The stock look keeps its flat row.
void TestThemeRender::paperLook_drawingRowToolsAreButtons() {
  QToolBar row;
  row.setObjectName(QStringLiteral("subToolbar"));
  row.setToolButtonStyle(Qt::ToolButtonTextOnly);
  QAction* plain = row.addAction(QStringLiteral("유구 그리기"));
  row.addAction(QStringLiteral("폴리곤 묶기"));
  QAction* chosen = row.addAction(QStringLiteral("도형선택"));
  chosen->setCheckable(true);
  chosen->setChecked(true);
  row.setGeometry(300, 300, 400, 44);  // away from the pointer: a hovered tool shows the hover wash
  row.show();
  QVERIFY(QTest::qWaitForWindowExposed(&row));
  // Left edge and face of a tool, half-way down: x = 1 is the edge (after the 1 px gap), x = 5 the face.
  const auto sample = [&row](QAction* action, int x) {
    QWidget* button = row.widgetForAction(action);
    return at(button->grab().toImage(), x, button->height() / 2);
  };
  QCOMPARE(sample(plain, 1), sample(plain, 5));  // stock: no edge around a tool
  KaTheme::DisplayOptions paper;
  paper.paperLook = true;
  KaTheme::setDisplayOptions(qApp, paper);
  const KaTheme::Tokens t = KaTheme::tokens();
  QCoreApplication::processEvents();
  const QColor edge = sample(plain, 1), face = sample(plain, 5), chosenEdge = sample(chosen, 1), chosenFace = sample(chosen, 5);
  if (const QString out = qEnvironmentVariable("KA_HGIS_QA_OUTPUT_DIR"); !out.isEmpty() && QDir(out).exists())
    row.grab().save(QDir(out).filePath(QStringLiteral("theme-paper-drawing-row.png")));
  KaTheme::setDisplayOptions(qApp, KaTheme::DisplayOptions());  // restore before any check can return early
  QVERIFY2(near(edge, t.border), qPrintable(edge.name()));
  QVERIFY2(near(face, t.surface), qPrintable(face.name()));
  QVERIFY2(near(chosenEdge, t.accent), qPrintable(chosenEdge.name()));
  QVERIFY2(near(chosenFace, t.selected), qPrintable(chosenFace.name()));
}

QTEST_MAIN(TestThemeRender)
#include "test_theme_render.moc"
