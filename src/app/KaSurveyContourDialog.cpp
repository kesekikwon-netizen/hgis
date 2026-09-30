#include "KaSurveyContourDialog.h"

#include "core/SurveyContourMath.h"
#include "core/SurveyPointArrange.h"
#include "core/SurveyPointReader.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QToolButton>

#include <algorithm>

KaSurveyContourDialog::KaSurveyContourDialog(const QString& crsAuthId, QWidget* parent)
    : QDialog(parent), m_crsAuthId(crsAuthId.isEmpty() ? QStringLiteral("EPSG:5186") : crsAuthId) {
  setWindowTitle(QStringLiteral("측량 등고선"));
  setMinimumWidth(460);
  auto* form = new QFormLayout(this);
  auto* fileRow = new QHBoxLayout();
  m_file = new QLabel(QStringLiteral("파일을 고르세요"), this);
  m_file->setObjectName(QStringLiteral("contourFile"));
  auto* browse = new QPushButton(QStringLiteral("찾기…"), this);
  fileRow->addWidget(m_file, 1);
  fileRow->addWidget(browse);
  form->addRow(QStringLiteral("좌표 파일"), fileRow);
  m_interval = new QDoubleSpinBox(this);
  m_interval->setObjectName(QStringLiteral("contourInterval"));
  m_interval->setRange(0.1, 100);
  m_interval->setSingleStep(0.1);
  m_interval->setDecimals(1);
  m_interval->setSuffix(QStringLiteral(" m"));
  m_interval->setValue(0.5);
  form->addRow(QStringLiteral("등고선 간격"), m_interval);
  m_swap = new QCheckBox(QStringLiteral("X를 북쪽, Y를 동쪽으로 읽기"), this);
  m_swap->setObjectName(QStringLiteral("contourSwapAxes"));
  m_swap->setToolTip(QStringLiteral("측량 성과표는 보통 X가 북쪽입니다. 파일을 고르면 자동으로 정하고, "
                                    "등고선이 대각선으로 뒤집혀 놓이면 이 칸을 바꾸세요."));
  form->addRow(QStringLiteral("축 순서"), m_swap);
  m_excludeSuspicious = new QCheckBox(QStringLiteral("의심점 빼기"), this);
  m_excludeSuspicious->setObjectName(QStringLiteral("contourExcludeSuspicious"));
  m_excludeSuspicious->setEnabled(false);
  m_excludeSuspicious->setToolTip(QStringLiteral("표고가 0이거나 999 m 이상인 점은 오측일 때가 많습니다. "
                                                 "빼면 삼각망에 넣지 않습니다."));
  form->addRow(QStringLiteral("이상한 점"), m_excludeSuspicious);

  auto* more = new QToolButton(this);
  more->setObjectName(QStringLiteral("contourAdvancedToggle"));
  more->setText(QStringLiteral("고급 설정"));
  more->setCheckable(true);
  more->setArrowType(Qt::RightArrow);
  more->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  auto* advanced = new QWidget(this);
  advanced->setObjectName(QStringLiteral("contourAdvanced"));
  advanced->setVisible(false);
  auto* advancedForm = new QFormLayout(advanced);
  advancedForm->setContentsMargins(0, 0, 0, 0);
  m_fileCrs = new QComboBox(advanced);
  m_fileCrs->setObjectName(QStringLiteral("contourFileCrs"));
  m_fileCrs->addItem(QStringLiteral("작업 좌표계와 같음 (%1)").arg(m_crsAuthId), QString());
  const QList<QPair<QString, QString>> fileCrs = {
      {QStringLiteral("EPSG:5185"), QStringLiteral("서부원점")},
      {QStringLiteral("EPSG:5186"), QStringLiteral("중부원점")},
      {QStringLiteral("EPSG:5187"), QStringLiteral("동부원점")},
      {QStringLiteral("EPSG:5179"), QStringLiteral("UTM-K")},
      {QStringLiteral("EPSG:4326"), QStringLiteral("경위도 WGS84")}};
  for (const auto& crs : fileCrs)
    if (crs.first != m_crsAuthId) m_fileCrs->addItem(crs.first + QLatin1Char(' ') + crs.second, crs.first);
  m_fileCrs->setToolTip(QStringLiteral("측량 파일이 작업 좌표계와 다른 원점으로 되어 있을 때만 고르세요. "
                                       "점을 작업 좌표계로 바꿔 놓습니다."));
  advancedForm->addRow(QStringLiteral("파일 좌표계"), m_fileCrs);
  m_maxEdge = new QDoubleSpinBox(advanced);
  m_maxEdge->setObjectName(QStringLiteral("contourMaxEdge"));
  m_maxEdge->setRange(0, 1000);
  m_maxEdge->setDecimals(1);
  m_maxEdge->setSuffix(QStringLiteral(" m"));
  m_maxEdge->setSpecialValueText(QStringLiteral("쓰지 않음"));
  m_maxEdge->setToolTip(QStringLiteral("변이 이보다 긴 삼각형은 비워 둡니다. "
                                       "트렌치·구덩이 사이 빈 곳을 가로지르는 가짜 사면을 막습니다."));
  advancedForm->addRow(QStringLiteral("최대 삼각형 변"), m_maxEdge);
  m_breaklines = new QCheckBox(QStringLiteral("높이가 있는 DXF 선을 단절선으로 쓰기"), advanced);
  m_breaklines->setObjectName(QStringLiteral("contourBreaklines"));
  m_breaklines->setEnabled(false);
  advancedForm->addRow(QStringLiteral("단절선"), m_breaklines);
  m_cell = new QDoubleSpinBox(advanced);
  m_cell->setObjectName(QStringLiteral("contourCell"));
  m_cell->setRange(0, 5);
  m_cell->setDecimals(2);
  m_cell->setSingleStep(0.05);
  m_cell->setSuffix(QStringLiteral(" m"));
  m_cell->setSpecialValueText(QStringLiteral("자동"));
  m_cell->setToolTip(QStringLiteral("등고선을 뽑는 표고 격자 크기입니다. 자동은 점 간격의 절반입니다."));
  advancedForm->addRow(QStringLiteral("계산 격자"), m_cell);
  form->addRow(more);
  form->addRow(advanced);

  m_preview = new QLabel(QStringLiteral("DXF·엑셀의 점번호, X(북), Y(동), 표고는 자동으로 읽습니다."), this);
  m_preview->setObjectName(QStringLiteral("contourPreview"));
  m_preview->setWordWrap(true);
  form->addRow(m_preview);
  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
  buttons->button(QDialogButtonBox::Ok)->setText(QStringLiteral("등고선 만들기"));
  buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("취소"));
  form->addRow(buttons);
  connect(browse, &QPushButton::clicked, this, &KaSurveyContourDialog::browse);
  connect(more, &QToolButton::toggled, this, [more, advanced](bool open) {
    advanced->setVisible(open);
    more->setArrowType(open ? Qt::DownArrow : Qt::RightArrow);
  });
  connect(m_interval, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this] {
    m_intervalTouched = true;
    showPreview();
  });
  connect(m_swap, &QCheckBox::toggled, this, &KaSurveyContourDialog::rearrange);
  connect(m_fileCrs, qOverload<int>(&QComboBox::currentIndexChanged), this, &KaSurveyContourDialog::rearrange);
  connect(m_excludeSuspicious, &QCheckBox::toggled, this, &KaSurveyContourDialog::showPreview);
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
  connect(buttons, &QDialogButtonBox::accepted, this, [this] {
    const QString why = SurveyContourBuilder::rejection(usedPoints());
    if (m_path.isEmpty() || !m_report.fatal.isEmpty() || !why.isEmpty()) {
      m_preview->setText(m_path.isEmpty() ? QStringLiteral("좌표 파일을 고르세요.")
                                          : (m_report.fatal.isEmpty() ? why : m_report.fatal));
      return;
    }
    accept();
  });
}

