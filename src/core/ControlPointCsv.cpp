#include "KaSessionLog.h"
#include "LayerOps.h"
#include "LayerLabelControls.h"
#include "DemPresentation.h"
#include "DemColorRampLegend.h"
#include "GeorefService.h"
#include "SoilMapService.h"
#include "VworldSettings.h"
#include "KaPortableRuntime.h"

#include <QSignalBlocker>
#include <QDateTime>
#include <QEvent>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QTextStream>
#include <QStringConverter>
#include <QRegularExpression>
#include <cmath>
#include <memory>
#include <limits>
#include <algorithm>
#include <QPainter>
#include <QScreen>
#include <QSize>
#include <QUrl>
#include <QWindow>
#include <QColor>
#include <QFont>
#include <QDir>
#include <QDomDocument>
#include <QUrlQuery>
#include <QSet>
#include <QTemporaryFile>
#include <QPointer>
#include <QScopedValueRollback>
#include <QTimer>
#include <QHash>
#include <functional>
#include <QNetworkRequest>

#include <qgis.h>
#include <QUndoStack>
#include <qgsproject.h>
#include <qgssnappingconfig.h>
#include <qgsvectorlayer.h>
#include <qgsrasterlayer.h>
#include <qgsbrightnesscontrastfilter.h>
#include <qgsmapcanvas.h>
#include <qgsvectorfilewriter.h>
#include <qgscoordinatereferencesystem.h>
#include <qgscoordinatetransformcontext.h>
#include <qgscoordinatetransform.h>
#include <qgsexception.h>
#include <qgsfield.h>
#include <qgsfields.h>
#include <qgsfeature.h>
#include <qgsfeaturerequest.h>
#include <qgsfeatureiterator.h>
#include <qgsgeometry.h>
#include <qgspoint.h>
#include <qgspointxy.h>
#include <qgslinestring.h>
#include <qgscategorizedsymbolrenderer.h>
#include <qgssinglesymbolrenderer.h>
#include <qgsinvertedpolygonrenderer.h>
#include <qgssymbol.h>
#include <qgssymbollayer.h>
#include <qgsfillsymbol.h>
#include <qgsfillsymbollayer.h>
#include <qgslinesymbol.h>
#include <qgslinesymbollayer.h>
#include <qgsmarkersymbol.h>
#include <qgsrenderer.h>
#include <qgsrectangle.h>
#include <qgslayertree.h>
#include <qgslayertreelayer.h>
#include <qgsbilinearrasterresampler.h>
#include <qgsrasterresamplefilter.h>
#include <qgsrasterdataprovider.h>
#include <qgsvectordataprovider.h>
#include <qgsrasterrenderer.h>
#include <qgsrastertransparency.h>
#include <qgssinglebandpseudocolorrenderer.h>
#include <qgsrastershader.h>
#include <qgscolorrampshader.h>
#include <qgscolorramplegendnodesettings.h>
#include <qgshillshaderenderer.h>
#include <qgsrasterbandstats.h>
#include <cpl_conv.h>
#include <cpl_error.h>
#include <gdal.h>
#include <gdal_utils.h>
#include <ogr_api.h>
#include <qgsnetworkaccessmanager.h>
#include <qgslayertreegroup.h>
#include <qgsdataprovider.h>
#include <qgsprojectviewsettings.h>
#include <qgspallabeling.h>
#include <qgsvectorlayerlabeling.h>
#include <qgstextformat.h>
#include <qgslabelobstaclesettings.h>
#include <qgsreferencedgeometry.h>

static QString normalizeCsvHeader(QString h) {
  h = h.trimmed().toLower();
  h.remove(QLatin1Char('"'));
  h.replace(QLatin1Char(' '), QLatin1Char('_'));
  if (h == QLatin1String("id") || h == QLatin1String("point") || h == QLatin1String("pid"))
    return QStringLiteral("point_id");
  if (h == QLatin1String("lon") || h == QLatin1String("longitude") || h == QStringLiteral("경도") ||
      h == QLatin1String("easting") || h == QLatin1String("east"))
    return QStringLiteral("x");
  if (h == QLatin1String("lat") || h == QLatin1String("latitude") || h == QStringLiteral("위도") ||
      h == QLatin1String("northing") || h == QLatin1String("north"))
    return QStringLiteral("y");
  if (h == QLatin1String("acc") || h == QLatin1String("accuracy"))
    return QStringLiteral("accuracy_m");
  if (h == QLatin1String("fix") || h == QLatin1String("fixtype"))
    return QStringLiteral("fix_type");
  if (h == QLatin1String("proj") || h == QLatin1String("crs"))
    return QStringLiteral("projection");
  return h;
}

