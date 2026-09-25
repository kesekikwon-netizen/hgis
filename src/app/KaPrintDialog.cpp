#include "KaPrintDialog.h"

#include <QApplication>
#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPageLayout>
#include <QPainter>
#include <QPdfDocument>
#include <QPdfWriter>
#include <QPixmap>
#include <QPrinter>
#include <QPrinterInfo>
#include <QPushButton>
#include <QRadioButton>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace {
constexpr double kPrintDpi = 300.0;      // 그림 해상도 상한. 「PDF 내보내기」와 같다
constexpr double kDefaultMarginMm = 5.0;  // 프린터를 모를 때의 가장자리
constexpr double kLabelBandMm = 6.0;      // 장 번호·안내 띠

QPageSize outputPage(KaPrintDialog::Output output) {
  switch (output) {
    case KaPrintDialog::OutputA0: return QPageSize(QPageSize::A0);
    case KaPrintDialog::OutputA1: return QPageSize(QPageSize::A1);
    case KaPrintDialog::OutputA2: return QPageSize(QPageSize::A2);
    case KaPrintDialog::OutputA3: return QPageSize(QPageSize::A3);
    case KaPrintDialog::OutputDrawing: break;
  }
  return {};
}

QString percent(double ratio) {
  return QString::number(int(std::lround(ratio * 100.0)));
}

// 배율 때문에 숫자 축척이 틀어지는지. 막대 축척은 도면과 같이 커지고 작아지므로 맞다.
QString scaleWarning(double ratio) {
  if (std::abs(ratio - 1.0) < 0.005) return {};
  return QStringLiteral("도면을 %1%로 %2 찍습니다. 숫자로 적은 축척(예: 1:5,000)은 실제와 달라지고, "
                        "막대 축척은 그대로 맞습니다.")
      .arg(percent(ratio), ratio > 1.0 ? QStringLiteral("키워") : QStringLiteral("줄여"));
}
}  // namespace

