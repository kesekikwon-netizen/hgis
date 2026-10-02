#include "SubmitReadme.h"
#include "LayerLabelControls.h"
#include "LayerOps.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSet>
#include <QTimeZone>
#include <QUrl>

#include <qgslayoutitemmap.h>
#include <qgslayoutmanager.h>
#include <qgsmaplayer.h>
#include <qgsprintlayout.h>
#include <qgsproject.h>
#include <qgsproviderregistry.h>

#ifndef KA_HGIS_VERSION
#define KA_HGIS_VERSION "unknown"
#endif
#ifndef KA_HGIS_GIT_HASH
#define KA_HGIS_GIT_HASH "unknown"
#endif

namespace SubmitReadme {
namespace {
QString nowWithOffset() {
  const QDateTime now = QDateTime::currentDateTime();
  return now.toOffsetFromUtc(now.offsetFromUtc()).toString(Qt::ISODate);
}

QString termsForHost(const QString& host) {
  const QString h = host.toLower();
  if (h.contains(QLatin1String("vworld"))) return QStringLiteral("VWorld 공간정보 오픈플랫폼(국토교통부) 이용 조건");
  if (h.contains(QLatin1String("ngii")) || h.contains(QLatin1String("nsdi")))
    return QStringLiteral("국토지리정보원 국토정보플랫폼 이용 조건");
  if (h.contains(QLatin1String("openstreetmap"))) return QStringLiteral("© OpenStreetMap 기여자, ODbL");
  if (h.contains(QLatin1String("google"))) return QStringLiteral("Google 지도 이용 약관");
  return QStringLiteral("제공처 이용 조건");
}

QString describe(QgsMapLayer* layer, const QString& today) {
  const QVariantMap parts = QgsProviderRegistry::instance()->decodeUri(layer->providerType(), layer->source());
  const QString path = parts.value(QStringLiteral("path")).toString();
  const QString fileDate = !path.isEmpty() && QFileInfo(path).isFile()
                               ? QFileInfo(path).lastModified().date().toString(Qt::ISODate)
                               : QString();
  if (LayerLabelControls::isHeritage(layer)) {
    return QStringLiteral("- 「%1」 · 국가유산 공간정보(인트라넷에서 받음) · 받은 날짜 %2 · 배포 제한 자료라 원본 파일은 넣지 않았습니다")
        .arg(layer->name(), fileDate.isEmpty() ? QStringLiteral("모름") : fileDate);
  }
  const QString url = parts.value(QStringLiteral("url")).toString();
  const QString host = QUrl(url).host();
  if (!host.isEmpty()) {
    // Only the host: VWorld keys live in the URL path and query.
    return QStringLiteral("- 「%1」 · 온라인 지도 %2 (%3) · 도면에 그린 날 %4 · %5")
        .arg(layer->name(), host, layer->providerType(), today, termsForHost(host));
  }
  if (!path.isEmpty()) {
    return QStringLiteral("- 「%1」 · 파일 %2 · 파일 날짜 %3")
        .arg(layer->name(), QFileInfo(path).fileName(), fileDate.isEmpty() ? QStringLiteral("모름") : fileDate);
  }
  return QStringLiteral("- 「%1」 · %2").arg(layer->name(), layer->providerType());
}
}  // namespace

QStringList referenceLines(QgsProject* project, const QStringList& layoutNames) {
  QStringList lines;
  if (!project || !project->layoutManager()) return lines;
  const QString today = QDate::currentDate().toString(Qt::ISODate);
  QSet<QString> seen;
  for (const QString& name : layoutNames) {
    auto* layout = dynamic_cast<QgsPrintLayout*>(project->layoutManager()->layoutByName(name));
    if (!layout) continue;
    QList<QgsLayoutItemMap*> maps;
    layout->layoutItems(maps);
    for (QgsLayoutItemMap* map : maps) {
      for (QgsMapLayer* layer : map ? map->layers() : QList<QgsMapLayer*>()) {
        if (!layer || seen.contains(layer->id())) continue;
        if (!LayerOps::isReferenceLayer(layer) && !LayerOps::isBasemapLayer(layer) &&
            !LayerOps::isCadastralLayer(layer) && !LayerLabelControls::isHeritage(layer))
          continue;
        seen.insert(layer->id());
        lines << describe(layer, today);
      }
    }
  }
  return lines;
}

QString sha256OfFile(const QString& path, QString* error) {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    if (error) *error = QStringLiteral("%1을(를) 열 수 없습니다: %2").arg(QFileInfo(path).fileName(), file.errorString());
    return {};
  }
  QCryptographicHash hash(QCryptographicHash::Sha256);
  if (!hash.addData(&file)) {
    if (error) *error = QStringLiteral("%1을(를) 끝까지 읽지 못했습니다.").arg(QFileInfo(path).fileName());
    return {};
  }
  return QString::fromLatin1(hash.result().toHex());
}

