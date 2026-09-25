#include "MainWindow.h"
#include "KaAlignMapTool.h"
#include "KaCaptureMapTool.h"
#include "KaDrawingStudio.h"
#include "KaFeatureSelectTool.h"
#include "KaTerrain3dLayoutStudio.h"
#include "core/LayerOps.h"

#include <QAbstractSpinBox>
#include <QApplication>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QStatusBar>
#include <QScopedValueRollback>
#include <QTextEdit>
#include <QTabWidget>
#include <qgslayertree.h>
#include <qgslayertreemodellegendnode.h>
#include <qgslayertreeview.h>
#include <qgsmapcanvas.h>
#include <qgsproject.h>
#include <qgsvectorlayer.h>
#include <qgsvectorlayereditbuffer.h>
#include <qgsvectordataprovider.h>
#include <algorithm>
#include <functional>
#include <QSet>

struct KaRemovedLayers {
  struct Entry {
    std::unique_ptr<QgsMapLayer> layer;
    std::unique_ptr<QgsLayerTreeNode> node;
    QPointer<QgsLayerTreeGroup> parent;
    int index = 0;
  };
  // Groups the removal left empty, detached but alive so Entry::parent stays valid for Ctrl+Z.
  struct Group {
    std::unique_ptr<QgsLayerTreeNode> node;
    QPointer<QgsLayerTreeGroup> parent;
    int index = 0;
  };
  std::vector<Entry> entries;
  std::vector<Group> groups;  // detach order: inner groups first
};

namespace {
bool editingText() {
  QWidget* focus = QApplication::focusWidget();
  return qobject_cast<QLineEdit*>(focus) || qobject_cast<QAbstractSpinBox*>(focus) ||
         qobject_cast<QTextEdit*>(focus) || qobject_cast<QPlainTextEdit*>(focus);
}

bool editFeatures(QgsVectorLayer* layer, const QString& title,
                  const std::function<bool()>& change, QString* error) {
  return LayerOps::runEditCommand(layer, title, change, error);
}

QgsVectorLayer* preferredMapLayer(MainWindow* window, QgsLayerTreeView* tree, QgsVectorLayer* editLayer) {
  Q_UNUSED(window);
  if (tree) {
    if (auto* current = qobject_cast<QgsVectorLayer*>(tree->currentLayer()))
      return current;
  }
  return editLayer;
}
}

void MainWindow::removeSelectedLayers() {
  removeLayersFromTree(m_layerTree);
}

void MainWindow::watchUndoFeatureIds(QgsVectorLayer* layer) {
  if (!layer || m_undoObservedLayers.contains(layer->id())) return;
  m_undoObservedLayers.insert(layer->id());
  const QPointer<QgsVectorLayer> guarded(layer);
  auto pending = std::make_shared<QgsFeatureList>();
  connect(layer, &QgsVectorLayer::beforeCommitChanges, this, [guarded, pending](bool) {
    pending->clear();
    if (guarded && guarded->editBuffer())
      *pending = guarded->editBuffer()->addedFeatures().values();
  });
  connect(layer, &QgsVectorLayer::committedFeaturesAdded, this,
      [this, guarded, pending](const QString& id, const QgsFeatureList& committed) {
    if (!guarded || !guarded->dataProvider()) return;
    const auto primaryKeys = guarded->dataProvider()->pkAttributeIndexes();
    for (const auto& saved : committed) {
      auto previous = std::find_if(pending->begin(), pending->end(), [&](const QgsFeature& temporary) {
        if (temporary.geometry().asWkb() != saved.geometry().asWkb()) return false;
        for (int i = 0; i < temporary.attributeCount(); ++i) {
          if (!primaryKeys.contains(i) && temporary.attribute(i) != saved.attribute(i)) return false;
        }
        return true;
      });
      if (previous == pending->end()) continue;
      const auto oldId = previous->id();
      for (auto& action : m_undoActions) {
        if (action.layerId == id && action.featureId == oldId) action.featureId = saved.id();
        for (auto& deleted : action.deletedFeatures) {
          if (deleted.first == id && deleted.second.id() == oldId) {
            deleted.second.setId(saved.id());
            for (int key : primaryKeys) deleted.second.setAttribute(key, saved.attribute(key));
          }
        }
      }
      pending->erase(previous);
    }
  });
}

