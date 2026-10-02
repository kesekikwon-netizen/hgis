#include "HeritageNearbyRegions.h"

#include "HeritageRegionResolver.h"
#include "VworldSettings.h"

#include <qgscoordinatereferencesystem.h>
#include <qgscoordinatetransform.h>
#include <qgsexception.h>
#include <qgsfeature.h>
#include <qgsfeatureiterator.h>
#include <qgsfeaturerequest.h>
#include <qgsgeometryengine.h>
#include <qgsjsonutils.h>
#include <qgsmaplayer.h>
#include <qgsproject.h>
#include <qgsrectangle.h>
#include <qgsvectorlayer.h>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrlQuery>

#include <memory>

namespace {

QgsCoordinateReferenceSystem wgs84() {
  return QgsCoordinateReferenceSystem(QStringLiteral("EPSG:4326"));
}

// 시·군·구 name columns of common boundary layers. District-level only: an 읍면동
// name alone does not resolve to a 시/군 and is skipped by fromAddressText.
const char* const kDistrictNameFields[] = {"full_nm", "SIG_KOR_NM", "sig_kor_nm", "SGG_NM",
                                           "sgg_nm",  "adm_nm",     "NAME",       "name"};

}  // namespace

void HeritageNearbyRegions::appendUnique(QList<HeritageCity>* list, const HeritageCity& city) {
  if (!list || !city.ok()) return;
  for (const HeritageCity& kept : std::as_const(*list))
    if (kept.sameAs(city)) return;
  list->append(city);
}

QgsGeometry HeritageNearbyRegions::scopeWgs84(const QgsGeometry& surveyArea,
                                              const QgsCoordinateReferenceSystem& crs,
                                              double meters) {
  if (surveyArea.isNull() || surveyArea.isEmpty() || !crs.isValid()) return {};
  if (crs.mapUnits() != Qgis::DistanceUnit::Meters || !(meters > 0.)) return {};
  QgsGeometry scope = surveyArea.buffer(meters, 24);
  if (scope.isNull() || scope.isEmpty()) return {};
  try {
    const QgsCoordinateTransform toWgs(crs, wgs84(), QgsCoordinateTransformContext());
    if (scope.transform(toWgs) != Qgis::GeometryOperationResult::Success) return {};
  } catch (const QgsCsException&) {
    return {};
  }
  return scope;
}

QUrl HeritageNearbyRegions::districtsUrl(const QString& apiKey, const QgsRectangle& box) {
  QUrl url(QStringLiteral("https://api.vworld.kr/req/data"));
  QUrlQuery q;
  q.addQueryItem(QStringLiteral("service"), QStringLiteral("data"));
  q.addQueryItem(QStringLiteral("version"), QStringLiteral("2.0"));
  q.addQueryItem(QStringLiteral("request"), QStringLiteral("GetFeature"));
  q.addQueryItem(QStringLiteral("format"), QStringLiteral("json"));
  q.addQueryItem(QStringLiteral("data"), QStringLiteral("LT_C_ADSIGG_INFO"));
  q.addQueryItem(QStringLiteral("size"), QStringLiteral("100"));
  q.addQueryItem(QStringLiteral("page"), QStringLiteral("1"));
  q.addQueryItem(QStringLiteral("crs"), QStringLiteral("EPSG:4326"));
  q.addQueryItem(QStringLiteral("geometry"), QStringLiteral("true"));
  q.addQueryItem(QStringLiteral("attribute"), QStringLiteral("true"));
  q.addQueryItem(QStringLiteral("geomFilter"),
                 QStringLiteral("BOX(%1,%2,%3,%4)")
                     .arg(box.xMinimum(), 0, 'f', 8)
                     .arg(box.yMinimum(), 0, 'f', 8)
                     .arg(box.xMaximum(), 0, 'f', 8)
                     .arg(box.yMaximum(), 0, 'f', 8));
  q.addQueryItem(QStringLiteral("key"), apiKey);
  url.setQuery(q);
  return url;
}

