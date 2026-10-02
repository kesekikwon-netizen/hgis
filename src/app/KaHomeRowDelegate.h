#pragma once

#include <QStyledItemDelegate>

class QPainter;
class QRectF;

// Paints the first three columns of the home page's recent-survey table: [thumb glyph |
// name over folder], [state chip via KaChip::paintChip] and [three 12 px dots for 구역 ·
// 유구 · 검수]. Colours come from KaTheme tokens; the item's own text stays in the model
// so the filter, tooltips and accessibility still read it. Rows are kRowHeight tall.
class KaHomeRowDelegate final : public QStyledItemDelegate {
public:
  explicit KaHomeRowDelegate(QObject* parent = nullptr);

  void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
  QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;

  // One dot (KaHomeRecentCard::Dot) inside box: ok = filled + check, warn/danger = filled
  // + mark, todo = a borderStrong ring. Shared with tests.
  static void paintDot(QPainter& painter, const QRectF& box, int dot);

private:
  void paintSurvey(QPainter* painter, const QRect& rect, const QModelIndex& index, qreal dpr) const;
  void paintState(QPainter* painter, const QRect& rect, const QModelIndex& index) const;
  void paintDots(QPainter* painter, const QRect& rect, const QModelIndex& index) const;
};
