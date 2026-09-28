#include "KaSurveyContourDialog.h"

#include "core/SurveyContourMath.h"
#include "core/SurveyPointReader.h"

#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>

KaSurveyContourDialog::KaSurveyContourDialog(const QString& crsAuthId, QWidget* parent)
    : QDialog(parent), m_crsAuthId(crsAuthId.isEmpty() ? QStringLiteral("EPSG:5186") : crsAuthId) {
  setWindowTitle(QStringLiteral("측량 등고선"));
  setMinimumWidth(420);
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
  m_preview = new QLabel(QStringLiteral("DXF·엑셀의 점번호, X(북), Y(동), 표고는 자동으로 읽습니다."), this);
  m_preview->setWordWrap(true);
  form->addRow(m_preview);
  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
  buttons->button(QDialogButtonBox::Ok)->setText(QStringLiteral("등고선 만들기"));
  buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("취소"));
  form->addRow(buttons);
  connect(browse, &QPushButton::clicked, this, &KaSurveyContourDialog::browse);
  connect(m_interval, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this] {
    m_intervalTouched = true;
    showPreview();
  });
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
  connect(buttons, &QDialogButtonBox::accepted, this, [this] {
    const QString why = SurveyContourBuilder::rejection(m_report.points);
    if (m_path.isEmpty() || !m_report.fatal.isEmpty() || !why.isEmpty()) {
      m_preview->setText(m_path.isEmpty() ? QStringLiteral("좌표 파일을 고르세요.")
                                          : (m_report.fatal.isEmpty() ? why : m_report.fatal));
      return;
    }
    accept();
  });
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
  m_report = SurveyPointReader::read(options);
  if (m_report.swapSuggested) {
    options.swapAxes = true;
    m_report = SurveyPointReader::read(options);
    m_report.swapSuggested = true;
  }
  if (!m_intervalTouched && m_report.maxCm > m_report.minCm && m_report.maxCm - m_report.minCm < 200) {
    const QSignalBlocker block(m_interval);
    m_interval->setValue(0.1);
  }
  showPreview();
}

void KaSurveyContourDialog::showPreview() {
  if (m_path.isEmpty()) return;
  if (!m_report.fatal.isEmpty()) {
    m_preview->setText(m_report.fatal);
    return;
  }
  const int intervalCm = qRound(m_interval->value() * 100.0);
  const int lines = intervalCm > 0 ? (m_report.maxCm - m_report.minCm) / intervalCm : 0;
  QStringList text;
  text << QStringLiteral("점 %1개, 표고 %2–%3 m. 등고선 약 %4줄.")
              .arg(m_report.points.size())
              .arg(surveyFormatMeters(m_report.minCm, 1), surveyFormatMeters(m_report.maxCm, 1))
              .arg(lines);
  if (m_report.swapSuggested) text << QStringLiteral("X는 북쪽, Y는 동쪽으로 읽었습니다.");
  if (m_report.skipped) text << QStringLiteral("읽지 못한 행 %1개.").arg(m_report.skipped);
  if (lines > 500) text << QStringLiteral("줄이 많습니다. 간격을 넓히세요.");
  m_preview->setText(text.join(QLatin1Char('\n')));
}

SurveyContourJob KaSurveyContourDialog::job() const {
  SurveyContourJob job;
  job.read.path = m_path;
  job.read.swapAxes = m_report.swapSuggested;
  job.read.crsAuthId = m_crsAuthId;
  job.intervalCm = qRound(m_interval->value() * 100.0);
  job.baseCm = 0;
  job.indexEvery = 5;
  job.cellSizeM = 0;
  job.colorBands = true;
  job.bandIntervalCm = SurveyContourBuilder::autoBandIntervalCm(m_report.minCm, m_report.maxCm);
  job.idw = false;
  job.excludeSuspicious = false;
  job.groupTitle = QStringLiteral("측량 등고선 · ") + QFileInfo(m_path).completeBaseName();
  return job;
}
