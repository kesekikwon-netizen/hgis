#include "CadastralPortal.h"
#include "KaPortableRuntime.h"
#include "KoreaRegionCatalog.h"
#include "TopographicArchive.h"
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSettings>
#include <QTemporaryDir>
#include <QTimer>
#include <QUrlQuery>
#include <qgscoordinatetransform.h>
#include <qgsexception.h>
#include <qgsjsonutils.h>
#include <cmath>

namespace {
const QString base = QStringLiteral("https://www.vworld.kr");
const QString dataset = QStringLiteral("/dtmk/dtmk_ntads_s002.do?dsId=30563");
QString accountPath() { return QDir(KaPortableRuntime::userConfigDir()).filePath(QStringLiteral("vworld-account.ini")); }

class Session {
public:
  CadastralImport::Cancel canceled;
  CadastralImport::Progress progress;
  QString error;
  QNetworkAccessManager network;
  QByteArray request(const QUrl& url, const QByteArray& post = {}, QSaveFile* file = nullptr,
                     int offset = 0, int span = 0) {
    error.clear(); QByteArray bytes;
    if (canceled && canceled()) return {};
    QNetworkRequest req(url);
    req.setRawHeader("User-Agent", "Mozilla/5.0 ka-hgis");
    req.setRawHeader("Referer", url.host() == QLatin1String("api.vworld.kr") ? QByteArray("https://localhost") : (base + dataset).toUtf8());
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::SameOriginRedirectPolicy);
    if (!post.isEmpty()) {
      req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/x-www-form-urlencoded"));
      req.setRawHeader("X-Requested-With", "XMLHttpRequest");
    }
    QNetworkReply* reply = post.isEmpty() ? network.get(req) : network.post(req, post);
    QEventLoop loop;
    QTimer timeout, poll;
    timeout.setSingleShot(true); timeout.start(file ? 120000 : 40000);
    poll.start(50);
    bool failed = false;
    qint64 received = 0;
    QByteArray prefix;
    auto read = [&]() {
      const auto chunk = reply->readAll(); received += chunk.size();
      if (!chunk.isEmpty()) timeout.start(file ? 120000 : 40000);
      if (prefix.size() < 4) prefix += chunk.left(4 - prefix.size());
      if (received > (file ? 512LL * 1024 * 1024 : 32LL * 1024 * 1024)) {
        failed = true; error = QStringLiteral("지적도 서버 응답이 허용 크기를 넘었습니다."); reply->abort(); return;
      }
      if (file) {
        if (file->write(chunk) != chunk.size()) {
          failed = true; error = QStringLiteral("지적도 저장 공간을 확인하세요."); reply->abort();
        }
      } else bytes += chunk;
    };
    QObject::connect(reply, &QNetworkReply::readyRead, &loop, read);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(reply, &QNetworkReply::downloadProgress, &loop, [&](qint64 done, qint64 total) {
      if (file && progress && total > 0) progress(offset + int(span * double(done) / double(total)), QStringLiteral("지적도 원본을 받고 있습니다…"));
    });
    QObject::connect(&timeout, &QTimer::timeout, &loop, [&] {
      failed = true; error = QStringLiteral("지적도 서버 응답 시간이 초과되었습니다. 다시 시도하세요."); reply->abort();
    });
    QObject::connect(&poll, &QTimer::timeout, &loop, [&] { if (canceled && canceled()) reply->abort(); });
    loop.exec(); read();
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (!failed && (reply->error() != QNetworkReply::NoError || status != 200)) {
      failed = true; error = QStringLiteral("VWorld 통신에 실패했습니다. 인터넷 연결과 계정 설정을 확인하세요.");
    }
    if (!failed && file && !prefix.startsWith("PK\003\004")) {
      failed = true; error = QStringLiteral("VWorld에서 ZIP 대신 로그인·오류 화면을 받았습니다. 계정을 확인하세요.");
    }
    delete reply;
    return failed ? QByteArray() : (file ? QByteArray("ZIP") : bytes);
  }
};