QList<HeritageCity> HeritageNearbyRegions::parseDistricts(const QByteArray& body,
                                                          const QgsGeometry& scopeWgs, bool* ok,
                                                          bool* more) {
  if (ok) *ok = false;
  if (more) *more = false;
  QList<HeritageCity> out;
  if (scopeWgs.isNull() || scopeWgs.isEmpty()) return out;
  const QJsonObject response =
      QJsonDocument::fromJson(body).object().value(QStringLiteral("response")).toObject();
  const QString status = response.value(QStringLiteral("status")).toString();
  // NOT_FOUND is a valid answer: no 시·군·구 in the box (for example the open sea).
  if (status.compare(QLatin1String("NOT_FOUND"), Qt::CaseInsensitive) == 0) {
    if (ok) *ok = true;
    return out;
  }
  if (status.compare(QLatin1String("OK"), Qt::CaseInsensitive) != 0) return out;

  std::unique_ptr<QgsGeometryEngine> inside(QgsGeometry::createGeometryEngine(scopeWgs.constGet()));
  inside->prepareGeometry();
  const QJsonObject collection = response.value(QStringLiteral("result"))
                                     .toObject()
                                     .value(QStringLiteral("featureCollection"))
                                     .toObject();
  const QJsonArray features = collection.value(QStringLiteral("features")).toArray();
  for (const QJsonValue& value : features) {
    const QJsonObject feature = value.toObject();
    const QString name = feature.value(QStringLiteral("properties"))
                             .toObject()
                             .value(QStringLiteral("full_nm"))
                             .toString();
    const QgsGeometry boundary = QgsJsonUtils::geometryFromGeoJson(QString::fromUtf8(
        QJsonDocument(feature.value(QStringLiteral("geometry")).toObject())
            .toJson(QJsonDocument::Compact)));
    if (name.isEmpty() || boundary.isNull() || boundary.isEmpty()) continue;
    if (!inside->intersects(boundary.constGet())) continue;
    const HeritageRegion region = HeritageRegionResolver::fromAddressText(name);
    appendUnique(&out, {region.sido, region.city});
  }
  const QJsonObject page = response.value(QStringLiteral("page")).toObject();
  const int total = page.value(QStringLiteral("total")).toVariant().toInt();
  if (more) *more = total > 1;
  if (ok) *ok = true;
  return out;
}

QList<HeritageCity> HeritageNearbyRegions::fromBoundaryLayers(QgsProject* project,
                                                              const QgsGeometry& scopeWgs) {
  QList<HeritageCity> out;
  if (!project || scopeWgs.isNull() || scopeWgs.isEmpty()) return out;
  for (QgsMapLayer* ml : project->mapLayers()) {
    auto* vl = qobject_cast<QgsVectorLayer*>(ml);
    if (!vl || !vl->isValid() || vl->geometryType() != Qgis::GeometryType::Polygon) continue;
    int nameIdx = -1;
    for (const char* candidate : kDistrictNameFields) {
      nameIdx = vl->fields().indexOf(QString::fromLatin1(candidate));
      if (nameIdx >= 0) break;
    }
    if (nameIdx < 0) continue;
    QgsGeometry scope = scopeWgs;
    if (vl->crs().isValid() && vl->crs() != wgs84()) {
      try {
        const QgsCoordinateTransform tr(wgs84(), vl->crs(), project);
        if (scope.transform(tr) != Qgis::GeometryOperationResult::Success) continue;
      } catch (const QgsCsException&) {
        continue;
      }
    }
    std::unique_ptr<QgsGeometryEngine> inside(QgsGeometry::createGeometryEngine(scope.constGet()));
    inside->prepareGeometry();
    QgsFeatureRequest request;
    request.setFilterRect(scope.boundingBox());
    request.setSubsetOfAttributes(QgsAttributeList{nameIdx});
    QgsFeatureIterator it = vl->getFeatures(request);
    QgsFeature f;
    while (it.nextFeature(f)) {
      if (!f.hasGeometry() || !inside->intersects(f.geometry().constGet())) continue;
      const HeritageRegion hit = HeritageRegionResolver::fromAddressText(f.attribute(nameIdx).toString());
      appendUnique(&out, {hit.sido, hit.city});
    }
  }
  return out;
}