static bool csvHeaderIsGeographic(const QString& raw) {
  const QString h = raw.trimmed().toLower();
  return h == QLatin1String("lon") || h == QLatin1String("lat") || h == QLatin1String("longitude") ||
         h == QLatin1String("latitude") || h == QStringLiteral("경도") || h == QStringLiteral("위도");
}

static QString decodeCsvBytes(const QByteArray& raw, QString* encodingOut) {
  QByteArray bytes = raw;
  if (bytes.startsWith("\xEF\xBB\xBF")) {
    if (encodingOut) *encodingOut = QStringLiteral("UTF-8");
    return QString::fromUtf8(bytes.mid(3));
  }
  QStringDecoder utf8(QStringDecoder::Utf8);
  const QString utf8Text = utf8.decode(bytes);
  if (!utf8.hasError()) {
    if (encodingOut) *encodingOut = QStringLiteral("UTF-8");
    return utf8Text;
  }
  QStringDecoder cp949(QStringLiteral("CP949"));
  if (!cp949.isValid()) cp949 = QStringDecoder(QStringLiteral("EUC-KR"));
  if (!cp949.isValid()) {
    if (encodingOut) *encodingOut = QStringLiteral("UTF-8");
    return QString::fromUtf8(bytes);
  }
  if (encodingOut) *encodingOut = QStringLiteral("CP949");
  return cp949.decode(bytes);
}

struct ControlCsvTable {
  QString encoding;
  bool headerGeographic = false;
  QStringList headers;
  QList<QStringList> rows;
  int idColumn = -1;
  int xColumn = -1;
  int yColumn = -1;
};

static bool loadControlCsv(const QString& csvPath, ControlCsvTable* table, QString* errorOut) {
  QFile file(csvPath);
  if (!file.open(QIODevice::ReadOnly)) {
    if (errorOut) *errorOut = QStringLiteral("CSV를 열 수 없습니다: %1").arg(csvPath);
    return false;
  }
  const QString text = decodeCsvBytes(file.readAll(), &table->encoding);
  const QRegularExpression sep(QStringLiteral("[,;\\t]"));
  for (const QString& rawLine : text.split(QRegularExpression(QStringLiteral("[\\r\\n]+")))) {
    const QString line = rawLine.trimmed();
    if (line.isEmpty() || line.startsWith(QLatin1Char('#'))) continue;
    const QStringList parts = line.split(sep);
    if (parts.isEmpty()) continue;
    const QString h0 = normalizeCsvHeader(parts.first());
    if (table->headers.isEmpty() &&
        (h0 == QLatin1String("point_id") || h0 == QLatin1String("x") ||
         parts.first().trimmed().compare(QStringLiteral("id"), Qt::CaseInsensitive) == 0 ||
         parts.first().trimmed().compare(QStringLiteral("point_id"), Qt::CaseInsensitive) == 0)) {
      for (const QString& part : parts) {
        if (csvHeaderIsGeographic(part)) table->headerGeographic = true;
        table->headers.append(normalizeCsvHeader(part));
      }
      continue;
    }
    if (parts.size() < 3) continue;
    table->rows.append(parts);
  }
  if (table->headers.isEmpty()) {
    table->headers = {QStringLiteral("point_id"), QStringLiteral("x"), QStringLiteral("y"),
                      QStringLiteral("datum"), QStringLiteral("ellipsoid"), QStringLiteral("projection"),
                      QStringLiteral("accuracy_m"), QStringLiteral("pdop"), QStringLiteral("fix_type")};
  }
  table->idColumn = table->headers.indexOf(QStringLiteral("point_id"));
  table->xColumn = table->headers.indexOf(QStringLiteral("x"));
  table->yColumn = table->headers.indexOf(QStringLiteral("y"));
  if (table->xColumn < 0 || table->yColumn < 0) {
    if (errorOut) *errorOut = QStringLiteral("CSV에 x,y(또는 lon/lat) 열이 필요합니다.");
    return false;
  }
  if (table->rows.isEmpty()) {
    if (errorOut) *errorOut = QStringLiteral("CSV에 가져올 데이터 행이 없습니다.");
    return false;
  }
  return true;
}

