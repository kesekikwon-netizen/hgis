// 검수·제출 dialog: the package button follows the hard block (errors or a
// missing rule file), and 「위치 보기」 / tool buttons report the chosen row.
#include "app/KaSubmitDialog.h"

#include <QLabel>
#include <QPushButton>
#include <QTreeWidget>
#include <QtTest>

namespace {
CheckResult result(const QString& id, const QString& severity, bool passed, const QString& action = {}) {
  CheckResult r;
  r.id = id;
  r.severity = severity;
  r.passed = passed;
  r.messageKo = id;
  r.fixKo = QStringLiteral("고치세요");
  r.action = action;
  return r;
}
}  // namespace

class TestSubmitDialog : public QObject {
  Q_OBJECT
private slots:
  void errorsBlockThePackage() {
    KaSubmitDialog dialog;
    auto* package = dialog.findChild<QPushButton*>(QStringLiteral("btnSubmitPackage"));
    QVERIFY(package);
    dialog.setResults({result(QStringLiteral("A"), QStringLiteral("error"), false),
                       result(QStringLiteral("B"), QStringLiteral("warn"), false)},
                      true);
    QVERIFY(!package->isEnabled());
    QCOMPARE(dialog.errorCount(), 1);
    QVERIFY(dialog.findChild<QLabel*>(QStringLiteral("submitBlockReason"))->text().contains(QStringLiteral("1건")));
    package->click();  // disabled: nothing happens
    QCOMPARE(dialog.choice(), KaSubmitDialog::Choice::None);

    dialog.setResults({result(QStringLiteral("A"), QStringLiteral("error"), true),
                       result(QStringLiteral("B"), QStringLiteral("warn"), false)},
                      true);
    QVERIFY2(package->isEnabled(), "warnings alone must not block");
  }

  void missingRulesBlockLikeAnError() {
    KaSubmitDialog dialog;
    auto* package = dialog.findChild<QPushButton*>(QStringLiteral("btnSubmitPackage"));
    dialog.setResults({}, false, QStringLiteral("C:/nowhere/drawing_checklist.v1.json"));
    QVERIFY(!package->isEnabled());
    QVERIFY(!dialog.packageAllowed());
    QVERIFY(dialog.findChild<QLabel*>(QStringLiteral("submitBlockReason"))->text().contains(QStringLiteral("규칙 파일")));
    dialog.setResults({}, true);  // loaded but nothing evaluated is not "clean" either
    QVERIFY(!dialog.packageAllowed());
  }

  void goToAndToolButtonsReportTheRow() {
    KaSubmitDialog dialog;
    CheckResult withTargets = result(QStringLiteral("GCP_DATUM_META"), QStringLiteral("error"), false,
                                     QStringLiteral("edit_attributes"));
    CheckTarget target;
    target.layerId = QStringLiteral("cp");
    target.layerName = QStringLiteral("기준점");
    target.featureId = 3;
    target.label = QStringLiteral("G2");
    withTargets.targets = {target};
    dialog.setResults({withTargets, result(QStringLiteral("REQ_DRAWING_SITE_LOCATION"), QStringLiteral("error"), false,
                                           QStringLiteral("open_layout"))},
                      true);
    auto* go = dialog.findChild<QPushButton*>(QStringLiteral("btnGoTo_GCP_DATUM_META"));
    QVERIFY(go);
    QVERIFY(!dialog.findChild<QPushButton*>(QStringLiteral("btnGoTo_REQ_DRAWING_SITE_LOCATION")));
    go->click();
    QCOMPARE(dialog.choice(), KaSubmitDialog::Choice::GoToTargets);
    QCOMPARE(dialog.chosenResult().targets.size(), 1);
    QCOMPARE(dialog.chosenResult().targets.first().featureId, qint64(3));

    auto* tool = dialog.findChild<QPushButton*>(QStringLiteral("btnAction_REQ_DRAWING_SITE_LOCATION"));
    QVERIFY(tool);
    QCOMPARE(tool->text(), KaSubmitDialog::actionLabel(QStringLiteral("open_layout")));
    tool->click();
    QCOMPARE(dialog.choice(), KaSubmitDialog::Choice::RunAction);
    QCOMPARE(dialog.chosenResult().action, QStringLiteral("open_layout"));
  }

  void convertNeedsASelectedLayer() {
    KaSubmitDialog dialog;
    auto* convert = dialog.findChild<QPushButton*>(QStringLiteral("btnConvertLayer5179"));
    QVERIFY(convert && !convert->isEnabled());
    dialog.setConvertLayerName(QStringLiteral("유구면"));
    QVERIFY(convert->isEnabled());
    QVERIFY(convert->toolTip().contains(QStringLiteral("유구면")));
    convert->click();
    QCOMPARE(dialog.choice(), KaSubmitDialog::Choice::ConvertLayer);
  }

  void everyRuleActionHasALabel() {
    for (const char* action : {"open_layout", "open_section", "draw_survey_area", "draw_feature", "add_control_point",
                               "edit_attributes", "vertex_edit", "select_tool", "set_work_crs"})
      QVERIFY2(!KaSubmitDialog::actionLabel(QString::fromLatin1(action)).isEmpty(), action);
  }
};

QTEST_MAIN(TestSubmitDialog)
#include "test_submit_dialog.moc"
