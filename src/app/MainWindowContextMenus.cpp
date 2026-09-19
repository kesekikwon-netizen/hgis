#include "KaCrashGuard.h"
#include "MainWindow.h"
#include "KaAttributeMapTool.h"
#include "KaCaptureMapTool.h"
#include "KaDrawingStudio.h"
#include "KaFeatureSelectTool.h"
#include "KaFoundLocationMark.h"
#include "KaMeasureMapTool.h"
#include "core/GeorefService.h"
#include "core/LayerOps.h"
#include "core/LayerLabelControls.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QFileInfo>
#include <QItemSelectionModel>
#include <QMenu>
#include <QMessageBox>
#include <QPointer>
#include <QStatusBar>
#include <QTabWidget>
#include <cmath>
#include <functional>

#include <qgsdistancearea.h>
#include <qgsfeatureiterator.h>
#include <qgsgeometry.h>
#include <qgsexception.h>
#include <qgslayertree.h>
#include <qgslayertreeview.h>
#include <qgsmapcanvas.h>
#include <qgsmapmouseevent.h>
#include <qgsmaptopixel.h>
#include <qgsproject.h>
#include <qgsrasterlayer.h>
#include <qgsvectordataprovider.h>
#include <qgsvectorlayer.h>

namespace {
enum class LayerMenuKind { Area, Feature, Control, Section, Trench, Reference, ExternalVector, ExternalRaster };

LayerMenuKind menuKind(QgsMapLayer* layer) {
  // Domain identity comes from metadata, never the displayed Korean title.
  const QString key = LayerOps::layerKeyOf(layer);
  if (qobject_cast<QgsVectorLayer*>(layer)) {
    if (key == QLatin1String("survey_area")) return LayerMenuKind::Area;
    if (key == QLatin1String("control_points")) return LayerMenuKind::Control;
    if (key == QLatin1String("section_line")) return LayerMenuKind::Section;
    if (key == QLatin1String("trial_trench")) return LayerMenuKind::Trench;
    if (key == QLatin1String("feature_poly") || key == QLatin1String("feature_line") ||
        key == QLatin1String("artifact_point")) return LayerMenuKind::Feature;
  }
  if (layer->customProperty(QStringLiteral("ka_hgis/imported_reference")).toBool() ||
      LayerOps::isAlignPending(layer) ||
      (layer->providerType() == QLatin1String("ogr") &&
       GeorefService::isCadPath(layer->source().section(QLatin1Char('|'), 0, 0))))
    return qobject_cast<QgsVectorLayer*>(layer) ? LayerMenuKind::ExternalVector : LayerMenuKind::ExternalRaster;
  if (layer->customProperty(QStringLiteral("ka_hgis/layer_role")).toString() == QLatin1String("reference") ||
      LayerOps::isBasemapLayer(layer)) return LayerMenuKind::Reference;
  return qobject_cast<QgsVectorLayer*>(layer) ? LayerMenuKind::ExternalVector : LayerMenuKind::ExternalRaster;
}

bool isDomain(LayerMenuKind kind) {
  return kind == LayerMenuKind::Area || kind == LayerMenuKind::Feature ||
         kind == LayerMenuKind::Control || kind == LayerMenuKind::Section || kind == LayerMenuKind::Trench;
}

QAction* addMenuAction(QMenu* menu, const char* id, const QString& label,
                      const QString& unavailable, const std::function<void()>& run,
                      const QString& help = {}) {
  QAction* action = menu->addAction(label);
  action->setObjectName(QString::fromLatin1(id));
  action->setEnabled(unavailable.isEmpty());
  action->setToolTip(unavailable.isEmpty() ? help : unavailable);
  QObject::connect(action, &QAction::triggered, menu, run);
  return action;
}

QMenu* addSubmenu(QMenu* parent, const char* id, const QString& title) {
  QMenu* menu = parent->addMenu(title);
  menu->setToolTipsVisible(true);
  menu->menuAction()->setObjectName(QString::fromLatin1(id));
  return menu;
}

QString editUnavailable(QgsVectorLayer* layer, Qgis::VectorProviderCapabilities capabilities) {
  if (!layer || !layer->isValid()) return QStringLiteral("레이어 파일을 다시 열어 주세요.");
  if (layer->readOnly()) return QStringLiteral("읽기 전용 자료입니다. 편집 가능한 조사 자료를 선택하세요.");
  if (!layer->dataProvider() || (layer->dataProvider()->capabilities() & capabilities) != capabilities)
    return QStringLiteral("이 자료는 해당 편집 기능을 지원하지 않습니다.");
  return {};
}

QString fieldTitle(const QgsField& field) {
  if (!field.alias().isEmpty()) return field.alias();
  static const QHash<QString, QString> titles = {
      {QStringLiteral("survey_name"), QStringLiteral("조사명")},
      {QStringLiteral("feature_no"), QStringLiteral("유구 번호")},
      {QStringLiteral("kind"), QStringLiteral("종류")},
      {QStringLiteral("period"), QStringLiteral("시대")},
      {QStringLiteral("point_id"), QStringLiteral("기준점 번호")},
      {QStringLiteral("section_id"), QStringLiteral("단면 번호")},
      {QStringLiteral("name"), QStringLiteral("이름")},
      {QStringLiteral("note"), QStringLiteral("비고")},
      {QStringLiteral("area_m2"), QStringLiteral("면적(㎡)")}};
  return titles.value(field.name(), field.name());
}
} // namespace

