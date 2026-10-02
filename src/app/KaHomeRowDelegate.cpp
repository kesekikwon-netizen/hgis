#include "KaHomeRowDelegate.h"

#include "KaChip.h"
#include "KaHomeRecentCard.h"
#include "KaIcons.h"
#include "KaTheme.h"

#include <QApplication>
#include <QFontMetrics>
#include <QPainter>
#include <QPen>
#include <QPolygonF>
#include <QStyle>
#include <QStyleOptionViewItem>

namespace {

constexpr int kLeftInset = 18;  // matches the 「최근 조사」 header margin
constexpr int kThumbWidth = 40;
constexpr int kThumbHeight = 32;
constexpr int kThumbGlyphPx = 22;
constexpr int kThumbGap = 12;
constexpr int kCellInset = 12;
constexpr int kDotPx = 12;
constexpr int kDotGap = 8;

QFont sized(QFont font, int px, bool bold) {
  font.setPixelSize(px);
  font.setBold(bold);
  return font;
}

}  // namespace

KaHomeRowDelegate::KaHomeRowDelegate(QObject* parent) : QStyledItemDelegate(parent) {}

void KaHomeRowDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                              const QModelIndex& index) const {
  QStyleOptionViewItem opt = option;
  initStyleOption(&opt, index);
  opt.text.clear();  // the model text feeds the filter and tooltips; the cell is drawn here
  opt.icon = QIcon();
  const QWidget* widget = opt.widget;
  QStyle* style = widget ? widget->style() : QApplication::style();
  style->drawPrimitive(QStyle::PE_PanelItemViewItem, &opt, painter, widget);  // hover/selected wash
  painter->save();
  painter->setRenderHint(QPainter::Antialiasing, true);
  const qreal dpr = painter->device() ? painter->device()->devicePixelRatioF() : 1.0;
  switch (index.column()) {
    case KaHomeRecentCard::SurveyColumn: paintSurvey(painter, opt.rect, index, dpr); break;
    case KaHomeRecentCard::StateColumn: paintState(painter, opt.rect, index); break;
    case KaHomeRecentCard::DotsColumn: paintDots(painter, opt.rect, index); break;
    default: break;
  }
  painter->restore();
}

QSize KaHomeRowDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const {
  int width = QStyledItemDelegate::sizeHint(option, index).width();
  if (index.column() == KaHomeRecentCard::StateColumn) {
    const QFontMetrics fm(KaChip::chipFont(option.font));
    width = KaChip::sizeForText(fm, index.data(Qt::DisplayRole).toString(), true).width() + 2 * kCellInset;
  } else if (index.column() == KaHomeRecentCard::DotsColumn) {
    width = 3 * kDotPx + 2 * kDotGap + 2 * kCellInset;
  }
  return QSize(width, KaHomeRecentCard::kRowHeight);
}

void KaHomeRowDelegate::paintSurvey(QPainter* painter, const QRect& rect, const QModelIndex& index,
                                    qreal dpr) const {
  const auto& t = KaTheme::tokens();
  const bool missing = index.data(KaHomeRecentCard::kThumbRole).toString() == QLatin1String("missing");
  const QColor ink = missing ? t.inkMuted : t.ink;
  const QRect cell = rect.adjusted(kLeftInset, 0, -kCellInset, 0);
  const QRect thumb(cell.left(), cell.top() + (cell.height() - kThumbHeight) / 2, kThumbWidth, kThumbHeight);
  painter->setPen(QPen(t.border, 1.0));
  painter->setBrush(t.altRow);
  painter->drawRoundedRect(QRectF(thumb).adjusted(0.5, 0.5, -0.5, -0.5), 6.0, 6.0);
  const QString glyphId = index.data(KaHomeRecentCard::kThumbRole).toString();
  const QPixmap glyph = KaIcons::glyphPixmap(glyphId.isEmpty() ? QStringLiteral("survey_thumb") : glyphId, ink,
                                             kThumbGlyphPx, dpr);
  const QRect glyphBox(thumb.center().x() - kThumbGlyphPx / 2, thumb.center().y() - kThumbGlyphPx / 2,
                       kThumbGlyphPx, kThumbGlyphPx);
  painter->drawPixmap(glyphBox, glyph);

  const int textLeft = thumb.right() + 1 + kThumbGap;
  const QRect text(textLeft, cell.top(), cell.right() - textLeft + 1, cell.height());
  const QFont nameFont = sized(painter->font(), 13, true);
  const QFont folderFont = sized(painter->font(), 12, false);  // folder line: 11 was hard to read
  const QFontMetrics nameFm(nameFont);
  const QFontMetrics folderFm(folderFont);
  const int block = nameFm.height() + 2 + folderFm.height();
  const int top = text.top() + (text.height() - block) / 2;
  painter->setFont(nameFont);
  painter->setPen(ink);
  painter->drawText(QRect(text.left(), top, text.width(), nameFm.height()), Qt::AlignLeft | Qt::AlignVCenter,
                    nameFm.elidedText(index.data(Qt::DisplayRole).toString(), Qt::ElideRight, text.width()));
  painter->setFont(folderFont);
  painter->setPen(t.inkMuted);
  painter->drawText(QRect(text.left(), top + nameFm.height() + 2, text.width(), folderFm.height()),
                    Qt::AlignLeft | Qt::AlignVCenter,
                    folderFm.elidedText(index.data(KaHomeRecentCard::kFolderRole).toString(), Qt::ElideMiddle,
                                        text.width()));
}

