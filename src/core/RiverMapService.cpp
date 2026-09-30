#include "RiverMapService.h"
#include "LayerOps.h"
#include "ReferenceKind.h"
#include "ReferenceTiledFetch.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QUrl>
#include <algorithm>

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
#include <qgsrectangle.h>
#include <qgstextformat.h>
#include <qgsvectorfilewriter.h>
#include <qgsvectorlayer.h>
#include <qgsvectorlayerlabeling.h>

#include <QNetworkRequest>

namespace {

constexpr const char* kWfsUrl = "https://api.vworld.kr/req/wfs";
constexpr const char* kTypeName = "lt_c_wkmstrm";
constexpr const char* kLayerTitle = "수계도(하천망)";
constexpr const char* kNameField = "riv_nm";
constexpr const char* kLevelField = "riv_level";

// 하천 등급별 물색 — 상위 등급일수록 진하게(보고서 수계도 관례).
struct LevelDef {
  const char* keyword;  // riv_level에 포함되는 문자열
  const char* label;    // 범례 표기
  int r, g, b;
};
constexpr LevelDef kLevels[] = {
    {"국가", "국가하천", 0, 126, 212},
    {"지방1급", "지방1급하천", 16, 146, 222},
    {"지방2급", "지방2급하천", 40, 166, 232},
};

int levelRank(const QString& levelText) {
  int i = 0;
  for (const LevelDef& d : kLevels) {
    if (levelText.contains(QString::fromUtf8(d.keyword))) return i;
    ++i;
  }
  return int(std::size(kLevels));
}

QgsSymbol* waterFillSymbol(const QColor& base) {
  QColor fill = base;
  fill.setAlpha(230);
  QColor line = base.darker(150);
  line.setAlpha(255);
  auto fs = QgsFillSymbol::createSimple({
      {QStringLiteral("color"), fill.name(QColor::HexArgb)},
      {QStringLiteral("outline_color"), line.name(QColor::HexArgb)},
      {QStringLiteral("outline_width"), QStringLiteral("0.32")},
      {QStringLiteral("outline_width_unit"), QStringLiteral("MM")},
  });
  return fs.release();
}

// VWorld가 200 + XML(ServiceException)로 돌려주는 오류에서 한국어 사유를 뽑는다.
QString vworldExceptionText(const QByteArray& body) {
  static const QRegularExpression re(
      QStringLiteral("<ServiceException[^>]*>([^<]+)</ServiceException>"));
  const QRegularExpressionMatch m = re.match(QString::fromUtf8(body));
  return m.hasMatch() ? m.captured(1).trimmed() : QString();
}

// bbox(위경도) 한 조각의 하천망 GeoJSON을 받는다.
// VWorld WFS는 인증키와 DOMAIN 파라미터가 필요하고(배경지도와 같은 키),
// MAXFEATURES 상한이 1000이라 STARTINDEX로 나눠 받는다.
ReferenceTiledFetch::Piece fetchRiverPiece(const QgsRectangle& extent4326, const QString& apiKey,
                                           QgsFeedback* feedback, const ReferenceDownload& download) {
  using ReferenceTiledFetch::Outcome;
  ReferenceTiledFetch::Piece piece;
  QString* errorOut = &piece.error;
  // Every non-transport failure below is a server/content problem.
  piece.outcome = Outcome::Fatal;
  constexpr int kPageSize = 1000;
  constexpr int kMaxPages = 10;
  QJsonObject rootDoc;
  QJsonArray allFeatures;

  for (int page = 0; page < kMaxPages; ++page) {
    const QString url =
        QStringLiteral(
            "%1?SERVICE=WFS&REQUEST=GetFeature&VERSION=2.0.0&TYPENAME=%2"
            "&OUTPUT=application/json&SRSNAME=EPSG:4326&MAXFEATURES=%3&STARTINDEX=%4"
            // DOMAIN 은 보내지 않는다 — VWorld 는 DOMAIN=localhost 가 붙으면 같은 키·같은
            // Referer 라도 INCORRECT_KEY 로 거절한다(2026-09-06 실측: 붙이면 407바이트
            // 오류, 빼면 2.4 MB 피처). 인증은 아래 Referer 헤더가 한다.
            "&BBOX=%5,%6,%7,%8,urn:ogc:def:crs:EPSG::4326&KEY=%9")
            .arg(QLatin1String(kWfsUrl), QLatin1String(kTypeName))
            .arg(kPageSize)
            .arg(page * kPageSize)
            .arg(extent4326.yMinimum(), 0, 'f', 8)
            .arg(extent4326.xMinimum(), 0, 'f', 8)
            .arg(extent4326.yMaximum(), 0, 'f', 8)
            .arg(extent4326.xMaximum(), 0, 'f', 8)
            .arg(apiKey);

    QNetworkRequest netReq{QUrl(url)};
    netReq.setHeader(QNetworkRequest::UserAgentHeader,
                     QStringLiteral("Mozilla/5.0 (Windows NT 10.0; Win64; x64) ka-hgis/0.3"));
    netReq.setRawHeader("Referer", "https://localhost");
    QByteArray body;
    ReferenceTransferFailure failure = ReferenceTransferFailure::None;
    if (!ReferenceMapPreparation::download(netReq, &body, errorOut, feedback, download,
                                           ReferenceMapPreparation::kLargeTransfer, &failure)) {
      piece.outcome = failure == ReferenceTransferFailure::Cancelled ? Outcome::Cancelled
          : failure == ReferenceTransferFailure::Timeout ? Outcome::Slow : Outcome::Transient;
      return piece;
    }

    if (body.isEmpty() || !body.trimmed().startsWith('{')) {
      const QString reason = vworldExceptionText(body);
      if (errorOut)
        *errorOut = reason.isEmpty()
                        ? QStringLiteral("서버가 GeoJSON 대신 다른 응답을 보냈습니다. "
                                         "VWorld 인증키를 확인하세요.")
                        : (reason.contains(QLatin1String("KEY"), Qt::CaseInsensitive)
                               ? QStringLiteral("VWorld 인증키가 거부되었습니다. 배경지도 설정에서 키를 확인한 뒤 다시 내려받으세요.")
                               : QStringLiteral("VWorld가 하천 지도를 보내지 못했습니다. 인터넷 연결을 확인한 뒤 잠시 후 다시 내려받으세요."));
      return piece;
    }
    if (!ReferenceMapPreparation::validateFeatureCollection(body, errorOut)) return piece;
    const QJsonDocument doc = QJsonDocument::fromJson(body);
    if (!doc.isObject()) {
      if (errorOut) *errorOut = QStringLiteral("VWorld 응답(JSON)을 해석하지 못했습니다.");
      return piece;
    }
    const QJsonObject obj = doc.object();
    const QJsonArray feats = obj.value(QStringLiteral("features")).toArray();
    if (page == 0) rootDoc = obj;
    for (const QJsonValue& v : feats) allFeatures.append(v);
    if (feats.size() < kPageSize) break;  // 마지막 페이지
    if (page + 1 == kMaxPages) {
      if (errorOut) *errorOut = QStringLiteral("하천 데이터가 한 번에 받을 수 있는 양을 넘었습니다. 범위를 좁혀 다시 내려받으세요. 기존 지도는 유지됩니다.");
      piece.outcome = Outcome::TooLarge;  // A smaller bbox needs fewer pages.
      return piece;
    }
  }

  rootDoc.insert(QStringLiteral("features"), allFeatures);
  piece.body = QJsonDocument(rootDoc).toJson(QJsonDocument::Compact);
  if (!ReferenceMapPreparation::validateCompleteFeatureCollection(piece.body, errorOut)) {
    piece.body.clear();
    piece.outcome = Outcome::TooLarge;
    return piece;
  }
  piece.outcome = Outcome::Complete;
  return piece;
}

// 현재 화면 bbox(위경도)의 하천망 GeoJSON을 임시 파일로 받아 경로를 돌려준다.
// 쪽수 상한을 넘거나 느린 범위는 네 조각으로 나눠 받고, 끊긴 조각은 한 번 더 받는다.
QString fetchRiverGeojson(const QgsRectangle& extent4326, const QString& apiKey,
                          QString* errorOut, const QString& path, QgsFeedback* feedback,
                          const ReferenceDownload& download) {
  const auto fetched = ReferenceTiledFetch::fetch(extent4326, [&](const QgsRectangle& piece) {
    return fetchRiverPiece(piece, apiKey, feedback, download);
  }, feedback);
  if (!fetched.ok) {
    if (errorOut) *errorOut = fetched.error;
    return {};
  }
  if (!ReferenceMapPreparation::writeResponse(path, fetched.body, errorOut)) return {};
  return path;
}

}  // namespace

