#pragma once
// Progress window with a cancel button for drawing PDF export and print.
// QGIS renders a layout synchronously, so a click on 취소 during the render is
// honoured right after it: the finished draft is discarded and no file is written.
#include <QPointer>
#include <QString>

class QProgressDialog;
class QWidget;

class KaDrawingPdfProgress {
public:
  KaDrawingPdfProgress(QWidget* parent, const QString& title);
  ~KaDrawingPdfProgress();
  KaDrawingPdfProgress(const KaDrawingPdfProgress&) = delete;
  KaDrawingPdfProgress& operator=(const KaDrawingPdfProgress&) = delete;

  // Shows the step and delivers pending clicks. False once the user cancelled.
  bool step(const QString& text);
  bool cancelled() const;

  // "용지 W×H mm · 300 DPI" plus a wait note for sheets larger than A3.
  static QString renderNote(double paperWidthMm, double paperHeightMm, double dpi);
  // Copies a finished draft over target (atomic replace).
  static bool commit(const QString& draft, const QString& target, QString* error);
  static QString cancelledText();

private:
  QPointer<QProgressDialog> m_dialog;
  bool m_cancelled = false;
};
