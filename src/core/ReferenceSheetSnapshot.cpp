#include "ReferenceSheetSnapshot.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <QSet>
#include <qgscoordinatereferencesystem.h>

namespace ReferenceSheetSnapshot {
namespace {

using Record = TopographicCatalog::Record;

QStringList datasetFiles(const QString& path) {
  const QFileInfo info(path);
  QStringList files{info.absoluteFilePath()};
  if (info.suffix().compare(QLatin1String("shp"), Qt::CaseInsensitive) == 0) {
    for (const auto* extension : {"shx", "dbf", "prj"}) {
      const QString part = info.dir().filePath(info.completeBaseName() + QLatin1Char('.') + QLatin1String(extension));
      if (QFileInfo(part).isFile()) files.append(QFileInfo(part).absoluteFilePath());
    }
  }
  return files;
}

QJsonObject stamp(const QString& path) {
  const QFileInfo file(path);
  return {{QStringLiteral("path"), file.absoluteFilePath()},
          {QStringLiteral("size"), double(file.size())},
          {QStringLiteral("modified"), double(file.lastModified().toMSecsSinceEpoch())}};
}

bool unchanged(const QJsonObject& recorded) {
  const QFileInfo file(recorded.value(QStringLiteral("path")).toString());
  return file.isFile() && double(file.size()) == recorded.value(QStringLiteral("size")).toDouble(-1) &&
         double(file.lastModified().toMSecsSinceEpoch()) == recorded.value(QStringLiteral("modified")).toDouble(-1);
}

QJsonObject toJson(const Record& record) {
  return {{QStringLiteral("source"), record.source}, {QStringLiteral("layerName"), record.layerName},
          {QStringLiteral("cadLayer"), record.cadLayer}, {QStringLiteral("geometryType"), record.geometryType},
          {QStringLiteral("displayName"), record.displayName}, {QStringLiteral("sourceSheet"), record.sourceSheet},
          {QStringLiteral("crsWkt"), record.crsWkt}, {QStringLiteral("hasExtent"), record.hasExtent},
          {QStringLiteral("extent"), QJsonArray{record.extent.xMinimum(), record.extent.yMinimum(),
                                                record.extent.xMaximum(), record.extent.yMaximum()}},
          {QStringLiteral("category"), static_cast<int>(record.category)},
          {QStringLiteral("signature"), record.signature}};
}

bool fromJson(const QJsonObject& object, Record* record) {
  const QJsonArray extent = object.value(QStringLiteral("extent")).toArray();
  const int category = object.value(QStringLiteral("category")).toInt(-1);
  if (extent.size() != 4 || category < 0 || category > static_cast<int>(TopographicCatalog::Category::PlaceName))
    return false;
  record->source = object.value(QStringLiteral("source")).toString();
  record->layerName = object.value(QStringLiteral("layerName")).toString();
  record->cadLayer = object.value(QStringLiteral("cadLayer")).toString();
  record->geometryType = object.value(QStringLiteral("geometryType")).toString();
  record->displayName = object.value(QStringLiteral("displayName")).toString();
  record->sourceSheet = object.value(QStringLiteral("sourceSheet")).toString();
  record->crsWkt = object.value(QStringLiteral("crsWkt")).toString();
  record->hasExtent = object.value(QStringLiteral("hasExtent")).toBool();
  record->extent = QgsRectangle(extent.at(0).toDouble(), extent.at(1).toDouble(),
                                extent.at(2).toDouble(), extent.at(3).toDouble());
  record->category = static_cast<TopographicCatalog::Category>(category);
  record->signature = object.value(QStringLiteral("signature")).toString();
  return !record->source.isEmpty() && !record->layerName.isEmpty() && !record->sourceSheet.isEmpty() &&
         record->hasExtent && record->extent.isFinite() && QgsCoordinateReferenceSystem(record->crsWkt).isValid();
}

}  // namespace

QJsonObject capture(const QString& source, const QList<Record>& records, int solverVersion,
                    const QString& crsName, const QString& evidence) {
  if (records.isEmpty() || !QFileInfo(source).isFile()) return {};
  QJsonArray files{stamp(source)};
  QSet<QString> seen{QFileInfo(source).absoluteFilePath()};
  QJsonArray prepared;
  for (const Record& record : records) {
    if (!QFileInfo(record.source).isFile()) return {};
    for (const QString& file : datasetFiles(record.source)) {
      if (seen.contains(file)) continue;
      seen.insert(file);
      files.append(stamp(file));
    }
    prepared.append(toJson(record));
  }
  return {{QStringLiteral("schema"), 1}, {QStringLiteral("solverVersion"), solverVersion},
          {QStringLiteral("source"), QFileInfo(source).absoluteFilePath()}, {QStringLiteral("files"), files},
          {QStringLiteral("records"), prepared}, {QStringLiteral("crsName"), crsName},
          {QStringLiteral("evidence"), evidence}};
}

Restored restore(const QJsonObject& snapshot, const QString& source, int solverVersion) {
  Restored result;
  if (snapshot.isEmpty() || snapshot.value(QStringLiteral("schema")).toInt() != 1 ||
      snapshot.value(QStringLiteral("solverVersion")).toInt(-1) != solverVersion ||
      snapshot.value(QStringLiteral("source")).toString() != QFileInfo(source).absoluteFilePath())
    return result;
  const QJsonArray files = snapshot.value(QStringLiteral("files")).toArray();
  if (files.isEmpty() || files.first().toObject().value(QStringLiteral("path")).toString() !=
                             QFileInfo(source).absoluteFilePath())
    return result;
  QSet<QString> stamped;
  for (const QJsonValue& value : files) {
    const QJsonObject recorded = value.toObject();
    if (!unchanged(recorded)) return result;
    stamped.insert(recorded.value(QStringLiteral("path")).toString());
  }
  const QJsonArray records = snapshot.value(QStringLiteral("records")).toArray();
  if (records.isEmpty()) return result;
  for (const QJsonValue& value : records) {
    Record record;
    if (!fromJson(value.toObject(), &record) || !stamped.contains(QFileInfo(record.source).absoluteFilePath()))
      return {};
    result.records.append(record);
  }
  result.crsName = snapshot.value(QStringLiteral("crsName")).toString();
  result.evidence = snapshot.value(QStringLiteral("evidence")).toString();
  result.valid = true;
  return result;
}

QString sidecarPath(const QString& indexFile) {
  const QFileInfo file(indexFile);
  return file.dir().filePath(file.completeBaseName() + QStringLiteral(".snapshot"));
}

void rememberBeside(const QString& indexFile, const QString& source, const QList<Record>& records,
                    bool clean, int solverVersion, const QString& crsName, const QString& evidence) {
  const QString path = sidecarPath(indexFile);
  const QJsonObject snapshot = clean ? capture(source, records, solverVersion, crsName, evidence) : QJsonObject();
  if (snapshot.isEmpty()) {
    QFile::remove(path);
    return;
  }
  const QByteArray bytes = QJsonDocument(snapshot).toJson(QJsonDocument::Compact);
  QSaveFile output(path);
  if (output.open(QIODevice::WriteOnly) && output.write(bytes) == bytes.size()) output.commit();
}

Restored restoreBeside(const QString& indexFile, const QString& source, const QString& sheet,
                       int solverVersion) {
  QFile input(sidecarPath(indexFile));
  if (!input.exists() || input.size() > 8 * 1024 * 1024 || !input.open(QIODevice::ReadOnly)) return {};
  Restored cached = restore(QJsonDocument::fromJson(input.readAll()).object(), source, solverVersion);
  for (const Record& record : cached.records)
    if (record.sourceSheet != sheet) return {};
  return cached;
}

}  // namespace ReferenceSheetSnapshot
