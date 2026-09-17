#pragma once

#include <QString>

class QgsMapLayer;
class QgsVectorLayer;

namespace LayerLabelControls {
struct Info {
  bool supported = false;
  bool enabled = false;
  bool needsField = false;
  bool areaEditable = false;
  bool fieldEditable = false;
  QString caption;
  QString reason;
  QString field;
};

Info describe(const QgsMapLayer* layer, bool layout = false, double scale = 0.);
bool setVisible(QgsMapLayer* layer, bool on, bool layout = false);
bool setField(QgsVectorLayer* layer, const QString& field);
bool setArea(QgsVectorLayer* layer, bool on);
bool isHeritage(const QgsMapLayer* layer);
}
