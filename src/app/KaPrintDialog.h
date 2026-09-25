#pragma once

#include "core/TilePrint.h"

#include <QDialog>
#include <QHash>
#include <QImage>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QPushButton;
class QRadioButton;

// 도면 인쇄 창. 용지 한 장에 맞춰 찍거나, 큰 도면을 작은 용지 여러 장으로 나눠 찍는다.
// 그림은 「PDF 내보내기」와 같은 도면 PDF(pdfPath)에서 가져오므로 인쇄물과 PDF가 같다.
class KaPrintDialog : public QDialog {
  Q_OBJECT
public:
  // 완성 크기 칸의 순서.
  enum Output { OutputDrawing = 0, OutputA0, OutputA1, OutputA2, OutputA3 };
  // 나눌 용지 칸의 순서.
  enum Sheet { SheetA4 = 0, SheetA3 };

  KaPrintDialog(const QString& pdfPath, const QString& title, QWidget* parent = nullptr);

  void setTiled(bool tiled);
  void setOutput(Output output);
  void setSheet(Sheet sheet);
  void setOverlapMm(double mm);
  void setMarks(bool on);
  bool isTiled() const;
  const TilePlan& plan() const { return m_plan; }
  QString summary() const;
  QString warning() const;
  // 나눈 장을 path 에 PDF로 쓴다. 프린터 없이 다른 PC나 출력소에서 찍을 때 쓴다.
  bool saveTilesPdf(const QString& path, QString* error);

private:
  void rebuild();
  void updatePreview();
  void printNow();
  void saveTilesAs();
  QString printerName() const;
  QPageSize sheetSize() const;
  QMarginsF marginsFor(const QPageSize& sheet) const;
  double fitRatio() const;

  QString m_pdfPath;
  QString m_title;
  QSizeF m_drawingMm;
  QImage m_thumb;
  TilePlan m_plan;
  QMarginsF m_margins;  // 고른 프린터·용지의 가장자리
  // 프린터 드라이버 조회는 느려서 프린터·용지마다 한 번만 묻는다.
  mutable QHash<QString, QMarginsF> m_marginCache;
  QComboBox* m_printer = nullptr;
  QRadioButton* m_modeTiles = nullptr;
  QRadioButton* m_modeFit = nullptr;
  QComboBox* m_output = nullptr;
  QComboBox* m_sheet = nullptr;
  QDoubleSpinBox* m_overlap = nullptr;
  QCheckBox* m_marks = nullptr;
  QLabel* m_summary = nullptr;
  QLabel* m_warning = nullptr;
  QLabel* m_preview = nullptr;
  QPushButton* m_print = nullptr;
  QPushButton* m_savePdf = nullptr;
};
