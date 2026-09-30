#include "PdfExportSettings.h"

#include <qgis.h>
#include <qgslayout.h>
#include <qgslayoutrendercontext.h>

namespace KaPdfExport {

QgsLayoutExporter::PdfExportSettings sheetSettings(double dpi) {
  QgsLayoutExporter::PdfExportSettings settings;
  settings.dpi = dpi > 0.0 ? dpi : kSheetDpi;
  settings.forceVectorOutput = true;
  settings.rasterizeWholeImage = false;
  // QGIS default is AlwaysOutlines. PreferText keeps plain labels selectable
  // and editable (Illustrator post-editing of sections) and only outlines text
  // whose buffer/shadow would render badly as text objects.
  settings.textRenderFormat = Qgis::TextRenderFormat::PreferText;
  settings.exportMetadata = true;
  settings.writeGeoPdf = false;
  return settings;
}

void prepareLayout(QgsLayout* layout) {
  if (!layout) return;
  layout->renderContext().setFlag(Qgis::LayoutRenderFlag::DisableTiledRasterLayerRenders, true);
}

QString describe(const QgsLayoutExporter::PdfExportSettings& settings) {
  return QStringLiteral("dpi=%1 vector=%2 rasterizeAll=%3 text=%4 geopdf=%5")
      .arg(settings.dpi, 0, 'f', 0)
      .arg(settings.forceVectorOutput ? 1 : 0)
      .arg(settings.rasterizeWholeImage ? 1 : 0)
      .arg(static_cast<int>(settings.textRenderFormat))
      .arg(settings.writeGeoPdf ? 1 : 0);
}

}  // namespace KaPdfExport