QString provincePrefix(const QString& code) {
  static const QMap<QString, QString> prefixes = {{"11", "서울"}, {"26", "부산"}, {"27", "대구"},
      {"28", "인천"}, {"29", "광주"}, {"30", "대전"}, {"31", "울산"}, {"36", "세종"},
      {"41", "경기"}, {"42", "강원"}, {"51", "강원"}, {"43", "충북"}, {"44", "충남"},
      {"45", "전북"}, {"52", "전북"}, {"46", "전남"}, {"47", "경북"}, {"48", "경남"}, {"50", "제주"}};
  return prefixes.value(code.left(2));
}
QString shortProvince(const QString& text) {
  for (const auto& sido : KoreaRegionCatalog::allSido())
    if (sido.name == text || sido.shortName == text) return sido.shortName;
  if (text.startsWith(QStringLiteral("강원"))) return QStringLiteral("강원");
  if (text.startsWith(QStringLiteral("전북"))) return QStringLiteral("전북");
  return text;
}
}

CadastralPortal::Credentials CadastralPortal::credentials() {
  QSettings settings(accountPath(), QSettings::IniFormat); settings.setFallbacksEnabled(false);
  return {settings.value(QStringLiteral("vworld_account/username")).toString(),
          settings.value(QStringLiteral("vworld_account/password")).toString()};
}

bool CadastralPortal::saveCredentials(const Credentials& account, QString* error) {
  const QString path = accountPath();
  QTemporaryDir temp(QFileInfo(path).absolutePath() + QStringLiteral("/.vworld-account-XXXXXX"));
  if (temp.isValid()) {
    const QString draft = temp.filePath(QStringLiteral("account.ini"));
    {
      QSettings settings(draft, QSettings::IniFormat); settings.setFallbacksEnabled(false);
      settings.setValue(QStringLiteral("vworld_account/username"), account.username.trimmed());
      settings.setValue(QStringLiteral("vworld_account/password"), account.password); settings.sync();
      if (settings.status() != QSettings::NoError) { if (error) *error = QStringLiteral("계정 설정을 기록하지 못했습니다."); return false; }
    }
    QFile in(draft); QSaveFile out(path); out.setDirectWriteFallback(false);
    if (in.open(QIODevice::ReadOnly) && out.open(QIODevice::WriteOnly)) {
      const auto bytes = in.readAll();
      if (out.write(bytes) == bytes.size() && out.commit()) return true;
    }
  }
  if (error) *error = QStringLiteral("VWorld 계정을 저장하지 못했습니다. 설정 폴더 권한을 확인하세요.");
  return false;
}

QList<CadastralPortal::Resource> CadastralPortal::parseResources(const QByteArray& html) {
  QList<Resource> result;
  const QRegularExpression row(QStringLiteral("<li\\b[^>]*>([\\s\\S]*?)</li>"));
  const QRegularExpression name(QStringLiteral("LSMD_CONT_LDREG_[^<>\\\"']+\\.zip"));
  const QRegularExpression action(QStringLiteral("listFnc\\.download\\(\\s*['\"]30563['\"]\\s*,\\s*['\"]([0-9]+)['\"]\\s*,\\s*['\"]([0-9]+)['\"]"));
  const QRegularExpression safeName(QStringLiteral("^LSMD_CONT_LDREG_[\\p{L}0-9_ -]+\\.zip$"));
  const QRegularExpression revision(QStringLiteral("갱신일\\s*<em[^>]*>\\s*([0-9]{4}-[0-9]{2}-[0-9]{2})"));
  auto rows = row.globalMatch(QString::fromUtf8(html));
  while (rows.hasNext()) {
    const QString body = rows.next().captured(1);
    const auto file = name.match(body), link = action.match(body);
    if (file.hasMatch() && link.hasMatch() && safeName.match(file.captured().trimmed()).hasMatch())
      result.append({link.captured(1), file.captured().trimmed(), link.captured(2).toLongLong(), revision.match(body).captured(1)});
  }
  return result;
}

