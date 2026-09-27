#pragma once

#include "core/TilePrint.h"

#include <QDialog>
#include <QHash>
#include <QImage>
#include <QList>
#include <QSet>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QFormLayout;
class QLabel;
class QPushButton;
class QRadioButton;
class KaTilePreview;

// 도면 인쇄 창. 용지 한 장에 찍거나, 큰 도면을 작은 용지 여러 장으로 나눠 찍는다.
// 그림은 「PDF 내보내기」와 같은 도면 PDF(pdfPath)에서 가져오므로 인쇄물과 PDF가 같다.
// 발굴 도면은 축척이 생명이라 한 장 인쇄도 들어가면 실제 크기로 찍고, 키우거나 줄일 때는
// 종이 위 실제 축척을 창과 각 장에 적는다.
class KaPrintDialog : public QDialog {
  Q_OBJECT
public:
  // 「붙였을 때 크기」에서 용지에 맞추는 칸.
  enum Output { OutputDrawing = 0, OutputA0, OutputA1, OutputA2, OutputA3 };
  // 나눌 용지 칸의 순서.
  enum Sheet { SheetA4 = 0, SheetA3 };

  // drawingScale: 도면 지도의 축척 분모(예: 5000). 0 이면 모른다.
  KaPrintDialog(const QString& pdfPath, const QString& title, double drawingScale = 0.0,
                QWidget* parent = nullptr);

  void setTiled(bool tiled);
  void setOutput(Output output);
  // 딱 떨어지는 축척(분모)으로 키우는 칸을 고른다. 그런 칸이 없으면 false.
  bool setOutputScale(double denominator);
  void setSheet(Sheet sheet);
  void setOverlapMm(double mm);
  void setMarks(bool on);
  void setOverview(bool on);
  // 미리보기에서 장을 누른 것과 같다. 뺀 장은 이번 인쇄에서 찍지 않는다.
  void toggleSheet(int index);
  QList<int> chosenSheets() const;  // 이번에 찍을 장(0부터)
  int pageCount() const;            // 안내 장까지 더한 이번 인쇄 쪽수
  bool isTiled() const;
  const TilePlan& plan() const { return m_plan; }
  const TileFit& fit() const { return m_fit; }
  double paperScale() const;  // 종이 위 축척 분모. 모르면 0
  QString summary() const;
  QString warning() const;
  QString printButtonText() const;
  QStringList outputLabels() const;
  // 나눈 장을 path 에 PDF로 쓴다. 프린터 없이 다른 PC나 출력소에서 찍을 때 쓴다.
  bool saveTilesPdf(const QString& path, QString* error, bool* cancelled = nullptr);

private:
  void buildOutputs();
  void rebuild();
  void printNow();
  void saveTilesAs();
  void loadSettings();
  void saveSettings() const;
  bool renderWithProgress(QPagedPaintDevice* device, const QString& verb, QString* error, bool* cancelled);
  bool printerSupports(QPageSize::PageSizeId id) const;
  TilePrint::Options tileOptions() const;
  QString printerName() const;
  QPageSize sheetSize() const;
  QMarginsF marginsFor(const QPageSize& sheet) const;
  double outputFactor() const;
  QString paperScaleText() const;

  QString m_pdfPath;
  QString m_title;
  double m_drawingScale = 0.0;
  QSizeF m_drawingMm;
  QImage m_thumb;
  TilePlan m_plan;
  TileFit m_fit;
  QMarginsF m_margins;  // 고른 프린터·용지의 가장자리
  // 프린터 드라이버 조회는 느려서 프린터·용지마다 한 번만 묻는다.
  mutable QHash<QString, QMarginsF> m_marginCache;
  QSet<int> m_skipped;  // 미리보기에서 뺀 장
  int m_gridRows = 0;
  int m_gridCols = 0;
  QComboBox* m_printer = nullptr;
  QRadioButton* m_modeTiles = nullptr;
  QRadioButton* m_modeFit = nullptr;
  // 나눠 찍기에만 쓰는 칸들. 한 장 인쇄에서는 회색으로 두지 않고 숨긴다.
  QFormLayout* m_form = nullptr;
  QWidget* m_checksRow = nullptr;
  QLabel* m_overlapLabel = nullptr;
  QLabel* m_outputLabel = nullptr;
  QComboBox* m_output = nullptr;
  QComboBox* m_sheet = nullptr;
  QDoubleSpinBox* m_overlap = nullptr;
  QCheckBox* m_marks = nullptr;
  QCheckBox* m_overview = nullptr;
  QLabel* m_summary = nullptr;
  QLabel* m_warning = nullptr;
  KaTilePreview* m_preview = nullptr;
  QLabel* m_pickHint = nullptr;
  QPushButton* m_pickAll = nullptr;
  QPushButton* m_print = nullptr;
  QPushButton* m_savePdf = nullptr;
};
