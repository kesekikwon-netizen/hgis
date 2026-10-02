#include "SoilMapService.h"
#include "LayerOps.h"
#include "ReferenceKind.h"

#include <qgis.h>
#include <qgsmaplayerlegend.h>
#include <qgslayertree.h>
#include <qgslayertreelayer.h>

#include <QDir>
#include <QStringList>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QUrl>

#include <qgsblockingnetworkrequest.h>
#include <qgscategorizedsymbolrenderer.h>
#include <qgscoordinatereferencesystem.h>
#include <qgscoordinatetransform.h>
#include <qgscoordinatetransformcontext.h>
#include <qgsexception.h>
#include <qgsfeature.h>
#include <qgsfeatureiterator.h>
#include <qgsfields.h>
#include <qgsfillsymbol.h>
#include <qgsgeometry.h>
#include <qgsmapcanvas.h>
#include <qgspallabeling.h>
#include <qgsproject.h>
#include <qgsrasterlayer.h>
#include <qgsrectangle.h>
#include <qgstextformat.h>
#include <qgsvectorfilewriter.h>
#include <qgsvectorlayer.h>
#include <qgsvectorlayerlabeling.h>

#include <QFont>
#include <QNetworkRequest>

namespace {

constexpr const char* kWfsBase = "https://gis.naas.go.kr/geoserver/soilmap/ows";
constexpr const char* kTerrainField = "soil_type_geo";
constexpr const char* kLayerTitle = "토양도(흙토람)";
constexpr const char* kPictureTitle = "토양도(흙토람 그림)";

// 흙토람 범례 API(mapService_select_color_data, colorgoup=PTR)의 공식 팔레트.
struct TerrainClass {
  const char* code;
  const char* name;
  int r, g, b;
};
constexpr TerrainClass kTerrain[] = {
    {"01", "산악지", 1, 178, 0},
    {"02", "구릉지", 177, 210, 93},
    {"03", "산록경사지", 139, 68, 108},
    {"04", "곡간지/선상지", 254, 244, 182},
    {"05", "해성평탄지", 132, 186, 230},
    {"06", "하성평탄지", 192, 239, 245},
    {"07", "고원지", 78, 78, 78},
    {"08", "홍적대지", 254, 160, 0},
    {"09", "용암류대지", 192, 151, 155},
    {"10", "용암류평탄", 194, 179, 182},
    {"99", "기타", 255, 255, 255},
};

QgsSymbol* terrainFillSymbol(const QColor& base) {
  QColor fill = base;
  fill.setAlpha(175);
  auto fs = QgsFillSymbol::createSimple({
      {QStringLiteral("color"), fill.name(QColor::HexArgb)},
      {QStringLiteral("outline_color"), QColor(40, 44, 52, 180).name(QColor::HexArgb)},
      {QStringLiteral("outline_width"), QStringLiteral("0.28")},
      {QStringLiteral("outline_width_unit"), QStringLiteral("MM")},
  });
  return fs.release();
}

QNetworkRequest soilWfsRequest(const QString& url) {
  QNetworkRequest netReq{QUrl(url)};
  netReq.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("ka-hgis/0.3"));
  return netReq;
}

}  // namespace

QString SoilMapService::wfsGetFeatureUrl(int tableNo, const QgsRectangle& extent4326,
                                         bool hitsOnly) {
  QString url =
      QStringLiteral("%1?service=WFS&version=2.0.0&request=GetFeature&typeNames=soilmap:SOIL_%2"
                     "&srsName=EPSG:5186&count=100000")
          .arg(QLatin1String(kWfsBase))
          .arg(tableNo);
  if (hitsOnly) {
    url += QStringLiteral("&resultType=hits");
  } else {
    url += QStringLiteral("&outputFormat=application/json&propertyName=%1,geom")
               .arg(QLatin1String(kTerrainField));
  }
  url += QStringLiteral("&bbox=%1,%2,%3,%4,urn:ogc:def:crs:EPSG::4326")
             .arg(extent4326.yMinimum(), 0, 'f', 8)
             .arg(extent4326.xMinimum(), 0, 'f', 8)
             .arg(extent4326.yMaximum(), 0, 'f', 8)
             .arg(extent4326.xMaximum(), 0, 'f', 8);
  return url;
}

