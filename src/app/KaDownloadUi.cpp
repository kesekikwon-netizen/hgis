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
  // Colours come from ka-hgis.qss (kaDownloadWindow, downloadHeader, downloadStatusCard,
  // downloadProgress): one flat Strata palette instead of the old sky gradient island.
  layout->setContentsMargins(20, 20, 20, 20); layout->setSpacing(14);
  auto* header = new QFrame(dialog); header->setObjectName(QStringLiteral("downloadHeader"));
  header->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
  auto* content = new QVBoxLayout(header); content->setContentsMargins(20, 17, 20, 17); content->setSpacing(7);
  auto* name = new QLabel(title, header); name->setObjectName(QStringLiteral("downloadTitle")); name->setWordWrap(true);
  auto* hint = new QLabel(subtitle, header); hint->setObjectName(QStringLiteral("downloadSubtitle")); hint->setWordWrap(true);
  header->setAccessibleName(title);
  header->setAccessibleDescription(subtitle);
  content->addWidget(name); content->addWidget(hint); layout->addWidget(header);
}
QFrame* statusCard(QWidget* parent) {
  auto* card = new QFrame(parent); card->setObjectName(QStringLiteral("downloadStatusCard"));
  auto* layout = new QVBoxLayout(card); layout->setContentsMargins(16, 16, 16, 16); layout->setSpacing(10);
  return card;
}
void styleProgress(QProgressBar* progress) {
  progress->setProperty("downloadProgress", true);
  progress->setTextVisible(true);
  if (progress->accessibleName().isEmpty()) progress->setAccessibleName(QStringLiteral("받기 진행률"));
}
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
  KaWindowGeometry::placeDownload(this);
}
void KaDownloadProgressDialog::setRange(int minimum, int maximum) { m_progress->setRange(minimum, maximum); }
void KaDownloadProgressDialog::setValue(int value) { m_progress->setValue(value); }
void KaDownloadProgressDialog::setLabelText(const QString& text) { m_detail->setText(text); }
void KaDownloadProgressDialog::reject() {
  if (!m_canceled) { m_canceled = true; emit canceled(); }
  QDialog::reject();
}
