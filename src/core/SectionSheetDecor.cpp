#include "SectionSheetDecor.h"

#include "LayoutService.h"

#include <QCryptographicHash>
#include <QDataStream>
#include <QDate>
#include <QDateTime>
#include <QFileInfo>
#include <QLineF>
#include <QPolygonF>

#include <algorithm>
#include <cmath>

#include <qgis.h>
#include <qgslayout.h>
#include <qgslayoutitemlabel.h>
#include <qgslayoutitemmap.h>
#include <qgslayoutitempolyline.h>
#include <qgslayoutitemscalebar.h>
#include <qgslayoutmanager.h>
#include <qgslayoutpoint.h>
#include <qgslinesymbol.h>
#include <qgsmaplayer.h>
#include <qgsprintlayout.h>
#include <qgsproject.h>
#include <qgstextformat.h>

namespace {
const QString kSheetName = QStringLiteral("section_sheet");
const QString kAutoX = QStringLiteral("ka_section/auto_x");
const QString kAutoY = QStringLiteral("ka_section/auto_y");
const QString kRefLineId = QStringLiteral("ka_section_reference_line");
const QString kNoteId = QStringLiteral("ka_section_note");
// Chrome the user may drag; a rebuild puts these back where the user left them.
const QStringList kMovableIds = {
    QStringLiteral("ka_section_title_block"), QStringLiteral("ka_section_scale_bar"),
    QStringLiteral("ka_section_scale"), QStringLiteral("ka_section_crs"), kNoteId};

QgsPrintLayout* sectionSheet(QgsProject* project) {
  if (!project) return nullptr;
  return dynamic_cast<QgsPrintLayout*>(project->layoutManager()->layoutByName(kSheetName));
}

QString fileVersion(const QgsMapLayer* layer) {
  const QString source = layer->source();
  const QFileInfo info(source.section(QLatin1Char('|'), 0, 0));
  if (!info.exists()) return source;
  return QStringLiteral("%1|%2|%3").arg(info.absoluteFilePath()).arg(info.size())
      .arg(info.lastModified().toMSecsSinceEpoch());
}
}  // namespace

namespace SectionSheetDecor {

QString tickKindKey() { return QStringLiteral("ka_section/tick_kind"); }
QString tickValueKey() { return QStringLiteral("ka_section/tick_value"); }

void setLabelFont(QgsLayoutItemLabel* label, const QFont& font) {
  if (!label) return;
  QgsTextFormat format = label->textFormat();
  format.setFont(font);
  format.setSize(font.pointSizeF() > 0 ? font.pointSizeF() : 5.0);
  format.setSizeUnit(Qgis::RenderUnit::Points);
  label->setTextFormat(format);
}

QString titleText(const SectionLayoutOptions& options) {
  const QString title = options.titleKo.isEmpty() ? QStringLiteral("단면도") : options.titleKo;
  const QString date = QDate::currentDate().toString(QStringLiteral("yyyy-MM-dd"));
  return QStringLiteral("%1  |  수직: 표고(m)  |  작성일: %2").arg(title, date);
}

QString elevationText(double value, const SectionLayoutOptions& options) {
  const QString number = QString::number(value, 'f', 2);
  return options.elevationPrefix ? QStringLiteral("EL. ") + number : number;
}

QString distanceText(double value) { return QStringLiteral("%1m").arg(value, 0, 'f', 2); }

QFont tickFont(const SectionLayoutOptions& options) {
  QFont font(QStringLiteral("Malgun Gothic"));
  font.setPointSizeF(std::clamp(options.tickLabelPt, 4.0, 12.0));
  return font;
}

QString crsText(const QString& authId) {
  const QString id = authId.isEmpty() ? QStringLiteral("-") : authId;
  const QString origin = LayoutService::koreanCrsName(authId);
  return origin.isEmpty() ? id : QStringLiteral("%1 · %2").arg(id, origin);
}

void styleReferenceLine(QgsLayoutItemPolyline* item, const SectionLayoutOptions& options) {
  if (!item) return;
  if (auto sym = QgsLineSymbol::createSimple({
          {QStringLiteral("line_color"), options.referenceLineColor},
          {QStringLiteral("line_width"), QString::number(options.referenceLineWidthMm)},
          {QStringLiteral("line_width_unit"), QStringLiteral("MM")},
          {QStringLiteral("line_style"), QStringLiteral("dash")}}))
    item->setSymbol(sym.get());
}

QgsLayoutItemPolyline* addReferenceLine(QgsLayout* layout, const QRectF& mapScene,
                                        const SectionLayoutOptions& options) {
  QPolygonF line;
  line << QPointF(mapScene.left(), mapScene.bottom()) << QPointF(mapScene.right(), mapScene.bottom());
  auto* item = new QgsLayoutItemPolyline(line, layout);
  item->setId(kRefLineId);
  item->setStartMarker(QgsLayoutItemPolyline::NoMarker);
  item->setEndMarker(QgsLayoutItemPolyline::NoMarker);
  styleReferenceLine(item, options);
  layout->addLayoutItem(item);
  return item;
}

QgsLayoutItemLabel* addNote(QgsLayout* layout, const QRectF& mapScene, const QString& text) {
  auto* label = new QgsLayoutItemLabel(layout);
  label->setId(kNoteId);
  label->setText(text);
  label->setHAlign(Qt::AlignLeft);
  label->setVAlign(Qt::AlignBottom);
  QFont font(QStringLiteral("Malgun Gothic"));
  font.setPointSizeF(8.0);
  font.setBold(true);
  setLabelFont(label, font);
  layout->addLayoutItem(label);
  label->attemptSetSceneRect(QRectF(mapScene.left(), std::max(1.0, mapScene.top() - 8.0),
                                    std::max(60.0, mapScene.width()), 7.0));
  markAutoPosition(label);
  return label;
}

void markAutoPosition(QgsLayoutItem* item) {
  if (!item) return;
  item->setCustomProperty(kAutoX, item->pos().x());
  item->setCustomProperty(kAutoY, item->pos().y());
}

}  // namespace SectionSheetDecor