QString SoilMapService::terrainPictureUri() {
  return QStringLiteral(
      "type=xyz&url=https://gis.naas.go.kr/Geodata/SF/TOP_A_SOIL_T_GEO/Layers/_alllayers/"
      "L%7Bz%7D/R%7By%7D/C%7Bx%7D.png&zmax=15&zmin=6&crs=EPSG:3857&tilePixelRatio=1");
}

QUrl SoilMapService::rewriteArcGisCacheUrl(const QUrl& url) {
  if (url.host().compare(QLatin1String("gis.naas.go.kr"), Qt::CaseInsensitive) != 0)
    return url;
  const QString path = url.path();
  if (!path.contains(QLatin1String("/TOP_A_SOIL_T_GEO/"), Qt::CaseInsensitive))
    return url;
  static const QRegularExpression re(
      QStringLiteral("/L(\\d+)/R(\\d+)/C(\\d+)\\.png$"),
      QRegularExpression::CaseInsensitiveOption);
  const QRegularExpressionMatch m = re.match(path);
  if (!m.hasMatch()) return url;
  const int z = m.captured(1).toInt();
  const int y = m.captured(2).toInt();
  const int x = m.captured(3).toInt();
  if (z < 0 || y < 0 || x < 0) return url;
  const QString padded =
      QStringLiteral("/L%1/R%2/C%3.png")
          .arg(z, 2, 10, QLatin1Char('0'))
          .arg(y, 8, 16, QLatin1Char('0'))
          .arg(x, 8, 16, QLatin1Char('0'));
  QUrl out = url;
  out.setPath(path.left(m.capturedStart()) + padded);
  return out;
}

QString SoilMapService::terrainName(const QString& code) {
  for (const TerrainClass& t : kTerrain)
    if (code == QLatin1String(t.code)) return QString::fromUtf8(t.name);
  return QStringLiteral("미분류");
}

QColor SoilMapService::terrainColor(const QString& code) {
  for (const TerrainClass& t : kTerrain)
    if (code == QLatin1String(t.code)) return QColor(t.r, t.g, t.b);
  return QColor(200, 200, 200);
}

bool SoilMapService::applyTerrainStyle(QgsVectorLayer* layer) {
  if (!layer || !layer->isValid()) return false;
  if (layer->fields().indexOf(QLatin1String(kTerrainField)) < 0) return false;

  QgsCategoryList cats;
  for (const TerrainClass& t : kTerrain) {
    if (QgsSymbol* sym = terrainFillSymbol(QColor(t.r, t.g, t.b)))
      cats.append(QgsRendererCategory(QVariant(QString::fromUtf8(t.code)), sym,
                                      QString::fromUtf8(t.name)));
  }
  if (QgsSymbol* rest = terrainFillSymbol(QColor(200, 200, 200)))
    cats.append(QgsRendererCategory(QVariant(), rest, QStringLiteral("미분류")));
  if (cats.isEmpty()) return false;

  layer->setRenderer(
      new QgsCategorizedSymbolRenderer(QLatin1String(kTerrainField), cats));
  applyTerrainLabels(layer);
  layer->triggerRepaint();
  return true;
}

QString SoilMapService::terrainLabelExpression(double minAreaM2, bool candidatesOnly) {
  QStringList parts;
  parts << QStringLiteral("CASE");
  if (minAreaM2 > 0.0)
    parts << QStringLiteral("WHEN area($geometry) < %1 THEN ''").arg(minAreaM2, 0, 'f', 1);
  if (candidatesOnly)
    parts << QStringLiteral("WHEN \"soil_type_geo\" NOT IN ('04','05','06','08') THEN ''");
  if (!candidatesOnly) {
    parts << QStringLiteral("WHEN \"soil_type_geo\"='01' THEN '산악지'");
    parts << QStringLiteral("WHEN \"soil_type_geo\"='02' THEN '구릉지'");
    parts << QStringLiteral("WHEN \"soil_type_geo\"='03' THEN '산록경사'");
  }
  parts << QStringLiteral("WHEN \"soil_type_geo\"='04' THEN '곡간·선상'");
  parts << QStringLiteral("WHEN \"soil_type_geo\"='05' THEN '해성평탄'");
  parts << QStringLiteral("WHEN \"soil_type_geo\"='06' THEN '하성평탄'");
  if (!candidatesOnly)
    parts << QStringLiteral("WHEN \"soil_type_geo\"='07' THEN '고원지'");
  parts << QStringLiteral("WHEN \"soil_type_geo\"='08' THEN '홍적대지'");
  if (!candidatesOnly) {
    parts << QStringLiteral("WHEN \"soil_type_geo\"='09' THEN '용암대지'");
    parts << QStringLiteral("WHEN \"soil_type_geo\"='10' THEN '용암평탄'");
    parts << QStringLiteral("WHEN \"soil_type_geo\"='99' THEN '기타'");
  }
  parts << QStringLiteral("ELSE '' END");
  return parts.join(QLatin1Char(' '));
}

