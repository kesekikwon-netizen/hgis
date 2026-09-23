#include "KaBasemapGallery.h"

#include "KaIcons.h"

#include <QAction>
#include <QGridLayout>
#include <QLabel>
#include <QLinearGradient>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QRadialGradient>
#include <QVBoxLayout>
#include <QWidgetAction>

namespace {

constexpr int kColumns = 5;
const QSize kPreviewSize(88, 52);

void stripes(QPainter& p, const QRectF& r, const QList<QColor>& colors, double width, double angle) {
  p.save();
  p.translate(r.center());
  p.rotate(angle);
  const double reach = r.width() + r.height();
  for (int i = 0; -reach + i * width < reach; ++i) {
    p.fillRect(QRectF(-reach + i * width, -reach, width, 2 * reach), colors.at(i % colors.size()));
  }
  p.restore();
}

void paintPreview(QPainter& p, const QString& id, const QRectF& r) {
  p.setPen(Qt::NoPen);
  if (id == QLatin1String("terrain")) {
    QLinearGradient ground(r.topLeft(), r.bottomRight());
    ground.setColorAt(0, QColor(0xC9, 0xD6, 0xB0));
    ground.setColorAt(1, QColor(0xEF, 0xE8, 0xD2));
    p.fillRect(r, ground);
    for (const QPointF& c : {QPointF(26, 34), QPointF(62, 18)}) {
      QRadialGradient hill(c, 22);
      hill.setColorAt(0, QColor(0x6E, 0x80, 0x5C, 150));
      hill.setColorAt(1, QColor(0x6E, 0x80, 0x5C, 0));
      p.fillRect(r, hill);
    }
  } else if (id == QLatin1String("contour") || id == QLatin1String("map1919")) {
    const bool old = id == QLatin1String("map1919");
    p.fillRect(r, old ? QColor(0xEF, 0xE6, 0xD2) : QColor(0xF7, 0xF1, 0xE3));
    p.setPen(QPen(old ? QColor(0xB8, 0x9C, 0x70) : QColor(0xB0, 0x87, 0x5A), 0.9));
    p.setBrush(Qt::NoBrush);
    for (int i = 1; i <= 7; ++i) p.drawEllipse(QPointF(58, 24), i * 6.5, i * 4.2);
    if (old) {
      p.setPen(QPen(QColor(0xD2, 0xC2, 0xA2), 0.6));
      for (int x = 0; x < 88; x += 22) p.drawLine(QPointF(x, 0), QPointF(x, 52));
    }
  } else if (id == QLatin1String("dem")) {
    QLinearGradient height(r.bottomLeft(), r.topRight());
    height.setColorAt(0, QColor(0x2F, 0x5E, 0x48));
    height.setColorAt(0.55, QColor(0xB9, 0xA5, 0x6C));
    height.setColorAt(1, QColor(0xF3, 0xEF, 0xE6));
    p.fillRect(r, height);
  } else if (id == QLatin1String("soil")) {
    stripes(p, r, {QColor(0xC4, 0x9A, 0x6C), QColor(0xE3, 0xC9, 0xA1), QColor(0xA7, 0x78, 0x4F),
                   QColor(0xD8, 0xB9, 0x8C)}, 22, 0);
  } else if (id == QLatin1String("paleo")) {
    p.fillRect(r, QColor(0xEB, 0xDD, 0xBE));
    p.setPen(QPen(QColor(0xCD, 0xB5, 0x8A), 1));
    for (int x = -52; x < 88; x += 7) p.drawLine(QPointF(x, 52), QPointF(x + 52, 0));
    QPainterPath meander(QPointF(0, 30));
    meander.cubicTo(20, 10, 34, 50, 52, 28);
    meander.cubicTo(64, 14, 76, 40, 88, 24);
    p.setPen(QPen(QColor(0x8C, 0x6D, 0x45), 2.2));
    p.drawPath(meander);
  } else if (id == QLatin1String("cadastral")) {
    p.fillRect(r, Qt::white);
    p.setPen(QPen(QColor(0x9A, 0xA6, 0xB2), 0.9));
    p.save();
    p.translate(44, 26);
    p.rotate(-12);
    for (int y = -40; y <= 40; y += 13) p.drawLine(QPointF(-60, y), QPointF(60, y));
    for (int x = -60; x <= 60; x += 19) p.drawLine(QPointF(x + (x / 19 % 2) * 4, -40), QPointF(x, 40));
    p.restore();
  } else if (id == QLatin1String("daedong")) {
    p.fillRect(r, QColor(0xEF, 0xE3, 0xC6));
    QPainterPath ridge(QPointF(4, 40));
    for (int i = 1; i <= 8; ++i) ridge.lineTo(QPointF(4 + i * 10, i % 2 ? 22 : 36));
    p.setPen(QPen(QColor(0x7A, 0x6A, 0x4E), 1.6));
    p.drawPath(ridge);
    QPainterPath river(QPointF(0, 12));
    river.cubicTo(30, 22, 50, 4, 88, 16);
    p.setPen(QPen(QColor(0x5E, 0x86, 0xA8), 2));
    p.drawPath(river);
  } else if (id == QLatin1String("geology")) {
    stripes(p, r, {QColor(0xD9, 0x8C, 0x6A), QColor(0xE8, 0xC0, 0x7A), QColor(0x9C, 0xB4, 0xC8)}, 9, 20);
  } else if (id == QLatin1String("river")) {
    p.fillRect(r, QColor(0xE6, 0xF0, 0xF7));
    p.setPen(QPen(QColor(0x3F, 0x7F, 0xB5), 2.6, Qt::SolidLine, Qt::RoundCap));
    p.drawLine(QPointF(10, 48), QPointF(46, 24));
    p.drawLine(QPointF(46, 24), QPointF(82, 8));
    p.setPen(QPen(QColor(0x3F, 0x7F, 0xB5), 1.4, Qt::SolidLine, Qt::RoundCap));
    p.drawLine(QPointF(46, 24), QPointF(70, 44));
    p.drawLine(QPointF(28, 36), QPointF(22, 12));
  } else {
    p.fillRect(r, QColor(0xE6, 0xEB, 0xF0));
  }
}

}  // namespace

