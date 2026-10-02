#include "LineMapStyle.h"

#include "CadDrawingLayers.h"
#include "LayerOps.h"

#include <QDomDocument>

#include <qgsfillsymbollayer.h>
#include <qgslinesymbol.h>
#include <qgsreadwritecontext.h>
#include <qgssinglesymbolrenderer.h>
#include <qgssymbol.h>
#include <qgssymbollayer.h>
#include <qgsvectorlayer.h>

namespace LineMapStyle {
namespace {

constexpr const char* kOriginal = "ka_hgis/original_style";  // 처음 바꾸기 전 모양(QGIS 스타일 XML, 기호만)
const QColor kMixed(QStringLiteral("#3C3C3C"));               // 도형마다 색이 다를 때 창의 처음 색

const QgsSymbol* singleSymbol(const QgsVectorLayer* layer) {
  const auto* single = dynamic_cast<const QgsSingleSymbolRenderer*>(layer->renderer());
  return single ? single->symbol() : nullptr;
}

// 받은 지적도처럼 면을 채우지 않고 외곽선만 그리는가.
bool outlineOnly(const QgsVectorLayer* layer) {
  const QgsSymbol* symbol = singleSymbol(layer);
  if (!symbol || symbol->symbolLayerCount() == 0) return false;
  for (int i = 0; i < symbol->symbolLayerCount(); ++i) {
    const auto* fill = dynamic_cast<const QgsSimpleFillSymbolLayer*>(symbol->symbolLayer(i));
    if (!fill || (fill->brushStyle() != Qt::NoBrush && fill->fillColor().alpha() > 0)) return false;
  }
  return true;
}

bool isDrawing(const QgsVectorLayer* layer) { return !CadDrawingLayers::drawingIdOf(layer).isEmpty(); }

void rememberOriginal(QgsVectorLayer* layer) {
  if (hasOriginal(layer)) return;
  QDomDocument doc;
  QString error;
  QgsReadWriteContext context;
  layer->exportNamedStyle(doc, error, context, QgsMapLayer::Symbology);
  layer->setCustomProperty(QString::fromLatin1(kOriginal), doc.toString());
}

}  // namespace

bool isLineMap(const QgsVectorLayer* layer) {
  if (!layer || !layer->isValid()) return false;
  if (layer->geometryType() == Qgis::GeometryType::Line) return true;
  return layer->geometryType() == Qgis::GeometryType::Polygon && (isDrawing(layer) || outlineOnly(layer));
}

bool apply(QgsVectorLayer* layer, const QColor& color, double widthMm) {
  if (!isLineMap(layer) || !color.isValid()) return false;
  rememberOriginal(layer);
  // 도면의 면은 CAD 처럼 같은 색을 옅게(알파 77) 채우고, 외곽선만 그리던 면은 계속 채우지 않는다.
  const bool fillDrawing = layer->geometryType() == Qgis::GeometryType::Polygon && isDrawing(layer);
  QColor fill = color;
  fill.setAlpha(77);
  return LayerOps::applySimpleVectorStyle(layer, fill, color, widthMm, 3.5, !fillDrawing, false, false);
}

bool hasOriginal(const QgsVectorLayer* layer) {
  return layer && !layer->customProperty(QString::fromLatin1(kOriginal)).toString().isEmpty();
}

bool restoreOriginal(QgsVectorLayer* layer) {
  if (!hasOriginal(layer)) return false;
  QDomDocument doc;
  if (!doc.setContent(layer->customProperty(QString::fromLatin1(kOriginal)).toString())) return false;
  QString error;
  if (!layer->importNamedStyle(doc, error, QgsMapLayer::Symbology)) return false;
  layer->removeCustomProperty(QString::fromLatin1(kOriginal));
  for (const char* key : {"ka_hgis/style_fill", "ka_hgis/style_stroke", "ka_hgis/style_width_mm", "ka_hgis/style_marker_mm",
                          "ka_hgis/style_no_fill", "ka_hgis/style_no_stroke", "ka_hgis/style_dashed"})
    layer->removeCustomProperty(QString::fromLatin1(key));
  layer->triggerRepaint();
  return true;
}

QColor currentColor(const QgsVectorLayer* layer) {
  const QgsSymbol* symbol = layer ? singleSymbol(layer) : nullptr;
  if (!symbol || symbol->symbolLayerCount() == 0) return kMixed;
  const QgsSymbolLayer* top = symbol->symbolLayer(symbol->symbolLayerCount() - 1);
  if (top->dataDefinedProperties().isActive(QgsSymbolLayer::Property::StrokeColor)) return kMixed;
  const QColor shown = layer->geometryType() == Qgis::GeometryType::Line ? top->color() : top->strokeColor();
  return shown.isValid() ? shown : kMixed;
}

double currentWidthMm(const QgsVectorLayer* layer) {
  const QVariant stored = layer ? layer->customProperty(QStringLiteral("ka_hgis/style_width_mm")) : QVariant();
  if (stored.isValid()) return stored.toDouble();
  const QgsSymbol* symbol = layer ? singleSymbol(layer) : nullptr;
  if (const auto* line = dynamic_cast<const QgsLineSymbol*>(symbol)) return line->width();
  if (symbol && symbol->symbolLayerCount() > 0)
    if (const auto* fill = dynamic_cast<const QgsSimpleFillSymbolLayer*>(symbol->symbolLayer(0))) return fill->strokeWidth();
  return 0.26;
}

}  // namespace LineMapStyle
