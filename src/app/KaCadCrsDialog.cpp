#include "KaCadCrsDialog.h"

#include <QCheckBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

namespace {

std::function<std::optional<int>(const CadCrsResult&)>& testChooser() {
  static std::function<std::optional<int>(const CadCrsResult&)> chooser;
  return chooser;
}

bool& testRemember() {
  static bool remember = false;
  return remember;
}

}  // namespace

KaCadCrsDialog::KaCadCrsDialog(const CadCrsResult& guess, QWidget* parent) : QDialog(parent) {
  setWindowTitle(QStringLiteral("도면 좌표계 고르기"));
  auto* layout = new QVBoxLayout(this);
  auto* hint = new QLabel(QStringLiteral("도면의 좌표계를 고르세요. 지역 이름이 조사 지역과 맞는 줄을 고르면 됩니다."), this);
  hint->setWordWrap(true);
  layout->addWidget(hint);

  m_list = new QListWidget(this);
  m_list->setObjectName(QStringLiteral("cadCrsList"));
  for (const CadCrsCandidate& candidate : guess.candidates) m_list->addItem(CadCrsGuess::describe(candidate));
  if (m_list->count() > 0) m_list->setCurrentRow(0);
  layout->addWidget(m_list);

  // 창은 그대로 두고, 고른 좌표계가 맞으면 그 도면(이름만 다른 복사본도)은 다음부터 묻지 않게 끌 수 있다.
  m_remember = new QCheckBox(QStringLiteral("맞으면 켜기: 이 도면은 다음부터 묻지 않고 고른 대로 올리기"), this);
  m_remember->setObjectName(QStringLiteral("cadCrsRemember"));
  layout->addWidget(m_remember);

  auto* acceptButton = new QPushButton(QStringLiteral("이 좌표계로 불러오기"), this);
  acceptButton->setObjectName(QStringLiteral("cadCrsAccept"));
  acceptButton->setDefault(true);
  auto* noCrsButton = new QPushButton(QStringLiteral("좌표 없는 도면으로 — 직접 맞추기"), this);
  noCrsButton->setObjectName(QStringLiteral("cadCrsNoCrs"));
  auto* cancelButton = new QPushButton(QStringLiteral("취소"), this);
  cancelButton->setObjectName(QStringLiteral("cadCrsCancel"));
  auto* buttons = new QHBoxLayout();
  buttons->addWidget(acceptButton);
  buttons->addWidget(noCrsButton);
  buttons->addStretch();
  buttons->addWidget(cancelButton);
  layout->addLayout(buttons);

  connect(acceptButton, &QPushButton::clicked, this, [this] {
    if (m_list->currentRow() < 0) return;
    m_outcome = m_list->currentRow();
    QDialog::accept();
  });
  connect(noCrsButton, &QPushButton::clicked, this, [this] {
    m_outcome = -1;
    QDialog::accept();
  });
  connect(cancelButton, &QPushButton::clicked, this, [this] {
    m_outcome.reset();
    QDialog::reject();
  });
}

bool KaCadCrsDialog::remember() const { return m_remember->isChecked(); }

std::optional<int> KaCadCrsDialog::choose(QWidget* parent, const CadCrsResult& guess, bool* remember) {
  if (testChooser()) {
    if (remember) *remember = testRemember();
    return testChooser()(guess);
  }
  KaCadCrsDialog dialog(guess, parent);
  dialog.exec();
  if (remember) *remember = dialog.remember();
  return dialog.outcome();
}

void KaCadCrsDialog::setChooserForTests(std::function<std::optional<int>(const CadCrsResult&)> chooser,
                                        bool remember) {
  testChooser() = std::move(chooser);
  testRemember() = remember;
}