bool SoilMapService::applyTerrainLabels(QgsVectorLayer* layer, double minAreaM2,
                                        bool candidatesOnly) {
  if (!layer || !layer->isValid()) return false;
  if (layer->fields().indexOf(QLatin1String(kTerrainField)) < 0) return false;

  const QString expr = terrainLabelExpression(minAreaM2, candidatesOnly);
  QgsPalLayerSettings s;
  s.drawLabels = true;
  s.isExpression = true;
  s.fieldName = expr;
  s.placement = Qgis::LabelPlacement::OverPoint;
  s.fitInPolygonOnly = true;
  s.setPolygonPlacementFlags(Qgis::LabelPolygonPlacementFlag::AllowPlacementInsideOfPolygon);
  QgsLabelObstacleSettings obs = s.obstacleSettings();
  obs.setIsObstacle(true);
  s.setObstacleSettings(obs);
  s.scaleVisibility = true;
  s.minimumScale = 40000.0;
  s.maximumScale = 0.0;

  QgsTextFormat fmt;
  QFont font = fmt.font();
  font.setFamily(QStringLiteral("Malgun Gothic"));
  font.setPointSize(8);
  font.setBold(true);
  fmt.setFont(font);
  fmt.setSize(8);
  fmt.setSizeUnit(Qgis::RenderUnit::Points);
  fmt.setColor(QColor(28, 32, 38));
  QgsTextBufferSettings buf = fmt.buffer();
  buf.setEnabled(true);
  buf.setSize(0.7);
  buf.setColor(QColor(255, 255, 255, 235));
  fmt.setBuffer(buf);
  s.setFormat(fmt);

  layer->setLabeling(new QgsVectorLayerSimpleLabeling(s));
  layer->setLabelsEnabled(true);
  layer->setDisplayExpression(expr);
  return true;
}

QgsVectorLayer* SoilMapService::downloadAndAdd(QgsProject* project, QgsMapCanvas* canvas,
    const QgsRectangle& extent5186, const QString& outGpkgPath, QString* errorOut) {
  if (!project) return nullptr;
  // Soil retains the actual viewport; do not expandExtentToMaxSpan here.
  const auto prepared = prepare(extent5186, outGpkgPath, project->transformContext());
  return addPrepared(project, canvas, prepared, errorOut);
}