void MainWindow::remapUndoFeatureIdsAfterSave() {
  auto* project = QgsProject::instance();
  if (!project) return;
  for (auto& action : m_undoActions) {
    auto* layer = qobject_cast<QgsVectorLayer*>(project->mapLayer(action.layerId));
    if (!layer || !layer->isValid()) continue;
    if (action.featureId >= 0 && layer->getFeature(action.featureId).isValid()) continue;
    if (!action.featureData.isValid()) continue;
    QgsAttributeList primary;
    if (QgsVectorDataProvider* provider = layer->dataProvider())
      primary = provider->pkAttributeIndexes();
    QgsFeature current;
    auto iterator = layer->getFeatures();
    while (iterator.nextFeature(current)) {
      bool same = current.attributeCount() == action.featureData.attributeCount();
      for (int i = 0; same && i < current.attributeCount(); ++i) {
        if (primary.contains(i)) continue;
        if (current.attribute(i) != action.featureData.attribute(i)) same = false;
      }
      if (!same) continue;
      action.featureId = current.id();
      break;
    }
  }
}

void MainWindow::removeLayersFromTree(QgsLayerTreeView* tree) {
  if (!tree || m_isOpeningSurvey || m_closingWindow || editingText()) return;
  QList<QgsMapLayer*> selected;
  auto addLayer = [&](QgsMapLayer* layer) {
    if (layer && !selected.contains(layer)) selected.append(layer);
  };
  for (QgsMapLayer* layer : tree->selectedLayers()) addLayer(layer);
  addLayer(tree->currentLayer());
  QSet<QgsLayerTreeNode*> nodes;
  if (auto* model = tree->selectionModel()) {
    for (const QModelIndex& idx : model->selectedIndexes()) {
      if (auto* node = tree->index2node(idx)) nodes.insert(node);
      if (auto* legend = tree->index2legendNode(idx)) {
        if (legend->layerNode()) nodes.insert(legend->layerNode());
      }
    }
  }
  if (auto* node = tree->currentNode()) nodes.insert(node);
  if (auto* legend = tree->index2legendNode(tree->currentIndex())) {
    if (legend->layerNode()) nodes.insert(legend->layerNode());
  }
  for (QgsLayerTreeNode* node : nodes) {
    for (QgsMapLayer* layer : LayerOps::removableLegendLayersFromNode(node))
      addLayer(layer);
  }
  auto* project = QgsProject::instance();
  auto* root = project ? project->layerTreeRoot() : nullptr;
  // 고른 묶음 줄은 줄 자체도 뺀다. 레이어 없이 빈 하위 묶음만 남은 묶음도 마찬가지다.
  // 바깥 묶음과 그 안쪽 묶음을 같이 골랐으면 바깥 것만 떼어 낸다. 안쪽은 따라간다.
  QList<QgsLayerTreeGroup*> groups;
  if (root) {
    for (QgsLayerTreeNode* node : nodes) {
      auto* group = qobject_cast<QgsLayerTreeGroup*>(node);
      if (!group || group == root) continue;
      bool inProject = false;
      for (QgsLayerTreeNode* up = group->parent(); up && !inProject; up = up->parent())
        inProject = up == root;
      if (inProject) groups.append(group);
    }
    const QList<QgsLayerTreeGroup*> picked = groups;
    groups.clear();
    for (QgsLayerTreeGroup* group : picked) {
      bool nested = false;
      for (QgsLayerTreeNode* up = group->parent(); up && !nested; up = up->parent())
        nested = picked.contains(qobject_cast<QgsLayerTreeGroup*>(up));
      if (!nested) groups.append(group);
    }
  }
  if ((selected.isEmpty() && groups.isEmpty()) || !project || !root) {
    statusBar()->showMessage(QStringLiteral("제거할 레이어를 먼저 클릭하세요."), 4000);
    return;
  }
  auto removed = std::make_shared<KaRemovedLayers>();
  // Snapshot all positions before any node is removed. Keep the actual layer,
  // including unsaved edit buffers and memory data, until restored or discarded.
  for (auto* node : root->findLayers()) {
    QgsMapLayer* layer = node->layer();
    if (!layer || !selected.contains(layer)) continue;
    KaRemovedLayers::Entry entry;
    entry.node.reset(node->clone());
    entry.parent = qobject_cast<QgsLayerTreeGroup*>(node->parent());
    entry.index = entry.parent ? entry.parent->children().indexOf(node) : 0;
    removed->entries.push_back(std::move(entry));
  }
  for (auto& entry : removed->entries) {
    auto* node = qobject_cast<QgsLayerTreeLayer*>(entry.node.get());
    QgsMapLayer* layer = node ? project->mapLayer(node->layerId()) : nullptr;
    if (!layer) continue;
    if (m_editLayer == layer) stopCaptureTool();
    if (auto* vector = qobject_cast<QgsVectorLayer*>(layer)) vector->removeSelection();
    const QString layerId = layer->id();
    // takeMapLayer는 등록만 뺀다. 레이어 창 노드는 이름(글자)을 남긴 채 도형만 사라진다.
    entry.layer.reset(project->takeMapLayer(layer));
    if (QgsLayerTreeLayer* live = root->findLayer(layerId)) {
      if (auto* parent = qobject_cast<QgsLayerTreeGroup*>(live->parent()))
        parent->removeChildNode(live);
    }
  }
  // A group emptied here (지적도, 주변유적) would otherwise stay behind as a bare title row.
  for (const auto& entry : removed->entries) {
    QgsLayerTreeGroup* group = entry.parent.data();
    while (group && group != root && group->children().isEmpty()) {
      auto* above = qobject_cast<QgsLayerTreeGroup*>(group->parent());
      if (!above) break;
      KaRemovedLayers::Group detached;
      detached.parent = above;
      detached.index = above->children().indexOf(group);
      if (!above->takeChild(group)) break;
      detached.node.reset(group);
      removed->groups.push_back(std::move(detached));
      group = above;
    }
  }
  // 고른 묶음 줄 자체. 레이어가 든 하위 묶음은 위에서 따로 떼어 두었고, 처음부터 빈 하위 묶음은
  // takeChild 동안 QGIS 쪽에서 빠져 Ctrl+Z 로 돌아오지 않는다(빈 제목 줄은 원래 남기지 않는다).
  for (QgsLayerTreeGroup* group : groups) {
    auto* above = qobject_cast<QgsLayerTreeGroup*>(group->parent());
    if (!above) continue;  // 비어서 위에서 이미 떼어 냈다.
    KaRemovedLayers::Group detached;
    detached.parent = above;
    detached.index = above->children().indexOf(group);
    if (!above->takeChild(group)) continue;
    detached.node.reset(group);
    removed->groups.push_back(std::move(detached));
  }
  if (removed->entries.empty() && removed->groups.empty()) return;
  const bool onlyGroups = removed->entries.empty();
  bool removedBackground = false;
  for (const auto& entry : removed->entries) {
    if (LayerOps::isVworldCadastralPicture(entry.layer.get()))
      removedBackground = true;
  }
  if (removedBackground)
    LayerOps::rememberUserRemovedCadastral(project);
  KaUndoAction action;
  action.type = KaUndoAction::LayersRemoved;
  action.removedLayers = std::move(removed);
  m_undoActions.append(action);
  if (m_featureSelectTool) m_featureSelectTool->refreshSelectedGeometry();
  project->setDirty(true);
  refreshLayerEmptyState();
  updateNextActionStatus();
  if (m_canvas) LayerOps::refreshCanvasIfIdle(m_canvas);
  if (m_drawingStudio) m_drawingStudio->refreshMapFromProject();
  updateUndoRedoActions();
  statusBar()->showMessage(onlyGroups
      ? QStringLiteral("빈 묶음을 목록에서 지웠습니다. Ctrl+Z로 복원할 수 있습니다.")
      : QStringLiteral("레이어를 목록에서 제거했습니다. Ctrl+Z로 복원할 수 있습니다. 원본 파일은 그대로입니다."), 6000);
}