KaBasemapGallery::KaBasemapGallery(QWidget* parent) : QToolButton(parent) {
  setObjectName(QStringLiteral("btnBasemapGallery"));
  setIcon(KaIcons::icon(QStringLiteral("layer")));
  setText(QStringLiteral("배경 지도"));
  setToolTip(QStringLiteral("지형·수치지형도·지적·지질 같은 참조 지도를 골라 겹칩니다"));
  setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
  setPopupMode(QToolButton::InstantPopup);
  m_menu = new QMenu(this);
  m_menu->setObjectName(QStringLiteral("basemapGalleryMenu"));
  m_panel = new QWidget(m_menu);
  m_panel->setObjectName(QStringLiteral("basemapGallery"));
  auto* column = new QVBoxLayout(m_panel);
  column->setContentsMargins(12, 10, 12, 12);
  column->setSpacing(8);
  auto* caption = new QLabel(QStringLiteral("참조 지도로 겹칩니다. 켜진 지도를 다시 누르면 숨깁니다."), m_panel);
  caption->setObjectName(QStringLiteral("basemapGalleryCaption"));
  column->addWidget(caption);
  m_grid = new QGridLayout;
  m_grid->setSpacing(8);
  column->addLayout(m_grid);
  auto* host = new QWidgetAction(m_menu);
  host->setDefaultWidget(m_panel);
  m_menu->addAction(host);
  setMenu(m_menu);
}

void KaBasemapGallery::place(QToolButton* button, const QString& previewId) {
  button->setParent(m_panel);
  button->setIconSize(kPreviewSize);
  button->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
  button->setFixedSize(kPreviewSize.width() + 16, kPreviewSize.height() + 34);
  button->setProperty("galleryPreview", previewId);
  m_grid->addWidget(button, m_count / kColumns, m_count % kColumns);
  ++m_count;
  // Picking a map closes the panel so the map (or its download dialog) is visible.
  connect(button, &QToolButton::clicked, m_menu, &QMenu::close);
}

void KaBasemapGallery::addButton(QToolButton* button, const QString& previewId) {
  if (!button) return;
  button->setIcon(preview(previewId));
  place(button, previewId);
}

QToolButton* KaBasemapGallery::addAction(QAction* action, const QString& previewId) {
  if (!action) return nullptr;
  // The action is only shown here, so its icon becomes the preview.
  action->setIcon(preview(previewId));
  auto* button = new QToolButton(m_panel);
  button->setDefaultAction(action);
  place(button, previewId);
  return button;
}

QIcon KaBasemapGallery::preview(const QString& id) {
  QIcon icon;
  for (qreal dpr : {1.0, 2.0}) {
    QPixmap pixmap(kPreviewSize * dpr);
    pixmap.setDevicePixelRatio(dpr);
    pixmap.fill(Qt::transparent);
    QPainter p(&pixmap);
    p.setRenderHint(QPainter::Antialiasing);
    const QRectF r(0, 0, kPreviewSize.width(), kPreviewSize.height());
    QPainterPath clip;
    clip.addRoundedRect(r.adjusted(0.5, 0.5, -0.5, -0.5), 6, 6);
    p.setClipPath(clip);
    paintPreview(p, id, r);
    p.setClipping(false);
    p.setPen(QPen(QColor(0, 0, 0, 38), 1));
    p.setBrush(Qt::NoBrush);
    p.drawPath(clip);
    p.end();
    icon.addPixmap(pixmap);
  }
  return icon;
}
