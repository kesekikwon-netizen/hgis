#include "LayerLabelControls.h"
#include "HeritageStyle.h"
#include "LayerOps.h"

#include <qgsexpression.h>
#include <qgslayertree.h>
#include <qgsmaplayer.h>
#include <qgspallabeling.h>
#include <qgsproject.h>
#include <qgsvectorlayer.h>
#include <qgsvectorlayerlabeling.h>
#include <memory>

namespace {
QString knownField(const QgsVectorLayer* layer) {
  // Deliberately omit arbitrary first fields, PNU, IDs and opaque A1/A2 codes.
  const QStringList names = {QStringLiteral("JIBUN"), QStringLiteral("지번"),
      QStringLiteral("survey_name"), QStringLiteral("feature_no"), QStringLiteral("artifact_no"),
      QStringLiteral("point_no"), QStringLiteral("유적명"), QStringLiteral("유적명칭"),
      QStringLiteral("사업명"), QStringLiteral("조사명"), QStringLiteral("site_name"),
      QStringLiteral("yujuk_nm"), QStringLiteral("yujeok_nm"), QStringLiteral("유구번호"),
      QStringLiteral("riv_nm"), QStringLiteral("하천명"), QStringLiteral("기호"),
      QStringLiteral("buld_nm"), QStringLiteral("road_nm"), QStringLiteral("지명"),
      QStringLiteral("명칭"), QStringLiteral("name"), QStringLiteral("title")};
  for (const auto& name : names) {
    const int index = layer->fields().lookupField(name);
    if (index >= 0) return layer->fields().at(index).name();
  }
  return {};
}

QString simpleField(const QgsVectorLayer* layer, const QgsPalLayerSettings& settings) {
  if (!settings.isExpression) {
    const int index = layer->fields().lookupField(settings.fieldName);
    return index >= 0 ? layer->fields().at(index).name() : QString();
  }
  for (const auto& field : layer->fields())
    if (settings.fieldName == QgsExpression::quotedColumnRef(field.name())) return field.name();
  return {};
}

bool managedArea(const QgsVectorLayer* layer, const QgsPalLayerSettings& settings) {
  if (!settings.isExpression) return false;
  if (settings.fieldName == QStringLiteral("format_number(area($geometry), 2) || ' ㎡'") ||
      settings.fieldName == QStringLiteral("format_number(area($geometry), 1) || ' ㎡'")) return true;
  const QString field = layer->customProperty(QStringLiteral("ka_hgis/label_field")).toString();
  return !field.isEmpty() && layer->fields().indexOf(field) >= 0 && settings.fieldName ==
      QStringLiteral("coalesce(%1, '') || '\\n(' || format_number(area($geometry), 1) || ' ㎡)'")
          .arg(QgsExpression::quotedColumnRef(field));
}

QString managedExpression(const QString& field, bool area) {
  const QString quoted = QgsExpression::quotedColumnRef(field);
  if (area && !field.isEmpty())
    return QStringLiteral("coalesce(%1, '') || '\\n(' || format_number(area($geometry), 1) || ' ㎡)'").arg(quoted);
  if (area) return QStringLiteral("format_number(area($geometry), 1) || ' ㎡'");
  return field.isEmpty() ? QStringLiteral("''") : quoted;
}

bool replaceManagedContent(QgsVectorLayer* layer, const QString& field, bool area) {
  if (!layer->labeling() || layer->labeling()->type() != QLatin1String("simple")) return false;
  auto settings = std::make_unique<QgsPalLayerSettings>(layer->labeling()->settings());
  settings->fieldName = managedExpression(field, area);
  settings->isExpression = true;
  std::unique_ptr<QgsAbstractVectorLayerLabeling> labeling(layer->labeling()->clone());
  labeling->setSettings(settings.release());
  layer->setLabeling(labeling.release());
  layer->setCustomProperty(QStringLiteral("ka_hgis/label_field"), field);
  layer->setCustomProperty(QStringLiteral("ka_hgis/label_show_area"), area);
  layer->triggerRepaint();
  return true;
}
}