void MainWindow::populateMapContextMenu(QMenu* menu, const QPoint& pos) {
  if (!menu || !m_canvas) return;
  menu->clear();
  menu->setObjectName(QStringLiteral("mapContextMenu"));
  menu->setToolTipsVisible(true);
  const bool busy = m_isOpeningSurvey;
  const QString surveyReason = busy ? QStringLiteral("조사 파일 처리가 끝난 뒤 다시 실행하세요.") :
      m_surveyPath.isEmpty() ? QStringLiteral("먼저 새 조사를 만들거나 조사 파일을 열어 주세요.") : QString();
  ensureAttributeTool();
  QgsVectorLayer* hit = nullptr;
  QgsFeature feature;
  if (m_attributeTool) m_attributeTool->pickAtScreen(pos, &hit, &feature);
  if (hit && feature.isValid() && isDomain(menuKind(hit))) {
    QPointer<QgsVectorLayer> target(hit);
    const bool ownSurvey = !m_surveyPath.isEmpty() &&
        QFileInfo(hit->source().section(QLatin1Char('|'), 0, 0)).absoluteFilePath().compare(
            QFileInfo(m_surveyPath).absoluteFilePath(), Qt::CaseInsensitive) == 0;
    addMenuAction(menu, "map.attributes", QStringLiteral("이 도형 기록 입력"),
        busy ? surveyReason : !ownSurvey ? QStringLiteral("이 자료가 저장된 조사 파일을 먼저 열어 주세요.") :
            editUnavailable(hit, Qgis::VectorProviderCapability::ChangeAttributeValues),
        [this, target, feature]() { if (target) editFeatureAttributes(target, feature); });
  }
  if (!m_surveyPath.isEmpty()) {
    QMenu* draw = addSubmenu(menu, "map.draw", QStringLiteral("조사 도형 그리기"));
    addMenuAction(draw, "map.drawArea", QStringLiteral("조사구역"), surveyReason, [this]() { startEditSurveyArea(); });
    addMenuAction(draw, "map.drawFeature", QStringLiteral("유구 면"), surveyReason, [this]() { startEditFeaturePoly(); });
    addMenuAction(draw, "map.drawLine", QStringLiteral("유구 선"), surveyReason, [this]() { startEditFeatureLine(); });
    addMenuAction(draw, "map.drawSection", QStringLiteral("단면선 그리기"), surveyReason, [this]() { startEditSectionLine(); });
    menu->addSeparator();
  }
  addMenuAction(menu, "map.previous", QStringLiteral("이전 지도 범위"),
      m_canZoomPrevious ? QString() : QStringLiteral("이전에 보던 지도 범위가 없습니다."),
      [this]() { m_canvas->zoomToPreviousExtent(); });
  addMenuAction(menu, "map.next", QStringLiteral("다음 지도 범위"),
      m_canZoomNext ? QString() : QStringLiteral("이전 범위로 돌아간 뒤 사용할 수 있습니다."),
      [this]() { m_canvas->zoomToNextExtent(); });
  QPointer<QgsMapLayer> current(m_layerTree ? m_layerTree->currentLayer() : nullptr);
  addMenuAction(menu, "map.layerZoom", QStringLiteral("선택한 레이어로 이동"),
      current && current->isValid() ? QString() : QStringLiteral("왼쪽 목록에서 레이어를 선택하세요."),
      [this, current]() { if (current) LayerOps::zoomToLayerMax(m_canvas, current); });
  addMenuAction(menu, "map.full", QStringLiteral("전체 지도 보기"), {}, [this]() { zoomMapToFullMax(); });
  addMenuAction(menu, "map.refresh", QStringLiteral("지도 새로고침"), {}, [this]() { m_canvas->refresh(); });
  menu->addSeparator();
  const QgsPointXY point = m_canvas->getCoordinateTransform()->toMapCoordinates(pos.x(), pos.y());
  const QString coordinates = QStringLiteral("%1, %2 (%3)")
      .arg(point.x(), 0, 'f', 3).arg(point.y(), 0, 'f', 3)
      .arg(m_canvas->mapSettings().destinationCrs().authid());
  addMenuAction(menu, "map.copyCoordinates", QStringLiteral("이 위치 좌표 복사"), {},
      [coordinates]() { QApplication::clipboard()->setText(coordinates); }, coordinates);
  if (m_locationMark) {
    menu->addSeparator();
    addMenuAction(menu, "map.clearLocation", QStringLiteral("찾은 위치 표식 지우기"), {},
        [this]() { clearFoundLocationMark(); });
  }
}