QString folderNamePart(const QString& text) {
  QString safe = text.trimmed();
  safe.replace(QRegularExpression(QStringLiteral("[\\\\/:*?\"<>|\\x00-\\x1f]")), QStringLiteral("_"));
  while (safe.endsWith(QLatin1Char('.')) || safe.endsWith(QLatin1Char(' '))) safe.chop(1);
  return safe.left(40);
}

QString build(const Inputs& in) {
  QString t;
  t += QStringLiteral("KA-HGIS submission package\n");
  t += QStringLiteral("created: %1\n").arg(nowWithOffset());
  t += QStringLiteral("timezone: %1\n").arg(QString::fromUtf8(QTimeZone::systemTimeZoneId()));
  t += QStringLiteral("app: Strata %1 (%2)\n")
           .arg(QLatin1String(KA_HGIS_VERSION), QLatin1String(KA_HGIS_GIT_HASH));
  t += QStringLiteral("crs: EPSG:5179\n");
  t += QStringLiteral("shp_encoding: %1\n").arg(in.encoding);
  t += QStringLiteral("survey_name: %1\n").arg(in.surveyName.isEmpty() ? QStringLiteral("-") : in.surveyName);
  t += QStringLiteral("survey_file: %1\n").arg(in.surveyFile.isEmpty() ? QStringLiteral("-") : in.surveyFile);
  t += QStringLiteral("survey_sha256: %1\n").arg(in.surveySha256.isEmpty() ? QStringLiteral("-") : in.surveySha256);
  t += in.unsavedEditsIncluded
           ? QStringLiteral("saved_state: unsaved_edits_included (저장하지 않은 편집이 들어 있습니다. "
                            "survey_sha256은 마지막으로 저장한 조사 파일의 값입니다)\n")
           : QStringLiteral("saved_state: saved\n");
  t += QStringLiteral("intranet: upload each domain SHP (feature_poly.shp = one file; merge polygons in-app first if required)\n\n");
  t += QStringLiteral("checklist:\n") + in.checklistSummary + QLatin1Char('\n');
  if (!in.fieldNotes.isEmpty()) t += QStringLiteral("\nshp_field_names:\n") + in.fieldNotes.join(QLatin1Char('\n')) + QLatin1Char('\n');
  if (!in.layerLines.isEmpty()) t += QStringLiteral("\nshp_layers:\n") + in.layerLines.join(QLatin1Char('\n')) + QLatin1Char('\n');
  t += QStringLiteral("\nreference_data (도면에 그린 참조 지도):\n");
  t += in.referenceLines.isEmpty() ? QStringLiteral("- 없음\n") : in.referenceLines.join(QLatin1Char('\n')) + QLatin1Char('\n');
  t += QStringLiteral("\nSee MANIFEST.sha256 for file hashes.\n");
  t += QStringLiteral("도면 PDF는 도면만들기 용지(user_sheet 및 section_sheet)를 넣습니다.\n");
  t += in.hasSheetPdf ? QStringLiteral("- 조사도면.pdf (도면만들기 user_sheet)\n")
                      : QStringLiteral("조사도면.pdf 없음: 도면만들기에서 용지를 만든 뒤 다시 보내기 하세요.\n");
  if (in.hasSectionPdf) t += QStringLiteral("- 단면도.pdf (단면도면만들기 section_sheet)\n");
  return t;
}

}  // namespace SubmitReadme
