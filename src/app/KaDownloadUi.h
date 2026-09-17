#pragma once
#include <QDialog>
#include <QString>
class QFrame;
class QLabel;
class QProgressBar;
class QVBoxLayout;
namespace KaDownloadUi {
void configure(QDialog* dialog, QVBoxLayout* layout, const QString& title, const QString& subtitle);
QFrame* statusCard(QWidget* parent);
void styleProgress(QProgressBar* progress);
}
class KaDownloadProgressDialog : public QDialog {
  Q_OBJECT
public:
  KaDownloadProgressDialog(const QString& title, const QString& subtitle, QWidget* parent = nullptr);
  void setRange(int minimum, int maximum);
  void setValue(int value);
  void setLabelText(const QString& text);
  void reject() override;
signals:
  void canceled();
private:
  QLabel* m_detail = nullptr;
  QProgressBar* m_progress = nullptr;
  bool m_canceled = false;
};
