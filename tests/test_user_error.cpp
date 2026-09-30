#include <QtTest>
#include <QAbstractButton>
#include <QApplication>
#include <QMessageBox>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTimer>
#include <QWidget>
#include "app/KaUserError.h"

namespace {

QMessageBox* visibleBox() {
  for (QWidget* w : QApplication::topLevelWidgets()) {
    auto* box = qobject_cast<QMessageBox*>(w);
    if (box && box->isVisible()) return box;
  }
  return nullptr;
}

QAbstractButton* buttonNamed(QMessageBox* box, const QString& text) {
  if (!box) return nullptr;
  for (QAbstractButton* button : box->buttons())
    if (button->text() == text) return button;
  return nullptr;
}

KaUserError::Spec sampleSpec() {
  KaUserError::Spec spec;
  spec.title = QStringLiteral("지적도 받기");
  spec.what = QStringLiteral("지적도를 받지 못했습니다.");
  spec.why = QStringLiteral("서버가 <오류>를 돌려주었습니다.");
  spec.how = QStringLiteral("잠시 뒤 다시 받으세요.");
  return spec;
}

}  // namespace

class TestUserError : public QObject {
  Q_OBJECT
private slots:
  void formatBody_containsWhatWhyHow();
  void warn_withAction_returnsActionChosen();
  void warn_withoutAction_returnsDismissed();
  void formatBodyHtml_boldsHeadingsAndEscapesText();
  void warn_detailsFoldAwayAndCloseIsTheEscape();
  void warn_logButtonOpensFolderWithoutClosing();
  void warn_noLogFolderMeansNoLogButton();
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

void TestUserError::formatBodyHtml_boldsHeadingsAndEscapesText() {
  const QString html = KaUserError::formatBodyHtml(sampleSpec());
  QVERIFY(html.contains(QStringLiteral("<b>무엇이 안 됐나</b>")));
  QVERIFY(html.contains(QStringLiteral("<b>왜 그런가</b>")));
  QVERIFY(html.contains(QStringLiteral("<b>어떻게 하나</b>")));
  QVERIFY(html.contains(QStringLiteral("&lt;오류&gt;")));
  QVERIFY(!html.contains(QStringLiteral("<오류>")));
  // The plain form stays unescaped for logs and existing callers.
  QVERIFY(KaUserError::formatBody(sampleSpec()).contains(QStringLiteral("<오류>")));
}

void TestUserError::warn_detailsFoldAwayAndCloseIsTheEscape() {
  KaUserError::setLogFolder(QString());
  QWidget parent;
  QString details;
  QString escape;
  QString defaultText;
  QTimer::singleShot(0, qApp, [&] {
    QMessageBox* box = visibleBox();
    if (!box) return;
    details = box->detailedText();
    escape = box->escapeButton() ? box->escapeButton()->text() : QString();
    defaultText = box->defaultButton() ? box->defaultButton()->text() : QString();
    if (auto* close = buttonNamed(box, QStringLiteral("닫기"))) close->click();
  });
  KaUserError::Spec spec = sampleSpec();
  spec.actionLabel = QStringLiteral("다시 받기");
  spec.details = QStringLiteral("HTTP 503 Service Unavailable");
  QCOMPARE(KaUserError::warn(&parent, spec), KaUserError::Result::Dismissed);
  QCOMPARE(details, spec.details);
  QCOMPARE(escape, QStringLiteral("닫기"));
  QCOMPARE(defaultText, QStringLiteral("다시 받기"));
}

void TestUserError::warn_logButtonOpensFolderWithoutClosing() {
  QTemporaryDir logs;
  QVERIFY(logs.isValid());
  QStringList opened;
  KaUserError::setFolderOpener([&opened](const QString& folder) {
    opened << folder;
    return true;
  });
  KaUserError::setLogFolder(logs.path());
  QWidget parent;
  bool stillOpen = false;
  QTimer::singleShot(0, qApp, [&] {
    QMessageBox* box = visibleBox();
    auto* logButton = buttonNamed(box, QStringLiteral("로그 폴더 열기"));
    if (!logButton) return;
    logButton->click();
    QTimer::singleShot(0, qApp, [&, box] {
      stillOpen = box->isVisible();
      if (auto* close = buttonNamed(box, QStringLiteral("닫기"))) close->click();
    });
  });
  QCOMPARE(KaUserError::warn(&parent, sampleSpec()), KaUserError::Result::Dismissed);
  QCOMPARE(opened, QStringList{logs.path()});
  QVERIFY2(stillOpen, "opening the log folder must not close the error dialog");
  KaUserError::setLogFolder(QString());
}

void TestUserError::warn_noLogFolderMeansNoLogButton() {
  KaUserError::setLogFolder(QString());
  QWidget parent;
  bool hadLogButton = true;
  QTimer::singleShot(0, qApp, [&] {
    QMessageBox* box = visibleBox();
    hadLogButton = buttonNamed(box, QStringLiteral("로그 폴더 열기")) != nullptr;
    if (auto* close = buttonNamed(box, QStringLiteral("닫기"))) close->click();
  });
  QCOMPARE(KaUserError::warn(&parent, sampleSpec()), KaUserError::Result::Dismissed);
  QVERIFY(!hadLogButton);
}

QTEST_MAIN(TestUserError)
#include "test_user_error.moc"
