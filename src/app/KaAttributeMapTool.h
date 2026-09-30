#pragma once
#include <qgsmaptoolidentify.h>
#include <qgsfeature.h>

class QgsMapLayer;
class QgsVectorLayer;

class KaAttributeMapTool : public QgsMapToolIdentify {
  Q_OBJECT
public:
  explicit KaAttributeMapTool(QgsMapCanvas* canvas);

  void canvasReleaseEvent(QgsMapMouseEvent* e) override;
  void keyPressEvent(QKeyEvent* e) override;
  void activate() override;

  // Top-most vector feature of any layer (map context menu decides what it offers).
  bool pickAtScreen(const QPoint& screenPos, QgsVectorLayer** outLayer, QgsFeature* outFeat);
  // Same, but only survey data: reference maps and cadastral layers are read-only and a
  // cadastral parcel drawn on top must not hide the feature under it.
  bool pickEditableAtScreen(const QPoint& screenPos, QgsVectorLayer** outLayer, QgsFeature* outFeat);
  static bool isEditableLayer(const QgsMapLayer* layer);

signals:
  void featurePicked(QgsVectorLayer* layer, const QgsFeature& feature);
  void pickCanceled();
};
