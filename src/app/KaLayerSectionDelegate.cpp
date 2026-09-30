#include "KaLayerSectionDelegate.h"

#include "core/LayerOps.h"
#include "core/LayerRole.h"

#include <QAbstractItemView>
#include <QFont>
#include <QPainter>
#include <QPen>

#include <qgslayertree.h>
#include <qgslayertreeview.h>
#include <qgsmaplayer.h>

namespace {
constexpr int kTextLeft = 6;
}  // namespace

KaLayerSectionDelegate::KaLayerSectionDelegate(QAbstractItemDelegate* inner, QgsLayerTreeView* view)
    : QStyledItemDelegate(view), m_inner(inner), m_view(view) {}

KaLayerSectionDelegate::Section KaLayerSectionDelegate::sectionOf(QgsLayerTreeNode* node) {
  if (!node || QgsLayerTree::isGroup(node)) return Section::None;
  auto* leaf = qobject_cast<QgsLayerTreeLayer*>(node);
  const QgsMapLayer* layer = leaf ? leaf->layer() : nullptr;
  if (!layer) return Section::Nearby;
  if (!LayerOps::layerKeyOf(layer).isEmpty() || LayerRole::resolve(layer) == LayerRole::Kind::Survey)
    return Section::Survey;
  if (LayerOps::isBasemapLayer(layer)) return Section::Basemap;
  return Section::Nearby;
}

QString KaLayerSectionDelegate::sectionTitle(Section section) {
  switch (section) {
    case Section::Survey: return QStringLiteral("조사 데이터");
    case Section::Nearby: return QStringLiteral("주변·참조");
    case Section::Basemap: return QStringLiteral("배경 지도");
    case Section::None: break;
  }
  return {};
}

bool KaLayerSectionDelegate::enabled() const {
  const QVariant flag = m_view ? m_view->property("kaLayerSections") : QVariant();
  return !flag.isValid() || flag.toBool();
}

bool KaLayerSectionDelegate::hasBand(const QModelIndex& index) const {
  if (!m_view || !index.isValid() || index.parent().isValid() || !enabled()) return false;
  const QModelIndex first = index.siblingAtColumn(0);
  const Section section = sectionOf(m_view->index2node(first));
  if (section == Section::None) return false;
  if (first.row() == 0) return true;
  return sectionOf(m_view->index2node(first.sibling(first.row() - 1, 0))) != section;
}

QStyleOptionViewItem KaLayerSectionDelegate::below(const QStyleOptionViewItem& option,
                                                   const QModelIndex& index) const {
  QStyleOptionViewItem moved(option);
  const int band = bandHeight(index);
  if (band > 0) moved.rect.adjust(0, band, 0, 0);
  return moved;
}

// The band keeps the row's own background (alternate colours stay); it adds a 1 px
// top line and the section title, 10 px bold in the muted ink, 6 px from the left.
// Colours come from the view palette KaTheme::palette() fills (Mid = border,
// PlaceholderText = inkMuted), so the ka_layer_* test targets need no theme sources.
void KaLayerSectionDelegate::paintBand(QPainter* painter, const QStyleOptionViewItem& option,
                                       const QModelIndex& index) const {
  if (index.column() != 0) return;
  const auto* view = qobject_cast<const QAbstractItemView*>(option.widget);
  const int width = view && view->viewport() ? view->viewport()->width() : option.rect.right() + 1;
  const QRect band(0, option.rect.top(), width, kBandHeight);
  painter->save();
  painter->setPen(QPen(option.palette.color(QPalette::Mid), 1));
  painter->drawLine(band.topLeft(), band.topRight());
  QFont font = option.font;
  font.setPixelSize(11);  // band caption; the list rows themselves stay 10 px (user decision)
  font.setBold(true);
  painter->setFont(font);
  painter->setPen(option.palette.color(QPalette::PlaceholderText));
  painter->drawText(band.adjusted(kTextLeft, 1, -4, 0), Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine,
                    sectionTitle(sectionOf(m_view->index2node(index))));
  painter->restore();
}

void KaLayerSectionDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                                   const QModelIndex& index) const {
  if (hasBand(index)) paintBand(painter, option, index);
  const QStyleOptionViewItem moved = below(option, index);
  if (m_inner) m_inner->paint(painter, moved, index);
  else QStyledItemDelegate::paint(painter, moved, index);
}

QSize KaLayerSectionDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const {
  QSize size = m_inner ? m_inner->sizeHint(option, index) : QStyledItemDelegate::sizeHint(option, index);
  if (hasBand(index)) size.rheight() += kBandHeight;
  return size;
}

bool KaLayerSectionDelegate::editorEvent(QEvent* event, QAbstractItemModel* model,
                                         const QStyleOptionViewItem& option, const QModelIndex& index) {
  const QStyleOptionViewItem moved = below(option, index);
  if (m_inner) return m_inner->editorEvent(event, model, moved, index);
  return QStyledItemDelegate::editorEvent(event, model, moved, index);
}

bool KaLayerSectionDelegate::helpEvent(QHelpEvent* event, QAbstractItemView* view,
                                       const QStyleOptionViewItem& option, const QModelIndex& index) {
  const QStyleOptionViewItem moved = below(option, index);
  if (m_inner) return m_inner->helpEvent(event, view, moved, index);
  return QStyledItemDelegate::helpEvent(event, view, moved, index);
}

void KaLayerSectionDelegate::updateEditorGeometry(QWidget* editor, const QStyleOptionViewItem& option,
                                                  const QModelIndex& index) const {
  QStyledItemDelegate::updateEditorGeometry(editor, below(option, index), index);
}
