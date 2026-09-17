#include "LocationSearch.h"
#include "VworldSettings.h"
#include "KoreaRegionCatalog.h"
#include "KaPortableRuntime.h"
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrl>
#include <QUrlQuery>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QSettings>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <cmath>
#include <QRegularExpression>
#include <QSet>

namespace {
struct ParcelAddress {
  QStringList area;
  bool mountain = false;
  int main = 0;
  int sub = 0;
  bool valid = false;
};

ParcelAddress parseParcelAddress(QString text) {
  text = text.normalized(QString::NormalizationForm_KC).simplified();
  text.replace(QChar(0x2013), QLatin1Char('-'));
  text.replace(QChar(0x2011), QLatin1Char('-'));
  static const QRegularExpression lot(
      QStringLiteral("^(.+?)\\s+(산\\s*)?([0-9]{1,4})(?:\\s*-\\s*([0-9]{1,4}))?\\s*(?:번지)?$"));
  const auto match = lot.match(text);
  ParcelAddress address;
  if (!match.hasMatch()) return address;
  address.area = match.captured(1).simplified().split(QLatin1Char(' '));
  address.area[0] = KoreaRegionCatalog::canonicalSido(address.area.first());
  const bool sejong = address.area.first() == QStringLiteral("세종특별자치시");
  if (sejong && address.area.value(1) == QStringLiteral("세종시")) address.area.removeAt(1);
  bool provinceKnown = false;
  for (const auto& province : KoreaRegionCatalog::allSido())
    if (province.name == address.area.first()) provinceKnown = true;
  if (!provinceKnown || address.area.size() < (sejong ? 2 : 3)) return address;
  if (!sejong && !address.area.at(1).endsWith(QStringLiteral("시"))
      && !address.area.at(1).endsWith(QStringLiteral("군"))
      && !address.area.at(1).endsWith(QStringLiteral("구"))) return address;
  // An 읍/면 alone cannot identify a parcel: require its legal 리 as well.
  // 세종로 is a legal dong despite its street-like suffix.
  static const QRegularExpression legalLocality(QStringLiteral("(?:동[0-9]*가?|리|[0-9]+가|세종로)$"));
  if (!legalLocality.match(address.area.last()).hasMatch()) return address;
  address.mountain = !match.captured(2).isEmpty();
  address.main = match.captured(3).toInt();
  address.sub = match.captured(4).toInt();
  address.valid = address.main > 0;
  return address;
}

bool sameParcel(const ParcelAddress& requested, const ParcelAddress& result) {
  if (!requested.valid || !result.valid || requested.mountain != result.mountain
      || requested.main != result.main || requested.sub != result.sub) return false;
  if (requested.area == result.area) return true;
  // The city picker omits general districts (e.g. 수원시 영통구).
  // Accept the extra 구 only here; all other supplied area tokens must match.
  QStringList expanded = result.area;
  if (expanded.size() == requested.area.size() + 1 && expanded.size() > 3
      && expanded.at(1).endsWith(QStringLiteral("시"))
      && expanded.at(2).endsWith(QStringLiteral("구"))) {
    expanded.removeAt(2);
    return requested.area == expanded;
  }
  return false;
}

QString parcelNotFound() {
  return QStringLiteral("정확히 일치하는 지번이 없습니다. 법정동·리, 산 여부와 본번·부번을 확인하세요. 최근 분할·합병된 지번은 검색 자료에 아직 반영되지 않았을 수 있습니다.");
}
}

static bool readCoordinate(const QJsonValue& value, double& coordinate) {
  bool ok = value.isDouble();
  coordinate = value.isString() ? value.toString().toDouble(&ok) : value.toDouble();
  return ok && std::isfinite(coordinate);
}

static QString secretsPath() {
  if (KaPortableRuntime::discover(KaPortableRuntime::resolvedExeDir()).looksBundled())
    return QDir(KaPortableRuntime::userConfigDir()).filePath(QStringLiteral("secrets.ini"));
  const QStringList cands = {
    QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("../config/secrets.ini")),
    QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("config/secrets.ini")),
    QDir::current().filePath(QStringLiteral("config/secrets.ini")),
    QDir(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation))
        .filePath(QStringLiteral("secrets.ini")),
  };
  for (const QString& p : cands) {
    if (QFile::exists(p)) return p;
  }
  return cands.first();
}

