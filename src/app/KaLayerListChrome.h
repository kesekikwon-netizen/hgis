#pragma once

#include <QModelIndex>
#include <QPointer>
#include <QSet>
#include <QString>
#include <QTimer>
#include <QWidget>

class QLabel;
class QLineEdit;
class QgsLayerTreeView;
class QgsLayerTreeViewIndicator;
class QgsVectorLayer;

// Chrome above the layer list: the 「레이어 찾기」 field and the 「편집 중」 pencil.
// The field filters through the view's own proxy (QgsLayerTreeProxyModel::setFilterText,
// 120 ms after typing; Esc clears) and says in one sentence when nothing matches. A
// current row that the filter hides is dropped so 「표시 설정」 never points at a hidden
// layer, and dragging is off while a filter is active. The pencil is a display-only
// QgsLayerTreeViewIndicator on every vector layer that is editable and modified.
class KaLayerListChrome : public QWidget {
  Q_OBJECT
 public:
  static constexpr int kDebounceMs = 120;

  explicit KaLayerListChrome(QgsLayerTreeView* view, QWidget* parent = nullptr);

  QLineEdit* filterEdit() const { return m_edit; }
  QString filterText() const;
  // Applies at once (the field itself waits kDebounceMs after the last keystroke).
  void setFilterText(const QString& text);
  bool isEmptySentenceShown() const;
  QString emptySentence() const;
  // Layer rows the filter still shows. Group rows do not count: this QGIS proxy keeps a
  // group row even when none of its layers match, so the sentence looks at layers only.
  int visibleLayerCount() const;
  QgsLayerTreeViewIndicator* editIndicator() const { return m_pencil; }
  // Re-reads every layer's edit state (also called by the layer signals).
  void syncEditIndicators();

 protected:
  bool eventFilter(QObject* watched, QEvent* event) override;

 private:
  void applyFilter();
  void updateEmptySentence();
  int visibleLayerRows(const QModelIndex& parent) const;
  void watchLayers();
  void syncIndicator(QgsVectorLayer* layer);

  QPointer<QgsLayerTreeView> m_view;
  QLineEdit* m_edit = nullptr;
  QLabel* m_empty = nullptr;
  QTimer m_debounce;
  QgsLayerTreeViewIndicator* m_pencil = nullptr;
  QSet<QString> m_watched;
  bool m_filtering = false;
  bool m_dragWasEnabled = true;
};
