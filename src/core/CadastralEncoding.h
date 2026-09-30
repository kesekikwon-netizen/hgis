#pragma once

#include <QByteArray>
#include <QString>

// Attribute encoding of a downloaded cadastral shapefile (evaluation F123).
//
// Korean public SHP files often come without a .cpg and store CP949 text (for example
// 「산 23-1」 in JIBUN). The soil import already falls back to CP949 in that case; the
// cadastral path reuses the same rule, but keeps a UTF-8 DBF readable.
// Nothing is written next to the source: the result is applied to the opened layer only.
namespace CadastralEncoding {

// Encoding to set on the opened OGR layer, or empty when nothing should be forced
// (not a shapefile, or a .cpg that GDAL/QGIS already honour).
QString fallbackFor(const QString& sourcePath);

// "UTF-8" when the DBF bytes are clearly UTF-8 Korean, otherwise "CP949".
QString detectDbfEncoding(const QByteArray& sample);

}  // namespace CadastralEncoding
