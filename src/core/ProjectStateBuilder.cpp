#include "ProjectStateBuilder.h"
#include "HeritagePledge.h"
#include "ChecklistStateChecks.h"
#include "FeatureNumbering.h"
#include "LayerOps.h"
#include "LayerFeatures.h"
#include "LayoutService.h"
#include "SectionLayoutService.h"
#include <QJsonObject>
#include <qgsproject.h>
#include <qgsvectorlayer.h>

namespace {
// 개별유구실측도로 볼 수 있는 가장 작은 축척(분모). 유적 전체 1:1000 용지는 아니다.
constexpr double kFeatureDetailMaxScale = 200.0;
}

QJsonObject ProjectStateBuilder::empty() {
  QJsonObject st;
  st.insert(QStringLiteral("survey_area_count"), 0);
  st.insert(QStringLiteral("control_points_count"), 0);
  st.insert(QStringLiteral("feature_poly_count"), 0);
  st.insert(QStringLiteral("feature_line_count"), 0);
  st.insert(QStringLiteral("project_crs_set"), false);
  st.insert(QStringLiteral("project_crs_authid"), QString());
  st.insert(QStringLiteral("has_datum"), false);
  st.insert(QStringLiteral("has_ellipsoid"), false);
  st.insert(QStringLiteral("has_projection"), false);
  st.insert(QStringLiteral("has_origin"), false);
  st.insert(QStringLiteral("has_accuracy"), false);
  st.insert(QStringLiteral("has_kind_period"), true);
  st.insert(QStringLiteral("has_abstract_marker"), false);
  st.insert(QStringLiteral("survey_is_polygon"), false);
  st.insert(QStringLiteral("features_within_survey"), true);
  st.insert(QStringLiteral("features_real_geometry"), true);
  st.insert(QStringLiteral("geometries_valid"), true);
  st.insert(QStringLiteral("geometries_nonempty"), true);
  st.insert(QStringLiteral("geometries_nonzero_area"), true);
  st.insert(QStringLiteral("export_crs_valid"), true);
  st.insert(QStringLiteral("required_fields_filled"), true);
  st.insert(QStringLiteral("control_points_point_id_unique"), true);
  st.insert(QStringLiteral("feature_poly_feature_no_unique"), true);
  st.insert(QStringLiteral("sheet_covers_targets"), true);
  st.insert(QStringLiteral("sheet_has_elements"), true);
  st.insert(QStringLiteral("HERITAGE_PLEDGE_SCOPE"), true);  // F169 guidance, no intranet data
  st.insert(QStringLiteral("layout_exists:site_location"), false);
  st.insert(QStringLiteral("layout_exists:feature_plan"), false);
  st.insert(QStringLiteral("layout_exists:feature_detail"), false);
  st.insert(QStringLiteral("layout_exists:section"), false);
  st.insert(QStringLiteral("layout_exists:survey_area_map"), false);
  return st;
}

