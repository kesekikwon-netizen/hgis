#pragma once

#include "KaHomeStatus.h"
#include "core/AccountStatus.h"

#include <QFrame>
#include <QMetaType>

class QHBoxLayout;
class QLabel;
class QLayout;
class QNetworkAccessManager;
class QPushButton;
class QVBoxLayout;

Q_DECLARE_METATYPE(AccountStatus::Source)

// 「연결 상태」 strip under the home cards: one cell per data source, side by side (dot · name ·
// 설정됨 / 설정 필요 · 「설정」), so all five are in view without scrolling; problem sentences for
// proj.db and the survey folder only when something is wrong, and the click-only internet check. No secret value is ever shown; the state
// says only whether a key or account exists on this PC (「연결됨」 would be a claim about the
// network, which nothing here tests). 「설정」 emits configureRequested for the owner to open
// the matching dialog. Nothing runs on the network until the user presses 「인터넷 확인」.
class KaHomeConnectionCard final : public QFrame {
  Q_OBJECT
public:
  explicit KaHomeConnectionCard(QWidget* parent = nullptr);
  // Runs the local checks again (cheap; no network).
  void refresh();
  void setInputs(const KaHomeStatus::Inputs& inputs);
  // Row wording: 「VWorld 키」, 「수치지형도 계정」, 「문화재 인트라넷」; the rest keep Entry::label.
  static QString displayName(const AccountStatus::Entry& entry);
  static QString stateText(bool ready);  // 「설정됨」 / 「설정 필요」

signals:
  void configureRequested(AccountStatus::Source source);

private:
  void checkInternet();
  void clear(QLayout* layout);

  QHBoxLayout* m_rows = nullptr;
  QVBoxLayout* m_problems = nullptr;
  QLabel* m_internet = nullptr;
  QPushButton* m_internetButton = nullptr;
  QNetworkAccessManager* m_network = nullptr;
};
