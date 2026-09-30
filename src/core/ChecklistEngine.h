#pragma once
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>

// One offending object named by the project state. featureId -1 means the whole layer.
struct CheckTarget {
  QString layerId;
  QString layerName;
  qint64 featureId = -1;
  QString label;
};

struct CheckResult {
  QString id;
  QString severity;
  QString messageKo;
  bool passed = false;
  QString fixKo;    // how to fix it, in one sentence
  QString basisKo;  // guideline or program rule behind the check
  QString action;   // tool id that fixes it; MainWindow maps it to a command
  QString detailKo; // extra facts from the state (missing items, current CRS)
  QVector<CheckTarget> targets;
};

class ChecklistEngine : public QObject {
  Q_OBJECT
public:
  explicit ChecklistEngine(QObject* parent = nullptr);
  // Keeps the previously loaded rules when the file is missing or invalid.
  bool loadRules(const QString& jsonPath);
  QVector<CheckResult> evaluate(const QJsonObject& projectState) const;
  int ruleCount() const { return m_rules.size(); }
  QString loadedPath() const { return m_path; }
  // Rules whose check_type has no evaluator. They always fail (fail-closed).
  QStringList unsupportedRuleIds() const;
  static bool isSupportedCheckType(const QString& checkType);
  // State key that holds the value and the offenders for a check_type.
  static QString stateKeyFor(const QString& checkType);
  static int failedCount(const QVector<CheckResult>& results, const QString& severity);
  // Plain text for README_submit.txt: failed rules with their fix hints, or OK.
  static QString summaryText(const QVector<CheckResult>& results);

private:
  struct Rule {
    QString id;
    QString severity;
    QString messageKo;
    QString checkType;
    QString fixKo;
    QString basisKo;
    QString action;
  };
  QVector<Rule> m_rules;
  QString m_path;
  static bool evalOne(const Rule& r, const QJsonObject& state, bool* supported);
};