static QString csvCell(const QStringList& row, int index) {
  if (index < 0 || index >= row.size()) return {};
  return row.at(index).trimmed().remove(QLatin1Char('"'));
}

static bool koreaDegreePair(double a, double b) {
  const auto lon = [](double v) { return v >= 124.0 && v <= 132.5; };
  const auto lat = [](double v) { return v >= 33.0 && v <= 43.5; };
  return (lon(a) && lat(b)) || (lat(a) && lon(b));
}

static bool rowsAreGeographic(const ControlCsvTable& table) {
  if (table.headerGeographic) {
    for (const QStringList& row : table.rows) {
      bool okX = false, okY = false;
      const double x = csvCell(row, table.xColumn).toDouble(&okX);
      const double y = csvCell(row, table.yColumn).toDouble(&okY);
      if (okX && okY && (std::abs(x) > 180.0 || std::abs(y) > 180.0)) return false;
    }
    return true;
  }
  bool any = false;
  for (const QStringList& row : table.rows) {
    bool okX = false, okY = false;
    const double x = csvCell(row, table.xColumn).toDouble(&okX);
    const double y = csvCell(row, table.yColumn).toDouble(&okY);
    if (!okX || !okY) continue;
    any = true;
    if (!koreaDegreePair(x, y)) return false;
  }
  return any;
}

static bool geographicPairInRange(double longitude, double latitude) {
  return std::abs(longitude) <= 180.0 && std::abs(latitude) <= 90.0;
}

static QgsPointXY mapControlPoint(double x, double y, bool swapAxes, bool geographic,
                                  const QgsCoordinateTransform* transform, bool* ok) {
  if (swapAxes) std::swap(x, y);
  if (geographic) {
    if (!transform || !transform->isValid() || !geographicPairInRange(x, y)) {
      if (ok) *ok = false;
      return {};
    }
    try {
      const QgsPointXY point = transform->transform(x, y);
      if (!std::isfinite(point.x()) || !std::isfinite(point.y())) {
        if (ok) *ok = false;
        return {};
      }
      if (ok) *ok = true;
      return point;
    } catch (const QgsCsException&) {
      if (ok) *ok = false;
      return {};
    }
  }
  if (ok) *ok = true;
  return QgsPointXY(x, y);
}

static double distanceToExtent(const QgsPointXY& point, const QgsRectangle& extent) {
  if (extent.isEmpty()) return std::numeric_limits<double>::infinity();
  if (extent.contains(point)) return 0.0;
  const double x = std::clamp(point.x(), extent.xMinimum(), extent.xMaximum());
  const double y = std::clamp(point.y(), extent.yMinimum(), extent.yMaximum());
  return std::hypot(point.x() - x, point.y() - y);
}

static QString formatMeters(double meters) {
  if (!std::isfinite(meters)) return QStringLiteral("알 수 없음");
  if (meters < 1000.0) return QStringLiteral("%1m").arg(qRound(meters));
  return QStringLiteral("%1km").arg(meters / 1000.0, 0, 'f', 1);
}

static QgsRectangle surveyExtentFor(QgsProject* project, const QgsCoordinateReferenceSystem& dest) {
  QgsRectangle box;
  bool any = false;
  if (!project) return box;
  for (QgsVectorLayer* layer : LayerOps::surveyAreaLayers(project)) {
    if (!layer || layer->featureCount() <= 0) continue;
    QgsRectangle extent = layer->extent();
    if (!extent.isFinite() || extent.isEmpty()) continue;
    if (layer->crs().isValid() && dest.isValid() && layer->crs() != dest) {
      try {
        extent = QgsCoordinateTransform(layer->crs(), dest, project->transformContext()).transformBoundingBox(extent);
      } catch (const QgsCsException&) {
        continue;
      }
    }
    if (!any) box = extent;
    else box.combineExtentWith(extent);
    any = true;
  }
  return any ? box : QgsRectangle();
}

