#include "CadCrsHints.h"

#include "CadCrsGuess.h"
#include "KoreaRegionCatalog.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSet>
#include <QSettings>
#include <QStandardPaths>

#include <qgscoordinatereferencesystem.h>

#include <algorithm>

namespace CadCrsHints {
namespace {

constexpr int kRecentMax = 5;

QString memoryFile() {
  return QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation) + QStringLiteral("/cad-crs.ini");
}

void add(QStringList* out, const QString& authId) {
  if (!authId.isEmpty() && !out->contains(authId) && CadCrsGuess::candidateAuthIds().contains(authId)) *out << authId;
}

// 원본 옆 같은 이름의 .prj(대소문자 무관). 권위 번호가 없는 ESRI 글이면 후보와 같은 정의인지 본다.
QString fromPrj(const QString& sourcePath) {
  const QFileInfo source(sourcePath);
  QString prjPath;
  for (const QFileInfo& prj : source.dir().entryInfoList({QStringLiteral("*.prj")}, QDir::Files))
    if (prj.completeBaseName().compare(source.completeBaseName(), Qt::CaseInsensitive) == 0) prjPath = prj.filePath();
  QFile file(prjPath);
  if (prjPath.isEmpty() || !file.open(QIODevice::ReadOnly)) return {};
  const QgsCoordinateReferenceSystem crs = QgsCoordinateReferenceSystem::fromWkt(QString::fromUtf8(file.read(65536)));
  if (!crs.isValid()) return {};
  if (CadCrsGuess::candidateAuthIds().contains(crs.authid())) return crs.authid();
  for (const QString& authId : CadCrsGuess::candidateAuthIds())
    if (QgsCoordinateReferenceSystem(authId).toProj() == crs.toProj()) return authId;
  return {};
}

// 「중부원점」 같은 원점 이름 → (GRS80 2010·2002, 베셀 보정). 2010 과 2002 는 북쪽 숫자가 100 km 다르다.
QPair<QStringList, QString> originCodes(const QString& origin) {
  static const QHash<QString, QPair<QStringList, QString>> codes = {
      {QStringLiteral("중부"), {{QStringLiteral("EPSG:5186"), QStringLiteral("EPSG:5181")}, QStringLiteral("EPSG:5174")}},
      {QStringLiteral("동부"), {{QStringLiteral("EPSG:5187"), QStringLiteral("EPSG:5183")}, QStringLiteral("EPSG:5176")}},
      {QStringLiteral("서부"), {{QStringLiteral("EPSG:5185"), QStringLiteral("EPSG:5180")}, QStringLiteral("EPSG:5173")}},
      {QStringLiteral("동해"), {{QStringLiteral("EPSG:5188"), QStringLiteral("EPSG:5184")}, QStringLiteral("EPSG:5177")}},
      {QStringLiteral("제주"), {{QStringLiteral("EPSG:5182")}, QStringLiteral("EPSG:5175")}},
  };
  return codes.value(origin);
}

void fromText(const QString& text, QStringList* out) {
  static const QRegularExpression epsg(QStringLiteral("EPSG\\s*[:_-]?\\s*(\\d{4,5})(?!\\d)"),
                                       QRegularExpression::CaseInsensitiveOption);
  static const QRegularExpression utmk(QStringLiteral("UTM\\s*-?\\s*K"), QRegularExpression::CaseInsensitiveOption);
  static const QRegularExpression origin(QStringLiteral("(중부|동부|서부|동해|제주)\\s*원점"));
  static const QRegularExpression grs80(QStringLiteral("GRS\\s*80|세계\\s*측지계|ITRF"),
                                        QRegularExpression::CaseInsensitiveOption);
  static const QRegularExpression bessel(QStringLiteral("베셀|Bessel|동경\\s*측지계|지역\\s*측지계"),
                                         QRegularExpression::CaseInsensitiveOption);
  for (auto it = epsg.globalMatch(text); it.hasNext();) add(out, QStringLiteral("EPSG:") + it.next().captured(1));
  if (utmk.match(text).hasMatch()) add(out, QStringLiteral("EPSG:5179"));
  const QRegularExpressionMatch named = origin.match(text);
  if (!named.hasMatch()) return;
  const QPair<QStringList, QString> codes = originCodes(named.captured(1));
  const bool isGrs80 = grs80.match(text).hasMatch();
  const bool isBessel = bessel.match(text).hasMatch();
  if (isGrs80 || !isBessel)
    for (const QString& authId : codes.first) add(out, authId);
  if (isBessel || !isGrs80) add(out, codes.second);
}

}  // namespace