bool RiverMapService::applyRiverStyle(QgsVectorLayer* layer) {
  if (!layer || !layer->isValid()) return false;
  const int levelIdx = layer->fields().indexOf(QLatin1String(kLevelField));
  const int nameIdx = layer->fields().indexOf(QLatin1String(kNameField));
  if (levelIdx < 0 && nameIdx < 0) return false;

  // 데이터에 실제로 있는 등급만 범례에 올린다(상위 등급 먼저).
  QStringList present;
  if (levelIdx >= 0) {
    QgsFeatureIterator it = layer->getFeatures();
    QgsFeature f;
    while (it.nextFeature(f)) {
      const QString lv = f.attribute(levelIdx).toString();
      if (!lv.isEmpty() && !present.contains(lv)) present << lv;
    }
  }
  std::sort(present.begin(), present.end(), [](const QString& a, const QString& b) {
    const int ra = levelRank(a), rb = levelRank(b);
    if (ra != rb) return ra < rb;
    return a < b;
  });

  QgsCategoryList cats;
  for (const QString& lv : present) {
    const int rank = levelRank(lv);
    const QColor col = rank < int(std::size(kLevels))
                           ? QColor(kLevels[rank].r, kLevels[rank].g, kLevels[rank].b)
                           : QColor(120, 196, 236);
    if (QgsSymbol* sym = waterFillSymbol(col))
      cats.append(QgsRendererCategory(QVariant(lv), sym, lv));
  }
  if (QgsSymbol* rest = waterFillSymbol(QColor(120, 196, 236)))
    cats.append(QgsRendererCategory(QVariant(), rest, QStringLiteral("기타 수계")));
  layer->setRenderer(
      new QgsCategorizedSymbolRenderer(QLatin1String(kLevelField), cats));

  // 하천명(riv_nm) 라벨 — 파란 글씨 + 흰 테두리(수계도 관례).
  if (nameIdx >= 0) {
    QgsPalLayerSettings s;
    s.drawLabels = true;
    s.fieldName = QLatin1String(kNameField);
    s.isExpression = false;
    s.placement = Qgis::LabelPlacement::OverPoint;
    s.setPolygonPlacementFlags(Qgis::LabelPolygonPlacementFlag::AllowPlacementInsideOfPolygon);
    QgsLabelObstacleSettings obs = s.obstacleSettings();
    obs.setIsObstacle(false);
    s.setObstacleSettings(obs);

    QgsTextFormat fmt;
    QFont font = fmt.font();
    font.setFamily(QStringLiteral("Malgun Gothic"));
    font.setPointSize(8);
    font.setBold(true);
    fmt.setFont(font);
    fmt.setSize(8);
    fmt.setSizeUnit(Qgis::RenderUnit::Points);
    fmt.setColor(QColor(0, 72, 186));
    QgsTextBufferSettings buf = fmt.buffer();
    buf.setEnabled(true);
    buf.setSize(0.7);
    buf.setColor(QColor(255, 255, 255, 225));
    fmt.setBuffer(buf);
    s.setFormat(fmt);

    layer->setLabeling(new QgsVectorLayerSimpleLabeling(s));
    layer->setLabelsEnabled(true);
  }
  layer->triggerRepaint();
  return true;
}

