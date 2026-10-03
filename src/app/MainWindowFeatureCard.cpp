// 「선택한 유구」 card in the inspector panel (evaluation F044; P6 moved it from the layer panel).
// The card shows exactly ONE selected feature. It is part of the right panel, never a popup.
// While the drawing tool is active the card keeps its record but is disabled, and the
// inspector says so in one sentence; a shape just drawn is shown at once without being selected
// (user 2026-10-03 「속성도 안나오고」; erase/split/Delete act on selections). Nothing single
// selected shows the inspector's sentence.
// Every card edit is one undoable command in the layer's edit buffer; Ctrl+S stays the only save.
#include "MainWindow.h"

#if KA_HGIS_HAS_QGIS
#include "KaCaptureMapTool.h"
#include "KaFeatureCard.h"
#include "KaInspectorPanel.h"
#include "core/LayerOps.h"

#include <QApplication>
#include <QBoxLayout>
#include <QTimer>

#include <qgslayertreeview.h>
#include <qgsmapcanvas.h>
#include <qgsmaplayer.h>
#include <qgsmaptool.h>
#include <qgsproject.h>
#include <qgsvectorlayer.h>

void MainWindow::setupFeatureCard(QWidget* host, QBoxLayout* layout) {
  if (m_featureCard || !host || !layout || !m_canvas) return;
  m_featureCard = new KaFeatureCard(host);
  m_featureCard->hide();  // shown only while one feature is selected
  layout->addWidget(m_featureCard, 0);

  connect(m_canvas, &QgsMapCanvas::selectionChanged, this, &MainWindow::syncFeatureCard);
  // Picking the drawing tool hides the card; leaving it reads the current selection again.
  connect(m_canvas, &QgsMapCanvas::mapToolSet, this, [this](QgsMapTool*, QgsMapTool*) {
    if (!m_featureCard) return;
    QgsMapLayer* layer = m_featureCard->layer();
    if (!layer && m_layerTree) layer = m_layerTree->currentLayer();
    syncFeatureCard(layer);
  });
  // A removed layer (survey closed or reset) clears the card itself; hide the empty card.
  connect(QgsProject::instance(), &QgsProject::layersRemoved, this, [this](const QStringList&) {
    QTimer::singleShot(0, this, [this] {
      if (m_featureCard) m_featureCard->setVisible(m_featureCard->hasFeature());
    });
  });
  connect(m_featureCard, &KaFeatureCard::edited, this,
          [this](QgsVectorLayer* layer, QgsFeatureId fid, const QgsFeature& before) {
            if (!layer) return;
            KaUndoAction undo;
            undo.type = KaUndoAction::AttributesChanged;
            undo.layerId = layer->id();
            undo.featureId = fid;
            undo.featureData = before;
            pushUndoAction(std::move(undo), layer);
            QgsProject::instance()->setDirty(true);  // title ' *'; Ctrl+S remains the only save
            updateUndoRedoActions();
            LayerOps::applyDomainDrawStyle(layer, LayerOps::layerKeyOf(layer));
            if (m_canvas) m_canvas->refresh();
          });
  // [P6] 「조사카드 열기」: the full attribute form of the feature on the card.
  connect(m_featureCard, &KaFeatureCard::formRequested, this, [this](QgsVectorLayer* layer, QgsFeatureId fid) {
    if (layer) editFeatureAttributes(layer, layer->getFeature(fid));
  });
}

void MainWindow::syncFeatureCard(QgsMapLayer* layer) {
  if (!m_featureCard) return;
  const bool drawing = m_captureTool && m_canvas && m_canvas->mapTool() == m_captureTool;
  // [P6] Drawing keeps the record on view (the inspector sentence explains) but does not
  // edit it; the selection is read again once the tool is put down.
  if (m_inspector) m_inspector->setDrawing(drawing);
  // Commit before locking: the ribbon chips are TabFocus, so a value still being typed in the
  // card leaves the box only here, and it must leave into the edit buffer, not be dropped.
  if (drawing)
    if (QWidget* focus = QApplication::focusWidget(); focus && m_featureCard->isAncestorOf(focus)) focus->clearFocus();
  m_featureCard->setEnabled(!drawing);
  if (drawing) return;
  // Another layer's selection emptying must not clear the card showing this layer's feature,
  // nor an empty selection the record of a shape just drawn.
  auto* vector = qobject_cast<QgsVectorLayer*>(layer);
  if (vector && (vector->selectedFeatureCount() == 1 ||
                 (vector == m_featureCard->layer() && m_featureCard->followsSelection())))
    m_featureCard->showSelection(vector);
  if (m_inspector) m_inspector->setSelectionCount(vector ? int(vector->selectedFeatureCount()) : 0);
  m_featureCard->setVisible(m_featureCard->hasFeature());
}
#endif  // KA_HGIS_HAS_QGIS
