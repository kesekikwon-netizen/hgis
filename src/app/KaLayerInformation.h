#pragma once

#include <qgslayertreemodel.h>
#include <qgslayertreeview.h>
#include <QPointer>
#include <QTimer>
#include <QWidget>

class QgsProject;
class QgsMapLayer;
class QgsLayerTreeView;
class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QFormLayout;

class KaLayerInformationView : public QgsLayerTreeView {
public:
  explicit KaLayerInformationView(QWidget* parent = nullptr) : QgsLayerTreeView(parent) {}
protected:
  void resizeEvent(QResizeEvent* event) override;
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
  void refreshLabels();
  static void configureView(QgsLayerTreeView* view);
signals:
  void labelsEdited(); // One settled notification for a burst/group operation.
  void labelStateChanged();
protected:
  bool eventFilter(QObject* watched, QEvent* event) override;
private:
  QList<QgsMapLayer*> targets(QgsLayerTreeNode* node) const;
  void watch(QgsMapLayer* layer);
  QPointer<QgsProject> m_project;
  bool m_layout = false;
  double m_scale = 0.;
  QTimer m_stateTimer;
  QTimer m_editTimer;
};

class KaLayerInformationPanel : public QWidget {
  Q_OBJECT
public:
  KaLayerInformationPanel(KaLayerInformationModel* model, QgsLayerTreeView* view,
                          QWidget* parent = nullptr);
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
  QFormLayout* m_details = nullptr;
};
