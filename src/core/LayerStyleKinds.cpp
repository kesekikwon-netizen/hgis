#include "LayerStyleKinds.h"

#include "LayerOps.h"
#include "LayerStyleDefaults.h"

#include <QStringList>
#include <algorithm>

#include <qgscategorizedsymbolrenderer.h>
#include <qgsfeature.h>
#include <qgsfeatureiterator.h>
#include <qgsfeaturerequest.h>
#include <qgssymbol.h>
#include <qgsvectorlayer.h>

namespace {
constexpr int kHueSteps = 24;                 // hues 15 degrees apart
constexpr int kCandidates = kHueSteps * 2;    // each hue in a light and a dark tone
constexpr int kStride = 7;                    // coprime with kCandidates: probing visits all once

// FNV-1a over UTF-8. qHash is seeded per process in Qt 6, so it would recolour
// the same kind on every start.
quint32 stableHash(const QString& text) {
  quint32 hash = 2166136261u;
  const QByteArray bytes = text.toUtf8();
  for (const char c : bytes) {
    hash ^= static_cast<quint8>(c);
    hash *= 16777619u;
  }
  return hash;
}

QColor candidate(int index) {
  const int i = index % kCandidates;
  return QColor::fromHsv((i % kHueSteps) * (360 / kHueSteps), 170, i < kHueSteps ? 215 : 150);
}
}  // namespace

QString LayerStyleKinds::unclassifiedLabel() { return QStringLiteral("미분류"); }

QColor LayerStyleKinds::colorForKind(const QString& kind, const QSet<QRgb>& taken) {
  const int start = static_cast<int>(stableHash(kind.trimmed()) % kCandidates);
  QColor firstFree;
  for (int step = 0; step < kCandidates; ++step) {
    const QColor color = candidate(start + step * kStride);
    if (LayerStyleDefaults::isNearReservedColor(color)) continue;
    if (!firstFree.isValid()) firstFree = color;
    if (!taken.contains(color.rgb())) return color;
  }
  // More kinds than free hues: share the kind's own first choice.
  return firstFree.isValid() ? firstFree : candidate(start);
}

QString LayerStyleKinds::categoryField(const QgsVectorLayer* layer) {
  if (!layer) return {};
  for (const QString& name : {QStringLiteral("kind"), QStringLiteral("period")}) {
    if (layer->fields().indexOf(name) >= 0) return name;
  }
  return {};
}

bool LayerStyleKinds::apply(QgsVectorLayer* layer) {
  if (!layer || !layer->isValid()) return false;
  const QString field = categoryField(layer);
  if (field.isEmpty()) return false;
  const int index = layer->fields().indexOf(field);

  QStringList kinds;  // raw attribute text, so categories match the stored values
  QgsFeatureRequest request;
  request.setFlags(Qgis::FeatureRequestFlag::NoGeometry);
  request.setSubsetOfAttributes(QgsAttributeList{index});
  QgsFeature feature;
  QgsFeatureIterator it = layer->getFeatures(request);
  while (it.nextFeature(feature)) {
    const QVariant value = feature.attribute(index);
    const QString text = value.isNull() ? QString() : value.toString();
    if (!text.trimmed().isEmpty() && !kinds.contains(text)) kinds.append(text);
  }
  std::sort(kinds.begin(), kinds.end());

  QColor fill, stroke;
  double widthMm = 1.0, markerMm = 3.5;
  LayerOps::readSimpleVectorStyle(layer, &fill, &stroke, &widthMm, &markerMm);
  const LayerStyleDefaults::DomainStyle base = LayerStyleDefaults::forKey(LayerOps::layerKeyOf(layer));
  const int geometry = static_cast<int>(layer->geometryType());

  QgsCategoryList categories;
  QSet<QRgb> taken;
  for (const QString& kind : std::as_const(kinds)) {
    const QColor color = colorForKind(kind, taken);
    taken.insert(color.rgb());
    QColor kindFill = color;
    kindFill.setAlpha(150);
    QgsSymbol* symbol = LayerStyleDefaults::buildSymbol(geometry, base, kindFill, color.darker(160),
                                                        widthMm, markerMm, false, false, false);
    if (symbol) categories.append(QgsRendererCategory(QVariant(kind), symbol, kind.trimmed()));
  }
  // Empty, NULL and not-yet-listed kinds keep the layer's own symbol.
  if (QgsSymbol* rest = LayerStyleDefaults::buildSymbol(geometry, base, fill, stroke, widthMm, markerMm,
                                                         false, false, false))
    categories.append(QgsRendererCategory(QVariant(), rest, unclassifiedLabel()));
  if (categories.isEmpty()) return false;
  layer->setRenderer(new QgsCategorizedSymbolRenderer(field, categories));
  layer->setCustomProperty(QString::fromLatin1(kPropStyleMode), QString::fromLatin1(kModeByKind));
  layer->triggerRepaint();
  return true;
}

bool LayerStyleKinds::isByKind(const QgsVectorLayer* layer) {
  return layer && layer->customProperty(QString::fromLatin1(kPropStyleMode)).toString() ==
                      QLatin1String(kModeByKind);
}

void LayerStyleKinds::clearMode(QgsVectorLayer* layer) {
  if (layer && isByKind(layer)) layer->removeCustomProperty(QString::fromLatin1(kPropStyleMode));
}

bool LayerStyleKinds::hasOtherAutomaticLook(const QgsVectorLayer* layer) {
  if (!layer || !layer->renderer()) return false;
  const QString mode = layer->customProperty(QString::fromLatin1(kPropStyleMode)).toString();
  return !mode.isEmpty() && mode != QLatin1String(kModeByKind);
}

void LayerStyleKinds::clearAnyAutomaticLook(QgsVectorLayer* layer) {
  if (layer) layer->removeCustomProperty(QString::fromLatin1(kPropStyleMode));
}