KaPrintDialog::KaPrintDialog(const QString& pdfPath, const QString& title, QWidget* parent)
    : QDialog(parent), m_pdfPath(pdfPath), m_title(title) {
  setObjectName(QStringLiteral("printDialog"));
  setWindowTitle(QStringLiteral("도면 인쇄"));
  QString readError;
  m_drawingMm = TilePrint::pdfPageSizeMm(pdfPath, &readError);
  {
    QPdfDocument doc;
    if (doc.load(pdfPath) == QPdfDocument::Error::None && doc.pageCount() > 0 && !m_drawingMm.isEmpty()) {
      const double k = 420.0 / std::max(m_drawingMm.width(), m_drawingMm.height());
      m_thumb = doc.render(0, QSize(int(m_drawingMm.width() * k), int(m_drawingMm.height() * k)));
    }
  }

  auto* root = new QVBoxLayout(this);
  auto* form = new QFormLayout;
  m_printer = new QComboBox(this);
  m_printer->setObjectName(QStringLiteral("printPrinter"));
  const QStringList printers = QPrinterInfo::availablePrinterNames();
  for (const QString& name : printers) m_printer->addItem(name, name);
  if (printers.isEmpty()) {
    m_printer->addItem(QStringLiteral("(설치된 프린터 없음)"), QString());
    m_printer->setEnabled(false);
  } else {
    const int preferred = m_printer->findData(QPrinterInfo::defaultPrinterName());
    if (preferred >= 0) m_printer->setCurrentIndex(preferred);
  }
  form->addRow(QStringLiteral("프린터"), m_printer);

  m_modeTiles = new QRadioButton(QStringLiteral("여러 장으로 나눠 크게 인쇄 (붙여서 큰 도면 만들기)"), this);
  m_modeFit = new QRadioButton(QStringLiteral("용지 한 장에 맞춰 인쇄"), this);
  m_modeTiles->setObjectName(QStringLiteral("printModeTiles"));
  m_modeFit->setObjectName(QStringLiteral("printModeFit"));
  auto* modes = new QButtonGroup(this);
  modes->addButton(m_modeTiles);
  modes->addButton(m_modeFit);
  auto* modeBox = new QVBoxLayout;
  modeBox->addWidget(m_modeTiles);
  modeBox->addWidget(m_modeFit);
  form->addRow(QStringLiteral("방법"), modeBox);

  m_output = new QComboBox(this);
  m_output->setObjectName(QStringLiteral("printOutput"));
  m_output->addItem(QStringLiteral("도면 크기 그대로 (%1×%2 mm)")
                        .arg(qRound(m_drawingMm.width()))
                        .arg(qRound(m_drawingMm.height())));
  for (const QString& name : {QStringLiteral("A0"), QStringLiteral("A1"), QStringLiteral("A2"), QStringLiteral("A3")})
    m_output->addItem(QStringLiteral("%1 크기로 맞춤").arg(name));
  form->addRow(QStringLiteral("완성 크기"), m_output);

  m_sheet = new QComboBox(this);
  m_sheet->setObjectName(QStringLiteral("printSheet"));
  m_sheet->addItem(QStringLiteral("A4"));
  m_sheet->addItem(QStringLiteral("A3"));
  bool a3 = false;
  if (!printers.isEmpty()) {
    for (const QPageSize& size : QPrinterInfo::printerInfo(printerName()).supportedPageSizes())
      a3 |= size.id() == QPageSize::A3;
  }
  m_sheet->setCurrentIndex(a3 ? SheetA3 : SheetA4);
  form->addRow(QStringLiteral("용지"), m_sheet);

  m_overlap = new QDoubleSpinBox(this);
  m_overlap->setObjectName(QStringLiteral("printOverlap"));
  m_overlap->setRange(0.0, 30.0);
  m_overlap->setDecimals(0);
  m_overlap->setValue(10.0);
  m_overlap->setSuffix(QStringLiteral(" mm"));
  m_overlap->setToolTip(QStringLiteral("이웃 장과 같은 그림이 겹치는 폭입니다. 붙일 때 맞추기 쉽습니다."));
  form->addRow(QStringLiteral("겹침"), m_overlap);

  m_marks = new QCheckBox(QStringLiteral("자르는 점선과 장 번호 넣기"), this);
  m_marks->setObjectName(QStringLiteral("printMarks"));
  m_marks->setChecked(true);
  form->addRow(QString(), m_marks);
  root->addLayout(form);

  m_summary = new QLabel(this);
  m_summary->setObjectName(QStringLiteral("printSummary"));
  m_summary->setWordWrap(true);
  root->addWidget(m_summary);
  m_warning = new QLabel(this);
  m_warning->setObjectName(QStringLiteral("printWarning"));
  m_warning->setWordWrap(true);
  m_warning->setStyleSheet(QStringLiteral("color: #b45309;"));
  root->addWidget(m_warning);
  m_preview = new QLabel(this);
  m_preview->setObjectName(QStringLiteral("printPreview"));
  m_preview->setAlignment(Qt::AlignCenter);
  m_preview->setMinimumSize(320, 320);
  root->addWidget(m_preview, 1);
  auto* help = new QLabel(QStringLiteral("여러 장으로 찍었다면 오른쪽·아래 점선을 따라 자르고, 옆 장 그림의 "
                                         "시작선(모서리 표시)에 맞춰 겹쳐 붙이세요."),
                          this);
  help->setWordWrap(true);
  root->addWidget(help);

  auto* buttons = new QHBoxLayout;
  m_savePdf = new QPushButton(QStringLiteral("나눈 장을 PDF로 저장…"), this);
  m_savePdf->setObjectName(QStringLiteral("printSaveTiles"));
  m_savePdf->setToolTip(QStringLiteral("프린터가 없는 PC나 출력소에서 찍을 수 있게 한 장씩 PDF로 저장합니다."));
  m_print = new QPushButton(QStringLiteral("인쇄"), this);
  m_print->setObjectName(QStringLiteral("printStart"));
  auto* close = new QPushButton(QStringLiteral("닫기"), this);
  buttons->addWidget(m_savePdf);
  buttons->addStretch(1);
  buttons->addWidget(m_print);
  buttons->addWidget(close);
  root->addLayout(buttons);

  // 도면이 고른 용지 한 장보다 크면 나눠 찍기로 연다.
  const QSizeF sheet = sheetSize().size(QPageSize::Millimeter);
  const double longSheet = std::max(sheet.width(), sheet.height()) - 2 * kDefaultMarginMm;
  const double shortSheet = std::min(sheet.width(), sheet.height()) - 2 * kDefaultMarginMm;
  const bool bigger = std::max(m_drawingMm.width(), m_drawingMm.height()) > longSheet + 0.5 ||
                      std::min(m_drawingMm.width(), m_drawingMm.height()) > shortSheet + 0.5;
  (bigger ? m_modeTiles : m_modeFit)->setChecked(true);

  connect(m_modeTiles, &QRadioButton::toggled, this, [this]() { rebuild(); });
  connect(m_printer, &QComboBox::currentIndexChanged, this, [this]() { rebuild(); });
  connect(m_output, &QComboBox::currentIndexChanged, this, [this]() { rebuild(); });
  connect(m_sheet, &QComboBox::currentIndexChanged, this, [this]() { rebuild(); });
  connect(m_overlap, &QDoubleSpinBox::valueChanged, this, [this]() { rebuild(); });
  connect(m_marks, &QCheckBox::toggled, this, [this]() { rebuild(); });
  connect(m_print, &QPushButton::clicked, this, &KaPrintDialog::printNow);
  connect(m_savePdf, &QPushButton::clicked, this, &KaPrintDialog::saveTilesAs);
  connect(close, &QPushButton::clicked, this, &QDialog::reject);

  if (m_drawingMm.isEmpty()) {
    m_summary->setText(readError.isEmpty() ? QStringLiteral("도면을 읽지 못했습니다.") : readError);
    m_print->setEnabled(false);
    m_savePdf->setEnabled(false);
    return;
  }
  rebuild();
}

