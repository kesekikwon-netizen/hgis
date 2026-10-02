#pragma once

#include "core/AccountStatus.h"
#include "core/RecentSurveys.h"

#include <QStringList>
#include <QVector>
#include <QWidget>

class KaChip;
class KaHomeConnectionCard;
class KaHomeGuideCard;
class KaHomeRecentCard;
class QLabel;
class QPushButton;
class QResizeEvent;
class QScrollArea;
class QShowEvent;
class QWidget;

// Home page: a navy band with the Strata name, the two start actions and a
// card for the most recent survey, above the recent-survey table on the left and
// the 「작업 순서」 and 「연결 상태」 cards on the right. Nothing opens on its own;
// every survey opens from a click. A survey whose last session left unsaved edits
// in a recovery copy gets one quiet line; the copy or its folder opens only when
// the user clicks it. The band is 184 px tall in windows under 820 px, else 236.
class KaStartPage : public QWidget {
  Q_OBJECT
public:
  explicit KaStartPage(QWidget* parent = nullptr);
  void reload();
  // Reads the 「연결 상태」 rows again on the next event-loop turn (local checks, no network).
  // Deferred because the caller may be a row's own 「설정」 button, which the rebuild deletes;
  // showEvent and the window's account dialogs (openAccountSetup) both use it.
  void refreshConnections();

  static int heroHeightFor(int windowHeight);
  static int rightColumnWidthFor(int windowWidth);
  // Recent surveys whose GPKG may be counted for the 구역·유구 dots: reachable, not remote.
  static QStringList probeCandidates(const QVector<RecentSurveys::Item>& items);

signals:
  void newSurveyRequested();
  void openRequested();
  void recentOpened(const QString& path);
  void forgetRequested(const QString& path);
  void configureRequested(AccountStatus::Source source);

protected:
  void showEvent(QShowEvent* event) override;
  void resizeEvent(QResizeEvent* event) override;

private:
  QWidget* buildHero();
  QWidget* buildRightColumn();
  void applyWindowMetrics();
  void probeSurveyFacts();
  void onRecoveryLink(const QString& link);
  void openRecoveryFolder(const QString& snapshotPath);
  void dismissRecovery(const QString& surveyPath);

  QWidget* m_hero = nullptr;
  QScrollArea* m_rightColumn = nullptr;
  KaHomeRecentCard* m_recentCard = nullptr;
  KaHomeGuideCard* m_guide = nullptr;
  KaHomeConnectionCard* m_connection = nullptr;
  QWidget* m_continue = nullptr;
  KaChip* m_badge = nullptr;
  QLabel* m_continueName = nullptr;
  QLabel* m_continuePath = nullptr;
  QLabel* m_continueWhen = nullptr;
  QPushButton* m_resume = nullptr;
  QLabel* m_recovery = nullptr;
  QVector<RecentSurveys::Item> m_items;
  QString m_continueTarget;
  QString m_recoverySnapshot;  // unsaved-edit copy of the continue survey, if any
};
