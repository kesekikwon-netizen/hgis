#include "ProjectStateBuilder.h"
#include "LayerOps.h"
#include "LayoutService.h"
#include <QJsonObject>
#include <qgsproject.h>
#include <qgsvectorlayer.h>
#include <qgsgeometry.h>
#include <qgsfeatureiterator.h>
#include <qgsfeature.h>
#include <qgswkbtypes.h>
#include <qgslayoutmanager.h>
#include <qgslayout.h>
#include <qgsprintlayout.h>
#include <qgslayoutitemmap.h>
#include <qgsrasterlayer.h>

QJsonObject ProjectStateBuilder::empty() {
  QJsonObject st;
  st.insert(QStringLiteral("survey_area_count"), 0);
  st.insert(QStringLiteral("control_points_count"), 0);
  st.insert(QStringLiteral("feature_poly_count"), 0);
  st.insert(QStringLiteral("feature_line_count"), 0);
  st.insert(QStringLiteral("project_crs_set"), false);
  st.insert(QStringLiteral("has_datum"), false);
  st.insert(QStringLiteral("has_ellipsoid"), false);
  st.insert(QStringLiteral("has_projection"), false);
  st.insert(QStringLiteral("has_origin"), false);
  st.insert(QStringLiteral("has_accuracy"), false);
  st.insert(QStringLiteral("has_kind_period"), true);
  st.insert(QStringLiteral("has_abstract_marker"), false);
  st.insert(QStringLiteral("survey_is_polygon"), false);
  st.insert(QStringLiteral("features_within_survey"), true);
  st.insert(QStringLiteral("geometries_valid"), true);
  st.insert(QStringLiteral("layout_exists:site_location"), false);
  st.insert(QStringLiteral("layout_exists:feature_plan"), false);
  st.insert(QStringLiteral("layout_exists:feature_detail"), false);
  st.insert(QStringLiteral("layout_exists:section"), false);
  st.insert(QStringLiteral("layout_exists:survey_area_map"), false);
  return st;
}