void KaHomeRowDelegate::paintState(QPainter* painter, const QRect& rect, const QModelIndex& index) const {
  const QString text = index.data(Qt::DisplayRole).toString();
  if (text.isEmpty()) return;
  const KaChip::Tone tone = KaChip::toneFromName(index.data(KaHomeRecentCard::kToneRole).toString());
  const QFontMetrics fm(KaChip::chipFont(painter->font()));
  const int width = qMin(KaChip::sizeForText(fm, text, true).width(), rect.width() - 2 * kCellInset);
  KaChip::paintChip(*painter, QRect(rect.left() + kCellInset, rect.top(), width, rect.height()), text, tone,
                    QString());  // empty glyph: the tone's own mark (check / warn / missing)
}

void KaHomeRowDelegate::paintDots(QPainter* painter, const QRect& rect, const QModelIndex& index) const {
  const QList<int> dots = index.data(KaHomeRecentCard::kDotsRole).value<QList<int>>();
  if (dots.isEmpty()) {
    painter->setFont(sized(painter->font(), 13, false));
    painter->setPen(KaTheme::tokens().inkMuted);
    painter->drawText(rect, Qt::AlignCenter, QStringLiteral("—"));  // 원본 없음 · 모름
    return;
  }
  const int total = static_cast<int>(dots.size()) * kDotPx + (static_cast<int>(dots.size()) - 1) * kDotGap;
  qreal x = rect.center().x() - total / 2.0;
  const qreal y = rect.center().y() - kDotPx / 2.0;
  for (int dot : dots) {
    paintDot(*painter, QRectF(x, y, kDotPx, kDotPx), dot);
    x += kDotPx + kDotGap;
  }
}

void KaHomeRowDelegate::paintDot(QPainter& p, const QRectF& box, int dot) {
  const auto& t = KaTheme::tokens();
  p.save();
  p.setRenderHint(QPainter::Antialiasing, true);
  const QPointF c = box.center();
  const qreal r = box.width() / 2.0;
  if (dot == KaHomeRecentCard::DotTodo) {
    p.setPen(QPen(t.borderStrong, 1.5));
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(c, r - 0.75, r - 0.75);
    p.restore();
    return;
  }
  const QColor fill = dot == KaHomeRecentCard::DotOk ? t.ok : dot == KaHomeRecentCard::DotWarn ? t.warn : t.danger;
  p.setPen(Qt::NoPen);
  p.setBrush(fill);
  p.drawEllipse(c, r, r);
  p.setPen(QPen(Qt::white, 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
  p.setBrush(Qt::NoBrush);
  if (dot == KaHomeRecentCard::DotOk) {
    p.drawPolyline(QPolygonF{c + QPointF(-r * 0.45, 0.0), c + QPointF(-r * 0.12, r * 0.38),
                             c + QPointF(r * 0.48, -r * 0.36)});
  } else {  // 「!」 for warn and danger
    p.drawLine(c + QPointF(0.0, -r * 0.5), c + QPointF(0.0, r * 0.15));
    p.setPen(Qt::NoPen);
    p.setBrush(Qt::white);
    p.drawEllipse(c + QPointF(0.0, r * 0.5), 0.95, 0.95);
  }
  p.restore();
}
