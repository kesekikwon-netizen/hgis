#include "KaShellGridControls.h"

#include "KaTheme.h"

#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFontMetrics>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

namespace {

// widthLines caps the box at that many line heights of its font. A spin box sizes itself for
// its widest legal text ("간격 10000.0 m" is about 180 px), which made the two rows about
// 500 px wide with the origin button and pushed the bar into the scale bar on a laptop map.
// The cap fits the values people use (a four-digit spacing, 360.0°, 5.0); a longer value
// still edits fine and scrolls inside the box. Line heights follow 큰 글씨 and DPI.
QDoubleSpinBox* makeSpin(QWidget* parent, double lo, double hi, int decimals, double value,
                         const QString& prefix, const QString& suffix, const QString& tip,
                         int widthLines) {
  auto* spin = new QDoubleSpinBox(parent);
  spin->setRange(lo, hi);
  spin->setDecimals(decimals);
  spin->setValue(value);
  spin->setPrefix(prefix);
  spin->setSuffix(suffix);
  spin->setToolTip(tip);
  // Typing "120" must not redraw at 1, 12 and 120; the debounce covers arrows and wheel.
  spin->setKeyboardTracking(false);
  spin->setMaximumWidth(widthLines * QFontMetrics(spin->font()).height());
  return spin;
}

}  // namespace

KaShellGridControls::KaShellGridControls(QWidget* parent) : QWidget(parent) {
  setObjectName(QStringLiteral("mapGridControls"));
  m_apply = new QTimer(this);
  m_apply->setObjectName(QStringLiteral("mapGridApply"));
  m_apply->setSingleShot(true);
  m_apply->setInterval(kApplyDelayMs);
  connect(m_apply, &QTimer::timeout, this, [this]() { emit settingsChanged(false); });

  m_check = new QCheckBox(QStringLiteral("좌표격자"), this);
  m_check->setObjectName(QStringLiteral("mapGridCheck"));
  m_check->setToolTip(QStringLiteral("지도에 좌표 격자를 켭니다. 켠 뒤 미터·경위도 격자를 고릅니다."));

  m_detail = new QWidget(this);
  m_detail->setObjectName(QStringLiteral("gridDetail"));
  auto* grid = new QGridLayout(m_detail);
  grid->setContentsMargins(0, 0, 0, 0);
  grid->setHorizontalSpacing(4);
  grid->setVerticalSpacing(3);

  m_kind = new QComboBox(m_detail);
  m_kind->setObjectName(QStringLiteral("mapGridKind"));
  m_kind->addItem(QStringLiteral("미터 격자"), false);
  m_kind->addItem(QStringLiteral("경위도 격자"), true);
  m_kind->setToolTip(QStringLiteral("미터 격자는 작업 좌표계(5186·5187) 좌표로, 경위도 격자는 도·분·초로 긋습니다."));
  m_step = makeSpin(m_detail, 0.1, 10000.0, 1, 20.0, QStringLiteral("간격 "), QStringLiteral(" m"),
                    QStringLiteral("격자 간격(미터). 시굴격자를 옮길 때도 이 간격에 붙습니다."), 7);
  m_step->setObjectName(QStringLiteral("mapGridStep"));
  m_rotation = makeSpin(m_detail, 0.0, 360.0, 1, 0.0, QStringLiteral("회전 "), QStringLiteral("°"),
                        QStringLiteral("격자 회전 (동쪽 기준 시계 방향)"), 6);
  m_width = makeSpin(m_detail, 0.5, 5.0, 1, 1.2, QStringLiteral("굵기 "), QString(),
                     QStringLiteral("격자 선 굵기"), 5);
  m_width->setSingleStep(0.1);
  m_dash = new QComboBox(m_detail);
  m_dash->addItem(QStringLiteral("실선"), static_cast<int>(Qt::SolidLine));
  m_dash->addItem(QStringLiteral("점선"), static_cast<int>(Qt::DashLine));
  m_dash->addItem(QStringLiteral("점"), static_cast<int>(Qt::DotLine));
  m_dash->setCurrentIndex(1);
  m_dash->setToolTip(QStringLiteral("격자 선 모양"));

  auto* colors = new QHBoxLayout();
  colors->setContentsMargins(0, 0, 0, 0);
  colors->setSpacing(3);
  const QVector<QPair<QColor, QString>> swatches = {
      {QColor(0xD9, 0x2B, 0x2B), QStringLiteral("빨간색 격자")},
      {QColor(0x1D, 0x4E, 0xD8), QStringLiteral("파란색 격자")},
      {QColor(0x1F, 0x29, 0x37), QStringLiteral("검정색 격자")},
  };
  for (const auto& sw : swatches) {
    auto* b = new QToolButton(m_detail);
    b->setObjectName(QStringLiteral("gridColorSwatch"));
    b->setFixedSize(20, 20);
    b->setCheckable(true);
    b->setAutoRaise(false);
    b->setStyleSheet(KaTheme::colorSwatchStyle(sw.first));
    b->setToolTip(sw.second);
    const QColor c = sw.first;
    connect(b, &QToolButton::clicked, this, [this, c]() { setColor(c); });
    m_swatches.append({b, c});
    colors->addWidget(b);
  }
  auto* pick = new QToolButton(m_detail);
  pick->setObjectName(QStringLiteral("gridColorPick"));
  pick->setText(QStringLiteral("…"));
  pick->setFixedSize(22, 20);
  pick->setToolTip(QStringLiteral("그 밖의 색을 직접 고릅니다"));
  connect(pick, &QToolButton::clicked, this, [this]() {
    const QColor picked = QColorDialog::getColor(m_color, window(), QStringLiteral("격자 선 색"),
                                                 QColorDialog::ShowAlphaChannel);
    if (picked.isValid()) setColor(picked);
  });
  colors->addWidget(pick);

  grid->addWidget(m_kind, 0, 0);
  grid->addWidget(m_step, 0, 1);
  grid->addWidget(m_rotation, 0, 2);
  grid->addWidget(m_width, 1, 0);
  grid->addWidget(m_dash, 1, 1);
  grid->addLayout(colors, 1, 2);

  auto* rows = new QVBoxLayout(this);
  rows->setContentsMargins(0, 0, 0, 0);
  rows->setSpacing(4);
  rows->addWidget(m_detail);
  rows->addWidget(m_check, 0, Qt::AlignRight);
  m_detail->setVisible(false);

  connect(m_check, &QCheckBox::toggled, this, [this](bool on) {
    m_detail->setVisible(on);
    emitNow(true);
  });
  connect(m_kind, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
    syncKindState();
    if (m_check->isChecked()) emitNow(true);
  });
  for (QDoubleSpinBox* spin : {m_step, m_rotation, m_width})
    connect(spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            [this](double) { scheduleQuietApply(); });
  connect(m_dash, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          [this](int) { scheduleQuietApply(); });
  syncKindState();
  syncColorButtons();
}