void MainWindow::onMapContextMenu(const QPoint& pos) {
  if (!m_canvas || (m_captureTool && m_canvas->mapTool() == m_captureTool) ||
      (m_measureTool && m_canvas->mapTool() == m_measureTool)) return;
  if (m_featureSelectTool && m_canvas->mapTool() == m_featureSelectTool && sender() != m_featureSelectTool) return;
  QMenu menu(this);
  populateMapContextMenu(&menu, pos);
  menu.exec(m_canvas->viewport()->mapToGlobal(pos));
}

void MainWindow::onLayerTreeContextMenu(const QPoint& pos) {
  showLayerTreeContextMenu(m_layerTree, pos);
}

void MainWindow::showLayerTreeContextMenu(QgsLayerTreeView* treeView, const QPoint& pos) {
  if (!treeView || !treeView->viewport()) return;
  // QAbstractScrollArea's customContextMenuRequested position is already in viewport coordinates.
  const QModelIndex index = treeView->indexAt(pos);
  auto* node = index.isValid() ? qobject_cast<QgsLayerTreeLayer*>(treeView->index2node(index)) : nullptr;
  QPointer<QgsMapLayer> layer(node ? node->layer() : nullptr);
  QMenu menu(this);
  menu.setObjectName(QStringLiteral("layerContextMenu"));
  menu.setToolTipsVisible(true);
  if (!layer) {
    addMenuAction(&menu, "layer.import", QStringLiteral("참고 자료 불러오기…"), {}, [this]() { addUserLayer(); });
    menu.exec(treeView->viewport()->mapToGlobal(pos));
    return;
  }
  treeView->setCurrentLayer(layer);
  if (m_layerTree && treeView != m_layerTree) m_layerTree->setCurrentLayer(layer);
  const LayerMenuKind kind = menuKind(layer);
  const bool domain = isDomain(kind);
  const QPointer<QgsVectorLayer> vector(qobject_cast<QgsVectorLayer*>(layer.data()));
  const bool layoutLabels = treeView->objectName() == QLatin1String("layoutLayerTree");
  const bool valid = layer->isValid();
  const bool hasFeatures = vector && valid && vector->featureCount() > 0;
  const QString invalidReason = valid ? QString() : QStringLiteral("레이어 파일을 찾을 수 없습니다. 자료를 다시 열어 주세요.");
  const QString emptyReason = !valid ? invalidReason : !hasFeatures ? QStringLiteral("이 레이어에는 도형이 없습니다.") : QString();
  const bool busy = m_isOpeningSurvey;
  const QString busyReason = busy ? QStringLiteral("조사 파일 처리가 끝난 뒤 다시 실행하세요.") : QString();
  const bool ownSurvey = domain && !m_surveyPath.isEmpty() &&
      QFileInfo(layer->source().section(QLatin1Char('|'), 0, 0)).absoluteFilePath().compare(
          QFileInfo(m_surveyPath).absoluteFilePath(), Qt::CaseInsensitive) == 0;
  QString drawReason = editUnavailable(vector, Qgis::VectorProviderCapability::AddFeatures);
  if (domain && !ownSurvey) drawReason = QStringLiteral("이 자료가 저장된 조사 파일을 먼저 열어 주세요.");
  if (busy) drawReason = busyReason;
  QString vertexReason = emptyReason.isEmpty() ? editUnavailable(vector, Qgis::VectorProviderCapability::ChangeGeometries) : emptyReason;
  if (domain && !ownSurvey) vertexReason = QStringLiteral("이 자료가 저장된 조사 파일을 먼저 열어 주세요.");
  if (busy) vertexReason = busyReason;

  const auto add = [&](QMenu* parent, const char* id, const QString& title, const QString& reason,
                       const std::function<void()>& run, const QString& help = QString()) {
    return addMenuAction(parent, id, title, reason, [this, layer, run]() {
      if (!layer || m_closingWindow) return;
      if (m_layerTree) m_layerTree->setCurrentLayer(layer);
      run();
    }, help);
  };
  const auto zoom = [&]() {
    const QString reason = vector ? emptyReason : invalidReason;
    add(&menu, "layer.zoom", QStringLiteral("이 레이어로 이동"),
        reason, [this, layer]() {
          if (!LayerOps::zoomToLayerMax(m_canvas, layer)) {
            statusBar()->showMessage(QStringLiteral("표시할 범위가 없습니다. 도형이나 자료 범위를 확인하세요."), 6000);
            return;
          }
          if (m_drawingStudio) m_drawingStudio->centerOnMapCanvas();
        });
  };
  const auto draw = [&]() { add(&menu, "layer.draw", QStringLiteral("그리기·편집 시작"), drawReason,
      [this, vector]() { if (vector) beginEdit(vector); }); };
  const auto vertices = [&]() {
    if (!vector || vector->geometryType() == Qgis::GeometryType::Point) return;
    add(&menu, "layer.vertices", QStringLiteral("꼭짓점 수정"), vertexReason, [this]() {
      if (!m_featureSelectTool || m_canvas->mapTool() != m_featureSelectTool) startSelectTool();
      statusBar()->showMessage(QStringLiteral("고칠 도형을 클릭한 뒤 꼭짓점을 끌어 옮기세요."), 8000);
    });
  };
  const auto attributes = [&]() {
    const QString reason = busy ? busyReason : domain && !ownSurvey ? QStringLiteral("이 자료가 저장된 조사 파일을 먼저 열어 주세요.") : !emptyReason.isEmpty() ? emptyReason :
        editUnavailable(vector, Qgis::VectorProviderCapability::ChangeAttributeValues);
    add(&menu, "layer.attributes", QStringLiteral("도형 속성"), vector ? reason : QStringLiteral("영상 지도에는 편집할 도형 속성이 없습니다."),
        [this, vector]() { if (vector) editCurrentLayerAttributes(vector); });
  };
  const auto refreshViews = [this]() {
    QgsProject::instance()->setDirty(true);
    if (m_canvas) m_canvas->refresh();
    if (m_drawingStudio) m_drawingStudio->refreshMapFromProject();
  };
  const auto appearance = [&]() {
    QMenu* display = &menu;
    add(display, "layer.rename", QStringLiteral("이름 바꾸기 (두 번 클릭)"), {}, [this, layer]() { renameSelectedLayer(layer); });
    add(display, "layer.style", QStringLiteral("면·외곽선 색"), vector ? invalidReason : QStringLiteral("영상 지도는 투명도로 표시를 조절하세요."),
        [this, layer]() { editCurrentLayerStyle(layer); });
    attributes();
    QMenu* labels = addSubmenu(display, "layer.labels", QStringLiteral("라벨·명칭 속성"));
    if (!vector) {
      labels->menuAction()->setEnabled(false);
      labels->menuAction()->setToolTip(QStringLiteral("영상에 포함된 글자는 별도로 크기를 바꿀 수 없습니다."));
      return;
    }
    const auto labelInfo = LayerLabelControls::describe(vector, layoutLabels);
    const bool labelable = valid && labelInfo.supported;
    const QString labelReason = labelable ? QString() : QStringLiteral("표시할 이름이나 번호 항목이 없습니다.");
    labels->menuAction()->setEnabled(labelable);
    labels->menuAction()->setToolTip(labelReason);
    const bool on = labelInfo.enabled;
    add(labels, "layer.labelsVisible", labelInfo.caption + (on ? QStringLiteral(" 숨기기") : QStringLiteral(" 보이기")), labelReason,
        [vector, on, layoutLabels, refreshViews]() {
          if (vector && LayerLabelControls::setVisible(vector, !on, layoutLabels)) refreshViews();
        });
    // Layout heritage numbers own their expression and badge size. Source name
    // fields must not overwrite that independent presentation from this menu.
    if (layoutLabels && LayerLabelControls::isHeritage(vector)) return;
    const QString currentField = LayerOps::currentLabelField(vector);
    QMenu* fields = addSubmenu(labels, "layer.labelField", QStringLiteral("표시할 항목"));
    fields->menuAction()->setEnabled(labelInfo.fieldEditable);
    const QgsFields available = vector->fields();
    QMenu* fieldGroup = fields;
    QMenu* fieldPage = fields;
    for (int i = 0; i < available.size(); ++i) {
      if (available.size() > 10 && i % 10 == 0) {
        if (i > 0 && i % 90 == 0)
          fieldPage = addSubmenu(fieldPage, "layer.labelFieldsNext", QStringLiteral("다음 항목…"));
        fieldGroup = addSubmenu(fieldPage, "layer.labelFields", QStringLiteral("항목 %1–%2").arg(i + 1).arg(qMin(i + 10, available.size())));
      }
      const QString field = available.at(i).name();
      QAction* action = add(fieldGroup, "layer.labelFieldValue", fieldTitle(available.at(i)), {},
          [this, vector, field, refreshViews]() { if (vector) {
            if (LayerLabelControls::setField(vector, field)) { applyLabelStackOrder(); refreshViews(); }
          }});
      action->setCheckable(true); action->setChecked(field == currentField);
    }
    if (labelInfo.areaEditable) {
      const bool areaOn = LayerOps::labelShowArea(vector, false);
      QAction* action = add(labels, "layer.labelArea", QStringLiteral("면적(㎡) 함께 표시"), labelReason,
          [this, vector, currentField, areaOn, refreshViews]() { if (vector) {
            if (LayerLabelControls::setArea(vector, !areaOn)) { applyLabelStackOrder(); refreshViews(); }
          }});
      action->setCheckable(true); action->setChecked(areaOn);
    }
    QMenu* sizes = addSubmenu(labels, "layer.labelSize", QStringLiteral("글자 크기"));
    sizes->menuAction()->setEnabled(vector->labeling() != nullptr);
    for (double size : {3., 4., 5., 6., 7., 8., 9., 10., 12., 14.}) {
      QAction* action = add(sizes, "layer.labelSizeValue", QStringLiteral("%1 pt").arg(size), labelReason,
          [this, vector, size, refreshViews]() { if (vector) {
            if (!LayerOps::setLabelFontSize(vector, size)) {
              statusBar()->showMessage(QStringLiteral("글자 크기를 바꾸지 못했습니다. 레이어를 다시 선택해 주세요."), 8000);
              return;
            }
            applyLabelStackOrder(); refreshViews();
          }});
      action->setCheckable(true); action->setChecked(qFuzzyCompare(size, LayerOps::labelFontSize(vector, 5.0)));
    }
    if (vector->source().section(QLatin1Char('|'), 0, 0).endsWith(QLatin1String(".shp"), Qt::CaseInsensitive)) {
      QMenu* encoding = addSubmenu(display, "layer.encoding", QStringLiteral("깨진 한글 바로잡기"));
      for (const QString& value : {QStringLiteral("CP949"), QStringLiteral("EUC-KR"), QStringLiteral("UTF-8"), QStringLiteral("System")})
        add(encoding, "layer.encodingValue", value == QLatin1String("System") ? QStringLiteral("이 컴퓨터 설정") : value, invalidReason,
            [vector, value, refreshViews]() { if (vector) { LayerOps::setShapefileEncoding(vector, value); refreshViews(); } });
    }
  };
  const auto area = [&]() {
    add(&menu, "layer.area", kind == LayerMenuKind::Trench ? QStringLiteral("면적·조사 비율 확인") : QStringLiteral("면적 확인"),
        emptyReason, [this, vector, kind]() { if (vector) showLayerAreaSummary(vector, kind == LayerMenuKind::Trench); });
  };
  const auto exportSurvey = [&]() {
    menu.addSeparator();
    const QString reason = busy ? busyReason : m_surveyPath.isEmpty() ? QStringLiteral("먼저 조사 파일을 열어 주세요.") : emptyReason;
    add(&menu, "layer.export", QStringLiteral("제출 파일 만들기…"), reason, [this]() { exportShpPackage(); },
        QStringLiteral("현재 조사 전체를 검수한 뒤 SHP·PDF·MANIFEST 제출 꾸러미를 만듭니다."));
  };

  add(&menu, "layer.import", QStringLiteral("레이어 불러오기 (SHP·DXF·GPKG)"), busyReason, [this]() { addUserLayer(); });
  menu.addSeparator();
  appearance();
  QMenu* opacity = addSubmenu(&menu, "layer.opacity", QStringLiteral("투명도"));
  opacity->menuAction()->setEnabled(valid);
  opacity->menuAction()->setToolTip(invalidReason);
  const double opacityNow = LayerOps::mapLayerOpacity(layer);
  for (int transparency : {0, 20, 40, 50, 60, 80}) {
    QAction* action = add(opacity, "layer.opacityValue", QStringLiteral("%1% 투명하게").arg(transparency), {},
        [this, layer, transparency, refreshViews]() {
          LayerOps::setMapLayerOpacity(layer, 1.0 - transparency / 100.0, m_canvas);
          updateLayerOpacityControl(); refreshViews();
        });
    action->setCheckable(true);
    action->setChecked(qAbs(opacityNow * 100.0 - (100 - transparency)) < 1.0);
  }
  menu.addSeparator();
  zoom();
  add(&menu, "layer.fullExtent", QStringLiteral("전체 보기"), {}, [this]() { zoomMapToFullMax(); });
  auto* group = qobject_cast<QgsLayerTreeGroup*>(node->parent());
  const bool alreadyBottom = !group || group->children().last() == node;
  add(&menu, "layer.bottom", QStringLiteral("같은 묶음의 맨 아래로"), alreadyBottom ? QStringLiteral("이 묶음에서 이미 맨 아래에 있습니다.") : QString(),
      [layer, refreshViews]() {
        auto* item = QgsProject::instance()->layerTreeRoot()->findLayer(layer->id());
        auto* parent = item ? qobject_cast<QgsLayerTreeGroup*>(item->parent()) : nullptr;
        if (!item || !parent) return;
        auto* copy = item->clone();
        parent->addChildNode(copy);
        parent->removeChildNode(item);
        refreshViews();
      });
  add(&menu, "layer.merge", QStringLiteral("폴리곤 묶기"),
      vector && vector->geometryType() == Qgis::GeometryType::Polygon ? vertexReason : QStringLiteral("조사에서 그린 면 도형을 선택하세요."),
      [this]() { mergeFeaturePolygons(); });
  if (kind == LayerMenuKind::Area) {
    QMenu* trench = addSubmenu(&menu, "layer.trenchCreate", QStringLiteral("시굴격자"));
    const QString reason = !drawReason.isEmpty() ? drawReason : emptyReason;
    trench->menuAction()->setEnabled(reason.isEmpty());
    trench->menuAction()->setToolTip(reason);
    add(trench, "layer.trench10", QStringLiteral("시굴 · 조사구역의 10%"), reason, [this]() { applyTrenchByRatio(10.0); });
    add(trench, "layer.trench2", QStringLiteral("표본 · 조사구역의 2%"), reason, [this]() { applyTrenchByRatio(2.0); });
    add(trench, "layer.trenchCustom", QStringLiteral("규격 지정·격자 만들기…"), reason, [this]() { startTrenchGrid(); });
  }
  if (layer->providerType() == QLatin1String("wms") && layer->source().contains(QLatin1String("type=xyz")))
    add(&menu, "layer.offline", QStringLiteral("오프라인 저장 — 지금 화면 범위"), invalidReason, [this]() { saveOfflineTilePack(); });
  menu.addSeparator();
  if (domain) {
    const QString reason = busy ? busyReason : !ownSurvey ? QStringLiteral("이 자료가 저장된 조사 파일을 먼저 열어 주세요.") : emptyReason;
    add(&menu, "layer.clear", QStringLiteral("그린 도형 모두 지우기…"), reason,
        [this]() { clearDrawnFeaturesOfCurrentLayer(); });
  }
  add(&menu, "layer.remove", QStringLiteral("레이어 삭제"), busyReason, [this, layer, treeView]() {
    // The clicked layer is the target even if another row was selected before.
    treeView->clearSelection();
    treeView->setCurrentLayer(layer);
    removeLayersFromTree(treeView);
  }, QStringLiteral("목록에서 제거합니다. 원본 파일은 보존하며 Ctrl+Z로 복원할 수 있습니다."));
  menu.exec(treeView->viewport()->mapToGlobal(pos));
}

