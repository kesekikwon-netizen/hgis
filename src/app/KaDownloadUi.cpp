#include "KaDownloadUi.h"
#include "KaWindowGeometry.h"
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QVBoxLayout>

namespace KaDownloadUi {
void configure(QDialog* dialog, QVBoxLayout* layout, const QString& title, const QString& subtitle) {
  dialog->setProperty("kaDownloadWindow", true);
  dialog->setWindowTitle(title);
  dialog->setStyleSheet(QStringLiteral(
      "QDialog[kaDownloadWindow=\"true\"] { background: #eef5fa; }"
      "QLabel { color: #263f54; }"
      "QFrame#downloadHeader { border: 1px solid #256c9e; border-radius: 14px;"
      " background: qlineargradient(x1:0,y1:0,x2:0,y2:1,stop:0 #438fbe,stop:0.48 #2676ad,stop:1 #185c91); }"
      "QLabel#downloadTitle { background: transparent; color: white; font-size: 20px; font-weight: 600; border: none; }"
      "QLabel#downloadSubtitle { background: transparent; color: #f0f8ff; border: none; }"
      "QFrame#downloadStatusCard { background: white; border: 1px solid #cedeea; border-radius: 12px; }"
      "QFrame#downloadStatusCard QLabel { background: transparent; color: #263f54; border: none; }"
      "QProgressBar[downloadProgress=\"true\"] { background: #e5eef6; border: 1px solid #c7d9e8;"
      " border-radius: 7px; min-height: 22px; text-align: center; color: #173b59; }"
      "QProgressBar[downloadProgress=\"true\"]::chunk { border-radius: 6px;"
      " background: qlineargradient(x1:0,y1:0,x2:0,y2:1,stop:0 #9dd2f2,stop:1 #69add8); }"
      "QPushButton { min-height: 30px; padding: 2px 14px; color: #263f54; border: 1px solid #b5cddd;"
      " border-radius: 6px; background: qlineargradient(x1:0,y1:0,x2:0,y2:1,stop:0 white,stop:1 #dbeaf5); }"
      "QPushButton:hover { background: #d9edf9; border-color: #5696c1; }"
      "QPushButton:pressed { background: #bdd9eb; }"
      "QPushButton:disabled { color: #687b89; background: #e7eef3; border-color: #cbd9e3; }"));
  layout->setContentsMargins(20, 20, 20, 20); layout->setSpacing(14);
  auto* header = new QFrame(dialog); header->setObjectName(QStringLiteral("downloadHeader"));
  header->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
  auto* content = new QVBoxLayout(header); content->setContentsMargins(20, 17, 20, 17); content->setSpacing(7);
  auto* name = new QLabel(title, header); name->setObjectName(QStringLiteral("downloadTitle")); name->setWordWrap(true);
  auto* hint = new QLabel(subtitle, header); hint->setObjectName(QStringLiteral("downloadSubtitle")); hint->setWordWrap(true);
  content->addWidget(name); content->addWidget(hint); layout->addWidget(header);
}
QFrame* statusCard(QWidget* parent) {
  auto* card = new QFrame(parent); card->setObjectName(QStringLiteral("downloadStatusCard"));
  auto* layout = new QVBoxLayout(card); layout->setContentsMargins(16, 16, 16, 16); layout->setSpacing(10);
  return card;
}
void styleProgress(QProgressBar* progress) { progress->setProperty("downloadProgress", true); progress->setTextVisible(true); }
}

KaDownloadProgressDialog::KaDownloadProgressDialog(const QString& title, const QString& subtitle, QWidget* parent)
    : QDialog(parent) {
  setObjectName(QStringLiteral("cadastralDownloadProgress")); setModal(false);
  auto* root = new QVBoxLayout(this); KaDownloadUi::configure(this, root, title, subtitle);
  auto* card = KaDownloadUi::statusCard(this); auto* status = qobject_cast<QVBoxLayout*>(card->layout());
  auto* stage = new QLabel(QStringLiteral("자료 준비 및 다운로드"), card);
  auto font = stage->font(); font.setBold(true); stage->setFont(font);
  m_detail = new QLabel(QStringLiteral("조사 범위를 확인하고 있습니다."), card);
  m_detail->setObjectName(QStringLiteral("downloadDetail")); m_detail->setWordWrap(true);
  m_progress = new QProgressBar(card); m_progress->setObjectName(QStringLiteral("downloadProgress")); m_progress->setRange(0, 0);
  KaDownloadUi::styleProgress(m_progress);
  status->addWidget(stage); status->addWidget(m_detail); status->addWidget(m_progress); root->addWidget(card);
  auto* actions = new QHBoxLayout;
  auto* hint = new QLabel(QStringLiteral("다운로드 중에도 지도 작업을 계속할 수 있습니다."), this); hint->setWordWrap(true);
  actions->addWidget(hint, 1);
  auto* cancel = new QPushButton(QStringLiteral("취소"), this); cancel->setObjectName(QStringLiteral("downloadCancel"));
  actions->addWidget(cancel); root->addLayout(actions);
  connect(cancel, &QPushButton::clicked, this, &KaDownloadProgressDialog::reject);
  resize(680, 330); KaWindowGeometry::fit(this);
}
void KaDownloadProgressDialog::setRange(int minimum, int maximum) { m_progress->setRange(minimum, maximum); }
void KaDownloadProgressDialog::setValue(int value) { m_progress->setValue(value); }
void KaDownloadProgressDialog::setLabelText(const QString& text) { m_detail->setText(text); }
void KaDownloadProgressDialog::reject() {
  if (!m_canceled) { m_canceled = true; emit canceled(); }
  QDialog::reject();
}