void KaSurveyContourDialog::setReferenceExtent(const QgsRectangle& extent) {
  m_reference = extent;
  showPreview();
}

void KaSurveyContourDialog::browse() {
  const QString path = QFileDialog::getOpenFileName(
      this, QStringLiteral("측량 좌표"), QString(),
      QStringLiteral("측량 좌표 (*.dxf *.xlsx *.xls *.csv)"));
  if (!path.isEmpty()) loadFile(path);
}

void KaSurveyContourDialog::loadFile(const QString& path) {
  m_path = path;
  m_file->setText(QFileInfo(path).fileName());
  reread();
}

void KaSurveyContourDialog::reread() {
  if (m_path.isEmpty()) return;
  SurveyReadOptions options;
  options.path = m_path;
  options.crsAuthId = m_crsAuthId;
  // The only file read: the contour task reuses these points instead of reading again.
  m_fileOrder = SurveyPointReader::read(options);
  {
    const QSignalBlocker block(m_swap);
    m_swap->setChecked(m_fileOrder.swapSuggested);
  }
  if (!m_intervalTouched && m_fileOrder.maxCm > m_fileOrder.minCm && m_fileOrder.maxCm - m_fileOrder.minCm < 200) {
    const QSignalBlocker block(m_interval);
    m_interval->setValue(0.1);
  }
  rearrange();
}

