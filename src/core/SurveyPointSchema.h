#pragma once

#include <QString>

class GDALDataset;
class OGRFeature;
class OGRFeatureDefn;
class OGRGeometry;

struct SurveyColumns {
  int id = -1;
  int x = -1;
  int y = -1;
  int z = -1;
};

struct SurveyDataset {
  GDALDataset* ds = nullptr;
  bool headerless = false;
  ~SurveyDataset();
  bool open(const QString& path);
};

SurveyColumns guessSurveyColumns(OGRFeatureDefn* definition);
bool surveyNumericHeaders(OGRFeatureDefn* definition);
bool surveyParseNumber(QString text, double* out);
QString surveyFieldText(OGRFeature* feature, int index);
bool surveyIsPoint(OGRGeometry* geometry);
bool surveyAxesNamedNorthEast(OGRFeatureDefn* definition, SurveyColumns columns);
