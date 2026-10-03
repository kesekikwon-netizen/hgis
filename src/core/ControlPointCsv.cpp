#include "ControlPointCsv.h"
#include "FeatureRecord.h"
#include "LayerFeatures.h"
#include "LayerOps.h"

#include <QFile>
#include <QRegularExpression>
#include <QStringDecoder>
#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <limits>

#include <qgscoordinatereferencesystem.h>
#include <qgscoordinatetransform.h>
#include <qgscoordinatetransformcontext.h>
#include <qgsexception.h>
#include <qgsfeature.h>
#include <qgsfeatureiterator.h>
#include <qgsfeaturerequest.h>
#include <qgsfields.h>
#include <qgsgeometry.h>
#include <qgsproject.h>
#include <qgsrectangle.h>
#include <qgsvectorlayer.h>

namespace ControlPointCsv {
namespace {
const QHash<QString, QString>& aliases() {
  static const QHash<QString, QString> table = [] {
    QHash<QString, QString> t;
    const auto add = [&t](const char* target, std::initializer_list<const char*> names) {
      for (const char* name : names) t.insert(QString::fromUtf8(name), QString::fromLatin1(target));
    };
    add("point_id", {"id", "point", "pid", "pt", "name", "no", "station", "point_name", "측점", "측점명", "측점번호",
                     "점명", "점번호", "점", "기준점", "기준점명", "기준점번호", "번호", "이름", "명칭"});
    add("x", {"lon", "longitude", "경도", "easting", "east", "e", "x좌표", "좌표x", "동거"});
    add("y", {"lat", "latitude", "위도", "northing", "north", "n", "y좌표", "좌표y", "북거"});
    add("z", {"h", "el", "elev", "elevation", "height", "alt", "altitude", "표고", "높이", "고도", "해발", "해발고도",
              "z좌표", "표고z"});
    add("datum", {"측지기준계", "측지계", "기준계", "측지기준"});
    add("ellipsoid", {"타원체"});
    add("projection", {"proj", "crs", "투영", "투영법", "좌표계"});
    add("origin", {"원점", "투영원점"});
    add("accuracy_m", {"acc", "accuracy", "정확도", "정밀도", "오차"});
    add("fix_type", {"fix", "fixtype", "수신상태", "측위방식"});
    return t;
  }();
  return table;
}

bool isKnownColumn(const QString& name) {
  return name == QLatin1String("pdop") || aliases().key(name).size() > 0;  // every alias target is a column
}

// Lower case, no quotes/BOM, no unit or axis note: "X(m)" -> "x", "표고 (m)" -> "표고".
QString bareHeader(const QString& raw) {
  static const QRegularExpression note(QStringLiteral("[\\(\\[（].*[\\)\\]）]"));
  QString h = raw.trimmed().toLower();
  h.remove(QLatin1Char('"'));
  h.remove(QChar(0xFEFF));
  h.remove(note);
  h = h.trimmed();
  h.replace(QLatin1Char(' '), QLatin1Char('_'));
  return h;
}

bool headerIsGeographic(const QString& raw) {
  const QString h = bareHeader(raw);
  return h == QLatin1String("lon") || h == QLatin1String("lat") || h == QLatin1String("longitude") ||
         h == QLatin1String("latitude") || h == QStringLiteral("경도") || h == QStringLiteral("위도");
}

bool isNumber(const QString& text) {
  bool ok = false;
  text.trimmed().toDouble(&ok);
  return ok;
}

QString decodeCsvBytes(const QByteArray& bytes, QString* encodingOut) {
  if (bytes.startsWith("\xEF\xBB\xBF")) {
    *encodingOut = QStringLiteral("UTF-8");
    return QString::fromUtf8(bytes.mid(3));
  }
  QStringDecoder utf8(QStringDecoder::Utf8);
  const QString utf8Text = utf8.decode(bytes);
  if (!utf8.hasError()) {
    *encodingOut = QStringLiteral("UTF-8");
    return utf8Text;
  }
  QStringDecoder cp949(QStringLiteral("CP949"));
  if (!cp949.isValid()) cp949 = QStringDecoder(QStringLiteral("EUC-KR"));
  if (!cp949.isValid()) {
    *encodingOut = QStringLiteral("UTF-8");
    return QString::fromUtf8(bytes);
  }
  *encodingOut = QStringLiteral("CP949");
  return cp949.decode(bytes);
}
}  // namespace

QString normalizeHeader(const QString& raw) {
  const QString h = bareHeader(raw);
  return aliases().value(h, h);
}

QChar detectDelimiter(const QString& line) {
  int comma = 0, semicolon = 0, tab = 0;
  bool quoted = false;
  for (const QChar c : line) {
    if (c == QLatin1Char('"')) quoted = !quoted;
    if (quoted) continue;
    if (c == QLatin1Char(',')) ++comma;
    else if (c == QLatin1Char(';')) ++semicolon;
    else if (c == QLatin1Char('\t')) ++tab;
  }
  if (comma == 0 && semicolon == 0 && tab == 0)
    return line.trimmed().contains(QLatin1Char(' ')) ? QLatin1Char(' ') : QLatin1Char(',');
  if (comma >= semicolon && comma >= tab) return QLatin1Char(',');
  return semicolon >= tab ? QLatin1Char(';') : QLatin1Char('\t');
}

QStringList splitLine(const QString& rawLine, QChar delimiter) {
  const QString line = delimiter == QLatin1Char(' ') ? rawLine.simplified() : rawLine;
  QStringList cells;
  QString current;
  bool quoted = false;
  for (qsizetype i = 0; i < line.size(); ++i) {
    const QChar c = line.at(i);
    if (quoted) {
      if (c != QLatin1Char('"')) current += c;
      else if (i + 1 < line.size() && line.at(i + 1) == QLatin1Char('"')) current += line.at(++i);
      else quoted = false;
    } else if (c == QLatin1Char('"')) {
      quoted = true;
    } else if (c == delimiter) {
      cells << current.trimmed();
      current.clear();
    } else {
      current += c;
    }
  }
  cells << current.trimmed();
  return cells;
}

bool parseText(const QString& text, Table* table, QString* errorOut) {
  static const QRegularExpression lineBreak(QStringLiteral("[\\r\\n]+"));
  QStringList rawHeaders;
  bool delimiterKnown = false;
  for (const QString& rawLine : text.split(lineBreak)) {
    const QString line = rawLine.trimmed();
    if (line.isEmpty() || line.startsWith(QLatin1Char('#'))) continue;
    if (!delimiterKnown) {
      table->delimiter = detectDelimiter(line);
      delimiterKnown = true;
    }
    const QStringList cells = splitLine(line, table->delimiter);
    if (!table->headerFound && table->rows.isEmpty()) {
      // A header names a known column and is not itself a coordinate row.
      int known = 0, numbers = 0;
      for (const QString& c : cells) {
        if (isKnownColumn(normalizeHeader(c))) ++known;
        if (isNumber(c)) ++numbers;
      }
      if (known > 0 && numbers < 2) {
        table->headerFound = true;
        rawHeaders = cells;
        for (const QString& c : cells) {
          if (headerIsGeographic(c)) table->headerGeographic = true;
          table->headers << normalizeHeader(c);
        }
        continue;
      }
    }
    if (cells.size() < 2) {
      ++table->shortRows;
      continue;
    }
    table->rows.append(cells);
  }
  if (!table->headerFound) {
    // No header: 점 이름, X, Y, then 표고 when the fourth cell is a number in every row
    // that has one (a datum is never a number), else the older datum-first order.
    int fourth = 0, fourthNumbers = 0;
    for (const QStringList& row : std::as_const(table->rows)) {
      if (row.size() < 4 || row.at(3).trimmed().isEmpty()) continue;
      ++fourth;
      if (isNumber(row.at(3))) ++fourthNumbers;
    }
    table->headers = {QStringLiteral("point_id"), QStringLiteral("x"), QStringLiteral("y")};
    if (fourth > 0 && fourthNumbers == fourth) table->headers << QStringLiteral("z");
    table->headers << QStringLiteral("datum") << QStringLiteral("ellipsoid") << QStringLiteral("projection")
                   << QStringLiteral("accuracy_m") << QStringLiteral("pdop") << QStringLiteral("fix_type");
  }
  // A named point column (측점, point_id) wins over a plain row number (No, 번호).
  for (int i = 0; i < table->headers.size() && table->idColumn < 0; ++i) {
    const QString bare = i < rawHeaders.size() ? bareHeader(rawHeaders.at(i)) : QString();
    if (table->headers.at(i) == QLatin1String("point_id") && bare != QLatin1String("no") && bare != QStringLiteral("번호"))
      table->idColumn = i;
  }
  if (table->idColumn < 0) table->idColumn = static_cast<int>(table->headers.indexOf(QStringLiteral("point_id")));
  table->xColumn = static_cast<int>(table->headers.indexOf(QStringLiteral("x")));
  table->yColumn = static_cast<int>(table->headers.indexOf(QStringLiteral("y")));
  table->zColumn = static_cast<int>(table->headers.indexOf(QStringLiteral("z")));
  if (table->xColumn < 0 || table->yColumn < 0) {
    if (errorOut) *errorOut = QStringLiteral("CSV에 X·Y(또는 경도·위도) 열이 필요합니다.");
    return false;
  }
  if (table->rows.isEmpty()) {
    if (errorOut) *errorOut = QStringLiteral("CSV에 가져올 데이터 행이 없습니다.");
    return false;
  }
  return true;
}

bool load(const QString& path, Table* table, QString* errorOut) {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    if (errorOut) *errorOut = QStringLiteral("CSV를 열 수 없습니다: %1").arg(path);
    return false;
  }
  return parseText(decodeCsvBytes(file.readAll(), &table->encoding), table, errorOut);
}

