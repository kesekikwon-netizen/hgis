#pragma once

#include <QPointer>
#include <QStyledItemDelegate>

class QgsLayerTreeNode;
class QgsLayerTreeView;

// Section headers of the layer list (「조사 데이터」「주변·참조」「배경 지도」) drawn as a
// 16 px band above the first top-level row of each section. The QgsLayerTree itself
// is untouched: no group is created and no node moves; the band is a painted row
// prefix only, so a user drag that mixes sections simply gets a band per change.
// QGIS's own item delegate (the private QgsLayerTreeViewItemDelegate, which paints
// the indicators) is wrapped, not replaced: every call is forwarded with option.rect
// moved below the band, so the check box, text, tooltips and indicator hit areas stay
// aligned with what is painted. The view property kaLayerSections=false switches the
// bands off (kill switch); without an inner delegate the plain styled one is used.
class KaLayerSectionDelegate : public QStyledItemDelegate {
  Q_OBJECT
 public:
  enum class Section { None, Survey, Nearby, Basemap };
  static constexpr int kBandHeight = 16;

  KaLayerSectionDelegate(QAbstractItemDelegate* inner, QgsLayerTreeView* view);

  // A group row is its own header (None). Survey data: a layer_key or the survey role.
  // Basemap: live tiles. Everything else (cadastral, reference, unmarked user files)
  // is 「주변·참조」.
  static Section sectionOf(QgsLayerTreeNode* node);
  static QString sectionTitle(Section section);

  bool enabled() const;
  // True for a top-level row that starts a section: row 0, or a section change.
  bool hasBand(const QModelIndex& index) const;
  int bandHeight(const QModelIndex& index) const { return hasBand(index) ? kBandHeight : 0; }
  QAbstractItemDelegate* inner() const { return m_inner.data(); }

  void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
  QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;
  bool editorEvent(QEvent* event, QAbstractItemModel* model, const QStyleOptionViewItem& option,
                   const QModelIndex& index) override;
  bool helpEvent(QHelpEvent* event, QAbstractItemView* view, const QStyleOptionViewItem& option,
                 const QModelIndex& index) override;
  void updateEditorGeometry(QWidget* editor, const QStyleOptionViewItem& option,
                            const QModelIndex& index) const override;

 private:
  QStyleOptionViewItem below(const QStyleOptionViewItem& option, const QModelIndex& index) const;
  void paintBand(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const;

  QPointer<QAbstractItemDelegate> m_inner;
  QgsLayerTreeView* m_view = nullptr;
};
