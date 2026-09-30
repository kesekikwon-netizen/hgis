#include "KaAttributeMapTool.h"
#include "core/LayerOps.h"

#include <qgsmapcanvas.h>
#include <qgsmapmouseevent.h>
#include <qgsvectorlayer.h>
#include <qgsmaplayer.h>

#include <QKeyEvent>
#include <QStatusTipEvent>
#include <QApplication>

KaAttributeMapTool::KaAttributeMapTool(QgsMapCanvas* canvas) : QgsMapToolIdentify(canvas) {
  setCursor(Qt::PointingHandCursor);
}

void KaAttributeMapTool::activate() {
  QgsMapToolIdentify::activate();
  setCursor(Qt::PointingHandCursor);
}

void KaAttributeMapTool::keyPressEvent(QKeyEvent* e) {
  if (e && e->key() == Qt::Key_Escape) {
    emit pickCanceled();
    e->accept();
    return;
  }
  QgsMapToolIdentify::keyPressEvent(e);
}

bool KaAttributeMapTool::pickAtScreen(const QPoint& screenPos, QgsVectorLayer** outLayer,
                                      QgsFeature* outFeat) {
  if (!mCanvas || !outLayer || !outFeat) return false;
  *outLayer = nullptr;
  *outFeat = QgsFeature();

  const QList<IdentifyResult> results = identify(
      screenPos.x(), screenPos.y(),
      QgsMapToolIdentify::TopDownStopAtFirst,
      QgsMapToolIdentify::VectorLayer);

  if (results.isEmpty()) return false;

  auto* vl = qobject_cast<QgsVectorLayer*>(results.first().mLayer);
  if (!vl || !results.first().mFeature.isValid()) return false;

  *outLayer = vl;
  *outFeat = results.first().mFeature;
  return true;
}

bool KaAttributeMapTool::isEditableLayer(const QgsMapLayer* layer) {
  const auto* vl = qobject_cast<const QgsVectorLayer*>(layer);
  if (!vl || !vl->isValid()) return false;
  return !LayerOps::isReferenceOrBasemapLayer(vl) && !LayerOps::isCadastralLayer(vl) &&
         !LayerOps::isReferenceLayer(vl);
}

bool KaAttributeMapTool::pickEditableAtScreen(const QPoint& screenPos, QgsVectorLayer** outLayer,
                                              QgsFeature* outFeat) {
  if (!mCanvas || !outLayer || !outFeat) return false;
  *outLayer = nullptr;
  *outFeat = QgsFeature();
  QList<QgsMapLayer*> candidates;
  for (QgsMapLayer* layer : mCanvas->layers()) {
    if (isEditableLayer(layer)) candidates.append(layer);
  }
  if (candidates.isEmpty()) return false;
  const QList<IdentifyResult> results =
      identify(screenPos.x(), screenPos.y(), candidates, QgsMapToolIdentify::TopDownStopAtFirst);
  for (const IdentifyResult& result : results) {
    auto* vl = qobject_cast<QgsVectorLayer*>(result.mLayer);
    if (!vl || !result.mFeature.isValid()) continue;
    *outLayer = vl;
    *outFeat = result.mFeature;
    return true;
  }
  return false;
}

void KaAttributeMapTool::canvasReleaseEvent(QgsMapMouseEvent* e) {
  if (!e || !mCanvas) return;
  if (e->button() != Qt::LeftButton) {
    QgsMapToolIdentify::canvasReleaseEvent(e);
    return;
  }

  QgsVectorLayer* layer = nullptr;
  QgsFeature feat;
  if (!pickEditableAtScreen(e->pos(), &layer, &feat) || !layer) {
    if (mCanvas) {
      const QString tip = QStringLiteral("이 위치에 조사 도형이 없습니다. 참조 지도·지적도는 고칠 수 없습니다.");
      mCanvas->setStatusTip(tip);
      QStatusTipEvent event(tip);
      QApplication::sendEvent(mCanvas, &event);
    }
    return;
  }
  emit featurePicked(layer, feat);
}