QJsonObject ProjectStateBuilder::fromProject(QgsProject* project) {
  QJsonObject st = empty();
  if (!project) return st;

  st.insert(QStringLiteral("project_crs_set"), project->crs().isValid());

  // 같은 키의 레이어가 여러 개일 수 있다. 하나만 보면 검수 결과가 실제와 달라진다.
  const auto sas = LayerOps::domainLayersForKey(project, QStringLiteral("survey_area"));
  const auto fps = LayerOps::domainLayersForKey(project, QStringLiteral("feature_poly"));
  const auto fls = LayerOps::domainLayersForKey(project, QStringLiteral("feature_line"));
  const auto cps = LayerOps::domainLayersForKey(project, QStringLiteral("control_points"));
  const auto sls = LayerOps::domainLayersForKey(project, QStringLiteral("section_line"));

  const auto totalFeatures = [](const QList<QgsVectorLayer*>& layers) {
    int count = 0;
    for (auto* layer : layers) count += int(layer->featureCount());
    return count;
  };
  const int saCount = totalFeatures(sas);
  const int fpCount = totalFeatures(fps);
  const int flCount = totalFeatures(fls);
  const int cpCount = totalFeatures(cps);
  const int slCount = totalFeatures(sls);
  st.insert(QStringLiteral("survey_area_count"), saCount);
  st.insert(QStringLiteral("feature_poly_count"), fpCount);
  st.insert(QStringLiteral("feature_line_count"), flCount);
  st.insert(QStringLiteral("control_points_count"), cpCount);

  bool surveyPoly = false;
  bool geosValid = true;
  QgsRectangle surveyExtent;
  bool hasSurveyExt = false;
  for (auto* saLayer : sas) {
    if (saLayer->featureCount() <= 0) continue;
    QgsFeatureIterator it = saLayer->getFeatures();
    QgsFeature f;
    while (it.nextFeature(f)) {
      const QgsGeometry g = f.geometry();
      if (g.isNull()) continue;
      if (!g.isGeosValid()) geosValid = false;
      const Qgis::GeometryType gt = QgsWkbTypes::geometryType(g.wkbType());
      if (gt == Qgis::GeometryType::Polygon) surveyPoly = true;
      if (!hasSurveyExt) { surveyExtent = g.boundingBox(); hasSurveyExt = true; }
      else surveyExtent.combineExtentWith(g.boundingBox());
    }
  }
  st.insert(QStringLiteral("survey_is_polygon"), surveyPoly || saCount == 0);
  st.insert(QStringLiteral("has_abstract_marker"), saCount > 0 && !surveyPoly);

  // 유구가 없으면 통과, 있으면 모든 유구에 종류와 시대가 있어야 통과한다.
  bool hasKindPeriod = true;
  for (auto* layer : fps) {
    if (layer->featureCount() <= 0) continue;
    QgsFeatureIterator it = layer->getFeatures();
    QgsFeature f;
    while (it.nextFeature(f)) {
      const QString kind = f.attribute(QStringLiteral("kind")).toString().trimmed();
      const QString period = f.attribute(QStringLiteral("period")).toString().trimmed();
      if (kind.isEmpty() || period.isEmpty()) hasKindPeriod = false;
      const QgsGeometry g = f.geometry();
      if (!g.isNull() && !g.isGeosValid()) geosValid = false;
      if (hasSurveyExt && !g.isNull()) {
        if (!surveyExtent.contains(g.boundingBox()) && !surveyExtent.intersects(g.boundingBox())) {
          st.insert(QStringLiteral("features_within_survey"), false);
        }
      }
    }
  }
  // 선과 단면선도 자기교차 같은 무효 도형이 그대로 제출되면 안 된다.
  for (const auto& layers : {fls, sls}) {
    for (auto* layer : layers) {
      if (layer->featureCount() <= 0) continue;
      QgsFeatureIterator it = layer->getFeatures();
      QgsFeature f;
      while (it.nextFeature(f)) {
        const QgsGeometry g = f.geometry();
        if (!g.isNull() && !g.isGeosValid()) geosValid = false;
      }
    }
  }
  st.insert(QStringLiteral("has_kind_period"), hasKindPeriod);
  st.insert(QStringLiteral("geometries_valid"), geosValid);

  bool d=false,e=false,p=false,o=false,a=false;
  for (auto* cp : cps) {
    if (cp->featureCount() <= 0) continue;
    QgsFeatureIterator it = cp->getFeatures();
    QgsFeature f;
    while (it.nextFeature(f)) {
      if (!f.attribute(QStringLiteral("datum")).toString().trimmed().isEmpty()) d = true;
      if (!f.attribute(QStringLiteral("ellipsoid")).toString().trimmed().isEmpty()) e = true;
      if (!f.attribute(QStringLiteral("projection")).toString().trimmed().isEmpty()) p = true;
      if (!f.attribute(QStringLiteral("origin")).toString().trimmed().isEmpty()) o = true;
      const QString acc = f.attribute(QStringLiteral("accuracy_m")).toString().trimmed();
      const QString acc2 = f.attribute(QStringLiteral("accuracy")).toString().trimmed();
      if (!acc.isEmpty() || !acc2.isEmpty()) a = true;
    }
  }
  st.insert(QStringLiteral("has_datum"), d);
  st.insert(QStringLiteral("has_ellipsoid"), e);
  st.insert(QStringLiteral("has_projection"), p);
  st.insert(QStringLiteral("has_origin"), o || cpCount == 0);
  st.insert(QStringLiteral("has_accuracy"), a || cpCount == 0);

  const bool composedUserSheet = LayoutService::isComposedStudioSheet(project, QStringLiteral("user_sheet"));
  const bool composedSectionSheet = LayoutService::isComposedStudioSheet(project, QStringLiteral("section_sheet"));

  auto namedLayoutComposed = [&](const QString& name) -> bool {
    if (!project || !project->layoutManager())
      return false;
    auto* ly = dynamic_cast<QgsPrintLayout*>(project->layoutManager()->layoutByName(name));
    if (!ly)
      return false;
    // 미조판 자동 템플릿 배제: 사용자가 편집/구성하지 않은 템플릿은 통과 불가
    if (ly->customProperty(QStringLiteral("ka_hgis/auto_template")).toBool() &&
        !ly->customProperty(QStringLiteral("ka_hgis/user_composed")).toBool()) {
      return false;
    }
    if (ly->itemById(QStringLiteral("empty_hint")))
      return false;
    QList<QgsLayoutItemMap*> maps;
    ly->layoutItems(maps);
    for (QgsLayoutItemMap* map : maps) {
      if (!map) continue;
      if (!(map->scale() > 0.0) || !std::isfinite(map->scale())) continue;
      const QgsRectangle ext = map->extent();
      if (!ext.isFinite() || ext.isEmpty() || !(ext.width() > 0.0) || !(ext.height() > 0.0)) continue;
      for (QgsMapLayer* l : map->layers()) {
        if (!l || !l->isValid()) continue;
        if (l->name() == QLatin1String("layout_blank") || l->name() == QLatin1String("ka_section_blank"))
          continue;
        if (LayerOps::isReferenceLayer(l) || LayerOps::isBasemapLayer(l))
          continue;
        if (auto* vl = qobject_cast<QgsVectorLayer*>(l)) {
          if (vl->featureCount() > 0) return true;
        } else if (auto* rl = qobject_cast<QgsRasterLayer*>(l)) {
          if (rl->width() > 0 && rl->height() > 0 && !rl->extent().isEmpty()) return true;
        }
      }
    }
    return false;
  };

  // 1. 유적위치도: survey_area 피처 > 0 및 (user_sheet 또는 site_location 조판 필요)
  const bool siteLocationPass = (saCount > 0) &&
      (composedUserSheet || namedLayoutComposed(QStringLiteral("site_location")));
  st.insert(QStringLiteral("layout_exists:site_location"), siteLocationPass);

  // 2. 유구배치도: (feature_poly 또는 feature_line 피처 > 0) 및 (user_sheet 또는 feature_plan 조판 필요)
  const bool hasFeatures = (fpCount > 0 || flCount > 0);
  const bool featurePlanPass = hasFeatures &&
      (composedUserSheet || namedLayoutComposed(QStringLiteral("feature_plan")));
  st.insert(QStringLiteral("layout_exists:feature_plan"), featurePlanPass);

  // 3. 조사구역도: survey_area 피처 > 0 및 (user_sheet 또는 survey_area_map 조판 필요)
  const bool surveyAreaMapPass = (saCount > 0) &&
      (composedUserSheet || namedLayoutComposed(QStringLiteral("survey_area_map")));
  st.insert(QStringLiteral("layout_exists:survey_area_map"), surveyAreaMapPass);

  // 4. 단면/층위도: section_sheet 조판 또는 (section_line 피처 > 0 및 section 조판) 필요
  const bool sectionPass = composedSectionSheet ||
      (slCount > 0 && namedLayoutComposed(QStringLiteral("section")));
  st.insert(QStringLiteral("layout_exists:section"), sectionPass);

  // 5. 개별유구실측도: 유구 피처 존재 및 feature_detail 조판 필요
  const bool featureDetailPass = hasFeatures &&
      namedLayoutComposed(QStringLiteral("feature_detail"));
  st.insert(QStringLiteral("layout_exists:feature_detail"), featureDetailPass);

  return st;
}