static int importParsedControlCsv(QgsVectorLayer* controlPoints, const ControlCsvTable& table, bool swapAxes,
                                  QString* errorOut) {
  const bool geographic = rowsAreGeographic(table);
  QgsCoordinateTransform transform;
  bool haveTransform = false;
  if (geographic) {
    const QgsCoordinateReferenceSystem wgs(QStringLiteral("EPSG:4326"));
    if (!wgs.isValid() || !controlPoints->crs().isValid()) {
      if (errorOut) *errorOut = QStringLiteral("경위도를 작업 좌표계로 변환할 수 없습니다.");
      return -1;
    }
    transform = QgsCoordinateTransform(wgs, controlPoints->crs(), QgsProject::instance()->transformContext());
    haveTransform = transform.isValid();
    if (!haveTransform) {
      if (errorOut) *errorOut = QStringLiteral("경위도를 작업 좌표계로 변환할 수 없습니다.");
      return -1;
    }
  }
  if (!controlPoints->isEditable() && !controlPoints->startEditing()) {
    if (errorOut) *errorOut = QStringLiteral("control_points 편집 모드 실패");
    return -1;
  }
  int added = 0;
  for (const QStringList& row : table.rows) {
    bool okX = false, okY = false;
    const double rawX = csvCell(row, table.xColumn).toDouble(&okX);
    const double rawY = csvCell(row, table.yColumn).toDouble(&okY);
    if (!okX || !okY) continue;
    bool mappedOk = false;
    const QgsPointXY point = mapControlPoint(rawX, rawY, swapAxes, geographic, haveTransform ? &transform : nullptr,
                                             &mappedOk);
    if (!mappedOk) continue;
    QgsFeature feat(controlPoints->fields());
    const QString pid = table.idColumn >= 0 ? csvCell(row, table.idColumn) : QStringLiteral("P%1").arg(added + 1);
    auto setStr = [&](const char* field, int idx) {
      const int fi = controlPoints->fields().indexOf(QString::fromUtf8(field));
      if (fi >= 0 && idx >= 0) feat.setAttribute(fi, csvCell(row, idx));
    };
    auto setNum = [&](const char* field, int idx) {
      const int fi = controlPoints->fields().indexOf(QString::fromUtf8(field));
      if (fi < 0 || idx < 0) return;
      bool ok = false;
      const double v = csvCell(row, idx).toDouble(&ok);
      if (ok) feat.setAttribute(fi, v);
      else if (!csvCell(row, idx).isEmpty()) feat.setAttribute(fi, csvCell(row, idx));
    };
    {
      const int fi = controlPoints->fields().indexOf(QStringLiteral("point_id"));
      if (fi >= 0) feat.setAttribute(fi, pid);
    }
    {
      const int fi = controlPoints->fields().indexOf(QStringLiteral("x"));
      if (fi >= 0) feat.setAttribute(fi, point.x());
    }
    {
      const int fi = controlPoints->fields().indexOf(QStringLiteral("y"));
      if (fi >= 0) feat.setAttribute(fi, point.y());
    }
    setStr("datum", table.headers.indexOf(QStringLiteral("datum")));
    setStr("ellipsoid", table.headers.indexOf(QStringLiteral("ellipsoid")));
    setStr("projection", table.headers.indexOf(QStringLiteral("projection")));
    setStr("origin", table.headers.indexOf(QStringLiteral("origin")));
    setStr("fix_type", table.headers.indexOf(QStringLiteral("fix_type")));
    setNum("accuracy_m", table.headers.indexOf(QStringLiteral("accuracy_m")));
    setNum("pdop", table.headers.indexOf(QStringLiteral("pdop")));
    {
      const int fi = controlPoints->fields().indexOf(QStringLiteral("accuracy"));
      const int ia = table.headers.indexOf(QStringLiteral("accuracy_m"));
      if (fi >= 0 && ia >= 0 && !csvCell(row, ia).isEmpty()) feat.setAttribute(fi, csvCell(row, ia));
    }
    feat.setGeometry(QgsGeometry::fromPointXY(point));
    if (controlPoints->addFeature(feat)) ++added;
  }
  if (added == 0) {
    if (controlPoints->isEditable()) controlPoints->rollBack();
    if (errorOut) *errorOut = QStringLiteral("좌표로 읽을 수 있는 기준점이 없습니다.");
    return -1;
  }
  if (!controlPoints->commitChanges()) {
    if (errorOut)
      *errorOut = QStringLiteral("커밋 실패: %1").arg(controlPoints->commitErrors().join(QLatin1Char(';')));
    controlPoints->rollBack();
    return -1;
  }
  return added;
}

