#pragma once

#include "core/SurveyFacts.h"

#include <QFrame>
#include <QString>

class KaChip;
class QLabel;

// 「작업 순서」 card of the home page: three steps whose badges follow the facts of the
// continue survey (1 = a survey to continue exists, 2 = it has an area and a feature,
// 3 = a submit package was made) and, under step 3, the last explicit check as
// 「제출 준비: 오류 n · 경고 m」 or the sentence that no check has run yet. Display only.
class KaHomeGuideCard final : public QFrame {
  Q_OBJECT
public:
  explicit KaHomeGuideCard(QWidget* parent = nullptr);

  // targetName: the survey the steps are judged against (empty when none can be continued).
  void setFacts(const QString& targetName, const SurveyFacts::Facts& facts);
  int doneCount() const { return m_done; }
  int nextStep() const { return m_next; }  // 1..3, or 0 when every step is done
  QString submitLine() const { return m_submitLine; }

private:
  struct Step {
    QLabel* badge = nullptr;
    KaChip* next = nullptr;
  };
  void applyStep(int index, bool done, bool isNext);

  KaChip* m_basis = nullptr;
  Step m_steps[3];
  QFrame* m_ready = nullptr;
  KaChip* m_errors = nullptr;
  KaChip* m_warnings = nullptr;
  QLabel* m_notChecked = nullptr;
  int m_done = 0;
  int m_next = 1;
  QString m_submitLine;
};
