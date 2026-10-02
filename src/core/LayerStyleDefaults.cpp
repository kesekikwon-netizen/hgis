#include "LayerStyleDefaults.h"

#include "HeritageStyle.h"
#include "LayerOps.h"

#include <cmath>

#include <qgsfillsymbol.h>
#include <qgslinesymbol.h>
#include <qgslinesymbollayer.h>
#include <qgsmarkersymbol.h>

namespace {
double linear(double channel) {
  return channel <= 0.04045 ? channel / 12.92 : std::pow((channel + 0.055) / 1.055, 2.4);
}

double labPivot(double t) {
  return t > 216.0 / 24389.0 ? std::cbrt(t) : (24389.0 / 27.0 * t + 16.0) / 116.0;
}

void toLab(const QColor& c, double* l, double* a, double* b) {
  const double r = linear(c.redF()), g = linear(c.greenF()), bl = linear(c.blueF());
  // sRGB (D65) to XYZ, normalised by the D65 white point.
  const double x = (0.4124 * r + 0.3576 * g + 0.1805 * bl) / 0.95047;
  const double y = (0.2126 * r + 0.7152 * g + 0.0722 * bl);
  const double z = (0.0193 * r + 0.1192 * g + 0.9505 * bl) / 1.08883;
  const double fx = labPivot(x), fy = labPivot(y), fz = labPivot(z);
  *l = 116.0 * fy - 16.0;
  *a = 500.0 * (fx - fy);
  *b = 200.0 * (fy - fz);
}

QColor opaque(QColor c) {
  c.setAlpha(255);
  return c;
}
}  // namespace

LayerStyleDefaults::DomainStyle LayerStyleDefaults::forKey(const QString& key) {
  DomainStyle s;
  s.fill = QColor(37, 99, 235, 90);
  s.stroke = QColor(37, 99, 235, 255);
  if (key == QLatin1String("survey_area")) {
    s.fill = QColor(180, 83, 9, 70);
    s.stroke = QColor(146, 64, 14, 255);
    s.strokeWidthMm = 1.6;
  } else if (key == QLatin1String("feature_poly")) {
    s.fill = QColor(22, 163, 74, 90);
    s.stroke = QColor(17, 94, 44, 255);
    s.strokeWidthMm = 1.8;
  } else if (key == QLatin1String("feature_line")) {
    s.stroke = QColor(202, 138, 4, 255);
    s.strokeWidthMm = 1.8;
  } else if (key == QLatin1String("section_line")) {
    s.stroke = QColor(190, 24, 93, 255);
    s.strokeWidthMm = 1.8;
    s.casing = true;
    s.casingColor = QColor(255, 255, 255, 255);
    s.casingWidthMm = 3.0;
  } else if (key == QLatin1String("control_points")) {
    s.fill = QColor(234, 179, 8, 255);
    s.stroke = QColor(161, 98, 7, 255);
    s.markerSizeMm = 4.0;
  } else if (key == QLatin1String("artifact_point")) {
    s.fill = QColor(185, 28, 28, 255);
    s.stroke = QColor(127, 29, 29, 255);
    s.markerSizeMm = 3.6;
  } else if (key == QLatin1String("trial_trench")) {
    // 시굴 트렌치 도면 관례: 붉은 외곽선 0.5, 채움은 거의 없음(위성·지적 위 판독).
    s.fill = QColor(220, 38, 38, 18);
    s.stroke = QColor(220, 38, 38, 255);
    s.strokeWidthMm = 0.5;
  }
  return s;
}

QgsSymbol* LayerStyleDefaults::casedLineSymbol(const DomainStyle& base, const QColor& core,
                                               double coreWidthMm, bool dashed) {
  auto line = QgsLineSymbol::createSimple({
      {QStringLiteral("line_color"), base.casingColor.name(QColor::HexArgb)},
      {QStringLiteral("line_width"), QString::number(base.casingWidthMm)},
      {QStringLiteral("line_width_unit"), QStringLiteral("MM")},
      {QStringLiteral("line_style"), QStringLiteral("solid")},
  });
  if (!line) return nullptr;
  auto* coreLayer = new QgsSimpleLineSymbolLayer(core, coreWidthMm, dashed ? Qt::DashLine : Qt::SolidLine);
  coreLayer->setWidthUnit(Qgis::RenderUnit::Millimeters);
  line->appendSymbolLayer(coreLayer);
  return line.release();
}

