#include "KaSubmitDialog.h"

#include <QHBoxLayout>
#include <QHash>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <algorithm>

namespace {
constexpr int kMaxTargetRows = 30;
constexpr int kTargetRole = Qt::UserRole + 1;
}  // namespace

KaSubmitDialog::KaSubmitDialog(QWidget* parent) : QDialog(parent) {
  setObjectName(QStringLiteral("submitReviewDialog"));
  setWindowTitle(QStringLiteral("검수·제출"));
  resize(820, 580);
  auto* layout = new QVBoxLayout(this);

  auto* intro = new QLabel(
      QStringLiteral("제출 꾸러미 = EPSG:5179 SHP + 조사도면.pdf + MANIFEST.sha256 입니다. "
                     "검수 오류가 한 건이라도 있으면 만들 수 없습니다.\n"
                     "필수 도면 5종은 조사구역도·유적위치도·유구배치도·개별유구실측도·층위도입니다. "
                     "유적위치도·유구배치도·개별유구실측도는 「도면」에서 조판한 용지로 확인합니다."),
      this);
  intro->setWordWrap(true);
  layout->addWidget(intro);

  m_summary = new QLabel(this);
  m_summary->setObjectName(QStringLiteral("submitReviewSummary"));
  QFont bold = m_summary->font();
  bold.setBold(true);
  m_summary->setFont(bold);
  layout->addWidget(m_summary);

  m_tree = new QTreeWidget(this);
  m_tree->setObjectName(QStringLiteral("submitReviewTree"));
  m_tree->setColumnCount(3);
  m_tree->setHeaderLabels({QStringLiteral("항목"), QStringLiteral("고치는 법"), QString()});
  m_tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
  m_tree->header()->setSectionResizeMode(1, QHeaderView::Stretch);
  m_tree->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
  m_tree->setWordWrap(true);
  m_tree->setUniformRowHeights(false);
  // Double-clicking one offending shape moves the map to that shape only.
  connect(m_tree, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem* item, int) {
    // Only target rows (group > result > target) carry a target index.
    if (!item || !item->parent() || !item->parent()->parent() || !item->data(0, kTargetRole).isValid())
      return;
    const int resultIndex = item->parent()->data(0, kTargetRole).toInt();
    const int targetIndex = item->data(0, kTargetRole).toInt();
    if (resultIndex < 0 || resultIndex >= m_results.size()) return;
    CheckResult one = m_results.at(resultIndex);
    if (targetIndex < 0 || targetIndex >= one.targets.size()) return;
    one.targets = {one.targets.at(targetIndex)};
    choose(Choice::GoToTargets, one);
  });
  layout->addWidget(m_tree, 1);

  m_blockReason = new QLabel(this);
  m_blockReason->setObjectName(QStringLiteral("submitBlockReason"));
  m_blockReason->setWordWrap(true);
  layout->addWidget(m_blockReason);

  auto* buttons = new QHBoxLayout;
  auto* recheck = new QPushButton(QStringLiteral("다시 검수"), this);
  recheck->setObjectName(QStringLiteral("btnSubmitRecheck"));
  connect(recheck, &QPushButton::clicked, this, &KaSubmitDialog::recheckRequested);
  m_convert = new QPushButton(QStringLiteral("레이어 5179 변환…"), this);
  m_convert->setObjectName(QStringLiteral("btnConvertLayer5179"));
  connect(m_convert, &QPushButton::clicked, this, [this]() { choose(Choice::ConvertLayer); });
  m_package = new QPushButton(QStringLiteral("제출 꾸러미 만들기…"), this);
  m_package->setObjectName(QStringLiteral("btnSubmitPackage"));
  m_package->setDefault(true);
  connect(m_package, &QPushButton::clicked, this, [this]() {
    if (packageAllowed()) choose(Choice::MakePackage);
  });
  auto* close = new QPushButton(QStringLiteral("닫기"), this);
  connect(close, &QPushButton::clicked, this, &QDialog::reject);
  buttons->addWidget(recheck);
  buttons->addWidget(m_convert);
  buttons->addStretch(1);
  buttons->addWidget(m_package);
  buttons->addWidget(close);
  layout->addLayout(buttons);
  setConvertLayerName({});
}