QgsVectorLayer* RiverMapService::downloadAndAdd(QgsProject* project, QgsMapCanvas* canvas,
    const QgsRectangle& extent5186, const QString& apiKey, const QString& outGpkgPath,
    QString* errorOut) {
  if (!project) return nullptr;
  const auto prepared = prepare(extent5186, apiKey, outGpkgPath, project->transformContext());
  return addPrepared(project, canvas, prepared, errorOut);
}

PreparedReferenceMap RiverMapService::prepare(const QgsRectangle& extent5186,
    const QString& apiKey, const QString& requestedBasePath,
    const QgsCoordinateTransformContext& transformContext, QgsFeedback* feedback,
    const ReferenceDownload& download) {
  PreparedReferenceMap result;
  result.tableName = QStringLiteral("river_map");
  if (ReferenceMapPreparation::cancelled(result, feedback)) return result;
  if (apiKey.trimmed().isEmpty()) {
    result.error = QStringLiteral(
          "VWorld 인증키가 없습니다. 지도 탭의 배경지도 설정에서 키를 먼저 등록하세요.");
    return result;
  }
  const QgsRectangle fetch5186 =
      LayerOps::expandExtentToMaxSpan(extent5186, maxSpanMeters());
  if (fetch5186.isEmpty() || !fetch5186.isFinite() || fetch5186.width() > maxSpanMeters() ||
      fetch5186.height() > maxSpanMeters()) {
    result.error = QStringLiteral(
          "범위가 너무 넓습니다. 지도를 조사구역(한 변 %1km 이하)으로 확대한 뒤 다시 "
          "내려받으세요.")
          .arg(maxSpanMeters() / 1000.0, 0, 'f', 0);
    return result;
  }

  QgsRectangle ext4326;
  try {
    const QgsCoordinateTransform tr(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")),
                                    QgsCoordinateReferenceSystem(QStringLiteral("EPSG:4326")),
                                    transformContext);
    ext4326 = tr.transformBoundingBox(fetch5186);
  } catch (const QgsException&) {
    result.error = QStringLiteral("좌표 변환에 실패했습니다.");
    return result;
  }

  if (!ReferenceMapPreparation::initializeStorage(result, requestedBasePath)) return result;
  QString netErr;
  const QString jsonPath = fetchRiverGeojson(ext4326, apiKey, &netErr,
      QDir(result.storage->path()).filePath(QStringLiteral("river.geojson")), feedback, download);
  if (ReferenceMapPreparation::cancelled(result, feedback)) return result;
  if (jsonPath.isEmpty()) {
    result.error = netErr.isEmpty() ? QStringLiteral("수계도를 내려받지 못했습니다.")
                                   : QStringLiteral("VWorld 서버 연결 실패: %1").arg(netErr);
    return result;
  }

  QgsVectorLayer src(jsonPath, QStringLiteral("part"), QStringLiteral("ogr"));
  if (!src.isValid() || src.featureCount() == 0) {
    QFile::remove(jsonPath);
    result.error = QStringLiteral("이 범위에는 하천망 데이터가 없습니다. "
                                 "(국가·지방하천이 없는 지역입니다)");
    return result;
  }

  // 4326 응답을 5186으로 재투영하며 메모리 레이어로 복사한다.
  auto merged = std::make_unique<QgsVectorLayer>(QStringLiteral("MultiPolygon?crs=EPSG:5186"),
                                    QStringLiteral("merge"), QStringLiteral("memory"));
  if (!merged->dataProvider()->addAttributes(src.fields().toList())) {
    result.error = QStringLiteral("하천 지도 항목을 준비하지 못했습니다. 다시 내려받으세요.");
    return result;
  }
  merged->updateFields();

  const QgsFields memFields = merged->fields();
  const QgsFields srcFields = src.fields();
  const QgsCoordinateTransform to5186(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:4326")),
                                      QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")),
                                      transformContext);

  QgsFeatureList batch;
  QgsFeatureIterator it = src.getFeatures();
  QgsFeature f;
  while (it.nextFeature(f)) {
    if (ReferenceMapPreparation::cancelled(result, feedback)) return result;
    QgsFeature nf(memFields);
    for (int i = 0; i < srcFields.count(); ++i) {
      const int dst = memFields.indexOf(srcFields.at(i).name());
      if (dst >= 0) nf.setAttribute(dst, f.attribute(i));
    }
    QgsGeometry g = f.geometry();
    try {
      if (g.transform(to5186) != Qgis::GeometryOperationResult::Success) {
        result.error = QStringLiteral("하천 좌표를 변환하지 못했습니다. 기존 지도는 유지됩니다. 다시 내려받으세요.");
        return result;
      }
    } catch (const QgsException&) {
      result.error = QStringLiteral("하천 좌표를 변환하지 못했습니다. 기존 지도는 유지됩니다. 다시 내려받으세요.");
      return result;
    }
    if (g.isNull() || g.isEmpty() || !g.convertToMultiType()) {
      result.error = QStringLiteral("하천 지도에 읽을 수 없는 구역이 있습니다. 기존 지도는 유지됩니다.");
      return result;
    }
    nf.setGeometry(g);
    batch.append(nf);
  }
  QFile::remove(jsonPath);
  if (batch.isEmpty()) {
    result.error = QStringLiteral("이 범위에는 하천망 데이터가 없습니다.");
    return result;
  }
  if (!merged->dataProvider()->addFeatures(batch)) {
    result.error = QStringLiteral("하천 지도 일부를 준비하지 못했습니다. 기존 지도는 유지됩니다.");
    return result;
  }
  merged->updateExtents();

  ReferenceMapPreparation::saveVector(result, merged.get(), transformContext, feedback);
  return result;
}

