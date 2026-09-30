#include "HeritageRegionResolver.h"

#include "HeritageNearbyRegions.h"
#include "KoreaRegionCatalog.h"
#include "VworldSettings.h"

#include <qgsexception.h>
#include <qgscoordinatereferencesystem.h>
#include <qgscoordinatetransform.h>
#include <qgsfeature.h>
#include <qgsfeatureiterator.h>
#include <qgsfeaturerequest.h>
#include <qgsgeometry.h>
#include <qgsmaplayer.h>
#include <qgsproject.h>
#include <qgsrectangle.h>
#include <qgsvectorlayer.h>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QStringList>
#include <QUrlQuery>

namespace {

QgsCoordinateReferenceSystem wgs84() {
  return QgsCoordinateReferenceSystem(QStringLiteral("EPSG:4326"));
}

// 시도 개략 경계 안에 드는지. 판정이 엉뚱한 도로 튀었을 때 잡아내는 값싼 확인이다.
bool insideSido(const QString& sido, const QgsPointXY& wgs) {
  const auto box = KoreaRegionCatalog::overviewBounds(sido);
  if (!box) return false;
  return wgs.x() >= box->west && wgs.x() <= box->east && wgs.y() >= box->south &&
         wgs.y() <= box->north;
}

// 행정구역 이름이 들어 있을 만한 컬럼. 앞에 있는 것부터 본다.
const char* const kNameFields[] = {"full_nm", "adm_nm",     "SIG_KOR_NM", "sig_kor_nm",
                                   "SGG_NM",  "sgg_nm",     "EMD_KOR_NM", "emd_kor_nm",
                                   "NAME",    "name"};

}  // namespace

QString HeritageRegion::display() const {
  if (!ok()) return {};
  if (sido == city) return sido;
  return sido + QLatin1Char(' ') + city;
}

HeritageRegionResolver::HeritageRegionResolver(QObject* parent)
    : HeritageRegionResolver(std::make_unique<QNetworkAccessManager>(), 15000, parent) {}

HeritageRegionResolver::HeritageRegionResolver(std::unique_ptr<QNetworkAccessManager> network,
                                               int timeoutMs, QObject* parent)
    : QObject(parent),
      m_nam(network ? std::move(network) : std::make_unique<QNetworkAccessManager>()),
      m_timeoutMs(timeoutMs > 0 ? timeoutMs : 15000) {
  m_deadline.setSingleShot(true);
  connect(&m_deadline, &QTimer::timeout, this, [this]() {
    QgsProject* project = m_fallbackProject.data();
    const QgsPointXY point = m_lastPoint;
    if (m_nearbyPhase) {
      // 판정은 끝났다. 이웃 확인이 늦을 뿐이니 판정한 시/군으로 계속한다.
      const HeritageRegion primary = m_primary;
      const QList<HeritageCity> local = HeritageNearbyRegions::fromBoundaryLayers(project, m_scope);
      cancel();
      emitWithNearby(primary, local,
                     QStringLiteral("주변 5km에 걸친 이웃 시·군 확인이 늦어 목록이 비어 있을 수 있습니다."));
      return;
    }
    cancel();
    // 시간 초과라고 바로 포기하지 않는다. 올라와 있는 행정경계로 한 번 더 본다.
    const HeritageRegion local = fromBoundaryLayers(project, point);
    if (local.ok()) {
      finishResolved(local, false);
      return;
    }
    emit failed(QStringLiteral("행정구역 서버의 응답 시간이 초과되었습니다. 시·군을 직접 고르세요."));
  });
}

HeritageRegionResolver::~HeritageRegionResolver() { cancel(); }

void HeritageRegionResolver::cancel() {
  m_deadline.stop();
  m_pending = false;
  m_nearbyPhase = false;
  if (m_reply) {
    QNetworkReply* reply = m_reply.data();
    m_reply.clear();
    disconnect(reply, nullptr, this, nullptr);
    reply->abort();
    reply->deleteLater();
  }
}

