#include "KaStartupSplash.h"

#include <QGuiApplication>
#include <QLabel>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QProgressBar>
#include <QScreen>
#include <QScrollArea>
#include <QTimer>
#include <QVBoxLayout>

QString KaStartupSplash::attributionText() {
  // Existing application notices (LICENSE and MainWindow::showAbout), with
  // WebEngine identified separately because its third-party terms also apply.
  return QStringLiteral(
      "QGIS  © QGIS Development Team · GNU GPL v2 이상\n"
      "Qt  © The Qt Company 및 기여자 · LGPLv3 / GPLv2 / GPLv3\n"
      "Qt WebEngine / Chromium · 구성 요소별 오픈소스 라이선스\n"
      "GDAL/OGR  © OSGeo 및 기여자 · MIT 계열\n"
      "PROJ  © PROJ contributors · MIT\n"
      "GEOS  © GEOS contributors · LGPLv2.1\n"
      "SQLite · Public domain\n"
      "지도·자료: VWorld · 국토정보플랫폼 · 국가유산 공간정보 · 흙토람 · KIGAM 등\n"
      "지도·자료 및 PROJ 데이터는 제공처의 저작권·이용조건을 따릅니다.");
}

KaStartupSplash::KaStartupSplash(QWidget* parent, int readingDurationMs)
    : QWidget(parent, Qt::SplashScreen | Qt::FramelessWindowHint),
      m_readingDurationMs(qMax(1, readingDurationMs)) {
  setObjectName(QStringLiteral("startupSplash"));
  setWindowTitle(QStringLiteral("필드고고학GIS v2 · 시작 안내"));
  setAttribute(Qt::WA_TranslucentBackground);
  // QWidget does not inherit QSplashScreen's click-to-hide behavior.
  setStyleSheet(QStringLiteral(
      "QWidget#startupSplash QLabel { background: transparent; color: #e5f2ff; }"
      "QWidget#startupSplash QScrollArea, QWidget#startupSplash QWidget#startupNotices {"
      " background: transparent; border: none; }"
      "QWidget#startupSplash QProgressBar { background: #123e6b; border: 1px solid #659dc9;"
      " border-radius: 5px; min-height: 10px; max-height: 10px; }"
      "QWidget#startupSplash QProgressBar::chunk { border-radius: 4px;"
      " background: qlineargradient(x1:0,y1:0,x2:0,y2:1,stop:0 #b7efff,stop:1 #50bfff); }"));

  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(36, 28, 36, 26);
  layout->setSpacing(10);
  auto label = [this](const QString& text, const QString& name, int pixels,
                      bool bold = false) {
    auto* result = new QLabel(text, this);
    result->setObjectName(name);
    QFont font(QStringLiteral("Malgun Gothic"));
    font.setPixelSize(pixels);
    font.setBold(bold);
    result->setFont(font);
    result->setWordWrap(true);
    return result;
  };
  layout->addWidget(label(QStringLiteral("동국문화재연구원  ·  만든이 권영인"),
                          QStringLiteral("startupAuthor"), 14));
  layout->addWidget(label(QStringLiteral("필드고고학GIS"),
                          QStringLiteral("startupTitle"), 34, true));
  auto* version = label(QStringLiteral("v2  ·  고고학 현장 조사와 도면 작성"),
                        QStringLiteral("startupVersion"), 16, true);
  version->setStyleSheet(QStringLiteral("color: #a8e7ff;"));
  layout->addWidget(version);
  layout->addSpacing(6);
  layout->addWidget(label(QStringLiteral("함께 사용한 기술 · 저작권 안내"),
                          QStringLiteral("startupNoticeTitle"), 14, true));

  auto* notices = label(attributionText(), QStringLiteral("startupNotices"), 13);
  notices->setTextInteractionFlags(Qt::TextSelectableByMouse);
  auto* scroll = new QScrollArea(this);
  scroll->setWidgetResizable(true);
  scroll->setFrameShape(QFrame::NoFrame);
  scroll->setWidget(notices);
  scroll->viewport()->setAutoFillBackground(false);
  layout->addWidget(scroll, 1);
  layout->addWidget(label(
      QStringLiteral("QGIS의 qgis_core / qgis_gui 라이브러리를 사용합니다.\n"
                     "본 소프트웨어는 GNU GPL v2 이상으로 배포됩니다.\n"
                     "자세한 저작권 안내는 앱의 도움말 → 정보에서 다시 볼 수 있습니다."),
      QStringLiteral("startupLicense"), 12));
  layout->addSpacing(6);
  auto* statusRow = new QHBoxLayout;
  m_status = label(QStringLiteral("앱을 준비하고 있습니다…"),
                   QStringLiteral("startupStatus"), 13);
  m_seconds = label(QStringLiteral("안내 시간 10초"),
                    QStringLiteral("startupCountdown"), 13);
  m_seconds->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
  statusRow->addWidget(m_status, 1);
  statusRow->addWidget(m_seconds);
  layout->addLayout(statusRow);
  m_progress = new QProgressBar(this);
  m_progress->setObjectName(QStringLiteral("startupProgress"));
  m_progress->setAccessibleName(QStringLiteral("시작 안내 읽기 시간"));
  m_progress->setRange(0, 1000);
  m_progress->setValue(0);
  m_progress->setTextVisible(false);
  layout->addWidget(m_progress);

  m_timer = new QTimer(this);
  m_timer->setInterval(25);
  m_timer->setTimerType(Qt::PreciseTimer);
  connect(m_timer, &QTimer::timeout, this, &KaStartupSplash::updateProgress);
  const QRect available = QGuiApplication::primaryScreen()
                              ? QGuiApplication::primaryScreen()->availableGeometry()
                              : QRect(0, 0, 1024, 768);
  resize(qMin(780, available.width() - 32), qMin(550, available.height() - 32));
  move(available.center() - rect().center());
}

