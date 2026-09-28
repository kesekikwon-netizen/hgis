#include "SurveyPointSchema.h"

#include <cmath>

#include <cpl_conv.h>
#include <gdal_priv.h>
#include <ogrsf_frmts.h>

namespace {

QString normName(const char* raw) {
  QString name = QString::fromUtf8(raw ? raw : "").trimmed().toLower();
  name.remove(QLatin1Char(' '));
  name.remove(QLatin1Char('_'));
  return name;
}

bool isId(const QString& name) {
  return name == QStringLiteral("점번호") || name == QStringLiteral("측점") || name == QStringLiteral("점명") ||
         name == QStringLiteral("포인트명") || name == QStringLiteral("번호") || name == QLatin1String("no") ||
         name == QLatin1String("pt") || name == QLatin1String("id") || name == QLatin1String("name") ||
         name == QLatin1String("point");
}
bool isNorth(const QString& name) {
  return name == QStringLiteral("북") || name == QStringLiteral("북쪽") || name == QLatin1String("x") ||
         name == QLatin1String("x(n)") || name == QLatin1String("n") || name == QLatin1String("north") ||
         name == QLatin1String("northing");
}
bool isEast(const QString& name) {
  return name == QStringLiteral("동") || name == QStringLiteral("동쪽") || name == QLatin1String("y") ||
         name == QLatin1String("y(e)") || name == QLatin1String("e") || name == QLatin1String("east") ||
         name == QLatin1String("easting");
}
bool isElev(const QString& name) {
  return name == QStringLiteral("표고") || name == QStringLiteral("높이") || name == QStringLiteral("해발") ||
         name == QLatin1String("z") || name == QLatin1String("z(h)") || name == QLatin1String("h") ||
         name == QLatin1String("el") || name == QLatin1String("elev") || name == QLatin1String("elevation");
}

}  // namespace

SurveyDataset::~SurveyDataset() {
  if (ds) GDALClose(ds);
}

bool SurveyDataset::open(const QString& path) {
  if (ds) GDALClose(ds);
  ds = nullptr;
  headerless = false;
  GDALAllRegister();
  CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "YES");
  ds = static_cast<GDALDataset*>(GDALOpenEx(
      path.toUtf8().constData(), GDAL_OF_VECTOR | GDAL_OF_READONLY, nullptr, nullptr, nullptr));
  if (!ds) return false;
  const bool dxf = QString::fromUtf8(ds->GetDriver()->GetDescription()) == QLatin1String("DXF");
  if (!dxf && ds->GetLayerCount() > 0 && surveyNumericHeaders(ds->GetLayer(0)->GetLayerDefn())) {
    GDALClose(ds);
    const char* options[] = {"HEADERS=DISABLE", nullptr};
    ds = static_cast<GDALDataset*>(GDALOpenEx(
        path.toUtf8().constData(), GDAL_OF_VECTOR | GDAL_OF_READONLY, nullptr, options, nullptr));
    headerless = ds != nullptr;
  }
  return ds != nullptr;
}

bool surveyNumericHeaders(OGRFeatureDefn* definition) {
  const int count = definition ? definition->GetFieldCount() : 0;
  if (count == 0) return false;
  int hits = 0;
  for (int i = 0; i < count; ++i) {
    const QString name = normName(definition->GetFieldDefn(i)->GetNameRef());
    bool ok = false;
    name.toDouble(&ok);
    if (ok || name.startsWith(QLatin1String("field"))) ++hits;
  }
  return hits == count;
}

SurveyColumns guessSurveyColumns(OGRFeatureDefn* definition) {
  SurveyColumns columns;
  const int count = definition->GetFieldCount();
  for (int i = 0; i < count; ++i) {
    const QString name = normName(definition->GetFieldDefn(i)->GetNameRef());
    if (columns.id < 0 && isId(name)) columns.id = i;
    else if (columns.x < 0 && isNorth(name)) columns.x = i;
    else if (columns.y < 0 && isEast(name)) columns.y = i;
    else if (columns.z < 0 && isElev(name)) columns.z = i;
  }
  if ((columns.x < 0 || columns.y < 0 || columns.z < 0) && count >= 3) {
    const int base = (count >= 4 && columns.id < 0) ? 1 : 0;
    if (base + 2 < count) {
      if (columns.id < 0 && base == 1) columns.id = 0;
      if (columns.x < 0) columns.x = base;
      if (columns.y < 0) columns.y = base + 1;
      if (columns.z < 0) columns.z = base + 2;
    }
  }
  return columns;
}

bool surveyParseNumber(QString text, double* out) {
  text = text.trimmed();
  if (text.isEmpty()) return false;
  if (text.count(QLatin1Char(',')) > 1 || text.contains(QLatin1Char('.')))
    text.remove(QLatin1Char(','));
  else text.replace(QLatin1Char(','), QLatin1Char('.'));
  bool ok = false;
  *out = text.toDouble(&ok);
  return ok && std::isfinite(*out);
}

QString surveyFieldText(OGRFeature* feature, int index) {
  if (!feature || index < 0 || !feature->IsFieldSetAndNotNull(index)) return {};
  return QString::fromUtf8(feature->GetFieldAsString(index)).trimmed();
}

bool surveyIsPoint(OGRGeometry* geometry) {
  if (!geometry) return false;
  const OGRwkbGeometryType flat = wkbFlatten(geometry->getGeometryType());
  return flat == wkbPoint || flat == wkbMultiPoint;
}

bool surveyAxesNamedNorthEast(OGRFeatureDefn* definition, SurveyColumns columns) {
  if (!definition || columns.x < 0 || columns.y < 0) return false;
  if (columns.x >= definition->GetFieldCount() || columns.y >= definition->GetFieldCount()) return false;
  const QString x = normName(definition->GetFieldDefn(columns.x)->GetNameRef());
  const QString y = normName(definition->GetFieldDefn(columns.y)->GetNameRef());
  const bool north = x.contains(QStringLiteral("북")) || x.contains(QLatin1String("north")) || x.contains(QLatin1String("(n)"));
  const bool east = y.contains(QStringLiteral("동")) || y.contains(QLatin1String("east")) || y.contains(QLatin1String("(e)"));
  return north && east;
}
