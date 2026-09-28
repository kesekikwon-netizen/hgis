#include "SurveyContourStyle.h"

#include "SurveyContourMath.h"
#include "HeritageImport.h"
#include "LayerOps.h"

#include <algorithm>

#include <qgsfillsymbol.h>
#include <qgsgraduatedsymbolrenderer.h>
#include <qgslayertree.h>
#include <qgslinesymbol.h>
#include <qgsmaplayer.h>
#include <qgsmarkersymbol.h>
#include <qgsproject.h>
#include <qgspallabeling.h>
#include <qgsrulebasedlabeling.h>
#include <qgsrulebasedrenderer.h>
#include <qgssinglesymbolrenderer.h>
#include <qgstextformat.h>
#include <qgsvectorlayer.h>
#include <qgsvectorlayerlabeling.h>

namespace {

const char* kColors[] = {"#72c6b5", "#81ceaa", "#91d396", "#a5d680", "#bad975", "#d0dc78", "#e0da7b",
                         "#ebd27a", "#edbe70", "#e8a666", "#d98b5a", "#c47452", "#ad6650", "#b69886",
                         "#d1c5bb", "#f4f3ef"};

QColor colorAt(int index, int count) {
  const double t = count <= 1 ? 0.5 : (index + 0.5) / count;
  const double scaled = t * 15.0;
  const int lo = std::min(15, static_cast<int>(scaled));
  const int hi = std::min(15, lo + 1);
  const double mix = scaled - lo;
  const QColor a(QString::fromLatin1(kColors[lo]));
  const QColor b(QString::fromLatin1(kColors[hi]));
  QColor color = QColor::fromRgbF(a.redF() * (1 - mix) + b.redF() * mix, a.greenF() * (1 - mix) + b.greenF() * mix,
                                  a.blueF() * (1 - mix) + b.blueF() * mix);
  color.setAlpha(170);
  return color;
}

QgsVectorLayer* openVector(const QString& gpkg, const QString& table, const QString& title) {
  auto* layer = new QgsVectorLayer(gpkg + QStringLiteral("|layername=") + table, title, QStringLiteral("ogr"));
  return layer->isValid() ? layer : (delete layer, nullptr);
}

void tag(QgsMapLayer* layer, const QString& group) {
  LayerOps::markReferenceLayer(layer);
  layer->setCustomProperty(QStringLiteral("ka_hgis/contour_group"), group);
}

}  // namespace

void SurveyContourStyle::removeGroup(QgsProject* project, const QString& groupTitle) {
  if (!project) return;
  QStringList ids;
  for (QgsMapLayer* layer : project->mapLayers()) {
    if (layer && layer->customProperty(QStringLiteral("ka_hgis/contour_group")).toString() == groupTitle)
      ids << layer->id();
  }
  if (!ids.isEmpty()) project->removeMapLayers(ids);
}

