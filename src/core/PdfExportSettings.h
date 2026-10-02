#pragma once

#include <qgslayoutexporter.h>

class QgsLayout;

// One PDF recipe for every drawing sheet: the drawing studio 「PDF 저장」 and
// print source, the submission package (조사도면.pdf) and the section sheet
// (단면도.pdf). Every exporter takes its settings from here, so the PDF the
// user checked on screen is the PDF that goes into the package.
namespace KaPdfExport {

// Print resolution of every sheet PDF (raster parts only; vectors stay vectors).
inline constexpr double kSheetDpi = 300.0;

// Vector output (semi-transparent grid/callouts are not rasterised), text kept
// as text where QGIS can do so without artefacts (buffers fall back to outlines).
QgsLayoutExporter::PdfExportSettings sheetSettings(double dpi = kSheetDpi);

// Layout render flags shared by every sheet PDF. Rasters (satellite, cadastral,
// section photos) draw in one pass so a missing tile never blanks a rectangle.
void prepareLayout(QgsLayout* layout);

// Short stable description of the settings, e.g. for logs and tests.
QString describe(const QgsLayoutExporter::PdfExportSettings& settings);

}  // namespace KaPdfExport