void MainWindow::updateUndoRedoActions() {
  QgsVectorLayer* preferred = preferredMapLayer(this, m_layerTree, m_editLayer);
  const bool captureVertex = m_captureTool && m_canvas && m_canvas->mapTool() == m_captureTool &&
                             m_captureTool->pointCount() > 0;
  const bool canUndo = captureVertex ||
                       LayerOps::preferredUndoLayer(QgsProject::instance(), preferred) ||
                       !m_undoActions.isEmpty();
  const bool canRedo = LayerOps::preferredRedoLayer(QgsProject::instance(), preferred);
  if (m_actUndo) m_actUndo->setEnabled(canUndo);
  if (m_actRedo) m_actRedo->setEnabled(canRedo);
}

void MainWindow::undoLastAction() {
  if (editingText()) return;
  if (m_viewTabs && m_terrain3dLayoutStudio && m_viewTabs->currentWidget() == m_terrain3dLayoutStudio) {
    m_terrain3dLayoutStudio->undoLastChange();
    return;
  }
  if (routeEditKeyToActiveStudio(false)) return;
  undoMapAction();
}

void MainWindow::redoLastAction() {
  if (editingText()) return;
  if (m_viewTabs && m_terrain3dLayoutStudio && m_viewTabs->currentWidget() == m_terrain3dLayoutStudio) {
    m_terrain3dLayoutStudio->redoLastChange();
    return;
  }
  if (m_viewTabs && m_drawingStudio && m_viewTabs->currentWidget() == m_drawingStudio) {
    m_drawingStudio->handleRedoKey();
    return;
  }
  redoMapAction();
}

