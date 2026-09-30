#pragma once

#include "core/RecentSurveys.h"

#include <QFrame>
#include <QVector>

class QLabel;
class QLineEdit;
class QSettings;
class QTableWidget;

// 「최근 조사」 card of the home page: a header row and 48 px rows of
// [survey thumb · name · folder] [state chip] [구역 · 유구 · 검수 dots] [last opened].
// A row opens its survey on a click (the header line says so); nothing opens without one.
// Offline entries stay listed with 「원본 없음」. Recovery copies are offered only from the
// row's right-click menu. Item roles are public so the delegate and tests read them.
class KaHomeRecentCard final : public QFrame {
  Q_OBJECT
public:
  enum Column { SurveyColumn = 0, StateColumn = 1, DotsColumn = 2, WhenColumn = 3 };
  enum Dot { DotTodo = 0, DotOk = 1, DotWarn = 2, DotDanger = 3 };
  static constexpr int kPathRole = Qt::UserRole;          // survey path (column 0)
  static constexpr int kSnapshotRole = Qt::UserRole + 1;  // recovery copy path (column 0)
  static constexpr int kFolderRole = Qt::UserRole + 2;    // 「바탕 화면 › 광령리」 (column 0)
  static constexpr int kThumbRole = Qt::UserRole + 3;     // glyph id: survey_thumb / missing (column 0)
  static constexpr int kToneRole = Qt::UserRole + 4;      // KaChip tone name (column 1)
  static constexpr int kDotsRole = Qt::UserRole + 5;      // QList<int> of Dot; empty = 「—」 (column 2)
  static constexpr int kRowHeight = 48;

  explicit KaHomeRecentCard(QWidget* parent = nullptr);
  void setItems(const QVector<RecentSurveys::Item>& items, QSettings& settings);

signals:
  void openRequested(const QString& path);
  void forgetRequested(const QString& path);
  void recoveryFolderRequested(const QString& snapshotPath);
  void recoveryDismissed(const QString& surveyPath);

private:
  void openRow(int row);
  void showMenu(const QPoint& pos);
  void applyFilter(const QString& text);

  QLabel* m_empty = nullptr;
  QLabel* m_count = nullptr;
  QLineEdit* m_filter = nullptr;
  QTableWidget* m_table = nullptr;
};