QUrl HeritageRegionResolver::buildAddressUrl(const QString& apiKey, double lonDeg, double latDeg) {
  QUrl url(QStringLiteral("https://api.vworld.kr/req/address"));
  QUrlQuery q;
  q.addQueryItem(QStringLiteral("service"), QStringLiteral("address"));
  q.addQueryItem(QStringLiteral("request"), QStringLiteral("getAddress"));
  q.addQueryItem(QStringLiteral("version"), QStringLiteral("2.0"));
  q.addQueryItem(QStringLiteral("crs"), QStringLiteral("EPSG:4326"));
  q.addQueryItem(QStringLiteral("point"),
                 QString::number(lonDeg, 'f', 8) + QLatin1Char(',') +
                     QString::number(latDeg, 'f', 8));
  q.addQueryItem(QStringLiteral("format"), QStringLiteral("json"));
  q.addQueryItem(QStringLiteral("type"), QStringLiteral("both"));
  q.addQueryItem(QStringLiteral("zipcode"), QStringLiteral("false"));
  q.addQueryItem(QStringLiteral("simple"), QStringLiteral("false"));
  q.addQueryItem(QStringLiteral("key"), apiKey);
  url.setQuery(q);
  return url;
}

HeritageRegion HeritageRegionResolver::fromAddressText(const QString& text) {
  HeritageRegion out;
  const QString t = text.trimmed();
  if (t.isEmpty()) return out;

  QStringList tokens = t.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
  if (tokens.isEmpty()) return out;
  if (tokens.first() == QStringLiteral("대한민국")) tokens.removeFirst();
  if (tokens.isEmpty()) return out;

  const QStringList known = KoreaRegionCatalog::sidoNames();
  int sidoAt = -1;
  for (int i = 0; i < tokens.size() && i < 2; ++i) {
    const QString canon = KoreaRegionCatalog::canonicalSido(tokens.at(i));
    if (known.contains(canon)) {
      out.sido = canon;
      sidoAt = i;
      break;
    }
  }
  if (sidoAt < 0) return out;

  const QStringList cities = KoreaRegionCatalog::citiesOf(out.sido);
  const QStringList rest = tokens.mid(sidoAt + 1);

  // 두 토막 먼저 본다: "수원시 장안구" 처럼 오는 표기를 목록의 "수원시" 로 접기 위해서다.
  for (int take = 2; take >= 1; --take) {
    if (rest.size() < take) continue;
    const QString joined = QStringList(rest.mid(0, take)).join(QLatin1Char(' '));
    if (cities.contains(joined)) {
      out.city = joined;
      return out;
    }
    for (const QString& c : cities) {
      if (joined == c || joined.startsWith(c + QLatin1Char(' '))) {
        out.city = c;
        return out;
      }
    }
  }

  // 목록에 없는 표기(신설·개편). 시·군·구로 끝나는 첫 토막을 그대로 쓴다.
  for (const QString& token : rest) {
    if (token.endsWith(QStringLiteral("시")) || token.endsWith(QStringLiteral("군")) ||
        token.endsWith(QStringLiteral("구"))) {
      out.city = token;
      return out;
    }
  }
  // 세종처럼 시/군이 따로 없는 곳.
  if (cities.size() == 1) out.city = cities.first();
  return out;
}