QgsSymbol* LayerStyleDefaults::buildSymbol(int geometryType, const DomainStyle& base, const QColor& fill,
                                           const QColor& stroke, double strokeWidthMm, double markerSizeMm,
                                           bool noFill, bool noStroke, bool dashed) {
  const auto gt = static_cast<Qgis::GeometryType>(geometryType);
  const QString lineStyle = noStroke ? QStringLiteral("no") : dashed ? QStringLiteral("dash") : QStringLiteral("solid");
  if (gt == Qgis::GeometryType::Polygon) {
    return QgsFillSymbol::createSimple({
        {QStringLiteral("color"), fill.name(QColor::HexArgb)},
        {QStringLiteral("style"), noFill ? QStringLiteral("no") : QStringLiteral("solid")},
        {QStringLiteral("outline_color"), stroke.name(QColor::HexArgb)},
        {QStringLiteral("outline_width"), QString::number(noStroke ? 0.0 : strokeWidthMm)},
        {QStringLiteral("outline_width_unit"), QStringLiteral("MM")},
        {QStringLiteral("outline_style"), lineStyle},
    }).release();
  }
  if (gt == Qgis::GeometryType::Line) {
    if (base.casing && !noStroke) return casedLineSymbol(base, stroke, strokeWidthMm, dashed);
    return QgsLineSymbol::createSimple({
        {QStringLiteral("line_color"), stroke.name(QColor::HexArgb)},
        {QStringLiteral("line_width"), QString::number(noStroke ? 0.0 : strokeWidthMm)},
        {QStringLiteral("line_width_unit"), QStringLiteral("MM")},
        {QStringLiteral("line_style"), lineStyle},
    }).release();
  }
  if (gt == Qgis::GeometryType::Point) {
    return QgsMarkerSymbol::createSimple({
        {QStringLiteral("name"), QStringLiteral("circle")},
        {QStringLiteral("color"), noFill ? QStringLiteral("#00000000") : fill.name(QColor::HexArgb)},
        {QStringLiteral("outline_color"), stroke.name(QColor::HexArgb)},
        {QStringLiteral("outline_width"), noStroke ? QStringLiteral("0") : QStringLiteral("0.6")},
        {QStringLiteral("outline_style"), noStroke ? QStringLiteral("no") : QStringLiteral("solid")},
        {QStringLiteral("size"), QString::number(markerSizeMm)},
        {QStringLiteral("size_unit"), QStringLiteral("MM")},
    }).release();
  }
  return nullptr;
}

QVector<QColor> LayerStyleDefaults::reservedColors() {
  QVector<QColor> out;
  for (const QString& key : LayerOps::domainLayerKeys()) {
    const DomainStyle s = forKey(key);
    out.append(opaque(s.stroke));
    if (s.fill.alpha() >= 60) out.append(opaque(s.fill));  // near-transparent fills are not seen
  }
  for (HeritageDataset dataset : HeritageStyle::allDatasets()) out.append(HeritageStyle::color(dataset));
  return out;
}

double LayerStyleDefaults::deltaE76(const QColor& a, const QColor& b) {
  double l1, a1, b1, l2, a2, b2;
  toLab(a, &l1, &a1, &b1);
  toLab(b, &l2, &a2, &b2);
  return std::sqrt((l1 - l2) * (l1 - l2) + (a1 - a2) * (a1 - a2) + (b1 - b2) * (b1 - b2));
}

bool LayerStyleDefaults::isNearReservedColor(const QColor& color, double minDeltaE) {
  if (HeritageStyle::isReservedColor(color)) return true;
  const QVector<QColor> reserved = reservedColors();
  for (const QColor& taken : reserved) {
    if (deltaE76(color, taken) < minDeltaE) return true;
  }
  return false;
}
