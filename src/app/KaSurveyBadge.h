#pragma once
#include "app/KaChip.h"

class QKeyEvent;
class QMouseEvent;

// The 「조사 열림 · 이름」 badge at the right end of the view tab bar (a QTabWidget corner
// widget). Display only: the window feeds setSurvey() from refreshWindowTitle(); a click
// or Space asks the window for the map tab through clicked(). No survey: hidden.
class KaSurveyBadge : public KaChip {
  Q_OBJECT
 public:
  explicit KaSurveyBadge(QWidget* parent = nullptr);

  // Empty name hides the badge. unsaved switches to the warn tone and wording.
  void setSurvey(const QString& name, bool unsaved);
  QString surveyName() const { return m_name; }
  bool unsaved() const { return m_unsaved; }
  // 「조사 열림 · {name}」 or 「{name} · 저장 안 됨」; empty for an empty name.
  static QString textFor(const QString& name, bool unsaved);
  // 창 제목(목업): 「<조사 이름> * - Strata」, * 는 저장 안 됐을 때만. 조사가 없으면 「Strata」.
  static QString windowTitleFor(const QString& name, bool unsaved);

  QSize sizeHint() const override;

 signals:
  void clicked();

 protected:
  void paintEvent(QPaintEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
  void keyPressEvent(QKeyEvent* event) override;

 private:
  QString m_name;
  bool m_unsaved = false;
  bool m_pressed = false;
};
