#include "LayerOps.h"
#include "LayerLabelControls.h"

#include <QColor>
#include <QFont>
#include <QPointer>
#include <QSet>
#include <cmath>
#include <memory>

#include <qgis.h>
#include <qgsfillsymbollayer.h>
#include <qgslabelobstaclesettings.h>
#include <qgslayertree.h>
#include <qgslayertreelayer.h>
#include <qgspallabeling.h>
#include <qgsproject.h>
#include <qgsproperty.h>
#include <qgsrendercontext.h>
#include <qgsrenderer.h>
#include <qgssymbol.h>
#include <qgssymbollayer.h>
#include <qgstextbuffersettings.h>
#include <qgstextformat.h>
#include <qgsvectorlayer.h>
#include <qgsvectorlayerlabeling.h>

namespace {
// Shared look of labels the app creates. Size and face come from LayerOps.
QgsTextFormat defaultLabelFormat(double sizePt, const QColor& ink) {
  QgsTextFormat fmt;
  QFont font = fmt.font();
  font.setFamily(QString::fromLatin1(LayerOps::kDefaultLabelFont));
  font.setPointSizeF(sizePt);
  font.setBold(true);
  fmt.setFont(font);
  fmt.setSize(sizePt);
  fmt.setSizeUnit(Qgis::RenderUnit::Points);
  fmt.setColor(ink);
  QgsTextBufferSettings buf = fmt.buffer();
  buf.setEnabled(true);
  buf.setSize(0.8);
  buf.setColor(QColor(255, 255, 255, 230));
  fmt.setBuffer(buf);
  return fmt;
}

// Rewrites every sub-provider's text format and keeps expressions, rules,
// placement and visibility. Returns false when the layer has no labeling.
template <typename Change>
bool editLabelFormats(QgsVectorLayer* layer, Change change) {
  if (!layer || !layer->isValid() || !layer->labeling()) return false;
  std::unique_ptr<QgsAbstractVectorLayerLabeling> labeling(layer->labeling()->clone());
  if (!labeling) return false;
  for (const QString& provider : labeling->subProviders()) {
    auto settings = std::make_unique<QgsPalLayerSettings>(labeling->settings(provider));
    QgsTextFormat format = settings->format();
    change(*settings, format);
    settings->setFormat(format);
    labeling->setSettings(settings.release(), provider);  // QGIS takes ownership.
  }
  layer->setLabeling(labeling.release());
  layer->triggerRepaint();
  return true;
}
}  // namespace

bool LayerOps::applyNameAttributeLabels(QgsVectorLayer* layer, const QString& fieldName,
                                       double fontSizePt, bool showArea) {
  if (!layer || !layer->isValid()) return false;

  QString targetField = fieldName;
  if (targetField.isEmpty())
    targetField = detectNameField(layer);

  if (fontSizePt <= 0.0)
    fontSizePt = kDefaultLabelSizePt;

  if (!targetField.isEmpty())
    layer->setCustomProperty(QStringLiteral("ka_hgis/label_field"), targetField);
  layer->setCustomProperty(QStringLiteral("ka_hgis/label_font_size"), fontSizePt);
  layer->setCustomProperty(QStringLiteral("ka_hgis/label_show_area"), showArea);

  QgsPalLayerSettings s;
  s.drawLabels = true;

  const bool isPolygon = (layer->geometryType() == Qgis::GeometryType::Polygon);
  const bool hasField = !targetField.isEmpty() && layer->fields().indexOf(targetField) >= 0;

  if (hasField && isPolygon && showArea) {
    s.fieldName = QStringLiteral("coalesce(\"%1\", '') || '\\n(' || format_number(area($geometry), 1) || ' ㎡)'")
                      .arg(targetField);
    s.isExpression = true;
  } else if (hasField) {
    s.fieldName = QStringLiteral("\"%1\"").arg(targetField);
    s.isExpression = true;
  } else if (isPolygon && showArea) {
    s.fieldName = QStringLiteral("format_number(area($geometry), 1) || ' ㎡'");
    s.isExpression = true;
  } else if (isPolygon) {
    s.fieldName = QStringLiteral("''");
    s.isExpression = true;
  } else if (layer->fields().count() > 0) {
    s.fieldName = QStringLiteral("\"%1\"").arg(layer->fields().at(0).name());
    s.isExpression = true;
  } else {
    return false;
  }

  if (isPolygon) {
    s.placement = Qgis::LabelPlacement::OverPoint;
    s.setPolygonPlacementFlags(Qgis::LabelPolygonPlacementFlag::AllowPlacementInsideOfPolygon);
  } else if (layer->geometryType() == Qgis::GeometryType::Line) {
    s.placement = Qgis::LabelPlacement::Line;
  } else {
    s.placement = Qgis::LabelPlacement::AroundPoint;
  }

  QgsLabelObstacleSettings obs = s.obstacleSettings();
  obs.setIsObstacle(false);
  s.setObstacleSettings(obs);
  s.setFormat(defaultLabelFormat(fontSizePt, QColor(31, 35, 40)));

  layer->setLabeling(new QgsVectorLayerSimpleLabeling(s));
  layer->setLabelsEnabled(true);
  layer->triggerRepaint();
  return true;
}

