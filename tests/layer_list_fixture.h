// Shared fixture of the P4 layer-list tests (kept out of test_layer_list_chrome.cpp for the
// 300-line limit): one project whose root mixes the three sections plus a group.
#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <qgslayertree.h>
#include <qgslayertreemodel.h>
#include <qgsproject.h>
#include <qgsrasterlayer.h>
#include <qgsvectorlayer.h>

#include "app/KaLayerInformation.h"
#include "app/KaLayerSectionDelegate.h"
#include "core/LayerOps.h"

namespace LayerListFixture {

inline QgsVectorLayer* vector(QgsProject& project, QgsLayerTreeGroup* group, const QString& name) {
  auto* layer = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5187&field=kind:string"), name,
                                   QStringLiteral("memory"));
  project.addMapLayer(layer, false);
  group->addLayer(layer);
  return layer;
}

// A live-tile layer (LayerOps::isBasemapLayer looks at the provider only); nothing is fetched.
inline QgsRasterLayer* basemap(QgsProject& project, QgsLayerTreeGroup* group, const QString& name) {
  auto* layer = new QgsRasterLayer(
      QStringLiteral("type=xyz&url=http://127.0.0.1:9/%7Bz%7D/%7Bx%7D/%7By%7D.png&zmax=19"), name,
      QStringLiteral("wms"));
  project.addMapLayer(layer, false);
  group->addLayer(layer);
  return layer;
}

// Root order: 조사구역 · 유구 면 (survey) | 지적도 (cadastral) | 위성 · 지형 (basemaps) |
// 「참조 지도」 group [수계도 (reference)]. Bands are expected at rows 0, 2 and 3.
struct Tree {
  QgsProject project;
  QgsVectorLayer* area = nullptr;
  QgsVectorLayer* poly = nullptr;
  QgsVectorLayer* cadastral = nullptr;
  QgsRasterLayer* satellite = nullptr;
  QgsRasterLayer* terrain = nullptr;
  QgsLayerTreeGroup* refs = nullptr;
  QgsVectorLayer* river = nullptr;

  Tree() {
    QgsLayerTreeGroup* root = project.layerTreeRoot();
    area = vector(project, root, QStringLiteral("조사구역"));
    LayerOps::markSurveyLayer(area, QStringLiteral("survey_area"));
    poly = vector(project, root, QStringLiteral("유구 면"));
    LayerOps::markSurveyLayer(poly, QStringLiteral("feature_poly"));
    cadastral = vector(project, root, QStringLiteral("지적도 · 조사 주변 5km"));
    LayerOps::markCadastralLayer(cadastral);
    satellite = basemap(project, root, QStringLiteral("위성"));
    terrain = basemap(project, root, QStringLiteral("지형"));
    refs = root->addGroup(QStringLiteral("참조 지도"));
    river = vector(project, refs, QStringLiteral("수계도"));
    LayerOps::markReferenceLayer(river);
  }
  QgsLayerTreeNode* node(const QgsMapLayer* layer) const { return project.layerTreeRoot()->findLayer(layer->id()); }
};

// The configured list view in a shown host, as MainWindow builds it.
struct View {
  QWidget host;
  KaLayerInformationModel model;
  KaLayerInformationView* view = nullptr;

  explicit View(Tree& tree, const QSize& size = QSize(340, 480)) : model(&tree.project, false) {
    model.setFlag(QgsLayerTreeModel::AllowNodeChangeVisibility);
    auto* column = new QVBoxLayout(&host);
    column->setContentsMargins(0, 0, 0, 0);
    view = new KaLayerInformationView(&host);
    view->setModel(&model);
    KaLayerInformationModel::configureView(view);
    column->addWidget(view, 1);
    host.resize(size);
    host.show();
    view->expandAll();
  }
  KaLayerSectionDelegate* sections() const { return qobject_cast<KaLayerSectionDelegate*>(view->itemDelegate()); }
  QModelIndex index(QgsLayerTreeNode* node, int column = 0) const { return view->node2index(node).siblingAtColumn(column); }
  QModelIndex row(int row) const { return view->model()->index(row, 0); }
  QList<int> bandRows() const {
    QList<int> rows;
    for (int r = 0; r < view->model()->rowCount(); ++r)
      if (sections()->hasBand(row(r))) rows << r;
    return rows;
  }
};

}  // namespace LayerListFixture