void MainWindow::showLayerAreaSummary(QgsVectorLayer* layer, bool showRatio) {
  if (!layer || !layer->isValid()) return;
  const auto measure = [](QgsVectorLayer* vector) {
    QgsDistanceArea calculator;
    calculator.setSourceCrs(vector->crs(), QgsProject::instance()->transformContext());
    calculator.setEllipsoid(QgsProject::instance()->ellipsoid());
    double area = 0.0;
    QgsFeature feature;
    auto iterator = vector->getFeatures();
    while (iterator.nextFeature(feature)) {
      if (!feature.hasGeometry()) continue;
      area += calculator.convertAreaMeasurement(calculator.measureArea(feature.geometry()), Qgis::AreaUnit::SquareMeters);
    }
    return area;
  };
  try {
    const double area = measure(layer);
    if (!std::isfinite(area)) throw QgsException(QStringLiteral("invalid area"));
    QString text = QStringLiteral("%1\n도형 %2개 · 합계 면적 %L3 ㎡")
        .arg(layer->name()).arg(layer->featureCount()).arg(area, 0, 'f', 2);
    if (showRatio) {
      auto* survey = LayerOps::findByLayerKey(QgsProject::instance(), QStringLiteral("survey_area"));
      const double surveyArea = survey ? measure(survey) : 0.0;
      if (surveyArea > 0.0 && std::isfinite(surveyArea))
        text += QStringLiteral("\n전체 조사구역 면적 합계 대비 %L1%\n(면이 겹치면 겹친 부분도 합계에 포함됩니다.)").arg(100.0 * area / surveyArea, 0, 'f', 2);
      else text += QStringLiteral("\n비율을 보려면 조사구역을 먼저 그려 주세요.");
    }
    QMessageBox::information(this, QStringLiteral("면적 확인"), text);
  } catch (...) {
    KaCrashGuard::logLine(QStringLiteral("[except] app/MainWindowContextMenus.cpp:425"));
    notify(Notice::Warning, QStringLiteral("면적 확인"), QStringLiteral("면적을 계산하지 못했습니다. 도형과 좌표계를 검수한 뒤 다시 실행하세요."));
  }
}
