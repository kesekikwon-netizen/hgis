// 좌표계 정보가 없는 CAD 도면의 좌표계 후보를 지역 이름과 함께 고르는 KaCadCrsDialog.
#include <QApplication>
#include <QCheckBox>
#include <QListWidget>
#include <QPushButton>
#include <QStandardPaths>
#include <QtTest>

#include "app/KaCadCrsDialog.h"

namespace {

CadCrsCandidate candidate(const QString& authId, const QString& region, double distanceM) {
  CadCrsCandidate c;
  c.authId = authId;
  c.label = CadCrsGuess::label(authId);
  c.region = region;
  c.distanceToSiteM = distanceM;
  return c;
}

CadCrsResult threeCandidates() {
  CadCrsResult r;
  r.verdict = CadCrsVerdict::Choose;
  r.candidates = {candidate(QStringLiteral("EPSG:5174"), QStringLiteral("경상북도"), 400),
                  candidate(QStringLiteral("EPSG:5186"), QStringLiteral("부산광역시"), 98000),
                  candidate(QStringLiteral("EPSG:5185"), QStringLiteral("전라남도"), 250000)};
  return r;
}

QPushButton* button(QWidget& dialog, const char* name) {
  return dialog.findChild<QPushButton*>(QString::fromLatin1(name));
}

}  // namespace

class TestCadDialog : public QObject {
  Q_OBJECT
 private slots:
  void dialog_listsDescribedCandidatesWithTheFirstSelected() {
    const CadCrsResult guess = threeCandidates();
    KaCadCrsDialog dialog(guess);
    QCOMPARE(dialog.windowTitle(), QStringLiteral("도면 좌표계 고르기"));
    auto* list = dialog.findChild<QListWidget*>(QStringLiteral("cadCrsList"));
    QVERIFY(list);
    QCOMPARE(list->count(), 3);
    for (int i = 0; i < 3; ++i) QCOMPARE(list->item(i)->text(), CadCrsGuess::describe(guess.candidates[i]));
    QCOMPARE(list->currentRow(), 0);
    auto* remember = dialog.findChild<QCheckBox*>(QStringLiteral("cadCrsRemember"));
    QVERIFY(remember);
    QVERIFY(!remember->isChecked());  // 창은 그대로 두고, 맞을 때만 사용자가 켠다
    remember->setChecked(true);
    QVERIFY(dialog.remember());
  }

  void dialog_buttonsGiveIndexNoCrsOrCancel() {
    const CadCrsResult guess = threeCandidates();
    {
      KaCadCrsDialog dialog(guess);
      auto* list = dialog.findChild<QListWidget*>(QStringLiteral("cadCrsList"));
      QVERIFY(list);
      list->setCurrentRow(2);
      QVERIFY(button(dialog, "cadCrsAccept"));
      button(dialog, "cadCrsAccept")->click();
      QCOMPARE(dialog.outcome(), std::optional<int>(2));
      QCOMPARE(dialog.result(), int(QDialog::Accepted));
    }
    {
      KaCadCrsDialog dialog(guess);
      QVERIFY(button(dialog, "cadCrsNoCrs"));
      button(dialog, "cadCrsNoCrs")->click();
      QCOMPARE(dialog.outcome(), std::optional<int>(-1));
    }
    {
      KaCadCrsDialog dialog(guess);
      QVERIFY(button(dialog, "cadCrsCancel"));
      button(dialog, "cadCrsCancel")->click();
      QVERIFY(!dialog.outcome().has_value());
      QCOMPARE(dialog.result(), int(QDialog::Rejected));
    }
  }

  void choose_usesTheTestChooser() {
    int calls = 0;
    int seen = 0;
    KaCadCrsDialog::setChooserForTests([&](const CadCrsResult& g) {
      ++calls;
      seen = g.candidates.size();
      return std::optional<int>(1);
    });
    QCOMPARE(KaCadCrsDialog::choose(nullptr, threeCandidates()), std::optional<int>(1));
    QCOMPARE(calls, 1);
    QCOMPARE(seen, 3);
    KaCadCrsDialog::setChooserForTests({});
  }
};

int main(int argc, char** argv) {
  QStandardPaths::setTestModeEnabled(true);
  QApplication app(argc, argv);
  TestCadDialog tests;
  return QTest::qExec(&tests, argc, argv);
}

#include "test_cad_dialog.moc"
