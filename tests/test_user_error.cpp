#include <QtTest>
#include <QAbstractButton>
#include <QApplication>
#include <QMessageBox>
#include <QPushButton>
#include <QTimer>
#include <QWidget>
#include "app/KaUserError.h"

class TestUserError : public QObject {
  Q_OBJECT
private slots:
  void formatBody_containsWhatWhyHow();
  void warn_withAction_returnsActionChosen();
  void warn_withoutAction_returnsDismissed();
};

void TestUserError::formatBody_containsWhatWhyHow() {
  KaUserError::Spec spec;
  spec.title = QStringLiteral("지적도 받기");
  spec.what = QStringLiteral("지적도를 받지 못했습니다.");
  spec.why = QStringLiteral("아이디나 비밀번호가 맞지 않습니다.");
  spec.how = QStringLiteral("아이디·비밀번호를 다시 입력한 뒤 받기를 누르세요.");
  const QString body = KaUserError::formatBody(spec);
  QVERIFY(body.contains(QStringLiteral("무엇이 안 됐나")));
  QVERIFY(body.contains(spec.what));
  QVERIFY(body.contains(QStringLiteral("왜 그런가")));
  QVERIFY(body.contains(spec.why));
  QVERIFY(body.contains(QStringLiteral("어떻게 하나")));
  QVERIFY(body.contains(spec.how));
}

void TestUserError::warn_withAction_returnsActionChosen() {
  QWidget parent;
  QTimer::singleShot(0, qApp, [] {
    for (QWidget* w : QApplication::topLevelWidgets()) {
      auto* box = qobject_cast<QMessageBox*>(w);
      if (!box || !box->isVisible()) continue;
      for (QAbstractButton* button : box->buttons()) {
        if (button->text() == QStringLiteral("다시 입력")) {
          button->click();
          return;
        }
      }
    }
  });
  KaUserError::Spec spec;
  spec.title = QStringLiteral("시험");
  spec.what = QStringLiteral("실패했습니다.");
  spec.why = QStringLiteral("시험용입니다.");
  spec.how = QStringLiteral("다시 입력 단추를 누르세요.");
  spec.actionLabel = QStringLiteral("다시 입력");
  QCOMPARE(KaUserError::warn(&parent, spec), KaUserError::Result::ActionChosen);
}

void TestUserError::warn_withoutAction_returnsDismissed() {
  QWidget parent;
  QTimer::singleShot(0, qApp, [] {
    for (QWidget* w : QApplication::topLevelWidgets()) {
      auto* box = qobject_cast<QMessageBox*>(w);
      if (!box || !box->isVisible()) continue;
      for (QAbstractButton* button : box->buttons()) {
        if (button->text() == QStringLiteral("닫기")) {
          button->click();
          return;
        }
      }
    }
  });
  KaUserError::Spec spec;
  spec.title = QStringLiteral("시험");
  spec.what = QStringLiteral("실패했습니다.");
  spec.why = QStringLiteral("시험용입니다.");
  spec.how = QStringLiteral("닫기를 누르세요.");
  QCOMPARE(KaUserError::warn(&parent, spec), KaUserError::Result::Dismissed);
}

QTEST_MAIN(TestUserError)
#include "test_user_error.moc"