void KaPrintDialog::setTiled(bool tiled) { (tiled ? m_modeTiles : m_modeFit)->setChecked(true); }
void KaPrintDialog::setOutput(Output output) { m_output->setCurrentIndex(int(output)); }
void KaPrintDialog::setSheet(Sheet sheet) { m_sheet->setCurrentIndex(int(sheet)); }
void KaPrintDialog::setOverlapMm(double mm) { m_overlap->setValue(mm); }
void KaPrintDialog::setMarks(bool on) { m_marks->setChecked(on); }
bool KaPrintDialog::isTiled() const { return m_modeTiles->isChecked(); }

QString KaPrintDialog::printerName() const { return m_printer->currentData().toString(); }

QPageSize KaPrintDialog::sheetSize() const {
  return QPageSize(m_sheet->currentIndex() == SheetA3 ? QPageSize::A3 : QPageSize::A4);
}

QMarginsF KaPrintDialog::marginsFor(const QPageSize& sheet) const {
  QMarginsF margins(kDefaultMarginMm, kDefaultMarginMm, kDefaultMarginMm, kDefaultMarginMm);
  const QString name = printerName();
  if (name.isEmpty()) return margins;
  const QString key = name + QLatin1Char('|') + QString::number(int(sheet.id()));
  if (auto cached = m_marginCache.constFind(key); cached != m_marginCache.constEnd()) return *cached;
  // 프린터가 찍지 못하는 가장자리보다 좁게 잡으면 인쇄 때 줄어들어 축척이 틀어진다.
  QPrinter printer(QPrinterInfo::printerInfo(name), QPrinter::HighResolution);
  printer.setPageSize(sheet);
  QPageLayout layout = printer.pageLayout();
  layout.setUnits(QPageLayout::Millimeter);
  const QMarginsF minimum = layout.minimumMargins();
  margins = QMarginsF(std::max(margins.left(), minimum.left()), std::max(margins.top(), minimum.top()),
                      std::max(margins.right(), minimum.right()), std::max(margins.bottom(), minimum.bottom()));
  m_marginCache.insert(key, margins);
  return margins;
}

double KaPrintDialog::fitRatio() const {
  if (m_drawingMm.isEmpty()) return 1.0;
  QSizeF sheet = sheetSize().size(QPageSize::Millimeter);
  if ((m_drawingMm.width() > m_drawingMm.height()) != (sheet.width() > sheet.height())) sheet.transpose();
  const QMarginsF& m = m_margins;
  return std::min((sheet.width() - m.left() - m.right()) / m_drawingMm.width(),
                  (sheet.height() - m.top() - m.bottom()) / m_drawingMm.height());
}

