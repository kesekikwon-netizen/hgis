#include "KoreaRegionCatalog.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace {

const QVector<KoreaSido>& builtInTable() {
  static const QVector<KoreaSido> k = {
      {QStringLiteral("서울특별시"), QStringLiteral("서울"),
       {QStringLiteral("종로구"), QStringLiteral("중구"), QStringLiteral("용산구"),
        QStringLiteral("성동구"), QStringLiteral("광진구"), QStringLiteral("동대문구"),
        QStringLiteral("중랑구"), QStringLiteral("성북구"), QStringLiteral("강북구"),
        QStringLiteral("도봉구"), QStringLiteral("노원구"), QStringLiteral("은평구"),
        QStringLiteral("서대문구"), QStringLiteral("마포구"), QStringLiteral("양천구"),
        QStringLiteral("강서구"), QStringLiteral("구로구"), QStringLiteral("금천구"),
        QStringLiteral("영등포구"), QStringLiteral("동작구"), QStringLiteral("관악구"),
        QStringLiteral("서초구"), QStringLiteral("강남구"), QStringLiteral("송파구"),
        QStringLiteral("강동구")}},
      {QStringLiteral("부산광역시"), QStringLiteral("부산"),
       {QStringLiteral("중구"), QStringLiteral("서구"), QStringLiteral("동구"),
        QStringLiteral("영도구"), QStringLiteral("부산진구"), QStringLiteral("동래구"),
        QStringLiteral("남구"), QStringLiteral("북구"), QStringLiteral("해운대구"),
        QStringLiteral("사하구"), QStringLiteral("금정구"), QStringLiteral("강서구"),
        QStringLiteral("연제구"), QStringLiteral("수영구"), QStringLiteral("사상구"),
        QStringLiteral("기장군")}},
      {QStringLiteral("대구광역시"), QStringLiteral("대구"),
       {QStringLiteral("중구"), QStringLiteral("동구"), QStringLiteral("서구"),
        QStringLiteral("남구"), QStringLiteral("북구"), QStringLiteral("수성구"),
        QStringLiteral("달서구"), QStringLiteral("달성군"), QStringLiteral("군위군")}},
      {QStringLiteral("인천광역시"), QStringLiteral("인천"),
       {QStringLiteral("중구"), QStringLiteral("동구"), QStringLiteral("미추홀구"),
        QStringLiteral("연수구"), QStringLiteral("남동구"), QStringLiteral("부평구"),
        QStringLiteral("계양구"), QStringLiteral("서구"), QStringLiteral("강화군"),
        QStringLiteral("옹진군")}},
      {QStringLiteral("광주광역시"), QStringLiteral("광주"),
       {QStringLiteral("동구"), QStringLiteral("서구"), QStringLiteral("남구"),
        QStringLiteral("북구"), QStringLiteral("광산구")}},
      {QStringLiteral("대전광역시"), QStringLiteral("대전"),
       {QStringLiteral("동구"), QStringLiteral("중구"), QStringLiteral("서구"),
        QStringLiteral("유성구"), QStringLiteral("대덕구")}},
      {QStringLiteral("울산광역시"), QStringLiteral("울산"),
       {QStringLiteral("중구"), QStringLiteral("남구"), QStringLiteral("동구"),
        QStringLiteral("북구"), QStringLiteral("울주군")}},
      {QStringLiteral("세종특별자치시"), QStringLiteral("세종"), {QStringLiteral("세종시")}},
      {QStringLiteral("경기도"), QStringLiteral("경기"),
       {QStringLiteral("수원시"), QStringLiteral("성남시"), QStringLiteral("의정부시"),
        QStringLiteral("안양시"), QStringLiteral("부천시"), QStringLiteral("광명시"),
        QStringLiteral("평택시"), QStringLiteral("동두천시"), QStringLiteral("안산시"),
        QStringLiteral("고양시"), QStringLiteral("과천시"), QStringLiteral("구리시"),
        QStringLiteral("남양주시"), QStringLiteral("오산시"), QStringLiteral("시흥시"),
        QStringLiteral("군포시"), QStringLiteral("의왕시"), QStringLiteral("하남시"),
        QStringLiteral("용인시"), QStringLiteral("파주시"), QStringLiteral("이천시"),
        QStringLiteral("안성시"), QStringLiteral("김포시"), QStringLiteral("화성시"),
        QStringLiteral("광주시"), QStringLiteral("양주시"), QStringLiteral("포천시"),
        QStringLiteral("여주시"), QStringLiteral("연천군"), QStringLiteral("가평군"),
        QStringLiteral("양평군")}},
      {QStringLiteral("강원특별자치도"), QStringLiteral("강원"),
       {QStringLiteral("춘천시"), QStringLiteral("원주시"), QStringLiteral("강릉시"),
        QStringLiteral("동해시"), QStringLiteral("태백시"), QStringLiteral("속초시"),
        QStringLiteral("삼척시"), QStringLiteral("홍천군"), QStringLiteral("횡성군"),
        QStringLiteral("영월군"), QStringLiteral("평창군"), QStringLiteral("정선군"),
        QStringLiteral("철원군"), QStringLiteral("화천군"), QStringLiteral("양구군"),
        QStringLiteral("인제군"), QStringLiteral("고성군"), QStringLiteral("양양군")}},
      {QStringLiteral("충청북도"), QStringLiteral("충북"),
       {QStringLiteral("청주시"), QStringLiteral("충주시"), QStringLiteral("제천시"),
        QStringLiteral("보은군"), QStringLiteral("옥천군"), QStringLiteral("영동군"),
        QStringLiteral("증평군"), QStringLiteral("진천군"), QStringLiteral("괴산군"),
        QStringLiteral("음성군"), QStringLiteral("단양군")}},
      {QStringLiteral("충청남도"), QStringLiteral("충남"),
       {QStringLiteral("천안시"), QStringLiteral("공주시"), QStringLiteral("보령시"),
        QStringLiteral("아산시"), QStringLiteral("서산시"), QStringLiteral("논산시"),
        QStringLiteral("계룡시"), QStringLiteral("당진시"), QStringLiteral("금산군"),
        QStringLiteral("부여군"), QStringLiteral("서천군"), QStringLiteral("청양군"),
        QStringLiteral("홍성군"), QStringLiteral("예산군"), QStringLiteral("태안군")}},
      {QStringLiteral("전북특별자치도"), QStringLiteral("전북"),
       {QStringLiteral("전주시"), QStringLiteral("군산시"), QStringLiteral("익산시"),
        QStringLiteral("정읍시"), QStringLiteral("남원시"), QStringLiteral("김제시"),
        QStringLiteral("완주군"), QStringLiteral("진안군"), QStringLiteral("무주군"),
        QStringLiteral("장수군"), QStringLiteral("임실군"), QStringLiteral("순창군"),
        QStringLiteral("고창군"), QStringLiteral("부안군")}},
      {QStringLiteral("전라남도"), QStringLiteral("전남"),
       {QStringLiteral("목포시"), QStringLiteral("여수시"), QStringLiteral("순천시"),
        QStringLiteral("나주시"), QStringLiteral("광양시"), QStringLiteral("담양군"),
        QStringLiteral("곡성군"), QStringLiteral("구례군"), QStringLiteral("고흥군"),
        QStringLiteral("보성군"), QStringLiteral("화순군"), QStringLiteral("장흥군"),
        QStringLiteral("강진군"), QStringLiteral("해남군"), QStringLiteral("영암군"),
        QStringLiteral("무안군"), QStringLiteral("함평군"), QStringLiteral("영광군"),
        QStringLiteral("장성군"), QStringLiteral("완도군"), QStringLiteral("진도군"),
        QStringLiteral("신안군")}},
      {QStringLiteral("경상북도"), QStringLiteral("경북"),
       {QStringLiteral("포항시"), QStringLiteral("경주시"), QStringLiteral("김천시"),
        QStringLiteral("안동시"), QStringLiteral("구미시"), QStringLiteral("영주시"),
        QStringLiteral("영천시"), QStringLiteral("상주시"), QStringLiteral("문경시"),
        QStringLiteral("경산시"), QStringLiteral("의성군"), QStringLiteral("청송군"),
        QStringLiteral("영양군"), QStringLiteral("영덕군"), QStringLiteral("청도군"),
        QStringLiteral("고령군"), QStringLiteral("성주군"), QStringLiteral("칠곡군"),
        QStringLiteral("예천군"), QStringLiteral("봉화군"), QStringLiteral("울진군"),
        QStringLiteral("울릉군")}},
      {QStringLiteral("경상남도"), QStringLiteral("경남"),
       {QStringLiteral("창원시"), QStringLiteral("진주시"), QStringLiteral("통영시"),
        QStringLiteral("사천시"), QStringLiteral("김해시"), QStringLiteral("밀양시"),
        QStringLiteral("거제시"), QStringLiteral("양산시"), QStringLiteral("의령군"),
        QStringLiteral("함안군"), QStringLiteral("창녕군"), QStringLiteral("고성군"),
        QStringLiteral("남해군"), QStringLiteral("하동군"), QStringLiteral("산청군"),
        QStringLiteral("함양군"), QStringLiteral("거창군"), QStringLiteral("합천군")}},
      {QStringLiteral("제주특별자치도"), QStringLiteral("제주"),
       {QStringLiteral("제주시"), QStringLiteral("서귀포시")}},
  };
  return k;
}