static QString readKeyFromSecretsFile() {
  const QString path = secretsPath();
  if (!QFile::exists(path)) return {};
  QSettings ini(path, QSettings::IniFormat);
  QString k = ini.value(QStringLiteral("vworld/apiKey")).toString().trimmed();
  if (k.isEmpty()) k = ini.value(QStringLiteral("apiKey")).toString().trimmed();
  return k;
}

LocationSearch::LocationSearch(QObject* parent)
    : LocationSearch(std::make_unique<QNetworkAccessManager>(), 20000, parent) {}

LocationSearch::LocationSearch(std::unique_ptr<QNetworkAccessManager> network, int timeoutMs,
                               QObject* parent)
    : QObject(parent), m_nam(network ? std::move(network)
                                   : std::make_unique<QNetworkAccessManager>()),
      m_timeoutMs(timeoutMs > 0 ? timeoutMs : 20000) {
  m_deadline.setSingleShot(true);
  connect(&m_deadline, &QTimer::timeout, this, [this]() {
    cancel();
    emit failed(QStringLiteral("위치 검색 서버의 응답 시간이 초과되었습니다. 인터넷 연결을 확인한 뒤 다시 검색하세요."));
  });
}

LocationSearch::~LocationSearch() { cancel(); }

void LocationSearch::cancel() {
  m_deadline.stop();
  m_pending = false;
  if (m_reply) {
    QNetworkReply* reply = m_reply.data();
    m_reply.clear();
    disconnect(reply, nullptr, this, nullptr);
    reply->abort();
    reply->deleteLater();
  }
}

void LocationSearch::completeRequest() {
  m_deadline.stop();
  m_reply.clear();
  m_pending = false;
}

QString LocationSearch::vworldApiKey() {
  const QByteArray env = qgetenv("VWORLD_API_KEY");
  if (!env.isEmpty()) return QString::fromUtf8(env).trimmed();
  return VworldSettings::loadApiKey();
}

void LocationSearch::setVworldApiKey(const QString& key) {
  VworldSettings::saveApiKey(key);
}

void LocationSearch::search(const QString& query) {
  startSearch(query, false);
}

void LocationSearch::searchParcel(const QString& query) {
  startSearch(query, true);
}

void LocationSearch::startSearch(const QString& query, bool parcel) {
  QString q = query.trimmed();
  if (q.isEmpty()) {
    emit failed(QStringLiteral("검색어를 입력하세요"));
    return;
  }
  if (m_pending) {
    emit failed(QStringLiteral("이전 위치를 검색하고 있습니다. 검색이 끝난 뒤 다시 검색하세요."));
    return;
  }
  if (parcel && !parseParcelAddress(q).valid) {
    emit failed(QStringLiteral("지번 검색에는 시·군·구와 법정동·리, 번지가 필요합니다. 읍·면은 리까지 입력하세요. 예: 고아읍 봉한리 / 산 12-3"));
    return;
  }
  if (parcel && vworldApiKey().isEmpty()) {
    emit failed(QStringLiteral("정확한 지번 검색에는 VWorld API 키가 필요합니다. VWorld API 키 설정을 확인하세요."));
    return;
  }
  if (parcel) {
    const auto address = parseParcelAddress(q);
    q = address.area.join(QLatin1Char(' ')) + QLatin1Char(' ')
        + (address.mountain ? QStringLiteral("산") : QString()) + QString::number(address.main);
    if (address.sub > 0) q += QLatin1Char('-') + QString::number(address.sub);
  }
  m_pending = true;
  // One deadline covers both providers; a slow fallback cannot extend it indefinitely.
  m_deadline.start(m_timeoutMs);
  if (!vworldApiKey().isEmpty())
    searchVworld(q, parcel);
  else
    searchNominatim(q);
}

