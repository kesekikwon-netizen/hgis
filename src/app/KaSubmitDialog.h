#pragma once
#include "core/ChecklistEngine.h"

#include <QDialog>
#include <QVector>

class QLabel;
class QPushButton;
class QTreeWidget;
class QTreeWidgetItem;

// 검수·제출: shows every checklist result (error / warn / passed) with the fix
// and a way to reach the offending shapes or the tool that fixes them, then
// offers the submission package (blocked while an error remains or the rules
// are missing) and the single-layer EPSG:5179 conversion. The dialog decides
// nothing itself; MainWindow acts on choice() after exec().
class KaSubmitDialog : public QDialog {
  Q_OBJECT
public:
  enum class Choice { None, MakePackage, ConvertLayer, GoToTargets, RunAction };

  explicit KaSubmitDialog(QWidget* parent = nullptr);
  // rulesLoaded false: the rule file was not found; rulesHint names where it was looked for.
  void setResults(const QVector<CheckResult>& results, bool rulesLoaded, const QString& rulesHint = {});
  // Selected vector layer for 「레이어 5179 변환」; empty disables the button with a hint.
  void setConvertLayerName(const QString& layerName);

  Choice choice() const { return m_choice; }
  // The row whose 「위치 보기」 or tool button was pressed (targets may be narrowed to one).
  CheckResult chosenResult() const { return m_chosen; }
  bool packageAllowed() const;
  int errorCount() const;
  int warnCount() const;

  // Korean button text for a rule action id ("open_layout" → "도면 만들기"); empty if unknown.
  static QString actionLabel(const QString& action);

signals:
  void recheckRequested();

private:
  void rebuild();
  void choose(Choice choice, const CheckResult& result = {});
  void addResultRow(QTreeWidgetItem* group, int index);

  QLabel* m_summary = nullptr;
  QLabel* m_blockReason = nullptr;
  QTreeWidget* m_tree = nullptr;
  QPushButton* m_package = nullptr;
  QPushButton* m_convert = nullptr;
  QVector<CheckResult> m_results;
  bool m_rulesLoaded = true;
  QString m_rulesHint;
  Choice m_choice = Choice::None;
  CheckResult m_chosen;
};