QSizeF SectionLayoutService::paperSizeMm(SectionLayoutOptions::Paper paper) {
  switch (paper) {
    case SectionLayoutOptions::Paper::A4: return QSizeF(297.0, 210.0);
    case SectionLayoutOptions::Paper::A2: return QSizeF(594.0, 420.0);
    case SectionLayoutOptions::Paper::A1: return QSizeF(841.0, 594.0);
    case SectionLayoutOptions::Paper::A3: break;
  }
  return QSizeF(420.0, 297.0);
}

QByteArray SectionLayoutService::inputSignature(const QList<QgsMapLayer*>& layers,
                                                const SectionLayoutOptions& options) {
  QByteArray bytes;
  QDataStream stream(&bytes, QIODevice::WriteOnly);
  for (const QgsMapLayer* layer : layers) {
    if (!layer) continue;
    stream << layer->id() << fileVersion(layer);
  }
  stream << int(options.paper) << options.scaleDenominator << options.elevationOffsetM
         << options.elevationIntervalM << options.manualDistanceIntervalM << options.mapCrsAuthId;
  return QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex();
}

bool SectionLayoutService::applyDecorationOptions(QgsProject* project,
                                                  const SectionLayoutOptions& options) {
  using namespace SectionSheetDecor;
  QgsPrintLayout* layout = sectionSheet(project);
  if (!layout) return false;
  if (auto* title = qobject_cast<QgsLayoutItemLabel*>(layout->itemById(QStringLiteral("ka_section_title_block")))) {
    title->setText(titleText(options));
    title->update();
  }
  auto* map = qobject_cast<QgsLayoutItemMap*>(layout->itemById(QStringLiteral("ka_section_map")));
  // The exact frame: the map's bounding rect is padded by half its frame stroke.
  const QRectF mapScene = map ? map->mapRectToScene(map->rect()) : QRectF();
  auto* refLine = qobject_cast<QgsLayoutItemPolyline*>(layout->itemById(kRefLineId));
  if (options.showReferenceLine) {
    if (refLine) styleReferenceLine(refLine, options);
    else if (map) addReferenceLine(layout, mapScene, options);
  } else if (refLine) {
    layout->removeLayoutItem(refLine);
  }
  if (auto* bar = qobject_cast<QgsLayoutItemScaleBar*>(layout->itemById(QStringLiteral("ka_section_scale_bar")))) {
    const QString style = options.scaleBarStyle.isEmpty() ? QStringLiteral("Double Box") : options.scaleBarStyle;
    if (bar->style() != style) {
      bar->setStyle(style);
      LayoutService::applySheetScaleBarInk(bar);
      bar->update();
    }
  }
  QList<QgsLayoutItemLabel*> labels;
  layout->layoutItems(labels);
  const QFont font = tickFont(options);
  for (QgsLayoutItemLabel* label : labels) {
    const QString kind = label->customProperty(tickKindKey()).toString();
    if (kind.isEmpty()) continue;
    const double value = label->customProperty(tickValueKey()).toDouble();
    label->setText(kind == QLatin1String("elevation") ? elevationText(value, options) : distanceText(value));
    setLabelFont(label, font);
    label->update();
  }
  const QString note = options.noteText.trimmed();
  auto* noteItem = qobject_cast<QgsLayoutItemLabel*>(layout->itemById(kNoteId));
  if (note.isEmpty()) {
    if (noteItem) layout->removeLayoutItem(noteItem);
  } else if (noteItem) {
    noteItem->setText(note);
    noteItem->update();
  } else if (map) {
    addNote(layout, mapScene, note);
  }
  return true;
}

QHash<QString, QPointF> SectionLayoutService::userMovedItems(QgsProject* project) {
  QHash<QString, QPointF> moved;
  QgsPrintLayout* layout = sectionSheet(project);
  if (!layout) return moved;
  for (const QString& id : kMovableIds) {
    QgsLayoutItem* item = layout->itemById(id);
    if (!item || !item->customProperty(kAutoX).isValid()) continue;
    const QPointF automatic(item->customProperty(kAutoX).toDouble(), item->customProperty(kAutoY).toDouble());
    if (QLineF(automatic, item->pos()).length() > 0.05) moved.insert(id, item->pos());
  }
  return moved;
}

void SectionLayoutService::restoreUserMovedItems(QgsProject* project, const QHash<QString, QPointF>& moved) {
  QgsPrintLayout* layout = sectionSheet(project);
  if (!layout) return;
  for (auto it = moved.cbegin(); it != moved.cend(); ++it) {
    if (QgsLayoutItem* item = layout->itemById(it.key()))
      item->attemptMove(QgsLayoutPoint(it.value().x(), it.value().y(), Qgis::LayoutUnit::Millimeters));
  }
}