void LocationSearch::searchNominatim(const QString& query) {
  QUrl url(QStringLiteral("https://nominatim.openstreetmap.org/search"));
  QUrlQuery uq;
  uq.addQueryItem(QStringLiteral("q"), query);
  uq.addQueryItem(QStringLiteral("format"), QStringLiteral("json"));
  uq.addQueryItem(QStringLiteral("addressdetails"), QStringLiteral("1"));
  uq.addQueryItem(QStringLiteral("limit"), QStringLiteral("12"));
  uq.addQueryItem(QStringLiteral("countrycodes"), QStringLiteral("kr"));
  uq.addQueryItem(QStringLiteral("accept-language"), QStringLiteral("ko"));
  url.setQuery(uq);

  QNetworkRequest req(url);
  req.setHeader(QNetworkRequest::UserAgentHeader,
                QStringLiteral("ka-hgis/0.3 (Korean archaeology HGIS; contact: local)"));
  req.setRawHeader("Accept", "application/json");
  req.setTransferTimeout(m_timeoutMs);

  QNetworkReply* reply = m_nam->get(req);
  m_reply = reply;
  connect(reply, &QNetworkReply::finished, this, [this, reply]() {
    completeRequest();
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError) {
      emit failed(QStringLiteral("위치 검색 서버에 연결하지 못했습니다. 인터넷 연결을 확인하고 잠시 후 다시 검색하세요."));
      return;
    }
    handleNominatim(reply->readAll());
  });
}

void LocationSearch::handleNominatim(const QByteArray& body) {
  const QJsonDocument doc = QJsonDocument::fromJson(body);
  if (!doc.isArray()) {
    emit failed(QStringLiteral("위치 검색 서버가 올바른 결과를 보내지 않았습니다. 잠시 후 다시 검색하세요."));
    return;
  }
  QVector<LocationHit> hits;
  for (const QJsonValue& v : doc.array()) {
    const QJsonObject o = v.toObject();
    LocationHit h;
    h.title = o.value(QStringLiteral("display_name")).toString();
    h.detail = o.value(QStringLiteral("type")).toString() + QStringLiteral(" / ")
               + o.value(QStringLiteral("class")).toString();
    if (!readCoordinate(o.value(QStringLiteral("lat")), h.lat)
        || !readCoordinate(o.value(QStringLiteral("lon")), h.lon)
        || h.lat < -90 || h.lat > 90 || h.lon < -180 || h.lon > 180)
      continue;
    const QJsonArray bb = o.value(QStringLiteral("boundingbox")).toArray();
    if (bb.size() == 4) {
      h.hasBbox = readCoordinate(bb.at(0), h.south)
                  && readCoordinate(bb.at(1), h.north)
                  && readCoordinate(bb.at(2), h.west)
                  && readCoordinate(bb.at(3), h.east)
                  && h.south >= -90 && h.north <= 90 && h.south < h.north
                  && h.west >= -180 && h.east <= 180 && h.west < h.east;
    }
    if (!h.title.isEmpty()) hits.push_back(h);
  }
  if (hits.isEmpty())
    emit failed(QStringLiteral("검색 결과 없음 (주소·지번·상호를 다시 입력)"));
  else
    emit finished(hits);
}

void LocationSearch::searchVworld(const QString& query, bool parcel) {
  QUrl url(QStringLiteral("https://api.vworld.kr/req/search"));
  QUrlQuery uq;
  uq.addQueryItem(QStringLiteral("service"), QStringLiteral("search"));
  uq.addQueryItem(QStringLiteral("request"), QStringLiteral("search"));
  uq.addQueryItem(QStringLiteral("version"), QStringLiteral("2.0"));
  uq.addQueryItem(QStringLiteral("crs"), QStringLiteral("EPSG:4326"));
  uq.addQueryItem(QStringLiteral("size"), parcel ? QStringLiteral("1000") : QStringLiteral("12"));
  uq.addQueryItem(QStringLiteral("page"), QStringLiteral("1"));
  uq.addQueryItem(QStringLiteral("query"), query);
  uq.addQueryItem(QStringLiteral("type"), parcel ? QStringLiteral("ADDRESS") : QStringLiteral("place"));
  if (parcel) uq.addQueryItem(QStringLiteral("category"), QStringLiteral("PARCEL"));
  uq.addQueryItem(QStringLiteral("format"), QStringLiteral("json"));
  uq.addQueryItem(QStringLiteral("errorformat"), QStringLiteral("json"));
  uq.addQueryItem(QStringLiteral("key"), vworldApiKey());
  url.setQuery(uq);

  QNetworkRequest req(url);
  req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("ka-hgis/0.3"));
  req.setTransferTimeout(m_timeoutMs);
  QNetworkReply* reply = m_nam->get(req);
  m_reply = reply;
  connect(reply, &QNetworkReply::finished, this, [this, reply, query, parcel]() {
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError) {
      if (parcel) {
        completeRequest();
        emit failed(QStringLiteral("지번 검색 서버에 연결하지 못했습니다. 인터넷 연결을 확인한 뒤 다시 검색하세요."));
        return;
      }
      searchNominatim(query);
      return;
    }
    const QByteArray body = reply->readAll();
    const QJsonObject root = QJsonDocument::fromJson(body).object();
    const QString status = root.value(QStringLiteral("response")).toObject()
                               .value(QStringLiteral("status")).toString();
    if (status != QLatin1String("OK")) {
      if (parcel) {
        completeRequest();
        emit failed(status == QLatin1String("NOT_FOUND") ? parcelNotFound()
          : QStringLiteral("지번 검색 서버가 정상 결과를 보내지 않았습니다. VWorld API 키와 서비스 상태를 확인하세요."));
        return;
      }
      searchNominatim(query);
      return;
    }
    completeRequest();
    handleVworld(body, parcel ? query : QString());
  });
}