bool LayerLabelControls::isHeritage(const QgsMapLayer* layer) {
  if (!qobject_cast<const QgsVectorLayer*>(layer)) return false;
  if (layer->project()) {
    for (QgsLayerTreeNode* node = layer->project()->layerTreeRoot()->findLayer(layer->id()); node;
         node = node->parent())
      if (HeritageStyle::fromLayerName(node->name())) return true;
  }
  return HeritageStyle::fromLayerName(layer->name()).has_value();
}

LayerLabelControls::Info LayerLabelControls::describe(const QgsMapLayer* layer, bool layout, double scale) {
  Info info;
  info.caption = QStringLiteral("글자");
  if (!layer) return info;
  const auto* vector = qobject_cast<const QgsVectorLayer*>(layer);
  if (!vector) {
    info.reason = QStringLiteral("이미지에 포함된 글자는 따로 숨길 수 없습니다. 색상 범례는 도면에서 설정합니다.");
    return info;
  }
  if (!vector->isValid()) {
    info.reason = QStringLiteral("도면 자료를 불러오지 못했습니다."); return info;
  }
  const bool heritage = isHeritage(layer);
  if (layout && heritage) {
    info.supported = true;
    info.enabled = layer->customProperty(QStringLiteral("ka_hgis/layout_numbers_visible"), true).toBool();
    info.caption = QStringLiteral("번호");
    return info;
  }
  const bool configured = vector->labeling() != nullptr;
  const QgsPalLayerSettings settings = configured ? vector->labeling()->settings() : QgsPalLayerSettings();
  info.field = configured ? simpleField(vector, settings) : knownField(vector);
  const bool area = configured && managedArea(vector, settings);
  if (area && settings.fieldName.startsWith(QLatin1String("coalesce(")))
    info.field = vector->customProperty(QStringLiteral("ka_hgis/label_field")).toString();
  const bool emptyManaged = configured && settings.isExpression && settings.fieldName == QLatin1String("''") &&
      vector->customProperty(QStringLiteral("ka_hgis/label_show_area")).isValid();
  const bool surveyArea = LayerOps::layerKeyOf(layer) == QLatin1String("survey_area") &&
                          vector->geometryType() == Qgis::GeometryType::Polygon;
  const QString expression = configured ? settings.fieldName : info.field;
  const QString lower = expression.toLower();
  const bool special = layer->customProperty(QStringLiteral("ka_hgis/cadastral")).toBool() ||
      !layer->customProperty(QStringLiteral("ka_hgis/topographic_group")).toString().isEmpty() ||
      info.field.compare(QLatin1String("JIBUN"), Qt::CaseInsensitive) == 0 ||
      info.field.compare(QLatin1String("PNU"), Qt::CaseInsensitive) == 0 ||
      info.field == QStringLiteral("지번") || info.field == QStringLiteral("기호") ||
      lower.contains(QLatin1String("elevation")) || lower.contains(QLatin1String("z_min(")) ||
      lower.contains(QStringLiteral("등고수치")) || lower.contains(QStringLiteral("\"수치\"")) ||
      lower.contains(QLatin1String("soil_type_geo")) || lower.contains(QLatin1String("riv_nm"));
  const bool simple = !configured || vector->labeling()->type() == QLatin1String("simple");
  info.areaEditable = vector->geometryType() == Qgis::GeometryType::Polygon && !special && !heritage &&
      simple && ((!configured && surveyArea) || area || emptyManaged || !info.field.isEmpty());
  info.supported = configured || !info.field.isEmpty() || surveyArea;
  info.enabled = vector->labelsEnabled();
  info.needsField = !info.supported;
  info.fieldEditable = !special && simple &&
      (info.needsField || info.areaEditable || !info.field.isEmpty());
  if (!info.supported) {
    info.reason = QStringLiteral("표시할 내용을 먼저 선택해 주세요."); return info;
  }
  if (heritage) info.caption = QStringLiteral("유적명");
  else if (lower.contains(QLatin1String("jibun")) || lower.contains(QStringLiteral("지번"))) info.caption = QStringLiteral("지번");
  else if (lower.contains(QLatin1String("elevation")) || lower.contains(QLatin1String("z_min(")) ||
           lower.contains(QStringLiteral("등고수치")) || lower.contains(QStringLiteral("\"수치\"")) ||
           info.field == QStringLiteral("수치")) info.caption = QStringLiteral("높이");
  else if (area || emptyManaged || (!configured && surveyArea)) info.caption = info.field.isEmpty() ? QStringLiteral("면적") : QStringLiteral("이름·면적");
  else if (info.field == QStringLiteral("기호")) info.caption = QStringLiteral("기호");
  else if (lower.contains(QLatin1String("riv_nm")) || info.field == QStringLiteral("하천명")) info.caption = QStringLiteral("하천명");
  else if (lower.contains(QLatin1String("soil_type_geo"))) info.caption = QStringLiteral("지형명");
  else if (info.field.contains(QLatin1String("_no")) || info.field.contains(QStringLiteral("번호"))) info.caption = QStringLiteral("번호");
  else info.caption = QStringLiteral("이름");
  if (configured && settings.scaleVisibility && scale > 0. &&
      ((settings.minimumScale > 0. && scale > settings.minimumScale) ||
       (settings.maximumScale > 0. && scale < settings.maximumScale)))
    info.reason = scale > settings.minimumScale && settings.minimumScale > 0.
        ? QStringLiteral("확대하면 %1 표시 (1:%2부터)").arg(info.caption, QString::number(settings.minimumScale, 'f', 0))
        : QStringLiteral("현재 축척에서는 %1 표시가 제한됩니다.").arg(info.caption);
  return info;
}