// --- HeritageRegionResolver: nearby phase (kept here so the resolver stays small) ---

void HeritageRegionResolver::emitWithNearby(HeritageRegion region, const QList<HeritageCity>& found,
                                            const QString& note) {
  region.nearby.clear();
  const HeritageCity first{region.sido, region.city};
  for (const HeritageCity& city : found)
    if (!city.sameAs(first)) HeritageNearbyRegions::appendUnique(&region.nearby, city);
  region.nearbyNote = note;
  emit resolved(region);
}

// 시/군 경계에 걸친 조사가 흔하다. 대표점 한 곳으로 정한 시/군 말고도 주변 5km가 걸치는
// 시/군을 찾아 확인창에 체크 목록으로 내놓는다. 기본 선택은 판정한 시/군 하나뿐이다.
void HeritageRegionResolver::finishResolved(const HeritageRegion& region, bool allowNetwork) {
  if (m_scope.isNull() || m_scope.isEmpty()) {
    emitWithNearby(region, {},
                   QStringLiteral("조사구역 좌표계로 주변 5km 범위를 만들지 못해 이웃 시·군은 확인하지 않았습니다."));
    return;
  }
  const QList<HeritageCity> local =
      HeritageNearbyRegions::fromBoundaryLayers(m_fallbackProject.data(), m_scope);
  const QString storedKey = VworldSettings::loadApiKey().trimmed();
  const QString key = allowNetwork ? storedKey : QString();
  if (key.isEmpty()) {
    const QString why =
        storedKey.isEmpty()
            ? QStringLiteral("VWorld API 키가 없어 주변 5km에 걸친 이웃 시·군은 확인하지 못했습니다.")
            : QStringLiteral("행정구역 서버가 늦어 주변 5km에 걸친 이웃 시·군은 확인하지 못했습니다.");
    emitWithNearby(region, local, local.isEmpty() ? why : QString());
    return;
  }
  m_primary = region;
  m_nearbyPhase = true;
  m_pending = true;
  m_deadline.start(qMin(m_timeoutMs, 8000));
  QNetworkRequest req(HeritageNearbyRegions::districtsUrl(key, m_scope.boundingBox()));
  req.setHeader(QNetworkRequest::UserAgentHeader,
                QStringLiteral("Mozilla/5.0 (Windows NT 10.0; Win64; x64) ka-hgis/0.3"));
  req.setRawHeader("Referer", "https://localhost");
  req.setRawHeader("Accept", "application/json");
  req.setTransferTimeout(m_timeoutMs);
  QNetworkReply* reply = m_nam->get(req);
  m_reply = reply;
  connect(reply, &QNetworkReply::finished, this, [this, reply, local]() {
    m_deadline.stop();
    m_reply.clear();
    m_pending = false;
    m_nearbyPhase = false;
    reply->deleteLater();
    bool ok = false;
    bool more = false;
    QList<HeritageCity> found;
    if (reply->error() == QNetworkReply::NoError)
      found = HeritageNearbyRegions::parseDistricts(reply->readAll(), m_scope, &ok, &more);
    QString note;
    if (!ok) {
      found = local;
      note = QStringLiteral("행정구역 서버에서 주변 5km 이웃 시·군을 확인하지 못했습니다.");
    } else if (more) {
      note = QStringLiteral("주변 5km 이웃 시·군이 많아 일부만 보입니다.");
    }
    emitWithNearby(m_primary, found, note);
  });
}