QString KaSubmitDialog::actionLabel(const QString& action) {
  static const QHash<QString, QString> labels = {
      {QStringLiteral("open_layout"), QStringLiteral("도면 열기")},
      {QStringLiteral("open_section"), QStringLiteral("단면도 열기")},
      {QStringLiteral("draw_survey_area"), QStringLiteral("조사구역 그리기")},
      {QStringLiteral("draw_feature"), QStringLiteral("유구 그리기")},
      {QStringLiteral("add_control_point"), QStringLiteral("기준점 찍기")},
      {QStringLiteral("edit_attributes"), QStringLiteral("속성 고치기")},
      {QStringLiteral("vertex_edit"), QStringLiteral("꼭짓점 편집")},
      {QStringLiteral("select_tool"), QStringLiteral("도형 선택")},
      {QStringLiteral("set_work_crs"), QStringLiteral("좌표계 고르기")},
  };
  return labels.value(action);
}

int KaSubmitDialog::errorCount() const { return ChecklistEngine::failedCount(m_results, QStringLiteral("error")); }
int KaSubmitDialog::warnCount() const { return ChecklistEngine::failedCount(m_results, QStringLiteral("warn")); }

bool KaSubmitDialog::packageAllowed() const {
  // Missing rules must block like an error; an empty result list is not "OK".
  return m_rulesLoaded && !m_results.isEmpty() && errorCount() == 0;
}

void KaSubmitDialog::setResults(const QVector<CheckResult>& results, bool rulesLoaded, const QString& rulesHint) {
  m_results = results;
  m_rulesLoaded = rulesLoaded;
  m_rulesHint = rulesHint;
  rebuild();
}

void KaSubmitDialog::setConvertLayerName(const QString& layerName) {
  m_convert->setEnabled(!layerName.isEmpty());
  m_convert->setToolTip(layerName.isEmpty()
                            ? QStringLiteral("지도 목록에서 레이어 하나를 고르면 그 레이어만 EPSG:5179 SHP 파일로 바꿉니다.")
                            : QStringLiteral("「%1」만 EPSG:5179 SHP 파일로 저장합니다. 검수·PDF·MANIFEST는 없습니다.")
                                  .arg(layerName));
}

void KaSubmitDialog::choose(Choice choice, const CheckResult& result) {
  m_choice = choice;
  m_chosen = result;
  accept();
}

void KaSubmitDialog::addResultRow(QTreeWidgetItem* group, int index) {
  const CheckResult& r = m_results.at(index);
  auto* row = new QTreeWidgetItem(group);
  row->setData(0, kTargetRole, index);
  QString text = r.messageKo;
  if (!r.detailKo.isEmpty()) text += QStringLiteral("\n%1").arg(r.detailKo);
  row->setText(0, text);
  // The basis (guideline or program rule) stays visible next to the fix.
  row->setText(1, r.passed ? QString()
                           : (r.basisKo.isEmpty() ? r.fixKo : QStringLiteral("%1\n근거: %2").arg(r.fixKo, r.basisKo)));
  const QString tip = r.basisKo.isEmpty() ? r.id : QStringLiteral("%1\n근거: %2").arg(r.id, r.basisKo);
  row->setToolTip(0, tip);
  row->setToolTip(1, r.fixKo);
  if (r.passed) return;
  for (int i = 0; i < r.targets.size() && i < kMaxTargetRows; ++i) {
    const CheckTarget& t = r.targets.at(i);
    auto* child = new QTreeWidgetItem(row);
    child->setText(0, QStringLiteral("%1 · %2").arg(t.layerName, t.label));
    child->setToolTip(0, QStringLiteral("두 번 누르면 지도에서 이 도형으로 이동합니다."));
    child->setData(0, kTargetRole, i);
  }
  if (r.targets.size() > kMaxTargetRows) {
    auto* more = new QTreeWidgetItem(row);
    more->setText(0, QStringLiteral("…외 %1건").arg(r.targets.size() - kMaxTargetRows));
  }
  bool hasLayerTargets = false;
  for (const CheckTarget& t : r.targets) hasLayerTargets = hasLayerTargets || !t.layerId.isEmpty();
  const QString toolLabel = actionLabel(r.action);
  if (!hasLayerTargets && toolLabel.isEmpty()) return;
  auto* cell = new QWidget(m_tree);
  auto* cellLayout = new QHBoxLayout(cell);
  cellLayout->setContentsMargins(0, 0, 0, 0);
  if (hasLayerTargets) {
    auto* go = new QPushButton(QStringLiteral("위치 보기"), cell);
    go->setObjectName(QStringLiteral("btnGoTo_%1").arg(r.id));
    connect(go, &QPushButton::clicked, this, [this, r]() { choose(Choice::GoToTargets, r); });
    cellLayout->addWidget(go);
  }
  if (!toolLabel.isEmpty()) {
    auto* tool = new QPushButton(toolLabel, cell);
    tool->setObjectName(QStringLiteral("btnAction_%1").arg(r.id));
    connect(tool, &QPushButton::clicked, this, [this, r]() { choose(Choice::RunAction, r); });
    cellLayout->addWidget(tool);
  }
  m_tree->setItemWidget(row, 2, cell);
}