bool LayerOps::setLabelFontSize(QgsVectorLayer* layer, double fontSizePt) {
  if (!layer || !layer->isValid() || !std::isfinite(fontSizePt) || fontSizePt <= 0.)
    return false;
  if (!layer->labeling()) {
    const bool visible = layer->labelsEnabled();
    const bool ok = applyNameAttributeLabels(layer, currentLabelField(layer), fontSizePt,
                                             layer->geometryType() == Qgis::GeometryType::Polygon);
    if (ok) layer->setLabelsEnabled(visible);
    return ok;
  }
  // Keep expressions, rules, placement, color, buffers and visibility intact.
  // In particular an area-only label must not become an empty name field.
  const bool ok = editLabelFormats(layer, [fontSizePt](QgsPalLayerSettings& settings, QgsTextFormat& format) {
    QFont font = format.font();
    font.setPointSizeF(fontSizePt);
    format.setFont(font);
    format.setSize(fontSizePt);
    format.setSizeUnit(Qgis::RenderUnit::Points);
    // The explicit pt choice supersedes imported size expressions, while
    // unrelated data-defined label properties remain untouched.
    settings.dataDefinedProperties().setProperty(QgsPalLayerSettings::Property::Size, QgsProperty());
    settings.dataDefinedProperties().setProperty(QgsPalLayerSettings::Property::FontSizeUnit, QgsProperty());
  });
  if (ok) layer->setCustomProperty(QStringLiteral("ka_hgis/label_font_size"), fontSizePt);
  return ok;
}

double LayerOps::labelFontSize(const QgsVectorLayer* layer, double defaultSize) {
  if (!layer) return defaultSize;
  const QVariant v = layer->customProperty(QStringLiteral("ka_hgis/label_font_size"));
  if (v.isValid() && v.toDouble() > 0.0)
    return v.toDouble();
  if (layer->labeling()) {
    const QgsPalLayerSettings s = layer->labeling()->settings();
    if (s.format().size() > 0.0)
      return s.format().size();
  }
  return defaultSize;
}

QColor LayerOps::labelColor(const QgsVectorLayer* layer) {
  if (!layer || !layer->labeling()) return {};
  return layer->labeling()->settings().format().color();
}

bool LayerOps::setLabelColor(QgsVectorLayer* layer, const QColor& color) {
  if (!color.isValid()) return false;
  const bool ok = editLabelFormats(layer, [color](QgsPalLayerSettings& settings, QgsTextFormat& format) {
    format.setColor(color);
    // A chosen colour must win over an imported colour expression.
    settings.dataDefinedProperties().setProperty(QgsPalLayerSettings::Property::Color, QgsProperty());
  });
  if (ok) layer->setCustomProperty(QStringLiteral("ka_hgis/label_color"), color.name(QColor::HexArgb));
  return ok;
}

