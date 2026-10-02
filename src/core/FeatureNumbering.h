#pragma once

#include <QList>
#include <QString>
#include <QStringList>

#include <qgsfeatureid.h>

class QgsVectorLayer;

// Feature numbers (유구번호 feature_no, 유물번호 artifact_no, 단면번호 section_id,
// 점ID point_id) are free text. This module only suggests the next number per kind and
// finds numbers used twice; it never forces or rewrites a number (no popup while
// drawing, the post-drawing form stays optional).
namespace FeatureNumbering {

// Comparable form: "1호", "01호", "1 호", "제1호" -> "1"; "s-01" -> "S-1". Empty stays empty.
QString normalized(const QString& number);

// The last run of digits and the text around it: "제03호" -> {"제", 3, width 2, "호"}.
struct Parts {
  QString prefix;
  qint64 value = -1;  // -1: no digits
  int width = 0;      // digit count as written (keeps zero padding)
  QString suffix;
};
Parts parse(const QString& number);

// Suggested new number for the number field: feature_no "%1호", artifact_no "%1",
// point_id "P%1". Empty for section_id (sections are often lettered, A-A').
QString defaultTemplate(const QString& numberField);

// The next number after `existing`, written like the highest one ("02호" -> "03호").
// Uses `fallbackTemplate` ("%1" = number) when none of them has digits.
QString nextFrom(const QStringList& existing, const QString& fallbackTemplate);

// The layer's number field (feature_no, artifact_no, section_id, point_id). `kind` is set
// only for feature_no: 유구번호 runs per kind, the other numbers run over the whole layer.
struct Fields {
  int number = -1;
  int kind = -1;
  QString numberName;
};
Fields fieldsOf(const QgsVectorLayer* layer);

// Next number among features of the same kind (kind compared without spaces; ignored
// when the layer does not number per kind). Empty when the layer has no number field or
// nothing sensible can be suggested.
QString suggestNext(const QgsVectorLayer* layer, const QString& kind);

// Other features of the same kind with the same normalized number.
QList<QgsFeatureId> sameNumber(const QgsVectorLayer* layer, const QString& kind, const QString& number,
                               QgsFeatureId exclude = FID_NULL);

// Every feature whose (kind, number) is shared with another feature. Label: "주거지 1호".
// For the submit checklist (warn only).
struct Duplicate {
  QgsFeatureId fid = FID_NULL;
  QString label;
};
QList<Duplicate> duplicates(const QgsVectorLayer* layer);

}  // namespace FeatureNumbering