QString cell(const QStringList& row, int index) {
  if (index < 0 || index >= row.size()) return {};
  return row.at(index).trimmed().remove(QLatin1Char('"'));
}

void DuplicateIndex::add(const QString& id, const QgsPointXY& point) {
  if (!id.isEmpty()) m_points[id].append(point);
}

bool DuplicateIndex::isSamePoint(const QString& id, const QgsPointXY& point) const {
  const auto found = m_points.constFind(id);
  if (id.isEmpty() || found == m_points.constEnd()) return false;
  return std::any_of(found->cbegin(), found->cend(),
                     [&](const QgsPointXY& p) { return p.distance(point) <= m_tolerance; });
}
}  // namespace ControlPointCsv

using ControlPointCsv::cell;

namespace {
bool koreaDegreePair(double a, double b) {
  const auto lon = [](double v) { return v >= 124.0 && v <= 132.5; };
  const auto lat = [](double v) { return v >= 33.0 && v <= 43.5; };
  return (lon(a) && lat(b)) || (lat(a) && lon(b));
}

bool rowsAreGeographic(const ControlPointCsv::Table& table) {
  bool any = false;
  for (const QStringList& row : table.rows) {
    bool okX = false, okY = false;
    const double x = cell(row, table.xColumn).toDouble(&okX);
    const double y = cell(row, table.yColumn).toDouble(&okY);
    if (!okX || !okY) continue;
    if (table.headerGeographic && (std::abs(x) > 180.0 || std::abs(y) > 180.0)) return false;
    any = true;
    if (!table.headerGeographic && !koreaDegreePair(x, y)) return false;
  }
  return table.headerGeographic || any;
}

QgsPointXY mapControlPoint(double x, double y, bool swapAxes, bool geographic, const QgsCoordinateTransform* transform,
                           bool* ok) {
  if (swapAxes) std::swap(x, y);
  *ok = false;
  if (!geographic) {
    *ok = true;
    return QgsPointXY(x, y);
  }
  if (!transform || !transform->isValid() || std::abs(x) > 180.0 || std::abs(y) > 90.0) return {};
  try {
    const QgsPointXY point = transform->transform(x, y);
    *ok = std::isfinite(point.x()) && std::isfinite(point.y());
    return *ok ? point : QgsPointXY();
  } catch (const QgsCsException&) {
    return {};
  }
}

double distanceToExtent(const QgsPointXY& point, const QgsRectangle& extent) {
  if (extent.isEmpty()) return std::numeric_limits<double>::infinity();
  if (extent.contains(point)) return 0.0;
  const double x = std::clamp(point.x(), extent.xMinimum(), extent.xMaximum());
  const double y = std::clamp(point.y(), extent.yMinimum(), extent.yMaximum());
  return std::hypot(point.x() - x, point.y() - y);
}

QString formatMeters(double meters) {
  if (!std::isfinite(meters)) return QStringLiteral("알 수 없음");
  if (meters < 1000.0) return QStringLiteral("%1m").arg(qRound(meters));
  return QStringLiteral("%1km").arg(meters / 1000.0, 0, 'f', 1);
}

// The layer's own extent follows its cached feature count, which a failed GDAL count or a save
// through another layer object leaves at zero (CI 2026-10-03); then the shapes are read instead.
QgsRectangle featureExtent(QgsVectorLayer* layer) {
  QgsRectangle box = layer->extent();
  if (!box.isEmpty()) return box;
  QgsFeatureIterator it = layer->getFeatures(QgsFeatureRequest().setNoAttributes());
  QgsFeature f;
  while (it.nextFeature(f)) {
    if (!f.hasGeometry()) continue;
    if (box.isEmpty()) box = f.geometry().boundingBox();
    else box.combineExtentWith(f.geometry().boundingBox());
  }
  return box;
}

QgsRectangle surveyExtentFor(QgsProject* project, const QgsCoordinateReferenceSystem& dest) {
  QgsRectangle box;
  if (!project) return box;
  for (QgsVectorLayer* layer : LayerOps::surveyAreaLayers(project)) {
    if (!LayerFeatures::any(layer)) continue;
    QgsRectangle extent = featureExtent(layer);
    if (!extent.isFinite() || extent.isEmpty()) continue;
    if (layer->crs().isValid() && dest.isValid() && layer->crs() != dest) {
      try {
        extent = QgsCoordinateTransform(layer->crs(), dest, project->transformContext()).transformBoundingBox(extent);
      } catch (const QgsCsException&) {
        continue;
      }
    }
    if (box.isEmpty()) box = extent;
    else box.combineExtentWith(extent);
  }
  return box;
}

// The points already in the layer, by point_id, for the re-import check.
ControlPointCsv::DuplicateIndex existingPoints(const QgsVectorLayer* layer) {
  ControlPointCsv::DuplicateIndex index;
  const int field = layer->fields().lookupField(QStringLiteral("point_id"));
  if (field < 0) return index;
  QgsFeatureRequest request;
  request.setSubsetOfAttributes(QgsAttributeList{field});
  QgsFeatureIterator it = layer->getFeatures(request);
  QgsFeature f;
  while (it.nextFeature(f)) {
    if (!f.hasGeometry()) continue;
    const QgsGeometry g = f.geometry();
    index.add(f.attribute(field).toString().trimmed(), g.isMultipart() ? g.centroid().asPoint() : g.asPoint());
  }
  return index;
}

bool makeTransform(const QgsVectorLayer* layer, QgsProject* project, QgsCoordinateTransform* out) {
  const QgsCoordinateReferenceSystem wgs(QStringLiteral("EPSG:4326"));
  if (!wgs.isValid() || !layer->crs().isValid()) return false;
  *out = QgsCoordinateTransform(wgs, layer->crs(),
                                project ? project->transformContext() : QgsCoordinateTransformContext());
  return out->isValid();
}

int importParsedControlCsv(QgsVectorLayer* controlPoints, const ControlPointCsv::Table& table, bool swapAxes,
                           QString* errorOut) {
  const bool geographic = rowsAreGeographic(table);
  QgsCoordinateTransform transform;
  if (geographic && !makeTransform(controlPoints, QgsProject::instance(), &transform)) {
    if (errorOut) *errorOut = QStringLiteral("경위도를 작업 좌표계로 변환할 수 없습니다.");
    return -1;
  }
  ControlPointCsv::DuplicateIndex seen = existingPoints(controlPoints);
  if (!controlPoints->isEditable() && !controlPoints->startEditing()) {
    if (errorOut) *errorOut = QStringLiteral("control_points 편집 모드 실패");
    return -1;
  }
  const QgsFields fields = controlPoints->fields();
  int added = 0;
  int alreadyThere = 0;
  for (const QStringList& row : table.rows) {
    bool okX = false, okY = false;
    const double rawX = cell(row, table.xColumn).toDouble(&okX);
    const double rawY = cell(row, table.yColumn).toDouble(&okY);
    if (!okX || !okY) continue;
    bool mappedOk = false;
    const QgsPointXY point = mapControlPoint(rawX, rawY, swapAxes, geographic, &transform, &mappedOk);
    if (!mappedOk) continue;
    const QString pid = table.idColumn >= 0 ? cell(row, table.idColumn) : QStringLiteral("P%1").arg(added + 1);
    // The same point imported again (same name, same place) is not added twice.
    if (seen.isSamePoint(pid, point)) {
      ++alreadyThere;
      continue;
    }
    seen.add(pid, point);
    QgsFeature feat(fields);
    const auto setValue = [&](const char* field, const QVariant& value) {
      const int fi = fields.lookupField(QString::fromLatin1(field));
      if (fi >= 0 && value.isValid()) feat.setAttribute(fi, value);
    };
    const auto column = [&](const char* name) { return table.headers.indexOf(QString::fromLatin1(name)); };
    const auto text = [&](const char* name) {
      const QString v = cell(row, static_cast<int>(column(name)));
      return v.isEmpty() ? QVariant() : QVariant(v);
    };
    // A cell that is not a number stays empty: text in a REAL column would be written as 0,
    // and 0 m is a believable elevation.
    const auto number = [&](const char* name) {
      const QString v = cell(row, static_cast<int>(column(name)));
      bool ok = false;
      const double d = v.toDouble(&ok);
      return ok ? QVariant(d) : QVariant();
    };
    setValue("point_id", pid);
    setValue("x", point.x());
    setValue("y", point.y());
    setValue("z", number("z"));
    for (const char* name : {"datum", "ellipsoid", "projection", "origin", "fix_type"}) setValue(name, text(name));
    setValue("accuracy_m", number("accuracy_m"));
    setValue("pdop", number("pdop"));
    setValue("accuracy", text("accuracy_m"));
    FeatureRecord::stampNew(feat);  // uid / created_at / updated_at when the survey has them
    feat.setGeometry(QgsGeometry::fromPointXY(point));
    if (controlPoints->addFeature(feat)) ++added;
  }
  if (added == 0) {
    if (controlPoints->isEditable()) controlPoints->rollBack();
    if (errorOut)
      *errorOut = alreadyThere > 0
                      ? QStringLiteral("CSV의 점 %1개가 모두 이미 가져온 점(같은 이름·같은 위치)이라 새로 넣은 점이 없습니다.")
                            .arg(alreadyThere)
                      : QStringLiteral("좌표로 읽을 수 있는 기준점이 없습니다.");
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
}  // namespace

int LayerOps::importControlPointsCsv(QgsVectorLayer* controlPoints, const QString& csvPath, QString* errorOut,
                                    bool swapAxes) {
  if (!controlPoints || !controlPoints->isValid()) {
    if (errorOut) *errorOut = QStringLiteral("control_points 레이어가 없습니다. 먼저 새 조사를 만드세요.");
    return -1;
  }
  ControlPointCsv::Table table;
  if (!ControlPointCsv::load(csvPath, &table, errorOut))
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
  ControlPointCsv::Table table;
  if (!ControlPointCsv::load(csvPath, &table, errorOut)) return preview;
  preview.encoding = table.encoding;
  preview.geographic = rowsAreGeographic(table);
  QgsCoordinateTransform transform;
  if (preview.geographic && !makeTransform(controlPoints, project, &transform)) {
    if (errorOut) *errorOut = QStringLiteral("경위도를 작업 좌표계로 변환할 수 없습니다.");
    return preview;
  }
  const QgsRectangle extent = surveyExtentFor(project, controlPoints->crs());
  // Re-import check for either axis order; the count shown follows the suggested one.
  ControlPointCsv::DuplicateIndex seenDirect = existingPoints(controlPoints);
  ControlPointCsv::DuplicateIndex seenSwapped = seenDirect;
  int sameDirect = 0, sameSwapped = 0, nameDirect = 0, nameSwapped = 0;
  double asIs = 0, swapped = 0;
  int counted = 0, swappedCount = 0, skipped = table.shortRows;
  QStringList samples;
  for (const QStringList& row : table.rows) {
    bool okX = false, okY = false;
    const double rawX = cell(row, table.xColumn).toDouble(&okX);
    const double rawY = cell(row, table.yColumn).toDouble(&okY);
    bool ok = false;
    const QgsPointXY direct = okX && okY ? mapControlPoint(rawX, rawY, false, preview.geographic, &transform, &ok)
                                         : QgsPointXY();
    if (!ok) {
      ++skipped;
      continue;
    }
    bool okSwap = false;
    const QgsPointXY flipped = mapControlPoint(rawX, rawY, true, preview.geographic, &transform, &okSwap);
    ++counted;
    const QString id = table.idColumn >= 0 ? cell(row, table.idColumn) : QString();
    const auto check = [&id](ControlPointCsv::DuplicateIndex& seen, const QgsPointXY& p, int& same, int& name) {
      if (seen.isSamePoint(id, p)) ++same;
      else if (seen.hasName(id)) ++name;
      seen.add(id, p);
    };
    check(seenDirect, direct, sameDirect, nameDirect);
    if (okSwap) check(seenSwapped, flipped, sameSwapped, nameSwapped);
    if (!extent.isEmpty()) {
      asIs += distanceToExtent(direct, extent);
      if (okSwap) {
        swapped += distanceToExtent(flipped, extent);
        ++swappedCount;
      }
    }
    if (samples.size() < 3) {
      QString sample = QStringLiteral("%1  X=%2  Y=%3")
                           .arg(id.isEmpty() ? QString::number(counted) : id)
                           .arg(rawX, 0, 'f', 3)
                           .arg(rawY, 0, 'f', 3);
      bool okZ = false;
      const double z = cell(row, table.zColumn).toDouble(&okZ);
      if (okZ) sample += QStringLiteral("  Z=%1").arg(z, 0, 'f', 3);
      samples << sample;
    }
  }
  if (counted == 0) {
    if (errorOut) *errorOut = QStringLiteral("좌표로 읽을 수 있는 기준점이 없습니다.");
    return preview;
  }
  preview.ok = true;
  preview.count = counted;
  if (!extent.isEmpty() && swappedCount == counted) {
    asIs /= counted;
    swapped /= counted;
    preview.swapSuggested = asIs > 1000.0 && swapped + 1000.0 < asIs;
  } else if (!extent.isEmpty()) {
    asIs /= counted;
  }
  QStringList lines;
  lines << QStringLiteral("%1 · %2점").arg(preview.encoding).arg(preview.count);
  if (skipped > 0) lines << QStringLiteral("좌표를 읽지 못한 %1행은 건너뜁니다.").arg(skipped);
  if (!table.headerFound) lines << QStringLiteral("첫 줄에 열 이름이 없어 점 이름, X, Y 순서로 읽습니다.");
  if (table.zColumn >= 0) lines << QStringLiteral("표고(Z) 열도 함께 가져옵니다.");
  const int same = preview.swapSuggested ? sameSwapped : sameDirect;
  const int name = preview.swapSuggested ? nameSwapped : nameDirect;
  if (same > 0) lines << QStringLiteral("이미 가져온 점과 이름·위치가 같은 %1행은 다시 넣지 않습니다.").arg(same);
  if (name > 0) lines << QStringLiteral("이름은 같은데 위치가 다른 점이 %1개 있습니다. 가져온 뒤 확인하세요.").arg(name);
  if (preview.geographic)
    lines << QStringLiteral("경위도로 읽고 EPSG:4326에서 작업 좌표계로 변환합니다.");
  else
    lines << controlPointAxisHint();
  lines << samples;
  if (!extent.isEmpty() && swappedCount == counted)
    lines << QStringLiteral("조사구역까지 이대로 %1, 교환하면 %2.").arg(formatMeters(asIs), formatMeters(swapped));
  else if (!extent.isEmpty())
    lines << QStringLiteral("조사구역까지 이대로 %1. 교환 좌표는 변환되지 않습니다.").arg(formatMeters(asIs));
  else
    lines << QStringLiteral("조사구역이 없어 떨어진 거리는 확인하지 못했습니다.");
  if (preview.swapSuggested) lines << QStringLiteral("X·Y가 바뀐 것 같습니다. 교환한 쪽이 조사구역에 더 가깝습니다.");
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

LayerOps::ControlCsvPreview LayerOps::suggestControlPointAxisSwap(QgsProject* project, double x, double y) {
  ControlCsvPreview preview;
  preview.ok = std::isfinite(x) && std::isfinite(y);
  if (!preview.ok) return preview;
  preview.count = 1;
  QStringList lines;
  lines << controlPointAxisHint();
  lines << QStringLiteral("입력  X=%1  Y=%2").arg(x, 0, 'f', 3).arg(y, 0, 'f', 3);
  const QgsCoordinateReferenceSystem dest = project ? project->crs() : QgsCoordinateReferenceSystem();
  const QgsRectangle extent = surveyExtentFor(project, dest);
  if (!extent.isEmpty()) {
    const double asIs = distanceToExtent(QgsPointXY(x, y), extent);
    const double swapped = distanceToExtent(QgsPointXY(y, x), extent);
    preview.swapSuggested = asIs > 1000.0 && swapped + 1000.0 < asIs;
    lines << QStringLiteral("조사구역까지 이대로 %1, 교환하면 %2.").arg(formatMeters(asIs), formatMeters(swapped));
    if (preview.swapSuggested)
      lines << QStringLiteral("X·Y가 바뀐 것 같습니다. 교환한 쪽이 조사구역에 더 가깝습니다.");
  } else {
    lines << QStringLiteral("조사구역이 없어 떨어진 거리는 확인하지 못했습니다.");
  }
  preview.summary = lines.join(QLatin1Char('\n'));
  return preview;
}