void MainWindow::undoMapAction() {
  if (editingText() || m_isOpeningSurvey || m_closingWindow) return;
  if (m_captureTool && m_canvas && m_canvas->mapTool() == m_captureTool && m_captureTool->undoLastVertex()) {
    statusBar()->showMessage(QStringLiteral("꼭짓점 하나를 되돌렸습니다."), 4000);
    return;
  }
  if (m_alignTool && m_canvas && m_canvas->mapTool() == m_alignTool && m_alignTool->removeLastPair()) {
    statusBar()->showMessage(m_alignTool->statusText(), 4000);
    return;
  }
  auto* project = QgsProject::instance();
  if (auto* layer = LayerOps::preferredUndoLayer(project, preferredMapLayer(this, m_layerTree, m_editLayer))) {
    if (LayerOps::undoLayerEdits(layer)) {
      project->setDirty(true);
      if (m_featureSelectTool) m_featureSelectTool->refreshSelectedGeometry();
      if (m_canvas) LayerOps::refreshCanvasIfIdle(m_canvas);
      if (m_drawingStudio) m_drawingStudio->refreshMapFromProject();
      refreshWorkPanel();
      statusBar()->showMessage(QStringLiteral("이전 상태로 되돌렸습니다."), 4000);
      return;
    }
  }
  if (m_undoActions.isEmpty()) {
    statusBar()->showMessage(QStringLiteral("되돌릴 것이 없습니다."), 4000);
    return;
  }
  const KaUndoAction action = m_undoActions.last();
  QString error;
  bool applied = false;
  if (action.type == KaUndoAction::LayersRemoved && action.removedLayers) {
    QScopedValueRollback<bool> restoring(m_isOpeningSurvey, true);
    // Outer groups first, so every layer below returns into the group it was removed from.
    auto& groups = action.removedLayers->groups;
    for (auto it = groups.rbegin(); it != groups.rend(); ++it) {
      if (!it->node) continue;
      auto* parent = it->parent ? it->parent.data() : project->layerTreeRoot();
      parent->insertChildNode(std::clamp(it->index, 0, int(parent->children().size())), it->node.release());
    }
    for (auto& entry : action.removedLayers->entries) {
      if (!entry.layer) continue;
      const QPointer<QgsMapLayer> layer(entry.layer.release());
      if (!project->addMapLayer(layer, false) || !layer) {
        if (layer && !project->mapLayer(layer->id())) entry.layer.reset(layer);
        error = QStringLiteral("레이어를 복원하지 못했습니다. 다시 Ctrl+Z를 눌러 보세요.");
        break;
      }
      auto* parent = entry.parent ? entry.parent.data() : project->layerTreeRoot();
      parent->insertChildNode(std::clamp(entry.index, 0, int(parent->children().size())), entry.node.release());
      if (m_layerTree) m_layerTree->setCurrentLayer(layer);
    }
    applied = error.isEmpty();
  } else if (action.type == KaUndoAction::LayerAdded) {
    if (project->mapLayer(action.layerId)) {
      project->removeMapLayer(action.layerId);
      applied = true;
    }
  } else if (action.type == KaUndoAction::FeatureAdded || action.type == KaUndoAction::FeatureChanged || action.type == KaUndoAction::AttributesChanged) {
    auto* layer = qobject_cast<QgsVectorLayer*>(project->mapLayer(action.layerId));
    if (layer && layer->getFeature(action.featureId).isValid()) {
      QgsGeometry previousGeometry = action.featureData.geometry();
      applied = editFeatures(layer, QStringLiteral("되돌리기"), [&]() {
        if (action.type == KaUndoAction::AttributesChanged) {
          QgsAttributeMap values;
          const auto primary = layer->dataProvider()->pkAttributeIndexes();
          for (int i = 0; i < action.featureData.attributeCount(); ++i)
            if (!primary.contains(i)) values.insert(i, action.featureData.attribute(i));
          return layer->changeAttributeValues(action.featureId, values);
        }
        return action.type == KaUndoAction::FeatureAdded ? layer->deleteFeature(action.featureId)
            : layer->changeGeometry(action.featureId, previousGeometry);
      }, &error);
    }
  } else if (action.type == KaUndoAction::FeatureDeleted) {
    // Restore one user operation (possibly several selected shapes), not one
    // arbitrary feature per key press. Successfully restored entries are removed
    // from the pending command so a failed layer can be retried without duplicates.
    auto& pending = m_undoActions.last().deletedFeatures;
    while (!pending.isEmpty()) {
      const QString id = pending.first().first;
      auto* layer = qobject_cast<QgsVectorLayer*>(project->mapLayer(id));
      if (!layer) break;
      QgsFeatureList restored;
      for (const auto& record : pending) if (record.first == id) restored.append(record.second);
      QgsFeatureList committed;
      const auto connection = connect(layer, &QgsVectorLayer::committedFeaturesAdded, this,
          [&committed](const QString&, const QgsFeatureList& features) { committed = features; });
      const bool ok = editFeatures(layer, QStringLiteral("삭제 되돌리기"), [&]() {
        return layer->addFeatures(restored);
      }, &error);
      disconnect(connection);
      if (!ok) break;
      const QgsFeatureList& actual = committed.isEmpty() ? restored : committed;
      int index = 0;
      for (const auto& record : pending) {
        if (record.first != id) continue;
        if (index < actual.size()) {
          for (auto& older : m_undoActions) {
            if (older.layerId == id && older.featureId == record.second.id())
              older.featureId = actual.at(index).id();
          }
        }
        ++index;
      }
      pending.removeIf([&](const auto& record) { return record.first == id; });
    }
    applied = pending.isEmpty();
  }
  if (!applied) {
    notify(Notice::Warning, QStringLiteral("되돌리기"), error.isEmpty()
        ? QStringLiteral("이전 작업의 레이어나 도형을 찾지 못했습니다. 레이어를 복원한 뒤 다시 실행하세요.") : error);
    return;
  }
  m_undoActions.removeLast();
  project->setDirty(true);
  if (m_featureSelectTool) m_featureSelectTool->refreshSelectedGeometry();
  if (m_canvas) LayerOps::refreshCanvasIfIdle(m_canvas);
  if (m_drawingStudio) m_drawingStudio->refreshMapFromProject();
  refreshWorkPanel();
  if (!error.isEmpty()) notify(Notice::Warning, QStringLiteral("되돌리기 저장 확인"), error);
  else statusBar()->showMessage(QStringLiteral("이전 상태로 되돌렸습니다."), 4000);
}

