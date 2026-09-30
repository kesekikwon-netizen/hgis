#pragma once

#include <QList>
#include <QMetaType>
#include <QString>
#include <QStringList>

class QgsFields;

// The one table of the survey GeoPackage schema: layers, geometry and fields that
// SurveyProjectFactory writes into every new survey. data/schemas/ka_hgis_layers.yaml
// documents the same table (tests/test_workflow.cpp compares the two).
//
// Version 3 appends optional field-record columns (layer number, depth, elevations,
// relation, status, photo, surveyor, date) and automatic audit columns (uid,
// created_at, updated_at). None of them is required: nothing asks for them while
// drawing, and the submit checklist does not block on them.
namespace SurveySchema {

// Written into every survey GPKG as a GDAL dataset metadata item (gpkg_metadata).
// Surveys made before the stamp read as 0.
constexpr int kCurrentVersion = 3;
constexpr const char* kVersionMetadataItem = "KA_HGIS_SCHEMA_VERSION";

struct FieldDef {
  const char* name;
  QMetaType::Type type;
  const char* labelKo;
};

struct LayerDef {
  const char* name;
  const char* geometry;  // "Polygon" | "LineString" | "Point"
  QList<FieldDef> fields;
};

const QList<LayerDef>& layers();
const LayerDef* layer(const QString& layerKey);
// Every field of the layer in schema order; empty for an unknown key.
QgsFields fieldsFor(const QString& layerKey);

// uid / created_at / updated_at: filled by FeatureRecord, never typed by hand.
bool isAutoField(const QString& fieldName);
// Korean label of a version-3 field; the field name itself when unknown.
QString labelKo(const QString& fieldName);

// 0 when the file has no stamp (older survey) or cannot be read.
int readVersion(const QString& gpkgPath);
bool writeVersion(const QString& gpkgPath, int version, QString* errorOut = nullptr);

struct MigrationResult {
  bool ok = false;
  int fromVersion = 0;
  QStringList addedFields;  // "table.field"
  QString error;
};

// Brings a survey GPKG up to kCurrentVersion: adds the missing schema fields to the
// domain tables that exist (absent tables stay absent) and stamps the version.
// Existing fields and values are never changed or removed. Nothing is written when no
// column is missing and the stamp is current; a newer stamp is never lowered.
//
// Call it ONLY on the generation copy inside the survey save (SurveyStorage), right
// after the copy is made and before the edit buffers are applied to it (the buffers are
// matched by field name, so the extra columns stay empty). Opening a survey must not
// change the user's file, so it is never called on open. Every added column is
// optional: a failure is logged by the caller and the save goes on.
MigrationResult migrateGenerationCopy(const QString& gpkgPath);

}  // namespace SurveySchema
