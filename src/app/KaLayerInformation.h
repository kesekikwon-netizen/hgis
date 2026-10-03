#pragma once

#include <qgslayertreemodel.h>
#include <qgslayertreeview.h>
#include <QHash>
#include <QPointer>
#include <QSet>
#include <QTimer>
#include <QWidget>
#include "core/LayerLabelControls.h"

class QgsProject;
class QgsMapLayer;
class QgsLayerTreeView;
class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QFormLayout;
class QSplitter;
class QToolButton;

// View methods live in KaLayerInformationView.cpp (with configureView).
class KaLayerInformationView : public QgsLayerTreeView {
public:
  static constexpr int kMinVisibleRows = 5;
  explicit KaLayerInformationView(QWidget* parent = nullptr) : QgsLayerTreeView(parent) {}
  int minimumListHeight() const;
  // Height of a row without a section band (KaLayerSectionDelegate adds 16 px to the
  // first row of each section); the five-row minimum is counted in these rows.
  int baseRowHeight() const { return baseRowHeightOf(this); }
  static int baseRowHeightOf(const QgsLayerTreeView* tree);
  static void protectSidebarList(QSplitter* split, QgsLayerTreeView* tree, QToolButton* filesToggle,
                                 QWidget* filesPane, class KaLayerInformationPanel* panel);
  static void openSidebarFiles(QSplitter* split, QWidget* filesPane);
protected:
  void resizeEvent(QResizeEvent* event) override;
  void drawBranches(QPainter* painter, const QRect& rect, const QModelIndex& index) const override;
private:
  bool m_columnsSized = false;
};

// Keep QGIS's visibility, legend and drag/drop column; add only label controls.
class KaLayerInformationModel : public QgsLayerTreeModel {
  Q_OBJECT
public:
  KaLayerInformationModel(QgsProject* project, bool layout, QObject* parent = nullptr);
  int columnCount(const QModelIndex& parent = {}) const override;
  int rowCount(const QModelIndex& parent = {}) const override;
  QModelIndex index(int row, int column, const QModelIndex& parent = {}) const override;
  QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
  QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
  Qt::ItemFlags flags(const QModelIndex& index) const override;
  bool setData(const QModelIndex& index, const QVariant& value, int role) override;
  bool layoutMode() const { return m_layout; }
  double scale() const { return m_scale; }
  void setScale(double scale);
  void notifyEdited();
  // Every row: tree, visibility, scale or layer set changed.
  void refreshLabels();
  // One layer's style, labels or fields changed: its rows and their groups only.
  void refreshLayer(const QString& layerId);
  // Cached LayerLabelControls::describe (dropped when the layer or tree changes).
  LayerLabelControls::Info labelInfo(const QgsMapLayer* layer, bool withScale = false) const;
  int describeCallsForTests() const { return m_describeCalls; }
  static void configureView(QgsLayerTreeView* view);
signals:
  void labelsEdited(); // One settled notification for a burst/group operation.
  void labelStateChanged();
protected:
  bool eventFilter(QObject* watched, QEvent* event) override;
private:
  QList<QgsMapLayer*> targets(QgsLayerTreeNode* node) const;
  void watch(QgsMapLayer* layer);
  void emitRowChanged(QgsLayerTreeNode* node);
  QPointer<QgsProject> m_project;
  bool m_layout = false;
  double m_scale = 0.;
  QTimer m_stateTimer;
  QTimer m_editTimer;
  // A freshly built model has nothing stale; starting true made the first per-layer change
  // announce every row (the timer only runs once something calls refreshLayer/refreshLabels).
  bool m_fullRefresh = false;
  QSet<QString> m_dirtyLayers;
  mutable QHash<QString, LayerLabelControls::Info> m_infoCache;
  mutable int m_describeCalls = 0;
};

class KaLayerInformationPanel : public QWidget {
  Q_OBJECT
public:
  KaLayerInformationPanel(KaLayerInformationModel* model, QgsLayerTreeView* view,
                          QWidget* parent = nullptr);
  void collapseDetails();
private:
  void refresh();
  QPointer<KaLayerInformationModel> m_model;
  QPointer<QgsLayerTreeView> m_view;
  QLabel* m_title = nullptr;
  QLabel* m_reason = nullptr;
  QCheckBox* m_labels = nullptr;
  QCheckBox* m_area = nullptr;
  QComboBox* m_field = nullptr;
  QDoubleSpinBox* m_size = nullptr;
  QComboBox* m_color = nullptr;   // optional label ink (presets)
  QCheckBox* m_halo = nullptr;    // optional white halo
  QFormLayout* m_details = nullptr;
};