QStringList stated(const QString& sourcePath, const QStringList& texts) {
  QStringList out;
  add(&out, fromPrj(sourcePath));
  // 파일 이름은 맨 번호(「_5187」)도 쓴다. 더 긴 숫자의 일부나 「산5174」·「5186-1번지」 같은 지번은 아니다.
  static const QRegularExpression code(
      QStringLiteral("EPSG[\\s_:-]*(\\d{4,5})(?!\\d)|(?<![\\d\\p{L}])(\\d{4,5})(?![\\d\\p{L}]|-\\d)"),
      QRegularExpression::CaseInsensitiveOption);
  for (auto it = code.globalMatch(QFileInfo(sourcePath).completeBaseName()); it.hasNext();) {
    const QRegularExpressionMatch match = it.next();
    add(&out, QStringLiteral("EPSG:") + (match.captured(1).isEmpty() ? match.captured(2) : match.captured(1)));
  }
  for (const QString& text : texts) fromText(text, &out);
  return out;
}

QStringList provinces(const QStringList& names) {
  struct Gazetteer {
    QHash<QString, QStringList> provincesOf;  // 이름(세 글자 이상) → 시·도
    QSet<QChar> lastChars;
    qsizetype longest = 0;
  };
  static const Gazetteer gazetteer = [] {
    Gazetteer g;
    const auto note = [&g](const QString& name, const QString& sido) {
      if (name.size() <= 2 || g.provincesOf[name].contains(sido)) return;
      g.provincesOf[name] << sido;
      g.lastChars.insert(name.back());
      g.longest = std::max(g.longest, name.size());
    };
    for (const QString& sido : KoreaRegionCatalog::sidoNames()) {
      note(sido, sido);
      for (const QString& city : KoreaRegionCatalog::citiesOf(sido)) {
        note(city, sido);
        for (const QString& dong : KoreaRegionCatalog::dongsOf(sido, city)) note(dong, sido);
      }
    }
    return g;
  }();
  QSet<QString> found;
  for (const QString& text : names)
    for (qsizetype end = 2; end < text.size(); ++end) {
      if (!gazetteer.lastChars.contains(text.at(end))) continue;
      for (qsizetype length = 3; length <= std::min(gazetteer.longest, end + 1); ++length) {
        const auto hit = gazetteer.provincesOf.constFind(text.mid(end + 1 - length, length));
        if (hit != gazetteer.provincesOf.cend())
          for (const QString& sido : *hit) found.insert(sido);
      }
    }
  QStringList out;
  for (const QString& sido : KoreaRegionCatalog::sidoNames())  // 시·도 순서를 지킨다
    if (found.contains(sido)) out << sido;
  return out;
}

QString remembered(const QString& sha256) {
  if (sha256.isEmpty()) return {};
  const QString authId =
      QSettings(memoryFile(), QSettings::IniFormat).value(QStringLiteral("crs/") + sha256).toString();
  return QgsCoordinateReferenceSystem(authId).isValid() ? authId : QString();
}

void remember(const QString& sha256, const QString& authId) {
  QSettings settings(memoryFile(), QSettings::IniFormat);
  if (authId.isEmpty()) {
    if (!sha256.isEmpty()) settings.remove(QStringLiteral("crs/") + sha256);
    return;
  }
  if (!sha256.isEmpty()) settings.setValue(QStringLiteral("crs/") + sha256, authId);
  QStringList latest = settings.value(QStringLiteral("recent")).toStringList();
  latest.removeAll(authId);
  latest.prepend(authId);
  settings.setValue(QStringLiteral("recent"), latest.mid(0, kRecentMax));
}

QStringList recent() {
  return QSettings(memoryFile(), QSettings::IniFormat).value(QStringLiteral("recent")).toStringList();
}

}  // namespace CadCrsHints