HeritageRegion HeritageRegionResolver::parseAddress(const QByteArray& body, QString* errorOut) {
  HeritageRegion out;
  if (errorOut) errorOut->clear();
  const QJsonObject root = QJsonDocument::fromJson(body).object();
  const QJsonObject response = root.value(QStringLiteral("response")).toObject();
  const QString status = response.value(QStringLiteral("status")).toString();
  if (status.compare(QLatin1String("OK"), Qt::CaseInsensitive) != 0) {
    if (errorOut) {
      const QString code = response.value(QStringLiteral("error")).toObject()
                               .value(QStringLiteral("code")).toString();
      if (code == QLatin1String("INVALID_KEY") || code == QLatin1String("INCORRECT_KEY")) {
        // Do not echo server text or a request URL: either can contain the key.
        *errorOut = QStringLiteral("VWorld API 키가 서버에서 거절되었습니다 (%1).\n"
                                  "더보기 → API 키 입력에서 유효한 VWorld 키를 저장한 뒤 다시 누르세요.\n"
                                  "이 키는 조사구역의 자동 시·도 판정과 지적도에 함께 사용됩니다.").arg(code);
      } else {
        *errorOut = QStringLiteral("VWorld에서 조사구역의 주소를 받지 못했습니다. 잠시 뒤 다시 시도하세요.");
      }
    }
    return out;
  }

  const QJsonArray results = response.value(QStringLiteral("result")).toArray();
  for (const QJsonValue& v : results) {
    const QJsonObject item = v.toObject();
    const QJsonObject s = item.value(QStringLiteral("structure")).toObject();
    const QString level1 = s.value(QStringLiteral("level1")).toString().trimmed();
    const QString level2 = s.value(QStringLiteral("level2")).toString().trimmed();
    if (!level1.isEmpty()) {
      HeritageRegion candidate =
          fromAddressText(level2.isEmpty() ? level1 : level1 + QLatin1Char(' ') + level2);
      if (candidate.ok()) {
        candidate.source = QStringLiteral("vworld");
        return candidate;
      }
    }
    const QString text = item.value(QStringLiteral("text")).toString();
    HeritageRegion fromText = fromAddressText(text);
    if (fromText.ok()) {
      fromText.source = QStringLiteral("vworld");
      return fromText;
    }
  }
  return out;
}

QgsPointXY HeritageRegionResolver::lookupPoint(const QgsGeometry& geometry) {
  if (geometry.isEmpty() || geometry.isNull()) return {};
  // 오목한 조사구역에서는 무게중심이 도형 밖으로 나간다. pointOnSurface 는 안쪽을 보장한다.
  const QgsGeometry surface = geometry.pointOnSurface();
  if (!surface.isNull() && !surface.isEmpty()) return surface.asPoint();
  const QgsGeometry centroid = geometry.centroid();
  if (!centroid.isNull() && !centroid.isEmpty()) return centroid.asPoint();
  return geometry.boundingBox().center();
}

QgsPointXY HeritageRegionResolver::toWgs84(const QgsPointXY& point,
                                           const QgsCoordinateReferenceSystem& crs) {
  if (!crs.isValid() || crs == wgs84()) return point;
  QgsCoordinateTransform tr(crs, wgs84(), QgsCoordinateTransformContext());
  try {
    return tr.transform(point);
  } catch (const QgsCsException&) {
    return point;
  }
}

HeritageRegion HeritageRegionResolver::fromBoundaryLayers(QgsProject* project,
                                                          const QgsPointXY& wgs) {
  HeritageRegion out;
  if (!project) return out;
  if (qIsNaN(wgs.x()) || qIsNaN(wgs.y())) return out;

  const QgsGeometry probe = QgsGeometry::fromPointXY(wgs);
  for (QgsMapLayer* ml : project->mapLayers()) {
    auto* vl = qobject_cast<QgsVectorLayer*>(ml);
    if (!vl || !vl->isValid()) continue;
    if (vl->geometryType() != Qgis::GeometryType::Polygon) continue;

    int nameIdx = -1;
    for (const char* candidate : kNameFields) {
      nameIdx = vl->fields().indexOf(QString::fromLatin1(candidate));
      if (nameIdx >= 0) break;
    }
    if (nameIdx < 0) continue;

    QgsPointXY local = wgs;
    if (vl->crs().isValid() && vl->crs() != wgs84()) {
      QgsCoordinateTransform tr(wgs84(), vl->crs(), project);
      try {
        local = tr.transform(wgs);
      } catch (const QgsCsException&) {
        continue;
      }
    }
    const QgsGeometry localProbe = QgsGeometry::fromPointXY(local);

    // 폭이 0인 사각형은 「필터 없음」으로 취급될 수 있어 아주 조금 넓힌다.
    const double eps = 1e-6 * qMax(1.0, qAbs(local.x()));
    QgsFeatureRequest request;
    request.setFilterRect(QgsRectangle(local.x() - eps, local.y() - eps, local.x() + eps,
                                       local.y() + eps));
    QgsFeatureIterator it = vl->getFeatures(request);
    QgsFeature f;
    while (it.nextFeature(f)) {
      if (!f.hasGeometry() || !f.geometry().contains(localProbe)) continue;
      HeritageRegion hit = fromAddressText(f.attribute(nameIdx).toString());
      if (hit.ok()) {
        hit.source = QStringLiteral("layer");
        hit.insideSidoBounds = insideSido(hit.sido, wgs);
        return hit;
      }
    }
  }
  return out;
}