PreparedReferenceMap SoilMapService::prepare(const QgsRectangle& extent5186,
    const QString& requestedBasePath, const QgsCoordinateTransformContext& transformContext,
    QgsFeedback* feedback, const ReferenceDownload& download) {
  PreparedReferenceMap result;
  result.tableName = QStringLiteral("soil_map");
  if (ReferenceMapPreparation::cancelled(result, feedback)) return result;
  const QgsRectangle fetch5186 = extent5186;
  if (fetch5186.isEmpty() || !fetch5186.isFinite() || fetch5186.width() > maxSpanMeters() ||
      fetch5186.height() > maxSpanMeters()) {
    result.error = QStringLiteral("토양도 범위가 너무 넓습니다. 한 변 80 km 이하로 확대하고 다시 내려받으세요.");
    return result;
  }
  QgsRectangle extent4326;
  try {
    QgsCoordinateTransform transform(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")),
        QgsCoordinateReferenceSystem(QStringLiteral("EPSG:4326")), transformContext);
    extent4326 = transform.transformBoundingBox(fetch5186);
  } catch (const QgsException&) {
    result.error = QStringLiteral("지도의 위치를 변환하지 못했습니다. 조사 좌표계를 확인하고 다시 내려받으세요.");
    return result;
  }
  if (!ReferenceMapPreparation::initializeStorage(result, requestedBasePath)) return result;
  QgsVectorLayer merged(QStringLiteral("MultiPolygon?crs=EPSG:5186"),
                         QStringLiteral("merge"), QStringLiteral("memory"));
  bool fieldsReady = false;
  for (int tableNo = 1; tableNo <= 3; ++tableNo) {
    if (ReferenceMapPreparation::cancelled(result, feedback)) return result;
    QByteArray body;
    QString hitError;
    qlonglong expectedCount = -1;
    const bool hitOk = ReferenceMapPreparation::download(
        soilWfsRequest(wfsGetFeatureUrl(tableNo, extent4326, true)), &body, &hitError, feedback, download);
    if (ReferenceMapPreparation::cancelled(result, feedback)) return result;
    if (hitOk) {
      const QRegularExpression re(QStringLiteral("numberMatched\\s*=\\s*\"(\\d+)\""));
      const auto match = re.match(QString::fromUtf8(body));
      if (match.hasMatch()) {
        const qlonglong hits = match.captured(1).toLongLong();
        expectedCount = hits;
        if (hits == 0) continue;
        if (hits > 100000) {
          result.error = QStringLiteral("토양도 데이터가 한 번에 받을 수 있는 양을 넘었습니다. 범위를 좁혀 다시 내려받으세요. 기존 지도는 유지됩니다.");
          return result;
        }
      }
    }
    // Polygon pages can be large: a progressing transfer is not cut at 15 seconds.
    if (!ReferenceMapPreparation::download(soilWfsRequest(wfsGetFeatureUrl(tableNo, extent4326, false)),
          &body, &result.error, feedback, download, ReferenceMapPreparation::kLargeTransfer) ||
        !ReferenceMapPreparation::validateCompleteFeatureCollection(body, &result.error)) {
      ReferenceMapPreparation::cancelled(result, feedback);
      return result;
    }
    const QString path = QDir(result.storage->path()).filePath(QStringLiteral("soil-%1.geojson").arg(tableNo));
    if (!ReferenceMapPreparation::writeResponse(path, body, &result.error)) return result;
    QgsVectorLayer part(path, QStringLiteral("part"), QStringLiteral("ogr"));
    if (!part.isValid() || part.featureCount() >= 100000 ||
        (expectedCount >= 0 && part.featureCount() < expectedCount)) {
      result.error = QStringLiteral("토양도 일부를 완전히 읽지 못했습니다. 범위를 좁혀 다시 내려받으세요. 기존 지도는 유지됩니다.");
      return result;
    }
    if (!fieldsReady) {
      if (!merged.dataProvider()->addAttributes(part.fields().toList())) {
        result.error = QStringLiteral("토양도 항목을 준비하지 못했습니다. 다시 내려받으세요.");
        return result;
      }
      merged.updateFields();
      fieldsReady = true;
    }
    QgsFeatureList batch;
    QgsFeature feature;
    auto iterator = part.getFeatures();
    while (iterator.nextFeature(feature)) {
      if (ReferenceMapPreparation::cancelled(result, feedback)) return result;
      QgsFeature next(merged.fields());
      for (int i = 0; i < part.fields().count(); ++i) {
        const int destination = merged.fields().indexOf(part.fields().at(i).name());
        if (destination >= 0) next.setAttribute(destination, feature.attribute(i));
      }
      QgsGeometry geometry = feature.geometry();
      if (geometry.isNull() || geometry.isEmpty() || !geometry.convertToMultiType()) {
        result.error = QStringLiteral("토양도에 읽을 수 없는 구역이 있습니다. 기존 지도는 유지됩니다. 잠시 후 다시 내려받으세요.");
        return result;
      }
      next.setGeometry(geometry);
      batch.append(next);
    }
    if (!merged.dataProvider()->addFeatures(batch)) {
      result.error = QStringLiteral("토양도 일부를 합치지 못했습니다. 기존 지도는 유지됩니다. 다시 내려받으세요.");
      return result;
    }
  }
  if (merged.featureCount() == 0) {
    result.error = QStringLiteral("이 범위에는 토양도 데이터가 없습니다. 다른 위치를 확인하세요. 기존 지도는 유지됩니다.");
    return result;
  }
  merged.updateExtents();
  ReferenceMapPreparation::saveVector(result, &merged, transformContext, feedback);
  return result;
}