QgsVectorLayer* RiverMapService::addPrepared(QgsProject* project, QgsMapCanvas* canvas,
    const PreparedReferenceMap& prepared, QString* errorOut) {
  if (!project || !prepared.isReady()) {
    if (errorOut) *errorOut = prepared.error;
    return nullptr;
  }
  QStringList removeIds;
  for (QgsMapLayer* old : project->mapLayers()) {
    // By title, or (renamed) by identity: tagged river and read from our GPKG table.
    const bool renamedOurs = ReferenceKind::of(old) == QLatin1String(ReferenceKind::kRiver) &&
                             old->source().contains(QLatin1String("layername=river_map"));
    if (old && (old->name() == QString::fromUtf8(kLayerTitle) ||
        old->name().startsWith(QString::fromUtf8(kLayerTitle) + QStringLiteral(" [")) ||
        renamedOurs))
      removeIds.append(old->id());
  }
  auto* layer = new QgsVectorLayer(prepared.gpkgPath + QStringLiteral("|layername=river_map"),
                                   QString::fromUtf8(kLayerTitle), QStringLiteral("ogr"));
  if (!layer->isValid()) {
    if (errorOut) *errorOut = QStringLiteral("저장한 수계도를 여는 데 실패했습니다.");
    delete layer;
    return nullptr;
  }
  if (!applyRiverStyle(layer)) {
    if (errorOut) *errorOut = QStringLiteral("받은 지도에 필요한 항목이 없습니다. 기존 지도는 유지됩니다. 잠시 후 다시 내려받으세요.");
    delete layer;
    return nullptr;
  }
  LayerOps::markReferenceLayer(layer);
  // Toggles find the layer by kind, so renaming it in the legend keeps the button working.
  ReferenceKind::tag(layer, QString::fromLatin1(ReferenceKind::kRiver));
  LayerOps::applyLegendCrsLabel(layer);
  if (!project->addMapLayer(layer, true)) {
    delete layer;
    if (errorOut) *errorOut = QStringLiteral("수계도 레이어를 프로젝트에 넣지 못했습니다.");
    return nullptr;
  }
  prepared.retainFiles();
  for (const QString& id : removeIds) project->removeMapLayer(id);
  LayerOps::placeInLegendGroup(project, layer, QStringLiteral("참조 지도"));
  LayerOps::applyThematicOverlayScaleRange(layer);
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