void KaStartupSplash::markReady() {
  if (m_readingClock.isValid())
    return;
  m_status->setText(QStringLiteral("안내를 확인해 주세요. 잠시 후 홈 화면이 열립니다."));
  m_readingClock.start();
  updateProgress();
  m_timer->start();
}

void KaStartupSplash::updateProgress() {
  if (!m_readingClock.isValid() || m_completed)
    return;
  const qint64 elapsed = m_readingClock.elapsed();
  m_progress->setValue(int(qMin<qint64>(1000, elapsed * 1000 / m_readingDurationMs)));
  const int remaining = int(qMax<qint64>(0, m_readingDurationMs - elapsed) + 999) / 1000;
  m_seconds->setText(QStringLiteral("%1초 남음").arg(remaining));
  if (elapsed < m_readingDurationMs)
    return;
  m_completed = true;
  m_timer->stop();
  m_status->setText(QStringLiteral("준비 완료"));
  emit readyToShow();
}

void KaStartupSplash::paintEvent(QPaintEvent*) {
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);
  const QRectF card = QRectF(rect()).adjusted(1, 1, -1, -1);
  QPainterPath outline;
  outline.addRoundedRect(card, 24, 24);
  QLinearGradient blue(0, 0, width() * 0.7, height());
  blue.setColorAt(0, QColor(QStringLiteral("#267abb")));
  blue.setColorAt(0.4, QColor(QStringLiteral("#12558d")));
  blue.setColorAt(1, QColor(QStringLiteral("#092f56")));
  painter.fillPath(outline, blue);
  painter.save();
  painter.setClipPath(outline);
  QLinearGradient gloss(0, 0, 0, height() * 0.45);
  gloss.setColorAt(0, QColor(255, 255, 255, 76));
  gloss.setColorAt(1, QColor(255, 255, 255, 0));
  painter.setPen(Qt::NoPen);
  painter.setBrush(gloss);
  painter.drawEllipse(QRectF(-width() * 0.3, -height() * 0.65,
                             width() * 1.7, height()));
  painter.restore();
  painter.setPen(QPen(QColor(182, 225, 255, 185), 1));
  painter.setBrush(Qt::NoBrush);
  painter.drawPath(outline);
}
