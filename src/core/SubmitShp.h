#pragma once
// Shapefile helpers for the submission package (ExportService).
#include <QList>
#include <QSet>
#include <QString>
#include <QStringList>
#include <memory>

class QgsProject;
class QgsVectorLayer;

namespace SubmitShp {

// Leftmost part of text whose UTF-8 form fits maxBytes, never splitting a code point.
QString leftUtf8(const QString& text, int maxBytes);
// DBF field names are 10 bytes. survey_name/artifact_no keep their program aliases.
QString fieldNameFor(const QString& name, QSet<QString>& used);

// Layers of one key merged into memory with the union of their fields and the
// first layer's CRS. notes collects values dropped for a type mismatch.
std::unique_ptr<QgsVectorLayer> mergeLayers(const QList<QgsVectorLayer*>& sources, const QString& key,
                                            QgsProject* project, QStringList* notes, QString* errorOut);
// Copy with 10-byte field names; nullptr when every name already fits (or on error).
std::unique_ptr<QgsVectorLayer> withShapefileFieldNames(QgsVectorLayer* source, const QString& key,
                                                        QStringList* notes, QString* errorOut);
// Reopen a written shapefile and compare its CRS (EPSG:5179) and feature count.
// Uses GDAL directly so no pooled QGIS connection keeps the staging folder busy.
bool verifyWritten(const QString& shpPath, long long expectedFeatures, QString* errorOut);

}  // namespace SubmitShp