void LocationSearch::handleVworld(const QByteArray& body, const QString& parcelQuery) {
  const QJsonObject resp = QJsonDocument::fromJson(body).object().value(QStringLiteral("response")).toObject();
  const QJsonArray items = resp.value(QStringLiteral("result")).toObject()
                               .value(QStringLiteral("items")).toArray();
  if (!parcelQuery.isEmpty()) {
    const auto total = resp.value(QStringLiteral("record")).toObject().value(QStringLiteral("total"));
    const int count = total.isString() ? total.toString().toInt() : total.toInt();
    const auto pages = resp.value(QStringLiteral("page")).toObject().value(QStringLiteral("total"));
    const int pageCount = pages.isString() ? pages.toString().toInt() : pages.toInt();
    if (count > items.size() || pageCount > 1) {
      emit failed(QStringLiteral("지번 후보가 너무 많아 정확한 필지를 확인하지 못했습니다. 구·법정동·리까지 입력하세요."));
      return;
    }
  }
  QVector<LocationHit> hits;
  const bool parcel = !parcelQuery.isEmpty();
  const ParcelAddress requested = parseParcelAddress(parcelQuery);
  QSet<QString> seenPnus;
  for (const QJsonValue& v : items) {
    const QJsonObject o = v.toObject();
    LocationHit h;
    h.title = o.value(QStringLiteral("title")).toString();
    if (h.title.isEmpty()) h.title = o.value(QStringLiteral("address")).toObject()
                                         .value(QStringLiteral("road")).toString();
    const QJsonObject addr = o.value(QStringLiteral("address")).toObject();
    h.detail = addr.value(QStringLiteral("parcel")).toString();
    if (parcel) {
      const auto address = parseParcelAddress(h.detail);
      if (!sameParcel(requested, address)) continue;
      h.title = h.detail;
      h.pnu = o.value(QStringLiteral("id")).toString();
      static const QRegularExpression pnuPattern(QStringLiteral("^[0-9]{19}$"));
      if (!pnuPattern.match(h.pnu).hasMatch()) continue;
      // A mismatched provider identifier must never select another local parcel.
      if (h.pnu.mid(10, 1) != (address.mountain ? QStringLiteral("2") : QStringLiteral("1"))
          || h.pnu.mid(11, 4).toInt() != address.main
          || h.pnu.mid(15, 4).toInt() != address.sub) continue;
      if (seenPnus.contains(h.pnu)) continue;
    }
    if (h.detail.isEmpty()) h.detail = addr.value(QStringLiteral("road")).toString();
    const QJsonObject pt = o.value(QStringLiteral("point")).toObject();
    if (!readCoordinate(pt.value(QStringLiteral("x")), h.lon)
        || !readCoordinate(pt.value(QStringLiteral("y")), h.lat)
        || h.lat < -90 || h.lat > 90 || h.lon < -180 || h.lon > 180)
      continue;
    if (parcel && (h.lon < 124 || h.lon > 132 || h.lat < 33 || h.lat > 39)) continue;
    if (parcel) seenPnus.insert(h.pnu);
    if (!h.title.isEmpty()) hits.push_back(h);
  }
  if (parcel && hits.size() > 1) {
    emit failed(QStringLiteral("같은 지번에 서로 다른 필지 후보가 있습니다. 구·법정동·리까지 정확히 입력하세요."));
    return;
  }
  if (hits.isEmpty())
    emit failed(parcel ? parcelNotFound() : QStringLiteral("검색한 위치를 찾지 못했습니다. 주소·지번·장소 이름을 확인한 뒤 다시 검색하세요."));
  else
    emit finished(hits);
}
