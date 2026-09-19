#pragma once

#include <QObject>
#include <QPointer>
#include <QStringList>
#include <QWidget>

class QCheckBox;
class QDoubleSpinBox;
class QEvent;
class QKeyEvent;
class QLabel;
class QLineEdit;
class QSpinBox;
class QTimer;
class QgsLayerTreeModel;
class QgsLayerTreeView;
class QgsLayoutView;
class QgsLayoutViewToolSelect;
class QgsLayoutViewToolPan;
class QgsPrintLayout;
class QgsProject;

// 입체지형 전용 조판 탭. 시트는 terrain3d_sheet. 조판 항목/방위/도명 카드는 2D와 같음.
class KaTerrain3dLayoutStudio : public QWidget {
  Q_OBJECT
public:
  explicit KaTerrain3dLayoutStudio(QgsProject* project, QWidget* parent = nullptr);
  void attachSheet();
  void detachSheet();
  void deleteSelectedItems();
  void undoLastChange();
  void redoLastChange();

public slots:
  void exportPdf();

signals:
  void overlaysChanged();
  void requestScale(int denominator);

protected:
  bool eventFilter(QObject* watched, QEvent* event) override;
  void keyPressEvent(QKeyEvent* event) override;

private:
  void attachLayoutToView();
  void detachLayoutFromView();
  QgsPrintLayout* currentLayout() const;
  void fitPaper();
  void addLegend();
  void applyLegendStyle();
  void applyNorthKind(int kind);
  void applyScaleNow();
  void applyBarStyle(const QString& style);
  void addScaleLabel();
  void addCrsLabel();
  void warn(const QString& err);
  void selectItemById(const char* id);
  void rememberPlaced(const char* id);
  bool editingText() const;

  QPointer<QgsProject> m_project;
  QgsLayoutView* m_view = nullptr;
  QgsLayoutViewToolSelect* m_toolSelect = nullptr;
  QgsLayoutViewToolPan* m_toolPan = nullptr;
  QgsLayerTreeView* m_layerTree = nullptr;
  QgsLayerTreeModel* m_layerModel = nullptr;
  QTimer* m_overlayTimer = nullptr;
  QLabel* m_status = nullptr;
  QLineEdit* m_legendTitle = nullptr;
  QSpinBox* m_legendFont = nullptr;
  QCheckBox* m_legendBold = nullptr;
  QCheckBox* m_legendItalic = nullptr;
  QDoubleSpinBox* m_northSize = nullptr;
  QSpinBox* m_scaleSpin = nullptr;
  QStringList m_placeUndo;
};
