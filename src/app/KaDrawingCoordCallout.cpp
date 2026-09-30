#include "KaDrawingCoordCallout.h"

#include <QColor>
#include <QFont>
#include <QList>
#include <QPolygonF>
#include <algorithm>
#include <cmath>

#include <qgsfillsymbol.h>
#include <qgslayout.h>
#include <qgslayoutitemlabel.h>
#include <qgslayoutitempolygon.h>
#include <qgslayoutitempolyline.h>
#include <qgslinesymbol.h>
#include <qgstextformat.h>

namespace {
QgsTextFormat calloutFormat(double points, bool bold) {
  QgsTextFormat format;
  format.setFont(QFont(QStringLiteral("Malgun Gothic")));
  format.setSize(points);
  format.setSizeUnit(Qgis::RenderUnit::Points);
  format.setForcedBold(bold);
  return format;
}
}  // namespace

namespace KaDrawingCoordCallout {

QString tagFor(int index) {
  return QString(QChar(static_cast<char>('A' + (std::max(0, index) % 26))));
}

QString xyText(double mapX, double mapY) {
  return QStringLiteral("X=%1\nY=%2").arg(mapY, 0, 'f', 3).arg(mapX, 0, 'f', 3);
}

QRectF clickBoxRect(const QPointF& tip, double paperWidthMm) {
  const bool placeRight = tip.x() + 34.0 < paperWidthMm - 3.0;
  const bool placeUp = tip.y() - 10.0 > 3.0;
  return QRectF(placeRight ? tip.x() + 6.0 : tip.x() - 32.0, placeUp ? tip.y() - 9.0 : tip.y() + 3.0,
                26.0, 7.2);
}

QRectF freeBoxRect(const QPointF& tip, const QVector<QRectF>& used) {
  const qreal bw = 26.0;
  const qreal bh = 7.2;
  const qreal gap = 8.0;
  const QRectF cands[] = {
      QRectF(tip.x() + gap, tip.y() - bh - 4.0, bw, bh),
      QRectF(tip.x() + gap, tip.y() + 4.0, bw, bh),
      QRectF(tip.x() - gap - bw, tip.y() - bh - 4.0, bw, bh),
      QRectF(tip.x() - gap - bw, tip.y() + 4.0, bw, bh),
      QRectF(tip.x() - bw * 0.5, tip.y() - bh - gap, bw, bh),
      QRectF(tip.x() - bw * 0.5, tip.y() + gap, bw, bh),
  };
  for (const QRectF& c : cands) {
    bool hit = false;
    for (const QRectF& u : used) {
      if (c.adjusted(-2, -2, 2, 2).intersects(u)) {
        hit = true;
        break;
      }
    }
    if (!hit) return c;
  }
  return cands[0];
}

void add(QgsLayout* layout, const QPointF& tip, const QString& tag, const QString& text,
         const QRectF& boxRect) {
  if (!layout) return;
  auto* box = new QgsLayoutItemLabel(layout);
  box->setId(QStringLiteral("ka_coord_box_%1").arg(tag));
  box->setText(text);
  box->setTextFormat(calloutFormat(6.0, false));
  box->setHAlign(Qt::AlignLeft);
  box->setVAlign(Qt::AlignVCenter);
  box->setFrameEnabled(true);
  box->setBackgroundEnabled(true);
  box->setBackgroundColor(QColor(255, 255, 255, 235));
  box->setMarginX(0.4);
  box->setMarginY(0.3);
  box->attemptSetSceneRect(boxRect);
  layout->addLayoutItem(box);

  auto* letter = new QgsLayoutItemLabel(layout);
  letter->setId(QStringLiteral("ka_coord_let_%1").arg(tag));
  letter->setText(tag);
  letter->setTextFormat(calloutFormat(8.0, true));
  letter->setHAlign(Qt::AlignCenter);
  letter->setVAlign(Qt::AlignVCenter);
  letter->setFrameEnabled(false);
  letter->setBackgroundEnabled(false);
  letter->attemptSetSceneRect(QRectF(tip.x() - 2.4, tip.y() - 5.4, 5.2, 4.2));
  layout->addLayoutItem(letter);

  const bool placeRight = boxRect.center().x() >= tip.x();
  const QPointF attach(placeRight ? boxRect.left() : boxRect.right(), boxRect.center().y());
  QPointF dir(tip.x() - attach.x(), tip.y() - attach.y());
  const double len = std::hypot(dir.x(), dir.y());
  if (len > 0.4) {
    dir.rx() /= len;
    dir.ry() /= len;
  } else {
    dir = QPointF(placeRight ? -1.0 : 1.0, 0.0);
  }
  const QPointF nor(-dir.y(), dir.x());
  const double ah = 2.4;
  const QPointF neck(tip.x() - dir.x() * ah, tip.y() - dir.y() * ah);
  QPolygonF shaft;
  shaft << attach << neck;
  auto* arrow = new QgsLayoutItemPolyline(shaft, layout);
  arrow->setId(QStringLiteral("ka_coord_arr_%1").arg(tag));
  arrow->setStartMarker(QgsLayoutItemPolyline::NoMarker);
  arrow->setEndMarker(QgsLayoutItemPolyline::NoMarker);
  if (auto sym = QgsLineSymbol::createSimple({{QStringLiteral("line_color"), QStringLiteral("#1C1917")},
                                              {QStringLiteral("line_width"), QStringLiteral("0.22")},
                                              {QStringLiteral("line_width_unit"), QStringLiteral("MM")}}))
    arrow->setSymbol(sym.get());
  layout->addLayoutItem(arrow);

  QPolygonF head;
  head << tip << QPointF(neck.x() + nor.x() * ah * 0.42, neck.y() + nor.y() * ah * 0.42)
       << QPointF(neck.x() - nor.x() * ah * 0.42, neck.y() - nor.y() * ah * 0.42);
  auto* headItem = new QgsLayoutItemPolygon(head, layout);
  headItem->setId(QStringLiteral("ka_coord_head_%1").arg(tag));
  if (auto fill = QgsFillSymbol::createSimple({{QStringLiteral("color"), QStringLiteral("#1C1917")},
                                               {QStringLiteral("style"), QStringLiteral("solid")},
                                               {QStringLiteral("outline_style"), QStringLiteral("no")}}))
    headItem->setSymbol(fill.get());
  layout->addLayoutItem(headItem);
}

void removeAll(QgsLayout* layout) {
  if (!layout) return;
  QList<QgsLayoutItem*> all;
  layout->layoutItems(all);
  for (QgsLayoutItem* item : all) {
    if (item && item->id().startsWith(QLatin1String("ka_coord_")))
      layout->removeLayoutItem(item);
  }
}

}  // namespace KaDrawingCoordCallout