KaShellGridSettings KaShellGridControls::settings() const {
  KaShellGridSettings s;
  s.enabled = m_check->isChecked();
  s.geographic = m_kind->currentData().toBool();
  s.stepMeters = m_step->value();
  s.rotationDeg = m_rotation->value();
  s.lineWidth = m_width->value();
  s.penStyle = static_cast<Qt::PenStyle>(m_dash->currentData().toInt());
  s.color = m_color;
  return s;
}

double KaShellGridControls::stepMeters() const { return m_step->value(); }

bool KaShellGridControls::applyPending() const { return m_apply->isActive(); }

void KaShellGridControls::setColor(const QColor& color) {
  if (!color.isValid() || color == m_color) {
    syncColorButtons();
    return;
  }
  m_color = color;
  syncColorButtons();
  scheduleQuietApply();
}

void KaShellGridControls::scheduleQuietApply() {
  // A hidden grid keeps the values for later; nothing to redraw now.
  if (!m_check->isChecked()) return;
  m_apply->start();
}

void KaShellGridControls::emitNow(bool announce) {
  m_apply->stop();
  emit settingsChanged(announce);
}

void KaShellGridControls::syncKindState() {
  // The 경위도 grid picks its own spacing from the scale and is never rotated.
  const bool metres = !m_kind->currentData().toBool();
  m_step->setEnabled(metres);
  m_rotation->setEnabled(metres);
}

void KaShellGridControls::syncColorButtons() {
  for (const auto& sw : m_swatches) {
    const QSignalBlocker block(sw.first);
    sw.first->setChecked(sw.second == m_color);
  }
}