bool LayerLabelControls::setVisible(QgsMapLayer* layer, bool on, bool layout) {
  const Info info = describe(layer, layout);
  if (!info.supported || info.enabled == on) return false;
  if (layout && isHeritage(layer)) {
    layer->setCustomProperty(QStringLiteral("ka_hgis/layout_numbers_visible"), on);
    layer->triggerRepaint();
    return true;
  }
  auto* vector = qobject_cast<QgsVectorLayer*>(layer);
  if (on && !vector->labeling()) {
    if (LayerOps::layerKeyOf(layer) == QLatin1String("survey_area") &&
        vector->geometryType() == Qgis::GeometryType::Polygon)
      return LayerOps::applyAreaM2Labels(vector);
    if (info.field.isEmpty()) return false;
    return LayerOps::applyNameAttributeLabels(vector, info.field, 8., false);
  }
  vector->setLabelsEnabled(on);
  vector->triggerRepaint();
  return true;
}

bool LayerLabelControls::setField(QgsVectorLayer* layer, const QString& field) {
  const Info info = describe(layer);
  if (!info.fieldEditable || !layer || layer->fields().indexOf(field) < 0) return false;
  if (layer->labeling()) {
    if (info.field == field) return false;
    return replaceManagedContent(layer, field, info.areaEditable && managedArea(layer, layer->labeling()->settings()));
  }
  // An explicit first selection can show the chosen content, never a fallback.
  return LayerOps::applyNameAttributeLabels(layer, field, 8., false);
}

bool LayerLabelControls::setArea(QgsVectorLayer* layer, bool on) {
  const Info info = describe(layer);
  if (!info.areaEditable || !layer) return false;
  if (layer->labeling()) {
    if (managedArea(layer, layer->labeling()->settings()) == on) return false;
    return replaceManagedContent(layer, info.field, on);
  }
  if (!on) return false;
  const bool visible = layer->labelsEnabled();
  if (!LayerOps::applyAreaM2Labels(layer)) return false;
  layer->setLabelsEnabled(visible);
  return true;
}