QJsonObject ProjectStateBuilder::fromProject(QgsProject* project) {
  namespace C = ChecklistStateChecks;
  QJsonObject st = empty();
  if (!project) return st;
  st.insert(HeritagePledge::checklistStateKey(), HeritagePledge::checklistPasses(project));
  C::Offenders off;

  st.insert(QStringLiteral("project_crs_set"), project->crs().isValid());
  st.insert(QStringLiteral("project_crs_authid"), project->crs().isValid() ? project->crs().authid() : QString());
  if (project->crs().isValid())
    off.note(QStringLiteral("project_crs_authid"), QStringLiteral("지금 %1").arg(project->crs().authid()));

  // 같은 키의 레이어가 여러 개일 수 있다. 하나만 보면 검수 결과가 실제와 달라진다.
  const auto total = [project](const char* key) {
    int count = 0;
    for (auto* layer : LayerOps::domainLayersForKey(project, QString::fromLatin1(key)))
      count += int(LayerFeatures::count(layer));
    return count;
  };
  const int saCount = total("survey_area");
  const int fpCount = total("feature_poly");
  const int flCount = total("feature_line");
  const int slCount = total("section_line");
  st.insert(QStringLiteral("survey_area_count"), saCount);
  st.insert(QStringLiteral("feature_poly_count"), fpCount);
  st.insert(QStringLiteral("feature_line_count"), flCount);
  st.insert(QStringLiteral("control_points_count"), total("control_points"));

  const C::DomainExtents extents = C::scanGeometry(project, st, off);
  C::scanFields(project, st, off);
  // [pkg K] F044/F054: the same 유구번호 twice within one kind (warn only, never blocks).
  // [int W6] kept here instead of ChecklistStateFields.cpp (outside this group's files).
  bool uniqueFeatureNumbers = true;
  for (QgsVectorLayer* layer : LayerOps::domainLayersForKey(project, QStringLiteral("feature_poly"))) {
    for (const FeatureNumbering::Duplicate& d : FeatureNumbering::duplicates(layer)) {
      uniqueFeatureNumbers = false;
      off.add(QStringLiteral("feature_poly_feature_no_unique"), layer, d.fid, d.label);
    }
  }
  st.insert(QStringLiteral("feature_poly_feature_no_unique"), uniqueFeatureNumbers);

  // 도면 판정은 조판 여부만 보지 않는다. 그 용지의 지도가 대상(조사구역·유구)을
  // 실제로 보여 줘야 통과한다. 다른 곳을 조판한 용지 한 장이 세 규칙을 통과시키던 문제.
  const QString userSheet = QStringLiteral("user_sheet");
  const bool composedUserSheet = LayoutService::isComposedStudioSheet(project, userSheet);
  const auto shows = [&](const QString& named, const C::Extent& target, double maxScale) {
    const auto passes = [&](const QString& layout) {
      const C::SheetView view = C::sheetView(project, layout, target);
      return view.shows && (maxScale <= 0.0 || view.minScale <= maxScale);
    };
    return (composedUserSheet && passes(userSheet)) ||
           (C::isNamedLayoutComposed(project, named) && passes(named));
  };

  // 1. 유적위치도: 조사구역이 있고, 조판한 용지가 조사구역을 보여 준다.
  st.insert(QStringLiteral("layout_exists:site_location"),
            saCount > 0 && shows(QStringLiteral("site_location"), extents.survey, 0.0));
  // 2. 유구배치도: 유구가 있고, 조판한 용지가 유구를 보여 준다.
  const bool hasFeatures = fpCount > 0 || flCount > 0;
  st.insert(QStringLiteral("layout_exists:feature_plan"),
            hasFeatures && shows(QStringLiteral("feature_plan"), extents.features, 0.0));
  // 3. 조사구역도 판정(규칙은 layer_nonempty:survey_area). 참고용 상태로만 남긴다.
  st.insert(QStringLiteral("layout_exists:survey_area_map"),
            saCount > 0 && shows(QStringLiteral("survey_area_map"), extents.survey, 0.0));
  // 4. 단면/층위도: section_sheet 조판 또는 (section_line 피처 > 0 및 section 조판).
  // Section display rasters are layout-owned temp files; after a reopen the sheet's map
  // is empty until they are derived again (same call as the package export, no-op when
  // the sheet is complete). Without it the rule warned falsely after every reopen.
  SectionLayoutService::restoreDisplayLayers(project);
  const bool sectionPass =
      LayoutService::isComposedStudioSheet(project, QStringLiteral("section_sheet")) ||
      (slCount > 0 && C::isNamedLayoutComposed(project, QStringLiteral("section")));
  st.insert(QStringLiteral("layout_exists:section"), sectionPass);
  // 5. 개별유구실측도: 유구를 1:200보다 크게 보여 주는 조판 용지가 있어야 한다.
  st.insert(QStringLiteral("layout_exists:feature_detail"),
            hasFeatures && shows(QStringLiteral("feature_detail"), extents.features, kFeatureDetailMaxScale));

  // 조판한 용지가 조사구역·유구 전체를 담는지, 방위·축척·범례가 있는지(warn).
  if (composedUserSheet) {
    bool covers = true;
    QStringList cut;
    if (extents.survey.isValid() && !C::sheetView(project, userSheet, extents.survey).covers) {
      covers = false;
      cut << QStringLiteral("조사구역");
    }
    if (extents.features.isValid() && !C::sheetView(project, userSheet, extents.features).covers) {
      covers = false;
      cut << QStringLiteral("유구");
    }
    st.insert(QStringLiteral("sheet_covers_targets"), covers);
    if (!covers) off.note(QStringLiteral("sheet_covers_targets"), QStringLiteral("잘린 것: %1").arg(cut.join(QStringLiteral("·"))));
    const QStringList missing = C::missingSheetElements(project, userSheet);
    st.insert(QStringLiteral("sheet_has_elements"), missing.isEmpty());
    if (!missing.isEmpty())
      off.note(QStringLiteral("sheet_has_elements"), QStringLiteral("빠진 것: %1").arg(missing.join(QStringLiteral("·"))));
  }

  off.writeTo(st);
  return st;
}