void KaSubmitDialog::rebuild() {
  m_tree->clear();
  const int errors = errorCount();
  const int warns = warnCount();
  const int passed = int(std::count_if(m_results.cbegin(), m_results.cend(),
                                       [](const CheckResult& r) { return r.passed; }));
  auto* errorGroup = new QTreeWidgetItem(m_tree, {QStringLiteral("막는 항목(오류) %1건").arg(errors)});
  auto* warnGroup = new QTreeWidgetItem(m_tree, {QStringLiteral("확인할 항목(주의) %1건").arg(warns)});
  auto* passGroup = new QTreeWidgetItem(m_tree, {QStringLiteral("통과 %1건").arg(passed)});
  for (int i = 0; i < m_results.size(); ++i) {
    const CheckResult& r = m_results.at(i);
    if (r.passed) addResultRow(passGroup, i);
    else if (r.severity == QLatin1String("error")) addResultRow(errorGroup, i);
    else addResultRow(warnGroup, i);
  }
  errorGroup->setExpanded(true);
  warnGroup->setExpanded(true);
  passGroup->setExpanded(false);
  for (QTreeWidgetItem* group : {errorGroup, warnGroup, passGroup}) {
    for (int i = 0; i < group->childCount(); ++i) group->child(i)->setExpanded(false);
  }

  m_summary->setText(QStringLiteral("오류 %1건 · 주의 %2건 · 통과 %3건").arg(errors).arg(warns).arg(passed));
  if (!m_rulesLoaded || m_results.isEmpty()) {
    m_blockReason->setText(
        QStringLiteral("검수 규칙 파일을 찾지 못해 제출 꾸러미를 만들 수 없습니다. 프로그램 폴더의 "
                       "data\\rules\\drawing_checklist.v1.json 이 있는지 확인하세요.%1")
            .arg(m_rulesHint.isEmpty() ? QString() : QStringLiteral("\n찾아본 곳: %1").arg(m_rulesHint)));
  } else if (errors > 0) {
    m_blockReason->setText(QStringLiteral("오류 %1건을 고쳐야 제출 꾸러미를 만들 수 있습니다. "
                                          "「위치 보기」나 옆 단추로 고칠 곳을 여세요.").arg(errors));
  } else {
    m_blockReason->setText(warns > 0 ? QStringLiteral("오류가 없습니다. 주의 항목은 확인한 뒤 진행할 수 있습니다.")
                                     : QStringLiteral("오류와 주의 항목이 없습니다."));
  }
  m_package->setEnabled(packageAllowed());
  m_package->setToolTip(packageAllowed()
                            ? QStringLiteral("검수 결과와 함께 5179 SHP·조사도면.pdf·MANIFEST를 새 폴더에 만듭니다.")
                            : m_blockReason->text());
}
