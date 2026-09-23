#include "KaLayerOpacityRail.h"

#include <QEvent>
#include <QGridLayout>
#include <QLabel>
#include <QShowEvent>
#include <QSlider>

KaLayerOpacityRail::KaLayerOpacityRail(QWidget* host) : QFrame(host), m_host(host) {
  setObjectName(QStringLiteral("layerOpacityRail"));
  setAttribute(Qt::WA_StyledBackground, true);
  setFocusPolicy(Qt::NoFocus);
  // Two horizontal rows, so the names read across instead of one letter per line.
  auto* lay = new QGridLayout(this);
  lay->setContentsMargins(12, 8, 12, 8);
  lay->setHorizontalSpacing(10);
  lay->setVerticalSpacing(6);

  m_title = new QLabel(QStringLiteral("투명도"), this);
  m_title->setObjectName(QStringLiteral("layerOpacityTitle"));
  m_title->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

  m_slider = new QSlider(Qt::Horizontal, this);
  m_slider->setObjectName(QStringLiteral("layerOpacitySlider"));
  m_slider->setRange(0, 100);
  m_slider->setValue(100);
  m_slider->setEnabled(false);
  m_slider->setInvertedAppearance(false);
  m_slider->setTickPosition(QSlider::NoTicks);
  m_slider->setToolTip(QStringLiteral("선택한 배경지도의 투명도. 오른쪽으로 밀면 진해집니다."));
  m_slider->setFocusPolicy(Qt::ClickFocus);

  m_value = new QLabel(QStringLiteral("-"), this);
  m_value->setObjectName(QStringLiteral("layerOpacityValue"));
  m_value->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
  m_value->setMinimumWidth(36);

  // 밝기: 오래된 항공사진은 원판이 어두워 그대로는 지번·경계가 잘 안 보인다.
  m_brightTitle = new QLabel(QStringLiteral("밝기"), this);
  m_brightTitle->setObjectName(QStringLiteral("layerOpacityTitle"));
  m_brightTitle->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

  m_bright = new QSlider(Qt::Horizontal, this);
  m_bright->setObjectName(QStringLiteral("layerBrightnessSlider"));
  m_bright->setRange(-255, 255);
  m_bright->setValue(0);
  m_bright->setEnabled(false);
  m_bright->setTickPosition(QSlider::NoTicks);
  m_bright->setToolTip(QStringLiteral(
      "고른 그림의 밝기. 가운데가 원본입니다. 원본 파일은 바뀌지 않습니다."));
  m_bright->setFocusPolicy(Qt::ClickFocus);

  m_brightValue = new QLabel(QStringLiteral("-"), this);
  m_brightValue->setObjectName(QStringLiteral("layerOpacityValue"));
  m_brightValue->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
  m_brightValue->setMinimumWidth(36);

  connect(m_bright, &QSlider::valueChanged, this, [this](int value) {
    if (m_brightValue && m_bright && m_bright->isEnabled())
      m_brightValue->setText(QString::number(value));
    emit brightnessChanged(value);
  });

  lay->addWidget(m_title, 0, 0);
  lay->addWidget(m_slider, 0, 1);
  lay->addWidget(m_value, 0, 2);
  lay->addWidget(m_brightTitle, 1, 0);
  lay->addWidget(m_bright, 1, 1);
  lay->addWidget(m_brightValue, 1, 2);
  lay->setColumnStretch(1, 1);

  connect(m_slider, &QSlider::valueChanged, this, [this](int value) {
    if (m_value && m_slider && m_slider->isEnabled())
      m_value->setText(QStringLiteral("%1%").arg(value));
    emit percentChanged(value);
  });

  if (m_host) {
    m_host->installEventFilter(this);
    reposition();
    raise();
  }
}

void KaLayerOpacityRail::setPercent(int percent, bool enabled) {
  const int val = qBound(0, percent, 100);
  if (m_slider) {
    m_slider->blockSignals(true);
    m_slider->setEnabled(enabled);
    m_slider->setValue(enabled ? val : 100);
    m_slider->blockSignals(false);
  }
  if (m_value) {
    m_value->setEnabled(enabled);
    m_value->setText(enabled ? QStringLiteral("%1%").arg(val) : QStringLiteral("-"));
  }
  if (m_title)
    m_title->setEnabled(enabled);
}

int KaLayerOpacityRail::percent() const {
  return m_slider ? m_slider->value() : 100;
}

void KaLayerOpacityRail::setBrightness(int value, bool enabled) {
  const int val = qBound(-255, value, 255);
  if (m_bright) {
    m_bright->blockSignals(true);
    m_bright->setEnabled(enabled);
    m_bright->setValue(enabled ? val : 0);
    m_bright->blockSignals(false);
  }
  if (m_brightValue) {
    m_brightValue->setEnabled(enabled);
    m_brightValue->setText(enabled ? QString::number(val) : QStringLiteral("-"));
  }
  if (m_brightTitle)
    m_brightTitle->setEnabled(enabled);
}

int KaLayerOpacityRail::brightness() const {
  return m_bright ? m_bright->value() : 0;
}

bool KaLayerOpacityRail::eventFilter(QObject* watched, QEvent* event) {
  if (watched == m_host && event &&
      (event->type() == QEvent::Resize || event->type() == QEvent::Show ||
       event->type() == QEvent::LayoutRequest)) {
    reposition();
  }
  return QFrame::eventFilter(watched, event);
}

void KaLayerOpacityRail::showEvent(QShowEvent* event) {
  QFrame::showEvent(event);
  reposition();
}

void KaLayerOpacityRail::reposition() {
  if (!m_host)
    return;
  move(10, 10);
  resize(260, 66);
  raise();
}
