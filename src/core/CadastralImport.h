#pragma once
#include "PreparedReferenceMap.h"
#include <qgsgeometry.h>
#include <qgscoordinatereferencesystem.h>
#include <qgscoordinatetransformcontext.h>
#include <functional>

class QgsProject;
class QgsMapCanvas;
namespace CadastralImport {
using Progress = std::function<void(int, const QString&)>;
using Cancel = std::function<bool()>;
// Worker-only inputs: no live project/layer objects cross the thread boundary.
PreparedReferenceMap prepare(const QStringList& sources, const QgsGeometry& scope,
    const QgsCoordinateReferenceSystem& crs, const QgsCoordinateTransformContext& context,
    const QString& directory, const Cancel& cancel = {}, const Progress& progress = {});
bool applyStyle(QgsVectorLayer* layer, const QColor& color = Qt::black, bool labels = true);
QgsVectorLayer* addPrepared(QgsProject* project, QgsMapCanvas* canvas,
    const PreparedReferenceMap& prepared, QString* error = nullptr);
// Select an exact PNU in an already loaded cadastral layer. No data edits/downloads.
bool focusParcel(QgsProject* project, QgsMapCanvas* canvas, const QString& pnu,
                 QgsPointXY* position = nullptr);
}
