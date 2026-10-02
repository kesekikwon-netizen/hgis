#pragma once

// Provider policy for web background tiles (evaluation F059).
//
// The offline tile pack exists so VWorld satellite/cadastral pictures of the survey area
// survive an LTE drop (user request, 2026-09-04). Google, OpenStreetMap, OpenTopoMap and
// Carto publish usage rules against bulk or offline copies of their tiles, so the app
// does not build a pack from them; other hosts are allowed only after the user confirms.
// The VWorld Referer header authenticates VWorld keys and is sent to vworld.kr only.
// Header-only so the ribbon, offline and layout code can share it without a CMake change.

#include <QString>
#include <QUrl>

#include <qgslayertree.h>
#include <qgslayertreelayer.h>
#include <qgsmaplayer.h>
#include <qgsproject.h>

namespace BasemapPolicy {

enum class Provider { VWorld, NasaGibs, Google, OpenStreetMap, OpenTopoMap, Carto, Other };

// Host of an XYZ template, a plain URL, a QGIS "type=xyz&url=…" / WMS "…&url=…" source,
// or GDAL_WMS XML (first http(s) address in the text).
inline QString hostOf(const QString& urlOrSource) {
  QString url = urlOrSource.trimmed();
  if (!url.startsWith(QLatin1String("http"), Qt::CaseInsensitive)) {
    QString found;
    // Split on '&' so "IgnoreGetMapUrl=1" is not mistaken for the url= item.
    for (const QString& part : url.split(QLatin1Char('&'))) {
      if (part.startsWith(QLatin1String("url="), Qt::CaseInsensitive)) {
        found = QUrl::fromPercentEncoding(part.mid(4).toUtf8());
        break;
      }
    }
    if (found.isEmpty()) {
      const int scheme = url.indexOf(QLatin1String("://"));
      if (scheme < 0) return {};
      const int start = url.lastIndexOf(QLatin1Char('h'), scheme);
      if (start < 0) return {};
      found = url.mid(start);
    }
    url = found;
  }
  // Keep only scheme://host so template braces or XML after the host cannot confuse QUrl.
  const int scheme = url.indexOf(QLatin1String("://"));
  if (scheme < 0) return {};
  int end = scheme + 3;
  while (end < url.size() && url.at(end) != QLatin1Char('/') && url.at(end) != QLatin1Char('?') &&
         url.at(end) != QLatin1Char('&') && url.at(end) != QLatin1Char('<') &&
         url.at(end) != QLatin1Char('"') && !url.at(end).isSpace())
    ++end;
  return QUrl(url.left(end)).host().toLower();
}

// "vworld.kr" or any "*.vworld.kr"; "evilvworld.kr" is not VWorld.
inline bool hostIsDomain(const QString& host, const char* domain) {
  const QLatin1String d(domain);
  return host == d || (host.endsWith(d) && host.size() > d.size() &&
                       host.at(host.size() - d.size() - 1) == QLatin1Char('.'));
}

inline Provider providerForUrl(const QString& urlOrSource) {
  const QString host = hostOf(urlOrSource);
  if (host.isEmpty()) return Provider::Other;
  if (hostIsDomain(host, "vworld.kr")) return Provider::VWorld;
  if (hostIsDomain(host, "earthdata.nasa.gov")) return Provider::NasaGibs;
  // mt0-3.google.com, khms*.google.com, *.googleapis.com
  if (hostIsDomain(host, "google.com") || hostIsDomain(host, "googleapis.com") ||
      hostIsDomain(host, "google.co.kr"))
    return Provider::Google;
  if (hostIsDomain(host, "openstreetmap.org")) return Provider::OpenStreetMap;
  if (hostIsDomain(host, "opentopomap.org")) return Provider::OpenTopoMap;
  if (hostIsDomain(host, "cartocdn.com")) return Provider::Carto;
  return Provider::Other;
}

inline Provider providerOf(const QgsMapLayer* layer) {
  return layer ? providerForUrl(layer->source()) : Provider::Other;
}

inline QString providerLabel(Provider provider) {
  switch (provider) {
  case Provider::VWorld: return QStringLiteral("VWorld");
  case Provider::NasaGibs: return QStringLiteral("NASA GIBS");
  case Provider::Google: return QStringLiteral("Google 지도");
  case Provider::OpenStreetMap: return QStringLiteral("OpenStreetMap");
  case Provider::OpenTopoMap: return QStringLiteral("OpenTopoMap");
  case Provider::Carto: return QStringLiteral("Carto");
  case Provider::Other: break;
  }
  return QStringLiteral("이 제공처");
}

struct OfflineDecision {
  bool allowed = false;    // a tile pack may be built
  bool askFirst = false;   // allowed only after the user confirms `message`
  QString message;         // shown when blocked or when asking
};

inline OfflineDecision offlineCachingForUrl(const QString& urlOrSource) {
  const Provider provider = providerForUrl(urlOrSource);
  switch (provider) {
  case Provider::VWorld:
  case Provider::NasaGibs:
    return {true, false, {}};
  case Provider::Google:
  case Provider::OpenStreetMap:
  case Provider::OpenTopoMap:
  case Provider::Carto:
    return {false, false,
            QStringLiteral("%1 타일은 이용조건상 한꺼번에 받아 두는 것이 허용되지 않아 오프라인 "
                           "저장을 하지 않습니다. VWorld 위성·지적을 올린 뒤 그 레이어를 저장하세요.")
                .arg(providerLabel(provider))};
  case Provider::Other:
    break;
  }
  return {true, true,
          QStringLiteral("이 배경지도의 제공처가 타일을 한꺼번에 받아 두는 것을 허용하는지 앱이 "
                         "확인할 수 없습니다. 제공처의 이용조건을 확인한 경우에만 계속하세요.")};
}

inline OfflineDecision offlineCaching(const QgsMapLayer* layer) {
  return offlineCachingForUrl(layer ? layer->source() : QString());
}

// Referer for a tile request or a GDAL_WMS <Referer>: only VWorld needs (and gets) one.
inline QString refererForUrl(const QString& urlOrSource) {
  return providerForUrl(urlOrSource) == Provider::VWorld ? QStringLiteral("https://localhost")
                                                         : QString();
}

// Note for a drawing (PDF) that shows this background, or empty when none is needed.
inline QString drawingNotice(const QgsMapLayer* layer) {
  if (providerOf(layer) != Provider::Google) return {};
  return QStringLiteral("Google 지도는 제출 도면에 넣기 전에 Google 이용조건과 출처 표기를 "
                        "확인하세요. 제출용 배경은 VWorld 위성을 권장합니다.");
}

// Google 위성을 올릴 때 알릴 말: 국내 정사영상이 아니어서 지적과 곳에 따라 어긋난다(2026-10-02 실측, R52).
inline QString googlePositionNotice() {
  return QStringLiteral("Google 위성을 올렸습니다. Google 사진은 국내 정사영상이 아니라서 지적선과 곳에 따라 수 m~15 m 넘게 "
                        "어긋날 수 있습니다. 지적·조사구역 위치 확인과 제출 도면에는 VWorld 위성을 쓰세요.");
}

// The notice for the first background shown in the legend that needs one, or empty.
inline QString drawingNoticeForProject(const QgsProject* project) {
  const QgsLayerTree* root = project ? project->layerTreeRoot() : nullptr;
  if (!root) return {};
  const auto nodes = root->findLayers();
  for (const QgsLayerTreeLayer* node : nodes) {
    if (!node || !node->isVisible()) continue;
    const QString notice = drawingNotice(node->layer());
    if (!notice.isEmpty()) return notice;
  }
  return {};
}

}  // namespace BasemapPolicy