bool SurveyContourStyle::apply(QgsProject* project, const QString& gpkgPath, const QString& groupTitle,
                               const SurveyContourResult& built, QString* error) {
  if (!project) return false;
  removeGroup(project, groupTitle);
  QgsLayerTree* root = project->layerTreeRoot();
  if (!root) {
    if (error) *error = QStringLiteral("레이어 목록이 없습니다.");
    return false;
  }
  QgsLayerTreeGroup* reference = root->findGroup(HeritageImport::referenceGroupName());
  if (!reference) reference = root->addGroup(HeritageImport::referenceGroupName());
  if (!reference) {
    if (error) *error = QStringLiteral("참조 지도 그룹을 만들지 못했습니다.");
    return false;
  }
  QgsLayerTreeGroup* group = reference->findGroup(groupTitle);
  if (!group) group = reference->addGroup(groupTitle);

  const int classes = surveyClassCount(built.minCm, built.maxCm, built.bandIntervalCm);
  if (built.bandCount > 0 && classes > 0) {
    QgsVectorLayer* bands = openVector(gpkgPath, QStringLiteral("contour_bands"), QStringLiteral("색 구간"));
    if (!bands) {
      if (error) *error = QStringLiteral("색 구간 레이어를 열지 못했습니다.");
      return false;
    }
    QgsRangeList ranges;
    for (int i = 0; i < classes; ++i) {
      const int lo = built.minCm + i * built.bandIntervalCm;
      const int hi = lo + built.bandIntervalCm;
      auto symbol = QgsFillSymbol::createSimple(
          {{QStringLiteral("color"), colorAt(i, classes).name(QColor::HexArgb)},
           {QStringLiteral("outline_style"), QStringLiteral("no")}});
      ranges.append(QgsRendererRange(surveyCmToMeters(lo), surveyCmToMeters(hi), symbol.release(),
                                     surveyBandLabel(lo, hi)));
    }
    bands->setRenderer(new QgsGraduatedSymbolRenderer(QStringLiteral("elev_mid"), ranges));
    tag(bands, groupTitle);
    project->addMapLayer(bands, false);
    group->insertLayer(0, bands);
  }

  if (QgsVectorLayer* lines = openVector(gpkgPath, QStringLiteral("contour_lines"), QStringLiteral("등고선"))) {
    auto* lineRules = new QgsRuleBasedRenderer::Rule(nullptr);
    lineRules->appendChild(new QgsRuleBasedRenderer::Rule(
        QgsLineSymbol::createSimple({{QStringLiteral("line_color"), QStringLiteral("#3a2f24")},
                                     {QStringLiteral("line_width"), QStringLiteral("0.7")},
                                     {QStringLiteral("line_width_unit"), QStringLiteral("MM")}})
            .release(),
        0, 0, QStringLiteral("\"index_line\" = 1"), QStringLiteral("계곡선")));
    lineRules->appendChild(new QgsRuleBasedRenderer::Rule(
        QgsLineSymbol::createSimple({{QStringLiteral("line_color"), QStringLiteral("#8a7a66")},
                                     {QStringLiteral("line_width"), QStringLiteral("0.25")},
                                     {QStringLiteral("line_width_unit"), QStringLiteral("MM")}})
            .release(),
        0, 0, QStringLiteral("\"index_line\" = 0"), QStringLiteral("주곡선")));
    lines->setRenderer(new QgsRuleBasedRenderer(lineRules));
    auto* settings = new QgsPalLayerSettings();
    settings->fieldName = QStringLiteral("\"elev_label\"");
    settings->isExpression = true;
    settings->placement = Qgis::LabelPlacement::Curved;
    settings->drawLabels = true;
    settings->repeatDistance = 80;
    QgsTextFormat format;
    QFont font = format.font();
    font.setFamily(QStringLiteral("Malgun Gothic"));
    font.setBold(true);
    format.setFont(font);
    format.setSize(8);
    format.setColor(QColor(31, 35, 40));
    QgsTextBufferSettings buffer = format.buffer();
    buffer.setEnabled(true);
    buffer.setSize(0.8);
    buffer.setColor(QColor(255, 255, 255, 230));
    format.setBuffer(buffer);
    settings->setFormat(format);
    auto* labelRoot = new QgsRuleBasedLabeling::Rule(nullptr);
    labelRoot->appendChild(new QgsRuleBasedLabeling::Rule(settings, 0, 0, QStringLiteral("\"index_line\" = 1"),
                                                          QStringLiteral("계곡선 표고")));
    lines->setLabeling(new QgsRuleBasedLabeling(labelRoot));
    lines->setLabelsEnabled(true);
    tag(lines, groupTitle);
    project->addMapLayer(lines, false);
    group->insertLayer(0, lines);
  }

  if (QgsVectorLayer* points = openVector(gpkgPath, QStringLiteral("survey_points"), QStringLiteral("측량점"))) {
    points->setRenderer(new QgsSingleSymbolRenderer(QgsMarkerSymbol::createSimple(
        {{QStringLiteral("color"), QStringLiteral("#5c6b73")},
         {QStringLiteral("size"), QStringLiteral("1.6")},
         {QStringLiteral("size_unit"), QStringLiteral("MM")}}).release()));
    tag(points, groupTitle);
    project->addMapLayer(points, false);
    QgsLayerTreeLayer* node = group->insertLayer(0, points);
    if (node) node->setItemVisibilityChecked(false);
  }
  return true;
}
