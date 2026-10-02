#include "KaReferenceStatusDialog.h"
#include "core/ReferenceInventory.h"

#include <QDialogButtonBox>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <qgsproject.h>

KaReferenceStatusDialog::KaReferenceStatusDialog(QgsProject* project, QWidget* parent)
    : QDialog(parent), m_project(project) {
  setObjectName(QStringLiteral("referenceStatusDialog"));
  setWindowTitle(QStringLiteral("자료 준비 상태"));
  setModal(false);
  resize(860, 420);
  auto* layout = new QVBoxLayout(this);
  auto* hint = new QLabel(QStringLiteral(
      "조사에 올린 참조 자료가 현장에서 인터넷 없이 열리는지 확인하는 표입니다. "
      "이 창은 자료를 새로 받거나 지우지 않습니다. 인터넷이 필요한 배경은 레이어를 오른쪽 클릭해 "
      "「오프라인 저장」으로 직접 받아 두세요."), this);
  hint->setWordWrap(true);
  layout->addWidget(hint);
  m_summary = new QLabel(this);
  m_summary->setObjectName(QStringLiteral("referenceStatusSummary"));
  m_summary->setWordWrap(true);
  layout->addWidget(m_summary);
  m_table = new QTreeWidget(this);
  m_table->setObjectName(QStringLiteral("referenceStatusTable"));
  m_table->setRootIsDecorated(false);
  m_table->setSelectionMode(QAbstractItemView::SingleSelection);
  m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
  m_table->setHeaderLabels({QStringLiteral("자료"), QStringLiteral("종류"), QStringLiteral("범위"),
                            QStringLiteral("받은 날짜"), QStringLiteral("크기"), QStringLiteral("현장(오프라인)")});
  m_table->header()->setSectionResizeMode(0, QHeaderView::Stretch);
  for (int column = 1; column < m_table->columnCount(); ++column)
    m_table->header()->setSectionResizeMode(column, QHeaderView::ResizeToContents);
  layout->addWidget(m_table, 1);
  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
  auto* again = buttons->addButton(QStringLiteral("다시 확인"), QDialogButtonBox::ActionRole);
  again->setObjectName(QStringLiteral("referenceStatusRefresh"));
  connect(again, &QPushButton::clicked, this, &KaReferenceStatusDialog::refresh);
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::close);
  layout->addWidget(buttons);
  refresh();
}

void KaReferenceStatusDialog::refresh() {
  m_table->clear();
  const auto items = ReferenceInventory::collect(m_project);
  m_summary->setText(ReferenceInventory::summary(items));
  for (const auto& item : items) {
    QString offline = ReferenceInventory::availabilityLabel(item.availability);
    if (!item.offlineNote.isEmpty()) offline += QStringLiteral(" · ") + item.offlineNote;
    auto* row = new QTreeWidgetItem(m_table, {item.name, item.kind, item.extentText, item.receivedText,
        ReferenceInventory::sizeLabel(item.bytes), offline});
    row->setData(0, Qt::UserRole, item.layerId);
    row->setData(5, Qt::UserRole, static_cast<int>(item.availability));
    row->setToolTip(0, item.location);
    row->setToolTip(5, item.location);
  }
}