bool LayerOps::labelHalo(const QgsVectorLayer* layer) {
  return layer && layer->labeling() && layer->labeling()->settings().format().buffer().enabled();
}

bool LayerOps::setLabelHalo(QgsVectorLayer* layer, bool on) {
  const bool ok = editLabelFormats(layer, [on](QgsPalLayerSettings&, QgsTextFormat& format) {
    QgsTextBufferSettings buffer = format.buffer();
    buffer.setEnabled(on);
    if (on && buffer.size() <= 0.0) buffer.setSize(0.8);
    format.setBuffer(buffer);
  });
  if (ok) layer->setCustomProperty(QStringLiteral("ka_hgis/label_halo"), on);
  return ok;
}

bool LayerOps::labelShowArea(const QgsVectorLayer* layer, bool defaultShow) {
  if (!layer) return defaultShow;
  // Old drawing code enabled the expression but left an earlier false flag.
  // The checkbox must reflect what the current labeling actually displays.
  if (layer->labeling()) {
    const auto settings = layer->labeling()->settings();
    if (settings.isExpression && settings.fieldName.contains(QLatin1String("area($geometry)")))
      return true;
  }
  const QVariant v = layer->customProperty(QStringLiteral("ka_hgis/label_show_area"));
  if (v.isValid())
    return v.toBool();
  return defaultShow;
}

QString LayerOps::currentLabelField(const QgsVectorLayer* layer) {
  if (!layer) return {};
  const QString f = layer->customProperty(QStringLiteral("ka_hgis/label_field")).toString();
  if (!f.isEmpty()) return f;
  return detectNameField(layer);
}

bool LayerOps::applyAreaM2Labels(QgsVectorLayer* layer) {
  if (!layer || !layer->isValid()) return false;
  if (layer->geometryType() != Qgis::GeometryType::Polygon) return false;
  // Geometry expressions update themselves when a feature changes. Replacing
  // existing labeling here would discard the user's menu choices on each edit.
  if (layer->labeling()) return true;

  QgsPalLayerSettings s;
  s.drawLabels = true;
  s.fieldName = QStringLiteral("format_number(area($geometry), 2) || ' ㎡'");
  s.isExpression = true;
  s.placement = Qgis::LabelPlacement::OverPoint;
  s.setPolygonPlacementFlags(Qgis::LabelPolygonPlacementFlag::AllowPlacementInsideOfPolygon);

  QgsLabelObstacleSettings obs = s.obstacleSettings();
  obs.setIsObstacle(false);
  s.setObstacleSettings(obs);
  s.setFormat(defaultLabelFormat(kDefaultLabelSizePt, QColor(15, 23, 42)));

  layer->setLabeling(new QgsVectorLayerSimpleLabeling(s));
  layer->setLabelsEnabled(true);
  layer->setCustomProperty(QStringLiteral("ka_hgis/label_show_area"), true);
  layer->setCustomProperty(QStringLiteral("ka_hgis/label_font_size"), kDefaultLabelSizePt);
  layer->triggerRepaint();
  return true;
}

// 「레이어가 밑에 있으면 글자도 밑으로 간다」: see LayerTreeLabelStack.cpp.

bool LayerOps::hasToggleableLabels(const QgsMapLayer* layer) {
  return LayerLabelControls::describe(layer).supported;
}

bool LayerOps::labelsVisible(const QgsMapLayer* layer) {
  const auto* vl = qobject_cast<const QgsVectorLayer*>(layer);
  return vl && vl->isValid() && vl->labelsEnabled();
}

bool LayerOps::setLabelsVisible(QgsMapLayer* layer, bool on) {
  return LayerLabelControls::setVisible(layer, on);
}