void HeritageRegionResolver::resolve(const QgsGeometry& surveyArea,
                                     const QgsCoordinateReferenceSystem& crs,
                                     QgsProject* fallbackProject) {
  if (m_pending) {
    emit failed(QStringLiteral("이전 판정이 아직 끝나지 않았습니다. 잠시 뒤 다시 누르세요."));
    return;
  }
  if (surveyArea.isNull() || surveyArea.isEmpty()) {
    emit failed(QStringLiteral("조사구역 도형이 없습니다. 조사구역을 먼저 그리세요."));
    return;
  }

  const QgsPointXY wgs = toWgs84(lookupPoint(surveyArea), crs);
  m_fallbackProject = fallbackProject;
  m_lastPoint = wgs;
  m_scope = HeritageNearbyRegions::scopeWgs84(surveyArea, crs);

  const QString key = VworldSettings::loadApiKey().trimmed();
  if (key.isEmpty()) {
    const HeritageRegion local = fromBoundaryLayers(fallbackProject, wgs);
    if (local.ok()) {
      finishResolved(local, false);
      return;
    }
    emit failed(QStringLiteral("VWorld 키가 없어 시·군을 판정하지 못했습니다. 더보기에서 키를 넣거나 시·군을 직접 고르세요."));
    return;
  }

  m_pending = true;
  m_deadline.start(m_timeoutMs);
  QNetworkRequest req(buildAddressUrl(key, wgs.x(), wgs.y()));
  req.setHeader(QNetworkRequest::UserAgentHeader,
                QStringLiteral("Mozilla/5.0 (Windows NT 10.0; Win64; x64) ka-hgis/0.3"));
  req.setRawHeader("Referer", "https://localhost");
  req.setRawHeader("Accept", "application/json");
  req.setTransferTimeout(m_timeoutMs);

  QNetworkReply* reply = m_nam->get(req);
  m_reply = reply;
  connect(reply, &QNetworkReply::finished, this, [this, reply, wgs]() {
    m_deadline.stop();
    m_reply.clear();
    m_pending = false;
    reply->deleteLater();

    HeritageRegion region;
    QString failure;
    if (reply->error() == QNetworkReply::NoError)
      region = parseAddress(reply->readAll(), &failure);
    else
      failure = QStringLiteral("행정구역 서버에 연결하지 못했습니다. 인터넷 연결을 확인한 뒤 다시 누르세요.");
    if (!region.ok()) region = fromBoundaryLayers(m_fallbackProject.data(), wgs);
    if (!region.ok()) {
      emit failed(failure.isEmpty()
                      ? QStringLiteral("좌표로 시·군을 찾지 못했습니다. 시·군을 직접 고르세요.")
                      : failure);
      return;
    }
    region.insideSidoBounds = insideSido(region.sido, wgs);
    finishResolved(region, true);
  });
}