int LayerOps::importControlPointsCsv(QgsVectorLayer* controlPoints, const QString& csvPath, QString* errorOut,
                                    bool swapAxes) {
  if (!controlPoints || !controlPoints->isValid()) {
    if (errorOut) *errorOut = QStringLiteral("control_points 레이어가 없습니다. 먼저 새 조사를 만드세요.");
    return -1;
  }
  ControlCsvTable table;
  if (!loadControlCsv(csvPath, &table, errorOut))
    return table.rows.isEmpty() && table.xColumn >= 0 ? 0 : -1;
  return importParsedControlCsv(controlPoints, table, swapAxes, errorOut);
}

LayerOps::ControlCsvPreview LayerOps::previewControlPointsCsv(QgsVectorLayer* controlPoints, const QString& csvPath,
                                                             QgsProject* project, QString* errorOut) {
  ControlCsvPreview preview;
  if (!controlPoints || !controlPoints->isValid()) {
    if (errorOut) *errorOut = QStringLiteral("control_points 레이어가 없습니다. 먼저 새 조사를 만드세요.");
    return preview;
  }
  ControlCsvTable table;
  if (!loadControlCsv(csvPath, &table, errorOut)) return preview;
  preview.encoding = table.encoding;
  preview.geographic = rowsAreGeographic(table);
  QgsCoordinateTransform transform;
  const QgsCoordinateTransform* transformPtr = nullptr;
  if (preview.geographic) {
    const QgsCoordinateReferenceSystem wgs(QStringLiteral("EPSG:4326"));
    if (wgs.isValid() && controlPoints->crs().isValid()) {
      transform = QgsCoordinateTransform(wgs, controlPoints->crs(),
                                        project ? project->transformContext() : QgsCoordinateTransformContext());
      if (transform.isValid()) transformPtr = &transform;
    }
    if (!transformPtr) {
      if (errorOut) *errorOut = QStringLiteral("경위도를 작업 좌표계로 변환할 수 없습니다.");
      return preview;
    }
  }
  const QgsRectangle extent = surveyExtentFor(project, controlPoints->crs());
  double asIs = 0;
  double swapped = 0;
  int counted = 0;
  int swappedCount = 0;
  QStringList samples;
  for (const QStringList& row : table.rows) {
    bool okX = false, okY = false;
    const double rawX = csvCell(row, table.xColumn).toDouble(&okX);
    const double rawY = csvCell(row, table.yColumn).toDouble(&okY);
    if (!okX || !okY) continue;
    bool ok = false;
    const QgsPointXY direct = mapControlPoint(rawX, rawY, false, preview.geographic, transformPtr, &ok);
    if (!ok) continue;
    bool okSwap = false;
    const QgsPointXY flipped = mapControlPoint(rawX, rawY, true, preview.geographic, transformPtr, &okSwap);
    ++counted;
    if (!extent.isEmpty()) {
      asIs += distanceToExtent(direct, extent);
      if (okSwap) {
        swapped += distanceToExtent(flipped, extent);
        ++swappedCount;
      }
    }
    if (samples.size() < 3) {
      const QString id = table.idColumn >= 0 ? csvCell(row, table.idColumn) : QString::number(counted);
      samples << QStringLiteral("%1  X=%2  Y=%3").arg(id).arg(rawX, 0, 'f', 3).arg(rawY, 0, 'f', 3);
    }
  }
  if (counted == 0) {
    if (errorOut) *errorOut = QStringLiteral("좌표로 읽을 수 있는 기준점이 없습니다.");
    return preview;
  }
  preview.ok = true;
  preview.count = counted;
  if (!extent.isEmpty() && counted > 0) {
    asIs /= counted;
    if (swappedCount == counted) {
      swapped /= counted;
      preview.swapSuggested = asIs > 1000.0 && swapped + 1000.0 < asIs;
    }
  }
  QStringList lines;
  lines << QStringLiteral("%1 · %2점").arg(preview.encoding).arg(preview.count);
  if (preview.geographic)
    lines << QStringLiteral("경위도로 읽고 EPSG:4326에서 작업 좌표계로 변환합니다.");
  else
    lines << controlPointAxisHint();
  lines << samples;
  if (!extent.isEmpty() && swappedCount == counted) {
    lines << QStringLiteral("조사구역까지 이대로 %1, 교환하면 %2.")
                 .arg(formatMeters(asIs), formatMeters(swapped));
  } else if (!extent.isEmpty()) {
    lines << QStringLiteral("조사구역까지 이대로 %1. 교환 좌표는 변환되지 않습니다.").arg(formatMeters(asIs));
  } else {
    lines << QStringLiteral("조사구역이 없어 떨어진 거리는 확인하지 못했습니다.");
  }
  if (preview.swapSuggested)
    lines << QStringLiteral("X·Y가 바뀐 것 같습니다. 교환한 쪽이 조사구역에 더 가깝습니다.");
  preview.summary = lines.join(QLatin1Char('\n'));
  return preview;
}