struct CatalogData {
  QVector<KoreaSido> sido;
  QHash<QString, QString> aliases;
  QString source;
};

// Same search as data/korea_dongs.json: working directory, then next to the app.
QStringList dataCandidates(const QString& name) {
  const QDir app(QCoreApplication::applicationDirPath());
  return {QDir::current().filePath(QStringLiteral("data/") + name),
          app.filePath(QStringLiteral("data/") + name), app.filePath(QStringLiteral("../data/") + name)};
}

CatalogData loadCatalog() {
  for (const QString& p : dataCandidates(QStringLiteral("korea_regions.json"))) {
    QFile f(p);
    if (!f.open(QIODevice::ReadOnly)) continue;
    CatalogData data;
    QString error;
    data.sido = KoreaRegionCatalog::parseTable(f.readAll(), &data.aliases, &error);
    if (data.sido.isEmpty()) {
      qWarning("korea_regions.json ignored: %s", qUtf8Printable(error));
      break;
    }
    data.source = QFileInfo(p).absoluteFilePath();
    return data;
  }
  return {builtInTable(), {}, QStringLiteral("built-in")};
}

// Loaded once (thread-safe static init). Read from worker threads too.
const CatalogData& catalog() {
  static const CatalogData data = loadCatalog();
  return data;
}