QList<CadastralPortal::Resource> CadastralPortal::selectResources(const QList<Resource>& resources,
    const QList<District>& districts, QString* error) {
  if (error) error->clear(); QList<Resource> selected; QSet<QString> ids;
  for (const auto& district : districts) {
    const QString province = provincePrefix(district.code);
    QString districtName = district.name.simplified().section(' ', 1).replace(' ', '_');
    QList<Resource> matches;
    for (const auto& resource : resources) {
      QString name = resource.name;
      name.remove(0, QStringLiteral("LSMD_CONT_LDREG_").size()); name.chop(4);
      // The portal publishes both 세종시 (municipality) and 세종 (province).
      // Select the municipality for either official full_nm spelling.
      if (district.code == QLatin1String("36110")) {
        if (name == QStringLiteral("세종시")) matches.append(resource);
        continue;
      }
      const QString fileProvince = shortProvince(name.section('_', 0, 0));
      const bool correctProvince = fileProvince == province || (fileProvince == QStringLiteral("전남광주") && (province == QStringLiteral("전남") || province == QStringLiteral("광주")));
      const QString county = name.section('_', 1);
      if (!province.isEmpty() && correctProvince && county == districtName) matches.append(resource);
    }
    if (matches.size() != 1) {
      if (error) *error = QStringLiteral("%1 지적도 파일을 정확히 하나로 확인하지 못했습니다. 행정구역 개편 또는 VWorld 자료 목록을 확인하세요.").arg(district.name);
      return {};
    }
    if (!ids.contains(matches.first().fileNo)) { ids.insert(matches.first().fileNo); selected.append(matches.first()); }
  }
  return selected;
}