QString LayerOps::controlPointAxisHint() {
  return QStringLiteral("이 프로그램은 X를 동쪽, Y를 북쪽으로 읽습니다. "
                        "한국 측량 성과표는 흔히 X=북쪽, Y=동쪽입니다. "
                        "그 값이면 X·Y 교환을 고르세요.");
}

QgsPointXY LayerOps::controlPointMapXy(double x, double y, bool swapAxes) {
  if (swapAxes) std::swap(x, y);
  return QgsPointXY(x, y);
}

LayerOps::ControlCsvPreview LayerOps::suggestControlPointAxisSwap(QgsProject* project, double x,
                                                                  double y) {
  ControlCsvPreview preview;
  preview.ok = std::isfinite(x) && std::isfinite(y);
  if (!preview.ok) return preview;
  preview.count = 1;
  QStringList lines;
  lines << controlPointAxisHint();
  lines << QStringLiteral("입력  X=%1  Y=%2").arg(x, 0, 'f', 3).arg(y, 0, 'f', 3);
  const QgsCoordinateReferenceSystem dest = project ? project->crs() : QgsCoordinateReferenceSystem();
  const QgsRectangle extent = surveyExtentFor(project, dest);
  bool ok = false;
  const QgsPointXY direct = mapControlPoint(x, y, false, false, nullptr, &ok);
  bool okSwap = false;
  const QgsPointXY flipped = mapControlPoint(x, y, true, false, nullptr, &okSwap);
  if (!extent.isEmpty() && ok && okSwap) {
    const double asIs = distanceToExtent(direct, extent);
    const double swapped = distanceToExtent(flipped, extent);
    preview.swapSuggested = asIs > 1000.0 && swapped + 1000.0 < asIs;
    lines << QStringLiteral("조사구역까지 이대로 %1, 교환하면 %2.")
                 .arg(formatMeters(asIs), formatMeters(swapped));
    if (preview.swapSuggested)
      lines << QStringLiteral("X·Y가 바뀐 것 같습니다. 교환한 쪽이 조사구역에 더 가깝습니다.");
  } else {
    lines << QStringLiteral("조사구역이 없어 떨어진 거리는 확인하지 못했습니다.");
  }
  preview.summary = lines.join(QLatin1Char('\n'));
  return preview;
}