const QVector<KoreaSido>& table() { return catalog().sido; }

}  // namespace

QVector<KoreaSido> KoreaRegionCatalog::parseTable(const QByteArray& json,
                                                  QHash<QString, QString>* aliases,
                                                  QString* error) {
  auto fail = [error](const QString& why) { if (error) *error = why; return QVector<KoreaSido>(); };
  const QJsonArray list = QJsonDocument::fromJson(json).object().value(QStringLiteral("sido")).toArray();
  if (list.isEmpty()) return fail(QStringLiteral("no sido entries"));
  QVector<KoreaSido> out;
  QHash<QString, QString> spelled;
  for (const QJsonValue& value : list) {
    const QJsonObject o = value.toObject();
    KoreaSido sido;
    sido.name = o.value(QStringLiteral("name")).toString().trimmed();
    sido.shortName = o.value(QStringLiteral("short")).toString().trimmed();
    for (const QJsonValue& c : o.value(QStringLiteral("cities")).toArray())
      if (const QString city = c.toString().trimmed(); !city.isEmpty() && !sido.cities.contains(city))
        sido.cities.append(city);
    // One bad entry rejects the whole file: a half table would hide real 시·군.
    if (sido.name.isEmpty() || sido.cities.isEmpty())
      return fail(QStringLiteral("entry without name or cities"));
    if (sido.shortName.isEmpty()) sido.shortName = sido.name;
    for (const QJsonValue& a : o.value(QStringLiteral("aliases")).toArray())
      if (const QString alias = a.toString().trimmed(); !alias.isEmpty()) spelled.insert(alias, sido.name);
    out.append(sido);
  }
  if (aliases) *aliases = spelled;
  if (error) error->clear();
  return out;
}

QString KoreaRegionCatalog::tableSource() { return catalog().source; }

QVector<KoreaSido> KoreaRegionCatalog::allSido() { return table(); }

QStringList KoreaRegionCatalog::sidoNames() {
  QStringList names;
  for (const KoreaSido& s : table()) names.append(s.name);
  return names;
}

QString KoreaRegionCatalog::canonicalSido(const QString& name) {
  const QString t = name.trimmed();
  if (t.isEmpty()) return {};
  if (t == QLatin1String("강원") || t == QStringLiteral("강원도"))
    return QStringLiteral("강원특별자치도");
  if (t == QLatin1String("전북") || t == QStringLiteral("전라북도") || t == QStringLiteral("전북도"))
    return QStringLiteral("전북특별자치도");
  if (t == QLatin1String("제주") || t == QStringLiteral("제주도"))
    return QStringLiteral("제주특별자치도");
  if (const auto alias = catalog().aliases.constFind(t); alias != catalog().aliases.cend())
    return alias.value();
  for (const KoreaSido& s : table()) {
    if (s.name == t || s.shortName == t) return s.name;
  }
  return t;
}