void KaPrintDialog::rebuild() {
  const bool tiled = isTiled();
  m_output->setEnabled(tiled);
  m_overlap->setEnabled(tiled);
  m_marks->setEnabled(tiled);
  m_margins = marginsFor(sheetSize());
  m_plan = TilePlan();
  if (tiled) {
    TilePlanRequest request;
    request.drawingMm = m_drawingMm;
    request.sheet = sheetSize();
    request.printerMarginMm = m_margins;
    request.overlapMm = m_overlap->value();
    request.labelBandMm = m_marks->isChecked() ? kLabelBandMm : 0.0;
    const auto output = Output(m_output->currentIndex());
    request.scale = output == OutputDrawing ? 1.0 : TilePrint::fitScale(m_drawingMm, outputPage(output));
    m_plan = TilePrint::bestPlan(request);
  }
  m_summary->setText(summary());
  const QString warn = warning();
  m_warning->setText(warn);
  m_warning->setVisible(!warn.isEmpty());
  const bool canDraw = !m_drawingMm.isEmpty() && (!tiled || m_plan.ok);
  m_print->setEnabled(canDraw && !printerName().isEmpty());
  m_print->setToolTip(printerName().isEmpty() ? QStringLiteral("설치된 프린터가 없습니다. 나눈 장을 PDF로 저장해 "
                                                               "다른 PC나 출력소에서 찍으세요.")
                                              : QString());
  m_savePdf->setEnabled(tiled && canDraw);
  updatePreview();
}

QString KaPrintDialog::summary() const {
  const QString sheetName = m_sheet->currentText();
  if (!isTiled()) {
    return QStringLiteral("%1 한 장에 맞춰 찍습니다 · 도면의 %2%").arg(sheetName, percent(fitRatio()));
  }
  if (!m_plan.ok) return m_plan.error;
  QString text = QStringLiteral("%1 %2 가로 %3장 × 세로 %4장 = %5장 · 붙이면 %6×%7 mm")
                     .arg(sheetName, m_plan.landscape ? QStringLiteral("가로") : QStringLiteral("세로"))
                     .arg(m_plan.cols)
                     .arg(m_plan.rows)
                     .arg(m_plan.sheets.size())
                     .arg(qRound(m_plan.outputMm.width()))
                     .arg(qRound(m_plan.outputMm.height()));
  text += std::abs(m_plan.scale - 1.0) < 0.005 ? QStringLiteral(" (도면 축척 그대로)")
                                               : QStringLiteral(" (도면의 %1%)").arg(percent(m_plan.scale));
  return text;
}

QString KaPrintDialog::warning() const {
  if (m_drawingMm.isEmpty()) return {};
  if (!isTiled()) return scaleWarning(fitRatio());
  return m_plan.ok ? scaleWarning(m_plan.scale) : QString();
}

void KaPrintDialog::updatePreview() {
  QPixmap pix(m_preview->minimumSize());
  pix.fill(Qt::white);
  QPainter p(&pix);
  p.setRenderHint(QPainter::Antialiasing);
  p.setRenderHint(QPainter::SmoothPixmapTransform);
  const QRectF box = QRectF(pix.rect()).adjusted(10, 10, -10, -10);
  if (m_thumb.isNull()) {
    p.drawText(box, Qt::AlignCenter, QStringLiteral("미리보기를 만들지 못했습니다."));
    m_preview->setPixmap(pix);
    return;
  }
  if (isTiled() && m_plan.ok) {
    // 끝 장은 도면 밖으로 나가므로 나눈 칸 전체가 보이게 맞춘다.
    QRectF extent(QPointF(0, 0), m_plan.outputMm);
    for (const TileSheet& tile : m_plan.sheets) extent = extent.united(tile.areaMm);
    const double k = std::min(box.width() / extent.width(), box.height() / extent.height());
    const QPointF origin(box.center().x() - extent.width() * k / 2.0, box.center().y() - extent.height() * k / 2.0);
    auto map = [&](const QRectF& mm) {
      return QRectF(origin.x() + mm.x() * k, origin.y() + mm.y() * k, mm.width() * k, mm.height() * k);
    };
    p.drawImage(map(QRectF(QPointF(0, 0), m_plan.outputMm)), m_thumb);
    QFont font = p.font();
    font.setBold(true);
    font.setPixelSize(13);
    p.setFont(font);
    int number = 0;
    for (const TileSheet& tile : m_plan.sheets) {
      const QRectF r = map(tile.areaMm);
      p.setPen(QPen(QColor(37, 99, 235), 1.5));
      p.setBrush(QColor(37, 99, 235, 18));
      p.drawRect(r);
      p.setPen(QColor(30, 64, 175));
      p.drawText(r, Qt::AlignCenter, QString::number(++number));
    }
  } else {
    QSizeF sheet = sheetSize().size(QPageSize::Millimeter);
    if ((m_drawingMm.width() > m_drawingMm.height()) != (sheet.width() > sheet.height())) sheet.transpose();
    const double k = std::min(box.width() / sheet.width(), box.height() / sheet.height());
    const QRectF paper(box.center().x() - sheet.width() * k / 2.0, box.center().y() - sheet.height() * k / 2.0,
                       sheet.width() * k, sheet.height() * k);
    p.setPen(QPen(QColor(120, 120, 120), 1.0));
    p.setBrush(Qt::NoBrush);
    p.drawRect(paper);
    const QSizeF drawn = m_drawingMm * fitRatio() * k;
    p.drawImage(QRectF(paper.center().x() - drawn.width() / 2.0, paper.center().y() - drawn.height() / 2.0,
                       drawn.width(), drawn.height()),
                m_thumb);
  }
  p.end();
  m_preview->setPixmap(pix);
}

