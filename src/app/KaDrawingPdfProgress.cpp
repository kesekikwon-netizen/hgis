#include "KaDrawingPdfProgress.h"

#include <QCoreApplication>
#include <QFile>
#include <QProgressDialog>
#include <QSaveFile>
#include <QWidget>

KaDrawingPdfProgress::KaDrawingPdfProgress(QWidget* parent, const QString& title) {
  m_dialog = new QProgressDialog(parent);
  m_dialog->setObjectName(QStringLiteral("drawingPdfProgress"));
  m_dialog->setWindowTitle(title);
  m_dialog->setWindowModality(Qt::ApplicationModal);
  m_dialog->setRange(0, 0);
  m_dialog->setMinimumDuration(0);
  m_dialog->setAutoClose(false);
  m_dialog->setAutoReset(false);
  m_dialog->setCancelButtonText(QStringLiteral("취소"));
  m_dialog->setLabelText(QStringLiteral("준비하는 중…"));
  m_dialog->setMinimumWidth(380);
  m_dialog->show();
}

KaDrawingPdfProgress::~KaDrawingPdfProgress() {
  if (m_dialog) {
    m_dialog->hide();
    delete m_dialog.data();
  }
}

bool KaDrawingPdfProgress::step(const QString& text) {
  if (m_cancelled) return false;
  if (!m_dialog) return true;
  m_dialog->setLabelText(text);
  if (!m_dialog->isVisible()) m_dialog->show();
  // Paint the step and deliver a queued 취소 click. The dialog is application
  // modal, so the drawing cannot be edited while this runs.
  QCoreApplication::processEvents();
  if (m_dialog && m_dialog->wasCanceled()) m_cancelled = true;
  return !m_cancelled;
}

bool KaDrawingPdfProgress::cancelled() const { return m_cancelled; }

QString KaDrawingPdfProgress::renderNote(double paperWidthMm, double paperHeightMm, double dpi) {
  QString note = QStringLiteral("PDF를 그리는 중… (용지 %1×%2 mm · %3 DPI)")
                     .arg(qRound(paperWidthMm))
                     .arg(qRound(paperHeightMm))
                     .arg(qRound(dpi));
  // A3 is 297 x 420 mm. Larger sheets take noticeably longer at print resolution.
  if (paperWidthMm * paperHeightMm > 297.0 * 420.0 * 1.05)
    note += QStringLiteral("\n큰 용지라 시간이 걸립니다. 취소하면 그리기가 끝난 뒤 파일을 만들지 않습니다.");
  else
    note += QStringLiteral("\n취소하면 그리기가 끝난 뒤 파일을 만들지 않습니다.");
  return note;
}

bool KaDrawingPdfProgress::commit(const QString& draft, const QString& target, QString* error) {
  QFile source(draft);
  QSaveFile destination(target);
  if (!source.open(QIODevice::ReadOnly) || !destination.open(QIODevice::WriteOnly)) {
    if (error) *error = QStringLiteral("PDF 저장 파일을 열지 못했습니다.");
    return false;
  }
  while (!source.atEnd()) {
    const QByteArray bytes = source.read(1024 * 1024);
    if (source.error() != QFileDevice::NoError || destination.write(bytes) != bytes.size()) {
      destination.cancelWriting();
      if (error) *error = QStringLiteral("PDF 파일을 저장하지 못했습니다.");
      return false;
    }
  }
  if (!destination.commit()) {
    if (error) *error = QStringLiteral("PDF 파일을 확정하지 못했습니다.");
    return false;
  }
  return true;
}

QString KaDrawingPdfProgress::cancelledText() {
  return QStringLiteral("PDF 만들기를 취소했습니다. 파일은 만들지 않았습니다.");
}