void MainWindow::redoMapAction() {
  if (editingText() || m_isOpeningSurvey || m_closingWindow) return;
  auto* project = QgsProject::instance();
  if (auto* layer = LayerOps::preferredRedoLayer(project, preferredMapLayer(this, m_layerTree, m_editLayer))) {
    if (LayerOps::redoLayerEdits(layer)) {
      project->setDirty(true);
      if (m_featureSelectTool) m_featureSelectTool->refreshSelectedGeometry();
      if (m_canvas) LayerOps::refreshCanvasIfIdle(m_canvas);
      if (m_drawingStudio) m_drawingStudio->refreshMapFromProject();
      refreshWorkPanel();
      statusBar()->showMessage(QStringLiteral("다시 실행했습니다."), 4000);
      return;
    }
  }
  statusBar()->showMessage(QStringLiteral("다시 실행할 것이 없습니다."), 4000);
}

void MainWindow::deleteFeaturesOrSelectedReferenceLayers() {
  if (editingText()) return;
  const auto selected = KaFeatureSelectTool::allSelectedFeatures(m_canvas);
  for (const auto& item : selected) {
    if (item.layer && !LayerOps::isReferenceOrBasemapLayer(item.layer.data())) {
      deleteSelectedFeatures();
      return;
    }
  }
  QgsMapLayer* current = nullptr;
  if (m_layerTree) current = m_layerTree->currentLayer();
  if (!current && m_canvas) current = m_canvas->currentLayer();
  QList<QgsMapLayer*> treeSelected;
  if (m_layerTree) treeSelected = m_layerTree->selectedLayers();
  if (treeSelected.isEmpty() && current) treeSelected << current;
  for (QgsMapLayer* layer : treeSelected) {
    if (!layer || !LayerOps::isReferenceOrBasemapLayer(layer)) continue;
    if (m_layerTree && m_layerTree->selectedLayers().isEmpty())
      m_layerTree->setCurrentLayer(layer);
    removeSelectedLayers();
    return;
  }
  if (m_layerTree) {
    // 지도에서 누른 Delete 로는 조사 레이어가 든 묶음을 통째로 빼지 않는다. 그런 묶음은 레이어 창에서 지운다.
    const QList<QgsMapLayer*> fromNode = LayerOps::removableLegendLayersFromNode(m_layerTree->currentNode());
    const bool touchesSurvey = std::any_of(fromNode.cbegin(), fromNode.cend(), [](const QgsMapLayer* layer) {
      return !LayerOps::layerKeyOf(layer).isEmpty();
    });
    if (!fromNode.isEmpty() && !touchesSurvey) {
      removeSelectedLayers();
      return;
    }
  }
  deleteSelectedFeatures();
}