QgsVectorLayer* SoilMapService::addPrepared(QgsProject* project, QgsMapCanvas* canvas,
    const PreparedReferenceMap& prepared, QString* errorOut) {
  if (!project || !prepared.isReady()) {
    if (errorOut) *errorOut = prepared.error;
    return nullptr;
  }
  QStringList removeIds;
  for (QgsMapLayer* old : project->mapLayers()) {
    // By title, or (renamed) by identity: tagged soil and read from our GPKG table.
    // An imported soil SHP is tagged soil too but is never replaced here.
    const bool renamedOurs = ReferenceKind::of(old) == QLatin1String(ReferenceKind::kSoil) &&
                             old->source().contains(QLatin1String("layername=soil_map"));
    if (old && (old->name() == QString::fromUtf8(kLayerTitle) ||
        old->name().startsWith(QString::fromUtf8(kLayerTitle) + QStringLiteral(" [")) ||
        old->name() == QString::fromUtf8(kPictureTitle) ||
        old->name().startsWith(QString::fromUtf8(kPictureTitle) + QStringLiteral(" [")) ||
        renamedOurs))
      removeIds.append(old->id());
  }
  auto* layer = new QgsVectorLayer(prepared.gpkgPath + QStringLiteral("|layername=soil_map"),
                                   QString::fromUtf8(kLayerTitle), QStringLiteral("ogr"));
  if (!layer->isValid()) {
    if (errorOut) *errorOut = QStringLiteral("저장한 토양도를 여는 데 실패했습니다.");
    delete layer;
    return nullptr;
  }
  if (!applyTerrainStyle(layer)) {
    if (errorOut) *errorOut = QStringLiteral("받은 지도에 필요한 항목이 없습니다. 기존 지도는 유지됩니다. 잠시 후 다시 내려받으세요.");
    delete layer;
    return nullptr;
  }
  LayerOps::markReferenceLayer(layer);
  // Toggles find the layer by kind, so renaming it in the legend keeps the button working.
  ReferenceKind::tag(layer, QString::fromLatin1(ReferenceKind::kSoil));
  LayerOps::applyLegendCrsLabel(layer);

  if (!project->addMapLayer(layer, true)) {
    delete layer;
    if (errorOut) *errorOut = QStringLiteral("토양도 레이어를 프로젝트에 넣지 못했습니다.");
    return nullptr;
  }
  prepared.retainFiles();
  for (const QString& id : removeIds) project->removeMapLayer(id);
  LayerOps::placeInLegendGroup(project, layer, QStringLiteral("참조 지도"));
  LayerOps::applyThematicOverlayScaleRange(layer);

  // 그림은 벡터 아래에 배치해 구역과 글자가 계속 보이게 한다.
  auto* picture = new QgsRasterLayer(terrainPictureUri(), QString::fromUtf8(kPictureTitle),
                                     QStringLiteral("wms"));
  if (picture->isValid()) {
    picture->setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:3857")));
    LayerOps::markReferenceLayer(picture);
    picture->setCustomProperty(QStringLiteral("ka_hgis/omit_sheet_legend"), true);
    if (QgsMapLayerLegend* lg = picture->legend())
      lg->setFlag(Qgis::MapLayerLegendFlag::ExcludeByDefault, true);
    LayerOps::applyLegendCrsLabel(picture);
    if (project->addMapLayer(picture, false)) {
      QgsLayerTree* root = project->layerTreeRoot();
      QgsLayerTreeLayer* vectorNode = root->findLayer(layer->id());
      const int index = vectorNode ? root->children().indexOf(vectorNode) : -1;
      root->insertLayer(index < 0 ? -1 : index + 1, picture);
      LayerOps::placeInLegendGroup(project, picture, QStringLiteral("참조 지도"));
      LayerOps::applyThematicOverlayScaleRange(picture);
    } else {
      delete picture;
    }
  } else {
    delete picture;
  }


  if (canvas) {
    const QString workAuth = project->crs().isValid() ? project->crs().authid()
                                                      : QStringLiteral("EPSG:5186");
    LayerOps::ensureOtfEnabled(project, canvas, workAuth);
    LayerOps::syncMapCanvas(project, canvas, false);
    // refreshAllLayers()는 배경 타일 캐시까지 버려 재다운로드를 유발한다.
    LayerOps::refreshCanvasIfIdle(canvas);
  }
  return layer;
}