PreparedReferenceMap CadastralPortal::prepare(const Request& request, const CadastralImport::Cancel& cancel,
    const CadastralImport::Progress& progress) {
  PreparedReferenceMap result;
  auto stopped = [&] { if (cancel && cancel()) { result.status = PreparedReferenceMap::Status::Cancelled; return true; } return false; };
  auto stage = [&](int value, const QString& text) { if (progress) progress(value, text); };
  if (stopped()) return result;
  if (request.survey.isEmpty() || !request.surveyCrs.isValid() || !request.workCrs.isValid() ||
      request.workCrs.mapUnits() != Qgis::DistanceUnit::Meters || !std::isfinite(request.bufferMeters) || request.bufferMeters <= 0.) {
    result.error = QStringLiteral("조사구역과 미터 단위 작업 좌표계를 먼저 확인하세요."); return result;
  }
  if (request.apiKey.isEmpty()) { result.error = QStringLiteral("5km 범위의 시·군·구를 찾으려면 더보기 → VWorld API 키를 설정하세요."); return result; }
  if (request.credentials.username.isEmpty() || request.credentials.password.isEmpty()) {
    result.error = QStringLiteral("지적도 메뉴에서 VWorld 계정을 설정하세요."); return result;
  }
  Session session; session.canceled = cancel; session.progress = progress;
  QgsGeometry scope = request.survey;
  QList<District> districts;
  try {
    scope.transform(QgsCoordinateTransform(request.surveyCrs, request.workCrs, request.context));
    scope = scope.buffer(request.bufferMeters, 24);
    if (scope.isEmpty() || !scope.isGeosValid()) { result.error = QStringLiteral("조사 주변 5km 범위를 만들지 못했습니다."); return result; }
    const QgsCoordinateReferenceSystem wgs(QStringLiteral("EPSG:4326"));
    const QgsCoordinateTransform toWgs(request.workCrs, wgs, request.context);
    const auto box = toWgs.transformBoundingBox(scope.boundingBox());
    const QgsCoordinateTransform toWork(wgs, request.workCrs, request.context);
    stage(2, QStringLiteral("조사 주변 5km의 시·군·구를 확인하고 있습니다…"));
    QSet<QString> seen; bool complete = false;
    for (int page = 1; page <= 10; ++page) {
      QUrl url(QStringLiteral("https://api.vworld.kr/req/data")); QUrlQuery query;
      for (const auto& item : QList<QPair<QString, QString>>{{"service","data"},{"version","2.0"},{"request","GetFeature"},
          {"format","json"},{"data","LT_C_ADSIGG_INFO"},{"size","100"},{"crs","EPSG:4326"},{"geometry","true"},{"attribute","true"}}) query.addQueryItem(item.first,item.second);
      query.addQueryItem(QStringLiteral("page"), QString::number(page)); query.addQueryItem(QStringLiteral("key"), request.apiKey);
      query.addQueryItem(QStringLiteral("geomFilter"), QStringLiteral("BOX(%1,%2,%3,%4)").arg(box.xMinimum(),0,'f',8).arg(box.yMinimum(),0,'f',8).arg(box.xMaximum(),0,'f',8).arg(box.yMaximum(),0,'f',8));
      url.setQuery(query);
      const auto data = session.request(url); if (stopped()) return result;
      if (!session.error.isEmpty()) { result.error = session.error; return result; }
      const auto response = QJsonDocument::fromJson(data).object().value("response").toObject();
      if (response.value("status").toString() != QLatin1String("OK")) {
        result.error = QStringLiteral("VWorld에서 시·군·구 경계를 확인하지 못했습니다. API 키와 연결을 확인하세요."); return result;
      }
      const auto collection = response.value("result").toObject().value("featureCollection").toObject();
      const QString crsName = collection.value("crs").toObject().value("properties").toObject().value("name").toString();
      if (!crsName.isEmpty() && QgsCoordinateReferenceSystem::fromOgcWmsCrs(crsName) != wgs) {
        result.error = QStringLiteral("시·군·구 응답 좌표계가 요청과 다릅니다."); return result;
      }
      const auto features = collection.value("features").toArray();
      for (const auto& item : features) {
        const auto feature = item.toObject(); const auto properties = feature.value("properties").toObject();
        const QString code = properties.value("sig_cd").toString();
        const QString name = properties.value("full_nm").toString();
        QgsGeometry boundary = QgsJsonUtils::geometryFromGeoJson(QString::fromUtf8(QJsonDocument(feature.value("geometry").toObject()).toJson(QJsonDocument::Compact)));
        if (code.size() != 5 || name.isEmpty() || boundary.isEmpty()) { result.error = QStringLiteral("시·군·구 경계 응답에 필요한 정보가 없습니다."); return result; }
        boundary.transform(toWork);
        if (scope.intersects(boundary) && !seen.contains(code)) { districts.append({code,name}); seen.insert(code); }
      }
      const int totalPages = response.value("page").toObject().value("total").toString().toInt();
      if ((totalPages > 0 && page >= totalPages) || (totalPages == 0 && features.size() < 100)) { complete = true; break; }
    }
    if (!complete || districts.isEmpty()) { result.error = QStringLiteral("조사 범위의 전체 시·군·구 목록을 확인하지 못했습니다."); return result; }
  } catch (const QgsCsException&) { result.error = QStringLiteral("조사 범위를 행정구역 좌표계로 변환하지 못했습니다."); return result; }
  stage(8, QStringLiteral("VWorld 계정을 확인하고 있습니다…"));
  session.request(QUrl(base + dataset));
  if (stopped()) return result;
  if (!session.error.isEmpty()) { result.error = session.error; return result; }
  QUrlQuery login;
  login.addQueryItem(QStringLiteral("usrIdeE"), QString::fromLatin1(request.credentials.username.toUtf8().toBase64()));
  login.addQueryItem(QStringLiteral("usrPwdE"), QString::fromLatin1(request.credentials.password.toUtf8().toBase64()));
  login.addQueryItem(QStringLiteral("nextUrl"), dataset);
  const auto authenticated = session.request(QUrl(base + QStringLiteral("/v4po_usrlogin_a004.do")), login.query(QUrl::FullyEncoded).toUtf8().replace("+", "%2B"));
  if (stopped()) return result;
  if (QJsonDocument::fromJson(authenticated).object().value("resultMap").toObject().value("result").toString() != QLatin1String("success")) {
    result.error = QStringLiteral("VWorld 로그인에 실패했습니다. 지적도 메뉴의 계정을 확인하세요. 추가 인증이 필요한 계정은 사이트에서 확인 후 다시 시도하세요."); return result;
  }
  stage(12, QStringLiteral("해당 시·군·구의 지적도 파일을 찾고 있습니다…"));
  QList<Resource> resources; bool catalogComplete = false;
  for (int page = 1; page <= 10; ++page) {
    QUrl url(base + dataset); QUrlQuery query(url);
    query.addQueryItem(QStringLiteral("datPageIndex"), QString::number(page)); query.addQueryItem(QStringLiteral("datPageSize"), QStringLiteral("100")); url.setQuery(query);
    const auto bytes = session.request(url); if (stopped()) return result;
    if (!session.error.isEmpty()) { result.error = session.error; return result; }
    const auto rows = parseResources(bytes); resources.append(rows);
    if (rows.size() < 100) { catalogComplete = true; break; }
  }
  if (!catalogComplete) { result.error = QStringLiteral("VWorld 자료 목록이 끝나지 않아 내려받기를 중단했습니다."); return result; }
  const auto files = selectResources(resources, districts, &result.error);
  if (files.isEmpty()) return result;
  const QDir originals(QDir(request.directory).filePath(QStringLiteral("원본")));
  if (request.directory.isEmpty() || !QDir().mkpath(originals.path())) { result.error = QStringLiteral("조사 폴더에 지적도 저장 위치를 만들지 못했습니다."); return result; }
  QStringList sources;
  int index = 0;
  for (const auto& file : files) {
    if (stopped()) return result;
    if (file.sizeKb <= 0 || file.sizeKb > 512000) { result.error = QStringLiteral("해당 자료는 자동 수신 가능한 시·군·구 ZIP 크기를 벗어났습니다."); return result; }
    // Include the published size so replaced monthly resources don't reuse an
    // older source solely because the portal retained its numeric file ID.
    const QString path = originals.filePath(QStringLiteral("%1-%2-%3-%4").arg(file.fileNo).arg(file.sizeKb).arg(file.revision, file.name));
    auto unpacked = TopographicArchive::Result{};
    if (QFileInfo::exists(path)) unpacked = TopographicArchive::prepare(path, originals.filePath(QStringLiteral("추출")), {}, cancel);
    if (stopped()) return result;
    if (unpacked.files.isEmpty()) {
      QSaveFile output(path); output.setDirectWriteFallback(false);
      if (!output.open(QIODevice::WriteOnly)) { result.error = QStringLiteral("지적도 원본 저장 위치를 열지 못했습니다."); return result; }
      QUrl url(base + QStringLiteral("/dtmk/downloadResourceFile.do")); QUrlQuery query;
      query.addQueryItem(QStringLiteral("ds_id"), QStringLiteral("30563")); query.addQueryItem(QStringLiteral("fileNo"), file.fileNo); url.setQuery(query);
      session.request(url, {}, &output, 15 + index * 50 / files.size(), 50 / files.size());
      if (stopped()) return result;
      if (!session.error.isEmpty()) { result.error = session.error; return result; }
      if (!output.commit()) { result.error = QStringLiteral("지적도 원본 저장을 완료하지 못했습니다."); return result; }
      unpacked = TopographicArchive::prepare(path, originals.filePath(QStringLiteral("추출")), {}, cancel);
    }
    if (stopped()) return result;
    if (!unpacked.error.isEmpty()) { result.error = unpacked.error; return result; }
    int count = 0;
    for (const auto& source : unpacked.files)
      if (source.endsWith(QStringLiteral(".shp"), Qt::CaseInsensitive)) { sources.append(source); ++count; }
    if (!count) { result.error = QStringLiteral("받은 시·군·구 ZIP에 SHP 도형이 없습니다."); return result; }
    ++index;
  }
  stage(65, QStringLiteral("5km 범위의 경계선과 지번을 정리하고 있습니다…"));
  return CadastralImport::prepare(sources, scope, request.workCrs, request.context,
      QDir(request.directory).filePath(QStringLiteral("표시")), cancel,
      [&](int value, const QString& message) { stage(65 + value * 35 / 100, message); });
}