bool KaPrintDialog::saveTilesPdf(const QString& path, QString* error) {
  if (!m_plan.ok) {
    if (error) *error = m_plan.error;
    return false;
  }
  QPdfWriter writer(path);
  writer.setResolution(int(kPrintDpi));
  writer.setTitle(m_title);
  writer.setCreator(QStringLiteral("ka-hgis"));
  TilePrint::Marks marks;
  marks.cutLines = m_marks->isChecked();
  marks.title = m_title;
  return TilePrint::renderTiles(m_pdfPath, m_plan, &writer, kPrintDpi, marks, error);
}

void KaPrintDialog::saveTilesAs() {
  const QString path = QFileDialog::getSaveFileName(this, QStringLiteral("나눈 장 PDF 저장"),
                                                    QStringLiteral("도면-나눠찍기.pdf"),
                                                    QStringLiteral("PDF (*.pdf)"));
  if (path.isEmpty()) return;
  QString error;
  QApplication::setOverrideCursor(Qt::WaitCursor);
  const bool ok = saveTilesPdf(path, &error);
  QApplication::restoreOverrideCursor();
  if (!ok) {
    QMessageBox::warning(this, QStringLiteral("나눈 장 PDF"), error);
    return;
  }
  QMessageBox::information(this, QStringLiteral("나눈 장 PDF"),
                           QStringLiteral("%1장을 저장했습니다.\n%2").arg(m_plan.sheets.size()).arg(path));
}

void KaPrintDialog::printNow() {
  const QString name = printerName();
  if (name.isEmpty()) return;
  const QPrinterInfo info = QPrinterInfo::printerInfo(name);
  const QList<QPageSize> sizes = info.supportedPageSizes();
  const bool supported = sizes.isEmpty() || std::any_of(sizes.cbegin(), sizes.cend(), [this](const QPageSize& size) {
    return size.id() == sheetSize().id();
  });
  if (!supported) {
    QMessageBox::warning(this, QStringLiteral("인쇄"),
                         QStringLiteral("이 프린터는 %1 용지를 지원하지 않습니다. 다른 용지를 고르세요.")
                             .arg(m_sheet->currentText()));
    return;
  }
  QPrinter printer(info, QPrinter::HighResolution);
  printer.setDocName(m_title);
  printer.setFullPage(false);
  QString error;
  QApplication::setOverrideCursor(Qt::WaitCursor);
  bool ok = false;
  if (isTiled()) {
    TilePrint::Marks marks;
    marks.cutLines = m_marks->isChecked();
    marks.title = m_title;
    ok = TilePrint::renderTiles(m_pdfPath, m_plan, &printer, kPrintDpi, marks, &error);
  } else {
    ok = TilePrint::renderFit(m_pdfPath, sheetSize(), m_margins, &printer, kPrintDpi, &error);
  }
  QApplication::restoreOverrideCursor();
  if (!ok) {
    QMessageBox::warning(this, QStringLiteral("인쇄"), error);
    return;
  }
  QMessageBox::information(this, QStringLiteral("인쇄"),
                           QStringLiteral("프린터로 %1장을 보냈습니다.").arg(isTiled() ? m_plan.sheets.size() : 1));
  accept();
}