void MainWindow::deleteSelectedFeatures() {
  if (editingText()) return;
  const auto selected = KaFeatureSelectTool::allSelectedFeatures(m_canvas);
  QSet<QString> done;
  QStringList errors;
  int deleted = 0;
  for (const auto& item : selected) {
    auto* layer = item.layer.data();
    if (!layer || done.contains(layer->id()) || LayerOps::isReferenceLayer(layer) ||
        LayerOps::isCadastralLayer(layer))
      continue;
    done.insert(layer->id());
    QgsFeatureList features;
    for (const auto& candidate : selected) {
      if (candidate.layer == layer) {
        const QgsFeature feature = layer->getFeature(candidate.fid);
        if (feature.isValid()) features.append(feature);
      }
    }
    if (features.isEmpty()) continue;
    QString error;
    if (editFeatures(layer, QStringLiteral("도형 삭제"), [&]() {
      for (const auto& feature : features) if (!layer->deleteFeature(feature.id())) return false;
      return true;
    }, &error)) {
      deleted += features.size();
      layer->removeSelection();
    }
    if (!error.isEmpty()) errors.append(layer->name() + QStringLiteral(": ") + error);
  }
  if (deleted > 0) {
    QgsProject::instance()->setDirty(true);
    if (m_featureSelectTool) m_featureSelectTool->refreshSelectedGeometry();
    if (m_canvas) LayerOps::refreshCanvasIfIdle(m_canvas);
    refreshWorkPanel();
    statusBar()->showMessage(QStringLiteral("도형 %1개를 지웠습니다. Ctrl+Z로 복원할 수 있습니다.").arg(deleted), 6000);
  }
  if (!errors.isEmpty()) notify(Notice::Warning, QStringLiteral("도형 삭제 확인"), errors.join(QLatin1Char('\n')));
}