std::optional<KoreaRegionBounds> KoreaRegionCatalog::overviewBounds(const QString& sido) {
  // Approximate mainland/Jeju framing: SGIS 2020 largest polygon extents,
  // transformed EPSG:5179 -> 4326 and rounded outward to 0.01 degrees.
  // Public SGIS snapshot: github.com/swcho/korea-maps, commit
  // 8d53dca28fed87f98a75886ea86ed75774f052eb, json/전국_시도_경계.json.
  // These navigation presets deliberately omit remote islands; never use them
  // as administrative boundaries or for clipping/exporting survey data.
  static const QHash<QString, KoreaRegionBounds> bounds = {
      {QStringLiteral("서울특별시"), {126.76, 37.42, 127.19, 37.71}},
      {QStringLiteral("부산광역시"), {128.79, 35.03, 129.31, 35.39}},
      // Include Gunwi: official extrema at gunwi.go.kr/ko/page.do?mnu_uid=186.
      {QStringLiteral("대구광역시"), {128.35, 35.60, 128.90, 36.33}},
      {QStringLiteral("인천광역시"), {126.57, 37.33, 126.80, 37.64}},
      {QStringLiteral("광주광역시"), {126.64, 35.05, 127.03, 35.26}},
      {QStringLiteral("대전광역시"), {127.24, 36.18, 127.56, 36.51}},
      {QStringLiteral("울산광역시"), {128.97, 35.32, 129.47, 35.73}},
      {QStringLiteral("세종특별자치시"), {127.12, 36.40, 127.42, 36.74}},
      {QStringLiteral("경기도"), {126.51, 36.89, 127.85, 38.29}},
      {QStringLiteral("강원특별자치도"), {127.09, 37.02, 129.37, 38.62}},
      {QStringLiteral("충청북도"), {127.27, 36.01, 128.66, 37.26}},
      {QStringLiteral("충청남도"), {126.11, 35.97, 127.64, 37.07}},
      {QStringLiteral("전북특별자치도"), {126.42, 35.29, 127.92, 36.16}},
      {QStringLiteral("전라남도"), {126.15, 34.29, 127.79, 35.49}},
      {QStringLiteral("경상북도"), {127.79, 35.56, 129.59, 37.15}},
      {QStringLiteral("경상남도"), {127.57, 34.76, 129.22, 35.91}},
      {QStringLiteral("제주특별자치도"), {126.16, 33.19, 126.95, 33.57}},
  };
  const auto it = bounds.constFind(canonicalSido(sido));
  if (it == bounds.cend()) return std::nullopt;
  return it.value();
}

const QHash<QString, QStringList>& dongTable() {
  static QHash<QString, QStringList> map;
  static bool loaded = false;
  if (loaded) return map;
  loaded = true;
  for (const QString& p : dataCandidates(QStringLiteral("korea_dongs.json"))) {
    QFile f(p);
    if (!f.open(QIODevice::ReadOnly)) continue;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isObject()) continue;
    const QJsonObject obj = doc.object();
    for (auto it = obj.begin(); it != obj.end(); ++it) {
      QStringList list;
      const QJsonArray arr = it.value().toArray();
      for (const QJsonValue& v : arr) {
        const QString d = v.toString().trimmed();
        if (!d.isEmpty() && !list.contains(d)) list.append(d);
      }
      map.insert(it.key(), list);
    }
    break;
  }
  return map;
}

QStringList KoreaRegionCatalog::dongsOf(const QString& sidoName, const QString& cityName) {
  const QString city = cityName.trimmed();
  if (city.isEmpty()) return {};
  return dongTable().value(canonicalSido(sidoName) + QLatin1Char('|') + city);
}

QStringList KoreaRegionCatalog::citiesOf(const QString& sidoName) {
  const QString canon = canonicalSido(sidoName);
  for (const KoreaSido& s : table()) {
    if (s.name == canon) return s.cities;
  }
  return {};
}

QString KoreaRegionCatalog::composeAddress(const QString& sido, const QString& city,
                                           const QString& dong, const QString& lot) {
  QStringList parts;
  const QString s = canonicalSido(sido);
  if (!s.isEmpty()) parts.append(s);
  const QString c = city.trimmed();
  if (!c.isEmpty()) parts.append(c);
  const QString d = dong.trimmed();
  if (!d.isEmpty()) parts.append(d);
  const QString l = lot.trimmed();
  if (!l.isEmpty()) parts.append(l);
  return parts.join(QLatin1Char(' '));
}