void KaSurveyContourDialog::rearrange() {
  if (m_path.isEmpty()) return;
  SurveyArrangeOptions options;
  options.swapAxes = m_swap->isChecked();
  options.sourceCrsAuthId = m_fileCrs->currentData().toString();
  options.targetCrsAuthId = m_crsAuthId;
  m_report = SurveyPointArrange::arrange(m_fileOrder, options);
  // Only when points fall outside Korea is the other axis order tried, so a clean file is arranged once.
  m_otherAxisFits = false;
  if (m_report.outsideCount > 0) {
    options.swapAxes = !options.swapAxes;
    const SurveyReadReport other = SurveyPointArrange::arrange(m_fileOrder, options);
    m_otherAxisFits = other.fatal.isEmpty() && !other.points.isEmpty() && other.outsideCount == 0;
  }
  {
    const QSignalBlocker block(m_excludeSuspicious);
    const int suspicious = m_report.suspiciousCount;
    m_excludeSuspicious->setEnabled(suspicious > 0);
    m_excludeSuspicious->setText(suspicious > 0 ? QStringLiteral("의심점 %1개 빼기").arg(suspicious)
                                                : QStringLiteral("의심점 빼기"));
    if (suspicious == 0) m_excludeSuspicious->setChecked(false);
  }
  const int lines = m_report.breaklines.size();
  m_breaklines->setEnabled(lines > 0);
  m_breaklines->setText(lines > 0 ? QStringLiteral("높이가 있는 DXF 선 %1개를 단절선으로 쓰기").arg(lines)
                                  : QStringLiteral("높이가 있는 DXF 선을 단절선으로 쓰기"));
  if (lines == 0) m_breaklines->setChecked(false);
  showPreview();
}

QVector<SurveyPoint> KaSurveyContourDialog::usedPoints() const {
  if (!m_excludeSuspicious->isChecked()) return m_report.points;
  QVector<SurveyPoint> kept;
  for (const SurveyPoint& point : m_report.points)
    if (!point.suspicious) kept.push_back(point);
  return kept;
}

void KaSurveyContourDialog::showPreview() {
  if (m_path.isEmpty()) return;
  if (!m_report.fatal.isEmpty()) {
    m_preview->setText(m_report.fatal);
    return;
  }
  const QVector<SurveyPoint> used = usedPoints();
  int minCm = 0, maxCm = 0;
  for (int i = 0; i < used.size(); ++i) {
    const int cm = surveyMetersToCm(used[i].z);
    minCm = i == 0 ? cm : std::min(minCm, cm);
    maxCm = i == 0 ? cm : std::max(maxCm, cm);
  }
  const int intervalCm = qRound(m_interval->value() * 100.0);
  const int lines = intervalCm > 0 ? (maxCm - minCm) / intervalCm : 0;
  QStringList text;
  text << QStringLiteral("점 %1개, 표고 %2–%3 m. 등고선 약 %4줄.")
              .arg(used.size())
              .arg(surveyFormatMeters(minCm, 1), surveyFormatMeters(maxCm, 1))
              .arg(lines);
  text += SurveyPointArrange::notes(m_report, m_reference, m_otherAxisFits);
  if (lines > 500) text << QStringLiteral("줄이 많습니다. 간격을 넓히세요.");
  m_preview->setText(text.join(QLatin1Char('\n')));
  m_preview->setToolTip(m_report.issues.join(QLatin1Char('\n')));
}

SurveyContourJob KaSurveyContourDialog::job() const {
  SurveyContourJob job;
  job.read.path = m_path;
  job.read.swapAxes = m_swap->isChecked();
  job.read.crsAuthId = m_crsAuthId;
  job.read.sourceCrsAuthId = m_fileCrs->currentData().toString();
  job.points = m_report.points;
  if (m_breaklines->isChecked()) job.breaklines = m_report.breaklines;
  job.intervalCm = qRound(m_interval->value() * 100.0);
  job.baseCm = 0;
  job.indexEvery = 5;
  job.cellSizeM = m_cell->value();
  job.maxEdgeM = m_maxEdge->value();
  job.colorBands = true;
  job.bandIntervalCm = 0;  // chosen from the heights actually used, after 의심점 빼기
  job.excludeSuspicious = m_excludeSuspicious->isChecked();
  job.groupTitle = QStringLiteral("측량 등고선 · ") + QFileInfo(m_path).completeBaseName();
  return job;
}
