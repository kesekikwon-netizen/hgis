#include "MainWindow.h"
#include "KaAttributeMapTool.h"
#include "KaCaptureMapTool.h"
#include "KaCoordPointMapTool.h"
#include "KaCrashGuard.h"
#include "KaDrawingStudio.h"
#include "KaFeatureFormDialog.h"
#include "KaFeatureSelectTool.h"
#include "KaVertexEditTool.h"
#include "KaMeasureMapTool.h"
#include "KaSurveyAreaDialog.h"
#include "KaTerrain3dLayoutStudio.h"
#include "KaTerrain3dStudio.h"
#include "KaTheme.h"
#include "KaUserError.h"
#include "core/LayerOps.h"

#include <QAbstractSpinBox>
#include <QAction>
#include <QCheckBox>
#include <QColor>
#include <QColorDialog>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHash>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QStatusBar>
#include <QVBoxLayout>

#include <exception>
#include <functional>

#if KA_HGIS_HAS_QGIS
#include <qgis.h>
#include <qgsfeature.h>
#include <qgsfeatureid.h>
#include <qgsfeaturerequest.h>
#include <qgsfields.h>
#include <qgsgeometry.h>
#include <qgslayertreeview.h>
#include <qgsmapcanvas.h>
#include <qgsmaplayer.h>
#include <qgsmaptoolpan.h>
#include <qgsproject.h>
#include <qgssnappingconfig.h>
#include <qgssnappingutils.h>
#include <qgsvectorlayer.h>
#endif

namespace {
void kaPaintColorButton(QPushButton* b, const QColor& c, const QString& suffix) {
  if (!b) return;
  b->setStyleSheet(KaTheme::colorSwatchStyle(c));
  b->setText(c.name(QColor::HexRgb).toUpper() + QStringLiteral("  ·  ") + suffix);
  b->setProperty("kaColor", c);
  b->setCursor(Qt::PointingHandCursor);
}

QPushButton* kaMakeColorButton(QWidget* parent, const QColor& c, const QString& suffix,
                               const QString& pickerTitle, const std::function<void()>& onChanged = {}) {
  auto* b = new QPushButton(parent);
  kaPaintColorButton(b, c.isValid() && c.alpha() > 0 ? c : QColor(22, 163, 74, 160), suffix);
  QObject::connect(b, &QPushButton::clicked, b, [b, suffix, pickerTitle, onChanged]() {
    QColorDialog picker(b->property("kaColor").value<QColor>(), b->window());
    picker.setOption(QColorDialog::DontUseNativeDialog, true);
    picker.setOption(QColorDialog::ShowAlphaChannel, true);
    picker.setWindowTitle(pickerTitle);
    if (picker.exec() != QDialog::Accepted) return;
    const QColor picked = picker.selectedColor();
    if (!picked.isValid()) return;
    kaPaintColorButton(b, picked, suffix);
    if (onChanged) onChanged();
  });
  return b;
}

QWidget* kaWrapLabeled(QWidget* parent, const QString& caption, QWidget* inner) {
  auto* box = new QWidget(parent);
  auto* v = new QVBoxLayout(box);
  v->setContentsMargins(0, 0, 0, 0);
  v->setSpacing(4);
  auto* lab = new QLabel(caption, box);
  v->addWidget(lab);
  v->addWidget(inner);
  return box;
}

QDoubleSpinBox* kaMakeArrowSpin(QWidget* parent, QWidget** rowOut, double minV, double maxV,
                                double step, int decimals, double value) {
  auto* row = new QWidget(parent);
  auto* h = new QHBoxLayout(row);
  h->setContentsMargins(0, 0, 0, 0);
  h->setSpacing(0);
  auto* spin = new QDoubleSpinBox(row);
  spin->setButtonSymbols(QAbstractSpinBox::UpDownArrows);
  spin->setRange(minV, maxV);
  spin->setSingleStep(step);
  spin->setDecimals(decimals);
  spin->setSuffix(QStringLiteral(" mm"));
  spin->setValue(value);
  spin->setMinimumHeight(29);
  spin->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
  h->addWidget(spin, 1);
  if (rowOut) *rowOut = row;
  return spin;
}

#if KA_HGIS_HAS_QGIS
bool isProjectSurveyDomainLayer(const QgsVectorLayer* vl, const QString& surveyGpkgPath) {
  if (!vl || !vl->isValid() || surveyGpkgPath.isEmpty()) return false;
  const QString key = LayerOps::layerKeyOf(vl);
  if (key.isEmpty() || key.startsWith(QLatin1String("user:"))) return false;
  const QString src = vl->source().split(QLatin1Char('|')).first();
  if (src.endsWith(QLatin1String(".shp"), Qt::CaseInsensitive)) return false;
  QFileInfo fiSrc(src);
  QFileInfo fiGpkg(surveyGpkgPath);
  if (fiSrc.absoluteFilePath().compare(fiGpkg.absoluteFilePath(), Qt::CaseInsensitive) != 0)
    return false;
  const QStringList domainKeys = {
    QStringLiteral("survey_area"), QStringLiteral("feature_poly"),
    QStringLiteral("feature_line"), QStringLiteral("section_line"),
    QStringLiteral("control_points"), QStringLiteral("trial_trench"),
    QStringLiteral("artifact_point")
  };
  return domainKeys.contains(key);
}
#endif
}  // namespace

void MainWindow::applySnapConfig() {
#if KA_HGIS_HAS_QGIS
  auto* project = QgsProject::instance();
  if (!project) return;
  LayerOps::SnapSettings settings = LayerOps::readSnapSettings(project);
  settings.enabled = m_snapEnabled;
  // 정합은 맞출 사진이 현재 레이어라 CurrentLayer면 지적 선에 안 붙는다.
  if (m_subToolsMode == QLatin1String("align"))
    settings.target = LayerOps::SnapTarget::SurveyLayers;
  LayerOps::applySnapSettings(project, settings);
  QgsSnappingConfig cfg = project->snappingConfig();
  cfg.setTypeFlag(Qgis::SnappingType::Vertex | Qgis::SnappingType::Segment);
  project->setSnappingConfig(cfg);
  if (m_canvas && m_canvas->snappingUtils())
    m_canvas->snappingUtils()->setConfig(cfg);
  if (m_alignLeftCanvas && m_alignLeftCanvas->snappingUtils())
    m_alignLeftCanvas->snappingUtils()->setConfig(cfg);
  if (m_captureTool)
    m_captureTool->setSnapEnabled(m_snapEnabled);
  if (m_measureTool)
    m_measureTool->setSnapEnabled(m_snapEnabled);
  if (m_featureSelectTool)
    m_featureSelectTool->setSnapEnabled(m_snapEnabled);
#endif
}

void MainWindow::startSelectTool() {
#if KA_HGIS_HAS_QGIS
  if (!m_canvas) return;
  if (m_captureTool && m_canvas->mapTool() == m_captureTool)
    stopCaptureTool();
  if (m_actMeasure)
    m_actMeasure->setChecked(false);

  if (!m_featureSelectTool) {
    m_featureSelectTool = new KaFeatureSelectTool(m_canvas);
    m_featureSelectTool->setParent(this);
    connect(m_featureSelectTool, &KaFeatureSelectTool::statusMessage, this,
            [this](const QString& t) { statusBar()->showMessage(t, 8000); });
    connect(m_featureSelectTool, &KaFeatureSelectTool::selectionChanged, this,
            [this](int count) {
              if (count == 2) {
                notify(Notice::Info, QStringLiteral("도형 2개 선택됨"),
                       QStringLiteral("상단의 [폴리곤 나누기]를 누르면 겹치는 구간을 자동으로 분할합니다."));
              }
            });
    connect(m_featureSelectTool, &KaFeatureSelectTool::requestMapContextMenu, this, &MainWindow::onMapContextMenu);
    connect(m_featureSelectTool, &KaFeatureSelectTool::requestMerge, this, &MainWindow::mergeFeaturePolygons);
    connect(m_featureSelectTool, &KaFeatureSelectTool::requestSplit, this, &MainWindow::startSplitPolygonTool);
    connect(m_featureSelectTool, &KaFeatureSelectTool::requestClip, this, &MainWindow::clipOverlappingLayers);
    connect(m_featureSelectTool, &KaFeatureSelectTool::featureGeometryEdited, this,
            [this](QgsVectorLayer* layer, const QgsFeature& before) {
      if (!layer || !before.isValid()) return;
      KaUndoAction action;
      action.type = KaUndoAction::FeatureChanged;
      action.layerId = layer->id();
      action.featureId = before.id();
      action.featureData = before;
      m_undoActions.append(action);
      QgsProject::instance()->setDirty(true);
      updateUndoRedoActions();
    });
  }

  if (m_canvas->mapTool() == m_featureSelectTool) {
    if (m_panTool) m_canvas->setMapTool(m_panTool);
    statusBar()->showMessage(QStringLiteral("도형 선택 종료"), 3000);
    return;
  }

  m_canvas->setMapTool(m_featureSelectTool);
  m_canvas->setFocus(Qt::OtherFocusReason);
  statusBar()->showMessage(
      QStringLiteral("도형선택 — 도형을 클릭하면 점이 나옵니다. 점 우클릭은 삭제, 선 우클릭은 점추가입니다."),
      10000);
#endif
}

void MainWindow::startVertexEditTool() {
#if KA_HGIS_HAS_QGIS
  if (!m_canvas) return;
  if (m_captureTool && m_canvas->mapTool() == m_captureTool)
    stopCaptureTool();
  if (m_actMeasure)
    m_actMeasure->setChecked(false);
  if (!m_vertexEditTool) {
    m_vertexEditTool = new KaVertexEditTool(m_canvas);
    m_vertexEditTool->setParent(this);
    connect(m_vertexEditTool, &KaVertexEditTool::statusMessage, this,
            [this](const QString& text) { statusBar()->showMessage(text, 8000); });
    connect(m_vertexEditTool, &KaVertexEditTool::featureGeometryEdited, this,
            [this](QgsVectorLayer* layer, const QgsFeature& before) {
              if (!layer || !before.isValid()) return;
              KaUndoAction action;
              action.type = KaUndoAction::FeatureChanged;
              action.layerId = layer->id();
              action.featureId = before.id();
              action.featureData = before;
              m_undoActions.append(action);
              QgsProject::instance()->setDirty(true);
              updateUndoRedoActions();
            });
  }
  m_vertexEditTool->setSnapEnabled(m_snapEnabled);
  if (m_canvas->mapTool() == m_vertexEditTool) {
    if (m_panTool) m_canvas->setMapTool(m_panTool);
    statusBar()->showMessage(QStringLiteral("도형 수정 종료"), 3000);
    return;
  }
  m_canvas->setMapTool(m_vertexEditTool);
  m_canvas->setFocus(Qt::OtherFocusReason);
  statusBar()->showMessage(
      QStringLiteral("도형 수정 — 도형을 클릭하면 점이 나옵니다. 점을 끌어 옮기고, 선 위에서 우클릭하면 점추가·점삭제입니다."),
      10000);
#endif
}

void MainWindow::startMeasureTool() {
#if KA_HGIS_HAS_QGIS
  if (!m_canvas) return;
  if (m_captureTool && m_canvas->mapTool() == m_captureTool)
    stopCaptureTool();
  showMapWorkspace();
  applySnapConfig();
  if (!m_measureTool) {
    m_measureTool = new KaMeasureMapTool(m_canvas);
    m_measureTool->setParent(this);
    connect(m_measureTool, &KaMeasureMapTool::statusMessage, this, [this](const QString& t) {
      statusBar()->showMessage(t, 0);
    });
  }
  m_measureTool->setSnapEnabled(m_snapEnabled);
  if (m_canvas->mapTool() == m_measureTool) {
    if (m_panTool) m_canvas->setMapTool(m_panTool);
    if (m_actMeasure) m_actMeasure->setChecked(false);
    statusBar()->showMessage(QStringLiteral("줄자 종료"), 3000);
    return;
  }
  m_canvas->setMapTool(m_measureTool);
  m_canvas->setFocus(Qt::OtherFocusReason);
  if (m_actMeasure)
    m_actMeasure->setChecked(true);
  statusBar()->showMessage(QStringLiteral("줄자: 점을 찍고 우클릭에서 마침을 고르세요. 면적은 면적만 나옵니다."), 0);
#endif
}

void MainWindow::startCoordPointTool() {
#if KA_HGIS_HAS_QGIS
  if (!m_canvas) return;
  showMapWorkspace();
  applySnapConfig();
  if (!m_coordPointTool) {
    m_coordPointTool = new KaCoordPointMapTool(m_canvas);
    m_coordPointTool->setParent(this);
    connect(m_coordPointTool, &KaCoordPointMapTool::statusMessage, this, [this](const QString& t) {
      statusBar()->showMessage(t, 8000);
    });
  }
  m_canvas->setMapTool(m_coordPointTool);
  m_canvas->setFocus(Qt::OtherFocusReason);
  statusBar()->showMessage(QStringLiteral("맵에서 꼭짓점을 찍으세요. Esc로 지웁니다."), 0);
#endif
}

void MainWindow::stopCaptureTool() {
  if (!m_canvas) return;
  if (m_captureTool && m_canvas->mapTool() == m_captureTool)
    m_canvas->unsetMapTool(m_captureTool);
  if (m_attributeTool && m_canvas->mapTool() == m_attributeTool)
    m_canvas->unsetMapTool(m_attributeTool);
  if (m_panTool)
    m_canvas->setMapTool(m_panTool);
  if (m_captureTool)
    m_captureTool->resetSession();
}

QString MainWindow::attributeFieldLabelKo(const QString& fieldName) {
  static const QHash<QString, QString> labels = {
      {QStringLiteral("survey_name"), QStringLiteral("조사명")},
      {QStringLiteral("site_name"), QStringLiteral("유적명")},
      {QStringLiteral("kind"), QStringLiteral("유구종류")},
      {QStringLiteral("period"), QStringLiteral("시대")},
      {QStringLiteral("feature_no"), QStringLiteral("유구번호")},
      {QStringLiteral("artifact_no"), QStringLiteral("유물번호")},
      {QStringLiteral("note"), QStringLiteral("비고")},
      {QStringLiteral("section_id"), QStringLiteral("단면번호")},
      {QStringLiteral("point_id"), QStringLiteral("점ID")},
      {QStringLiteral("x"), QStringLiteral("X")},
      {QStringLiteral("y"), QStringLiteral("Y")},
      {QStringLiteral("z"), QStringLiteral("표고 Z")},
      {QStringLiteral("datum"), QStringLiteral("측지기준계")},
      {QStringLiteral("ellipsoid"), QStringLiteral("타원체")},
      {QStringLiteral("projection"), QStringLiteral("투영")},
      {QStringLiteral("origin"), QStringLiteral("원점")},
      {QStringLiteral("accuracy"), QStringLiteral("정확도 메모")},
      {QStringLiteral("accuracy_m"), QStringLiteral("정확도(m)")},
      {QStringLiteral("pdop"), QStringLiteral("PDOP")},
      {QStringLiteral("fix_type"), QStringLiteral("수신상태")},
      {QStringLiteral("pixel_x"), QStringLiteral("픽셀 X")},
      {QStringLiteral("pixel_y"), QStringLiteral("픽셀 Y")},
  };
  return labels.value(fieldName, fieldName);
}

void MainWindow::ensureAttributeTool() {
#if KA_HGIS_HAS_QGIS
  if (!m_canvas) return;
  if (m_attributeTool) return;
  m_attributeTool = new KaAttributeMapTool(m_canvas);
  m_attributeTool->setParent(this);
  connect(m_attributeTool, &KaAttributeMapTool::featurePicked, this,
          [this](QgsVectorLayer* layer, const QgsFeature& feat) {
            editFeatureAttributes(layer, feat);
          });
  connect(m_attributeTool, &KaAttributeMapTool::pickCanceled, this, [this]() {
    if (m_panTool && m_canvas) m_canvas->setMapTool(m_panTool);
    statusBar()->showMessage(QStringLiteral("속성 편집 종료"), 3000);
  });
#endif
}

void MainWindow::editCurrentLayerAttributes(QgsMapLayer* targetLayer) {
#if KA_HGIS_HAS_QGIS
  auto* layer = qobject_cast<QgsVectorLayer*>(targetLayer);
  if (!layer && m_layerTree) {
    layer = qobject_cast<QgsVectorLayer*>(m_layerTree->currentLayer());
  }
  if (!layer || !layer->isValid()) {
    QMessageBox::information(this, QStringLiteral("속성"),
                             QStringLiteral("벡터 레이어(조사구역·유구 등)를 선택한 뒤 다시 실행하세요."));
    return;
  }
  if (layer->featureCount() <= 0) {
    QMessageBox::information(
        this, QStringLiteral("속성"),
        QStringLiteral("「%1」에 도형이 없습니다.\n먼저 그리기로 도형을 만든 뒤 속성을 입력하세요.")
            .arg(layer->name()));
    return;
  }

  QList<QgsFeature> feats;
  QgsFeatureIterator it = layer->getFeatures(QgsFeatureRequest().setFlags(Qgis::FeatureRequestFlag::NoGeometry));
  QgsFeature f;
  while (it.nextFeature(f))
    feats.append(f);

  if (feats.isEmpty()) {
    it = layer->getFeatures();
    while (it.nextFeature(f))
      feats.append(f);
  }
  if (feats.isEmpty()) {
    QMessageBox::information(this, QStringLiteral("속성"),
                             QStringLiteral("피처를 읽을 수 없습니다. 레이어 파일을 확인하세요."));
    return;
  }

  int pick = 0;
  if (feats.size() > 1) {
    QStringList labels;
    labels.reserve(feats.size());
    const QgsFields fields = layer->fields();
    const int kindIdx = fields.indexOf(QStringLiteral("kind"));
    const int nameIdx = fields.indexOf(QStringLiteral("survey_name"));
    const int noIdx = fields.indexOf(QStringLiteral("feature_no"));
    const int idIdx = fields.indexOf(QStringLiteral("point_id"));
    const int secIdx = fields.indexOf(QStringLiteral("section_id"));
    for (const QgsFeature& ft : feats) {
      QString label = QStringLiteral("#%1").arg(ft.id());
      auto take = [&](int idx) {
        if (idx < 0) return;
        const QVariant v = ft.attribute(idx);
        if (v.isValid() && !v.toString().trimmed().isEmpty())
          label += QStringLiteral("  ") + v.toString().trimmed();
      };
      take(noIdx);
      take(kindIdx);
      take(nameIdx);
      take(idIdx);
      take(secIdx);
      labels << label;
    }
    bool ok = false;
    const QString chosen = QInputDialog::getItem(
        this, QStringLiteral("도형 선택 — %1").arg(layer->name()),
        QStringLiteral("속성을 편집할 도형 (%1개):").arg(feats.size()),
        labels, 0, false, &ok);
    if (!ok) return;
    pick = labels.indexOf(chosen);
    if (pick < 0) pick = 0;
  }

  QgsFeature full;
  if (!layer->getFeatures(QgsFeatureRequest(feats.at(pick).id())).nextFeature(full))
    full = feats.at(pick);
  editFeatureAttributes(layer, full);
#else
  QMessageBox::information(this, QStringLiteral("속성"), QStringLiteral("QGIS 빌드 필요"));
#endif
}

void MainWindow::startAttributeEditTool() {
#if KA_HGIS_HAS_QGIS
  if (!m_canvas) return;
  if (m_captureTool && m_canvas->mapTool() == m_captureTool) {
    stopCaptureTool();
  }
  ensureAttributeTool();
  if (!m_attributeTool) return;
  if (m_canvas->mapTool() == m_attributeTool) {
    if (m_panTool) m_canvas->setMapTool(m_panTool);
    statusBar()->showMessage(QStringLiteral("속성 편집 종료"), 3000);
    return;
  }
  m_canvas->setMapTool(m_attributeTool);
  m_canvas->setFocus(Qt::OtherFocusReason);
  statusBar()->showMessage(
      QStringLiteral("속성 편집: 지도에서 도형을 좌클릭 · ESC=종료"), 0);
#else
  QMessageBox::information(this, QStringLiteral("속성"), QStringLiteral("QGIS 빌드 필요"));
#endif
}

void MainWindow::editCurrentLayerStyle(QgsMapLayer* targetLayer) {
#if KA_HGIS_HAS_QGIS
  auto* layer = qobject_cast<QgsVectorLayer*>(targetLayer);
  if (!layer && m_layerTree) {
    layer = qobject_cast<QgsVectorLayer*>(m_layerTree->currentLayer());
  }
  if (!layer || !layer->isValid()) {
    QMessageBox::information(this, QStringLiteral("모양"),
                             QStringLiteral("벡터 레이어를 선택한 뒤 다시 실행하세요."));
    return;
  }
  if (LayerOps::isCadastralLayer(layer) || LayerOps::isReferenceLayer(layer)) {
    QMessageBox::information(this, QStringLiteral("모양"),
                             QStringLiteral("지적도·배경 지도는 여기서 색을 바꾸지 않습니다.\n"
                                            "조사 데이터 레이어(유구·구역 등)를 선택하세요."));
    return;
  }

  QColor fill, stroke;
  double widthMm = 1.2;
  double markerMm = 3.5;
  bool noFill = false;
  bool noStroke = false;
  bool dashed = false;
  LayerOps::readSimpleVectorStyle(layer, &fill, &stroke, &widthMm, &markerMm, &noFill, &noStroke,
                                  &dashed);

  const Qgis::GeometryType gt = layer->geometryType();
  const bool isPoly = gt == Qgis::GeometryType::Polygon;
  const bool isLine = gt == Qgis::GeometryType::Line;
  const bool isPoint = gt == Qgis::GeometryType::Point;

  QDialog dlg(this);
  dlg.setObjectName(QStringLiteral("kaStyleDlg"));
  dlg.setWindowTitle(QStringLiteral("도형 색"));
  dlg.setWindowFlag(Qt::MSWindowsFixedSizeDialogHint, true);

  auto* root = new QVBoxLayout(&dlg);
  root->setSpacing(8);
  root->setContentsMargins(16, 14, 16, 12);
  root->setSizeConstraint(QLayout::SetFixedSize);

  auto* title = new QLabel(QStringLiteral("도형 색"), &dlg);
  auto* sub = new QLabel(layer->name(), &dlg);
  sub->setWordWrap(true);
  root->addWidget(title);
  root->addWidget(sub);

  QCheckBox* noFillCheck = nullptr;
  QWidget* fillBox = nullptr;
  QPushButton* fillBtn = nullptr;
  if (isPoly || isPoint) {
    noFillCheck = new QCheckBox(QStringLiteral("채우기 없음 (외곽선만)"), &dlg);
    noFillCheck->setChecked(noFill);
    root->addWidget(noFillCheck);
    fillBtn = kaMakeColorButton(&dlg, fill.alpha() == 0 ? QColor(22, 163, 74, 160) : fill,
                                QStringLiteral("클릭해서 색 고르기"), QStringLiteral("면 색"));
    fillBox = kaWrapLabeled(&dlg, QStringLiteral("면 색"), fillBtn);
    root->addWidget(fillBox);
  }

  auto* noStrokeCheck = new QCheckBox(QStringLiteral("외곽선 없음"), &dlg);
  noStrokeCheck->setChecked(noStroke);
  root->addWidget(noStrokeCheck);

  QCheckBox* dashCheck = nullptr;
  if (isLine || isPoly) {
    dashCheck = new QCheckBox(QStringLiteral("점선"), &dlg);
    dashCheck->setChecked(dashed);
    root->addWidget(dashCheck);
  }

  auto* strokeBtn = kaMakeColorButton(&dlg, stroke.alpha() == 0 ? QColor(21, 128, 61) : stroke,
                                      QStringLiteral("클릭해서 색 고르기"),
                                      isLine ? QStringLiteral("선 색") : QStringLiteral("외곽선 색"));
  auto* strokeBox = kaWrapLabeled(&dlg, isLine ? QStringLiteral("선 색") : QStringLiteral("외곽선 색"),
                                  strokeBtn);
  root->addWidget(strokeBox);

  QWidget* widthRow = nullptr;
  auto* widthSpin = kaMakeArrowSpin(&dlg, &widthRow, 0.2, 12.0, 0.2, 1, widthMm);
  auto* widthBox = kaWrapLabeled(&dlg, isLine ? QStringLiteral("선 굵기") : QStringLiteral("외곽선 굵기"),
                                 widthRow);
  root->addWidget(widthBox);

  QDoubleSpinBox* markerSpin = nullptr;
  if (isPoint) {
    QWidget* markerRow = nullptr;
    markerSpin = kaMakeArrowSpin(&dlg, &markerRow, 1.0, 20.0, 0.5, 1, markerMm);
    root->addWidget(kaWrapLabeled(&dlg, QStringLiteral("점 크기"), markerRow));
  }

  QCheckBox* catCheck = nullptr;
  if (LayerOps::layerKeyOf(layer) == QLatin1String("feature_poly")) {
    catCheck = new QCheckBox(QStringLiteral("종류별 자동 색"), &dlg);
    root->addWidget(catCheck);
  }

  auto applyLive = [this, layer, fillBtn, strokeBtn, noFillCheck, noStrokeCheck, dashCheck, widthSpin,
                    markerSpin, markerMm, catCheck]() {
    if (catCheck && catCheck->isChecked()) {
      LayerOps::applyFeaturePolyStyle(layer);
    } else {
      const QColor outFill = fillBtn ? fillBtn->property("kaColor").value<QColor>() : QColor();
      const QColor outStroke = strokeBtn->property("kaColor").value<QColor>();
      LayerOps::applySimpleVectorStyle(layer, outFill, outStroke, widthSpin->value(),
                                       markerSpin ? markerSpin->value() : markerMm,
                                       noFillCheck && noFillCheck->isChecked(),
                                       noStrokeCheck && noStrokeCheck->isChecked(),
                                       dashCheck && dashCheck->isChecked());
    }
    if (m_canvas) m_canvas->refresh();
    if (m_drawingStudio) m_drawingStudio->refreshMapFromProject();
  };

  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dlg);
  buttons->button(QDialogButtonBox::Save)->setText(QStringLiteral("적용"));
  buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("취소"));
  root->addWidget(buttons);
  connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

  auto syncVisible = [&dlg, fillBox, strokeBox, widthBox, noFillCheck, noStrokeCheck, dashCheck,
                      applyLive]() {
    const bool nf = noFillCheck && noFillCheck->isChecked();
    const bool ns = noStrokeCheck && noStrokeCheck->isChecked();
    if (fillBox) fillBox->setVisible(!nf);
    if (strokeBox) strokeBox->setVisible(!ns);
    if (widthBox) widthBox->setVisible(!ns);
    if (dashCheck) dashCheck->setVisible(!ns);
    dlg.adjustSize();
    applyLive();
  };
  if (noFillCheck) connect(noFillCheck, &QCheckBox::toggled, &dlg, syncVisible);
  connect(noStrokeCheck, &QCheckBox::toggled, &dlg, syncVisible);
  if (dashCheck)
    connect(dashCheck, &QCheckBox::toggled, &dlg, [applyLive](bool) { applyLive(); });
  connect(widthSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), &dlg,
          [applyLive](double) { applyLive(); });
  if (markerSpin)
    connect(markerSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), &dlg,
            [applyLive](double) { applyLive(); });
  if (catCheck)
    connect(catCheck, &QCheckBox::toggled, &dlg, [applyLive](bool) { applyLive(); });
  if (fillBtn)
    connect(fillBtn, &QPushButton::clicked, &dlg, [applyLive]() { applyLive(); });
  connect(strokeBtn, &QPushButton::clicked, &dlg, [applyLive]() { applyLive(); });
  syncVisible();

  if (dlg.exec() != QDialog::Accepted) {
    LayerOps::applySimpleVectorStyle(layer, fill, stroke, widthMm, markerMm, noFill, noStroke, dashed);
    if (m_canvas) m_canvas->refresh();
    if (m_drawingStudio) m_drawingStudio->refreshMapFromProject();
    return;
  }
  applyLive();
  statusBar()->showMessage(QStringLiteral("면·선 색 적용: %1").arg(layer->name()), 5000);
#else
  QMessageBox::information(this, QStringLiteral("모양"), QStringLiteral("QGIS 빌드 필요"));
#endif
}

void MainWindow::editAttributesAtCanvasPos(const QPoint& canvasPos) {
#if KA_HGIS_HAS_QGIS
  if (!m_canvas) return;
  ensureAttributeTool();
  if (!m_attributeTool) return;
  QgsVectorLayer* layer = nullptr;
  QgsFeature feat;
  if (!m_attributeTool->pickAtScreen(canvasPos, &layer, &feat) || !layer) {
    QMessageBox::information(this, QStringLiteral("속성"),
                             QStringLiteral("이 위치에 도형이 없습니다.\n"
                                            "유구·조사구역 등을 그린 뒤 다시 클릭하세요."));
    return;
  }
  editFeatureAttributes(layer, feat);
#else
  Q_UNUSED(canvasPos);
#endif
}

void MainWindow::editFeatureAttributes(QgsVectorLayer* layer, const QgsFeature& feature) {
#if KA_HGIS_HAS_QGIS
  if (!layer || !layer->isValid() || !feature.isValid()) return;

  QgsFeature feat = feature;
  if (!layer->getFeatures(QgsFeatureRequest(feat.id())).nextFeature(feat)) {
    QMessageBox::warning(this, QStringLiteral("속성"), QStringLiteral("피처를 다시 읽을 수 없습니다."));
    return;
  }

  QDialog dlg(this);
  dlg.setWindowTitle(QStringLiteral("도형 속성 — %1").arg(layer->name()));
  dlg.setMinimumWidth(420);
  auto* form = new QFormLayout(&dlg);
  form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
  auto* tip = new QLabel(
      QStringLiteral("그린 도형의 속성입니다. 종류·시대 등을 입력한 뒤 저장하세요."), &dlg);
  tip->setWordWrap(true);
  form->addRow(tip);

  struct Row {
    int index = -1;
    QString name;
    QWidget* editor = nullptr;
    QMetaType::Type type = QMetaType::QString;
  };
  QVector<Row> rows;
  const QgsFields fields = layer->fields();
  for (int i = 0; i < fields.count(); ++i) {
    const QgsField f = fields.at(i);
    const QString name = f.name();
    if (name.compare(QLatin1String("fid"), Qt::CaseInsensitive) == 0) continue;
    if (name.startsWith(QLatin1String("ogc_"), Qt::CaseInsensitive)) continue;

    Row row;
    row.index = i;
    row.name = name;
    row.type = static_cast<QMetaType::Type>(f.type());

    const QVariant cur = feat.attribute(i);
    if (row.type == QMetaType::Double || row.type == QMetaType::Float ||
        row.type == QMetaType::Int || row.type == QMetaType::LongLong) {
      auto* edit = new QLineEdit(&dlg);
      if (cur.isValid() && !cur.isNull())
        edit->setText(cur.toString());
      row.editor = edit;
    } else {
      auto* edit = new QLineEdit(&dlg);
      edit->setText(cur.toString());
      if (name == QLatin1String("kind"))
        edit->setPlaceholderText(QStringLiteral("예: 주거지, 수혈, 구"));
      else if (name == QLatin1String("period"))
        edit->setPlaceholderText(QStringLiteral("예: 청동기, 원삼국"));
      else if (name == QLatin1String("feature_no"))
        edit->setPlaceholderText(QStringLiteral("예: 1호"));
      row.editor = edit;
    }
    form->addRow(attributeFieldLabelKo(name), row.editor);
    rows.push_back(row);
  }

  if (rows.isEmpty()) {
    QMessageBox::information(this, QStringLiteral("속성"),
                             QStringLiteral("이 레이어에 편집할 속성 필드가 없습니다."));
    return;
  }

  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dlg);
  buttons->button(QDialogButtonBox::Save)->setText(QStringLiteral("저장"));
  form->addRow(buttons);
  connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
  if (dlg.exec() != QDialog::Accepted) return;

  const bool wasEditable = layer->isEditable();
  if (!wasEditable && !layer->startEditing()) {
    QString detail = layer->dataProvider() ? layer->dataProvider()->error().message() : QString();
    KaUserError::warn(this, {
        QStringLiteral("속성"),
        QStringLiteral("속성 편집 모드를 열지 못했습니다."),
        QStringLiteral("%1\n%2").arg(layer->name(),
                                     detail.isEmpty() ? QStringLiteral("레이어가 잠겨 있거나 다른 프로그램에서 열려 있을 수 있습니다.")
                                                      : detail),
        QStringLiteral("다른 프로그램에서 같은 파일을 닫은 뒤 다시 저장하세요."),
    });
    return;
  }

  layer->beginEditCommand(QStringLiteral("속성 편집"));
  bool ok = true;
  for (const Row& row : rows) {
    auto* edit = qobject_cast<QLineEdit*>(row.editor);
    if (!edit) continue;
    const QString text = edit->text().trimmed();
    QVariant value;
    if (text.isEmpty()) {
      value = QVariant(QString());
    } else if (row.type == QMetaType::Double || row.type == QMetaType::Float) {
      bool conv = false;
      value = text.toDouble(&conv);
      if (!conv) {
        QMessageBox::warning(this, QStringLiteral("속성"),
                             QStringLiteral("숫자 형식이 아닙니다: %1").arg(attributeFieldLabelKo(row.name)));
        ok = false;
        break;
      }
    } else if (row.type == QMetaType::Int || row.type == QMetaType::LongLong) {
      bool conv = false;
      value = text.toLongLong(&conv);
      if (!conv) {
        QMessageBox::warning(this, QStringLiteral("속성"),
                             QStringLiteral("정수 형식이 아닙니다: %1").arg(attributeFieldLabelKo(row.name)));
        ok = false;
        break;
      }
    } else {
      value = text;
    }
    if (!layer->changeAttributeValue(feat.id(), row.index, value)) {
      ok = false;
      break;
    }
  }

  if (!ok) {
    layer->destroyEditCommand();
    if (!wasEditable) layer->rollBack();
    return;
  }
  layer->endEditCommand();

  KaUndoAction undo;
  undo.type = KaUndoAction::AttributesChanged;
  undo.layerId = layer->id();
  undo.featureId = feat.id();
  undo.featureData = feat;
  m_undoActions.append(undo);
  if (!wasEditable) {
    if (!layer->commitChanges()) {
      KaUserError::warn(this, {
          QStringLiteral("속성"),
          QStringLiteral("바꾼 속성을 저장하지 못했습니다."),
          layer->commitErrors().join(QLatin1Char('\n')),
          QStringLiteral("파일이 다른 곳에서 열려 있지 않은지 확인한 뒤 다시 저장하세요."),
      });
      return;
    }
  }

  LayerOps::applyDomainDrawStyle(layer, LayerOps::layerKeyOf(layer));
  layer->triggerRepaint();
  if (m_canvas) {
    if (m_canvas->isCachingEnabled())
      layer->triggerRepaint();
    m_canvas->refresh();
  }
  statusBar()->showMessage(QStringLiteral("속성 저장: %1 (#%2)").arg(layer->name()).arg(feat.id()), 5000);
#else
  Q_UNUSED(layer);
  Q_UNUSED(feature);
#endif
}

#if KA_HGIS_HAS_QGIS
void MainWindow::onGeometryCaptured(const QgsGeometry& geom) {
  try {
    QgsVectorLayer* layer = m_editLayer;
    if (!layer || !layer->isValid()) {
      statusBar()->showMessage(QStringLiteral("편집 레이어 없음 — 그리기 도구를 다시 선택하세요"), 5000);
      return;
    }
    if (geom.isEmpty() || geom.isNull()) {
      statusBar()->showMessage(QStringLiteral("빈 도형 (면≥3점, 선≥2점) — 이어서 그리세요"), 5000);
      return;
    }

    if (m_isSplittingPolygon) {
      m_isSplittingPolygon = false;
      const QVector<QgsPointXY> pts = geom.asPolyline();
      if (pts.size() < 2) {
        statusBar()->showMessage(QStringLiteral("분할선은 2점 이상이어야 합니다."), 5000);
        return;
      }
      QString err;
      if (!LayerOps::splitPolygonWithLine(layer, pts, &err)) {
        QMessageBox::warning(this, QStringLiteral("폴리곤 나누기 실패"), err);
        return;
      }
      if (m_canvas) m_canvas->refresh();
      statusBar()->showMessage(QStringLiteral("폴리곤을 성공적으로 나누었습니다."), 6000);
      notify(Notice::Success, QStringLiteral("폴리곤 나누기"),
             QStringLiteral("분할선을 기준으로 폴리곤을 나누었습니다."));
      return;
    }
    QgsFeature feat(layer->fields());
    feat.setGeometry(geom);
    if (LayerOps::layerKeyOf(layer) == QLatin1String("paleo_landform")) {
      const int kindIdx = layer->fields().indexOf(QStringLiteral("kind"));
      const int statusIdx = layer->fields().indexOf(QStringLiteral("status"));
      if (kindIdx >= 0) feat.setAttribute(kindIdx, QStringLiteral("미분류"));
      if (statusIdx >= 0) feat.setAttribute(statusIdx, QStringLiteral("가설"));
    }
    QString addError;
    if (!LayerOps::runEditCommand(layer, QStringLiteral("도형 그리기"), [&]() {
          return layer->addFeature(feat);
        }, &addError)) {
      QMessageBox::warning(this, QStringLiteral("오류"), addError);
      return;
    }
    QgsProject::instance()->setDirty(true);

    if (KaFeatureFormDialog::canOffer(layer)) {
      KaFeatureFormDialog form(layer, this);
      if (form.exec() == QDialog::Accepted) {
        QString formError;
        if (!LayerOps::runEditCommand(layer, QStringLiteral("이름·번호"), [&]() {
              return LayerOps::applyFeatureFormValues(layer, static_cast<qint64>(feat.id()),
                                                      form.nameText(), form.numberText(), nullptr);
            }, &formError)) {
          QMessageBox::warning(this, QStringLiteral("속성"), formError);
        }
      }
    }

    const QString drawnKey = LayerOps::layerKeyOf(layer);
    const bool domain = drawnKey == QLatin1String("survey_area")
                        || drawnKey == QLatin1String("feature_poly")
                        || drawnKey == QLatin1String("feature_line")
                        || drawnKey == QLatin1String("section_line")
                        || drawnKey == QLatin1String("control_points")
                        || drawnKey == QLatin1String("artifact_point");
    if (domain)
      LayerOps::applyDomainDrawStyle(layer, drawnKey);
    if (layer->geometryType() == Qgis::GeometryType::Polygon
        && (domain || drawnKey.startsWith(QLatin1String("user_poly"))))
      LayerOps::applyAreaM2Labels(layer);
    layer->updateExtents();
    layer->triggerRepaint();
    if (m_terrain3dStudio && m_terrain3dStudio->hasScene()) {
      if (m_terrain3dLayoutStudio)
        refreshTerrain3dDrapeAndSheet();
      else
        m_terrain3dStudio->refreshDrape();
    }
    if (m_canvas) {
      m_canvas->freeze(false);
      m_canvas->setRenderFlag(true);
      m_canvas->refresh();
    }
    if (m_captureTool && m_canvas && m_canvas->mapTool() != m_captureTool) {
      m_canvas->setMapTool(m_captureTool);
      m_canvas->setFocus(Qt::OtherFocusReason);
    }

    const long long n = static_cast<long long>(layer->featureCount());
    statusBar()->showMessage(
        QStringLiteral("도형을 넣었습니다 (%1, %2개). Ctrl+Z로 되돌리기 · 조사 저장으로 파일에 씁니다")
            .arg(layer->name())
            .arg(n),
        8000);
    refreshWorkPanel();
  } catch (const std::exception& ex) {
    QMessageBox::critical(this, QStringLiteral("그리기 오류"), QString::fromUtf8(ex.what()));
  } catch (...) {
    KaCrashGuard::logLine(QStringLiteral("[except] app/MainWindow.cpp:5391"));
    QMessageBox::critical(this, QStringLiteral("그리기 오류"), QStringLiteral("알 수 없는 오류"));
  }
}

void MainWindow::beginEdit(QgsVectorLayer* layer) {
  try {
    if (!layer || !layer->isValid()) {
      KaUserError::warn(this, {
          QStringLiteral("알림"),
          QStringLiteral("그릴 조사 레이어가 없습니다."),
          QStringLiteral("아직 새 조사를 만들지 않았거나 조사 파일이 열려 있지 않습니다."),
          QStringLiteral("먼저 「새 조사」로 프로젝트를 만든 뒤 다시 그리세요."),
      });
      return;
    }
    if (!m_canvas) return;
    if (m_subToolsMode == QLatin1String("align")) stopAlignSession();

    m_canvas->freeze(false);
    m_canvas->setRenderFlag(true);

    if (!layer->isEditable()) {
      if (!layer->startEditing()) {
        QString detail = layer->dataProvider() ? layer->dataProvider()->error().message() : QString();
        KaUserError::warn(this, {
            QStringLiteral("편집"),
            QStringLiteral("편집 모드를 열지 못했습니다."),
            QStringLiteral("%1\n%2")
                .arg(layer->name(),
                     detail.isEmpty() ? QStringLiteral("GPKG가 다른 프로그램에서 열려 있는지 확인")
                                      : detail),
            QStringLiteral("같은 파일을 연 다른 프로그램을 닫은 뒤 다시 그리세요."),
        });
        return;
      }
    }

    m_editLayer = layer;
    if (m_layerTree)
      m_layerTree->setCurrentLayer(layer);
    // 조사구역 ↔ 유구면처럼 같은 캡처 도구로 대상만 바뀌면 mapToolSet 이 오지 않는다.
    updateSubToolbarChecks();

    applySnapConfig();

    KaCaptureMapTool::Mode mode = KaCaptureMapTool::Mode::Polygon;
    const Qgis::GeometryType gt = layer->geometryType();
    if (gt == Qgis::GeometryType::Line) mode = KaCaptureMapTool::Mode::Line;
    else if (gt == Qgis::GeometryType::Point) mode = KaCaptureMapTool::Mode::Point;
    else if (gt == Qgis::GeometryType::Null || gt == Qgis::GeometryType::Unknown) {
      QMessageBox::warning(this, QStringLiteral("편집"),
                           QStringLiteral("이 레이어 지오메트리 타입을 알 수 없습니다: %1").arg(layer->name()));
      return;
    }

    if (!m_captureTool) {
      m_captureTool = new KaCaptureMapTool(m_canvas);
      m_captureTool->setParent(this);
      connect(m_captureTool, &KaCaptureMapTool::geometryCaptured, this, &MainWindow::onGeometryCaptured,
              Qt::DirectConnection);
      connect(m_captureTool, &KaCaptureMapTool::vertexMoved, this, [this]() {
        QgsProject::instance()->setDirty(true);
        if (m_canvas) m_canvas->refresh();
        statusBar()->showMessage(QStringLiteral("꼭짓점을 고쳤습니다. 끌어서 계속 수정하세요."), 5000);
      });
      connect(m_captureTool, &KaCaptureMapTool::vertexMoveFailed, this, [this](const QString& message) {
        QgsProject::instance()->setDirty(true);
        notify(Notice::Warning, QStringLiteral("꼭짓점 수정 확인 필요"), message);
      });
      connect(m_captureTool, &KaCaptureMapTool::captureCanceled, this, [this]() {
        statusBar()->showMessage(
            QStringLiteral("아직 저장 안 됨 — 면은 점 3개 이상, 선은 2개 이상 필요. 우클릭으로 완료."),
            8000);
      });
    }

    m_captureTool->setTargetLayer(layer);
    m_captureTool->setMode(mode);
    m_captureTool->setEasyDraw(false);
    m_canvas->setMapTool(m_captureTool);
    m_canvas->setFocus(Qt::OtherFocusReason);
    m_canvas->setCursor(Qt::CrossCursor);

    const QString how = (mode == KaCaptureMapTool::Mode::Point)
                            ? QStringLiteral("지도 좌클릭 = 점")
                            : QStringLiteral("좌클릭=꼭짓점 / 우클릭=완료 / 그린 뒤 점을 끌어 수정 / ESC=취소");
    statusBar()->showMessage(QStringLiteral("그리기 중: %1 | %2").arg(layer->name(), how), 0);
  } catch (const std::exception& ex) {
    QMessageBox::critical(this, QStringLiteral("그리기 시작 실패"), QString::fromUtf8(ex.what()));
  } catch (...) {
    KaCrashGuard::logLine(QStringLiteral("[except] app/MainWindow.cpp:5473"));
    QMessageBox::critical(this, QStringLiteral("그리기 시작 실패"), QStringLiteral("내부 오류"));
  }
}
#endif

void MainWindow::startEasyDraw() {
#if KA_HGIS_HAS_QGIS
  m_snapEnabled = true;
  applySnapConfig();
  if (m_surveyPath.isEmpty()) {
    QMessageBox::information(this, QStringLiteral("쉽게그리기"),
                             QStringLiteral("먼저 「새 조사」로 저장 위치를 만드세요."));
    return;
  }
  QgsVectorLayer* layer = nullptr;
  if (QgsProject* proj = QgsProject::instance()) {
    for (QgsMapLayer* l : proj->mapLayers()) {
      auto* vl = qobject_cast<QgsVectorLayer*>(l);
      if (!vl || !vl->isValid()) continue;
      if (vl->name() == QLatin1String("쉽게그리기")
          && LayerOps::layerKeyOf(vl).startsWith(QLatin1String("user_poly"))) {
        layer = vl;
        break;
      }
    }
  }
  if (!layer) {
    QString err;
    layer = LayerOps::createUserPolygonLayer(QgsProject::instance(), m_surveyPath,
                                             QStringLiteral("쉽게그리기"), m_workCrs, &err);
    if (!layer) {
      QMessageBox::warning(this, QStringLiteral("쉽게그리기"), err);
      return;
    }
    LayerOps::applySimpleVectorStyle(layer, QColor(30, 103, 198, 70), QColor(30, 103, 198), 1.4, 3.5,
                                     false, false);
    LayerOps::applyAreaM2Labels(layer);
  }
  if (m_layerTree) m_layerTree->setCurrentLayer(layer);
  beginEdit(layer);
  if (m_captureTool) {
    m_captureTool->setEasyDraw(true);
    m_captureTool->setSnapEnabled(true);
  }
  statusBar()->showMessage(
      QStringLiteral("쉽게그리기 → 「쉽게그리기」레이어에 저장. 지적은 자석만 사용. 우클릭=완료"),
      0);
#else
  statusBar()->showMessage(QStringLiteral("쉽게그리기 (스텁)"));
#endif
}

void MainWindow::startEditSurveyArea() {
#if KA_HGIS_HAS_QGIS
  if (m_surveyPath.isEmpty()) {
    const auto ans = QMessageBox::question(
        this, QStringLiteral("레이어"),
        QStringLiteral("먼저 「새 조사」로 저장 경로를 만드세요.\n\n지금 「새 조사」를 만들까요?"),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
    if (ans == QMessageBox::Yes)
      newSurvey();
    if (m_surveyPath.isEmpty()) return;
  }

  KaSurveyAreaDialog dlg(this, QgsProject::instance(), m_surveyPath);
  if (dlg.exec() != QDialog::Accepted) {
    return;
  }

  QgsVectorLayer* targetLayer = nullptr;
  if (!dlg.isNewLayer()) {
    targetLayer = dlg.selectedExistingLayer();
  }

  if (!targetLayer) {
    QString err;
    targetLayer = LayerOps::createSurveyAreaLayer(
        QgsProject::instance(), m_surveyPath, dlg.layerName(),
        dlg.strokeColor(), dlg.fillColor(), dlg.strokeWidthMm(), &err);
    if (!targetLayer) {
      QMessageBox::critical(this, QStringLiteral("조사구역 생성 실패"),
                            err.isEmpty() ? QStringLiteral("레이어를 생성하지 못했습니다.") : err);
      return;
    }
  }

  if (m_canvas && !QgsProject::instance()->mapLayer(targetLayer->id())) {
    LayerOps::syncMapCanvas(QgsProject::instance(), m_canvas, false);
  }
  if (m_layerTree)
    m_layerTree->setCurrentLayer(targetLayer);

  statusBar()->showMessage(
      QStringLiteral("조사구역 [%1] 그리기 시작 — 점을 찍고 Enter로 완성").arg(targetLayer->name()), 8000);
  beginEdit(targetLayer);
#else
  m_stubSurveyArea++;
  statusBar()->showMessage(QStringLiteral("스텁: 조사구역 폴리곤 %1개").arg(m_stubSurveyArea));
#endif
}

void MainWindow::startEditFeaturePoly() {
#if KA_HGIS_HAS_QGIS
  QgsVectorLayer* cur =
      m_layerTree ? qobject_cast<QgsVectorLayer*>(m_layerTree->currentLayer()) : nullptr;
  QgsVectorLayer* target =
      LayerOps::digitizeTargetLayer(QgsProject::instance(), cur, QStringLiteral("feature_poly"));
  if (!target)
    target = ensureDomainLayerForEdit(QStringLiteral("feature_poly"), QStringLiteral("유구면"));
  beginEdit(target);
#else
  m_stubFeatures++;
  statusBar()->showMessage(QStringLiteral("스텁: 유구 %1").arg(m_stubFeatures));
#endif
}

void MainWindow::startEditFeatureLine() {
#if KA_HGIS_HAS_QGIS
  beginEdit(ensureDomainLayerForEdit(QStringLiteral("feature_line"), QStringLiteral("유구선")));
#else
  m_stubFeatures++;
  statusBar()->showMessage(QStringLiteral("스텁: 선 %1").arg(m_stubFeatures));
#endif
}

void MainWindow::startEditSectionLine() {
#if KA_HGIS_HAS_QGIS
  beginEdit(ensureDomainLayerForEdit(QStringLiteral("section_line"), QStringLiteral("단면선")));
#else
  statusBar()->showMessage(QStringLiteral("스텁: 단면선"), 3000);
#endif
}

void MainWindow::startEditArtifact() {
#if KA_HGIS_HAS_QGIS
  beginEdit(ensureDomainLayerForEdit(QStringLiteral("artifact_point"), QStringLiteral("유물")));
#else
  statusBar()->showMessage(QStringLiteral("스텁: 유물"), 3000);
#endif
}

void MainWindow::startSplitPolygonTool() {
#if KA_HGIS_HAS_QGIS
  auto selected = KaFeatureSelectTool::allSelectedFeatures(m_canvas);

  // 1. 도형 2개가 선택된 경우: 겹치는 곳을 잘라서 나누기 (A\B, B\A, A∩B)
  if (selected.size() == 2) {
    auto* l1 = selected[0].layer.data();
    auto* l2 = selected[1].layer.data();
    if (l1 && l2) {
      QString err;
      qint64 createdFid = -1;
      QgsVectorLayer* targetLayer = nullptr;
      if (!LayerOps::splitTwoOverlappingFeatures(l1, selected[0].fid, l2, selected[1].fid,
                                                 &createdFid, &targetLayer, &err)) {
        QMessageBox::warning(this, QStringLiteral("폴리곤 중첩 분할 실패"), err);
        return;
      }
      if (createdFid >= 0 && targetLayer) {
        KaUndoAction act;
        act.type = KaUndoAction::FeatureAdded;
        act.layerId = targetLayer->id();
        act.featureId = createdFid;
        act.description = QStringLiteral("중첩 분할 도형 생성");
        m_undoActions.append(act);
      }
      if (m_canvas) m_canvas->refresh();
      statusBar()->showMessage(QStringLiteral("선택한 두 도형의 겹치는 구간을 잘라 분할했습니다! (A, B, 중첩부 3개로 분할됨)"), 8000);
      notify(Notice::Success, QStringLiteral("폴리곤 중첩 분할 완료"),
             QStringLiteral("선택된 두 도형의 겹치는 경계를 따라 분할하고 겹친 구간을 독립 폴리곤으로 생성했습니다."));
      return;
    }
  }

  // 2. 그룹으로 묶여있는 폴리곤(멀티폴리곤) 나누기
  if (selected.size() >= 1) {
    auto* l = selected[0].layer.data();
    if (l && l->isValid() && l->geometryType() == Qgis::GeometryType::Polygon) {
      QgsFeature f;
      if (l->getFeatures(QgsFeatureRequest(selected[0].fid)).nextFeature(f) && f.hasGeometry()) {
        if (f.geometry().isMultipart()) {
          QString err;
          QgsFeatureIds fids;
          for (const auto& item : selected) {
            if (item.layer == l) fids.insert(item.fid);
          }
          if (LayerOps::explodeMultipartFeatures(l, fids, &err)) {
            if (m_canvas) m_canvas->refresh();
            statusBar()->showMessage(QStringLiteral("그룹으로 묶여 있던 폴리곤을 개별 폴리곤들로 분리했습니다!"), 8000);
            notify(Notice::Success, QStringLiteral("폴리곤 그룹 분리 완료"),
                   QStringLiteral("하나로 묶여 있던 멀티폴리곤을 개별 단일 폴리곤들로 정상 분리했습니다."));
            return;
          }
        }
      }
    }
  }

  // 3. 단일 폴리곤인 경우: 분할선 그리기 모드로 전환하여 선으로 자르기
  QgsVectorLayer* cur = m_layerTree ? qobject_cast<QgsVectorLayer*>(m_layerTree->currentLayer()) : nullptr;
  if (!selected.isEmpty() && selected[0].layer) {
    cur = selected[0].layer.data();
  }
  if (!cur || !cur->isValid() || cur->geometryType() != Qgis::GeometryType::Polygon) {
    QMessageBox::information(this, QStringLiteral("폴리곤 나누기"),
                             QStringLiteral("도형을 Shift+클릭으로 선택하거나, 나눌 폴리곤 레이어를 좌측 목록에서 선택해 주세요."));
    return;
  }
  m_isSplittingPolygon = true;
  beginEdit(cur);
  if (m_captureTool) {
    m_captureTool->setMode(KaCaptureMapTool::Mode::Line);
  }
  statusBar()->showMessage(
      QStringLiteral("폴리곤 나누기 모드 — 폴리곤을 가로지르는 선을 클릭하여 그리고 우클릭으로 분할 (또는 Shift로 2개 도형 선택 후 실행)."),
      12000);
#else
  statusBar()->showMessage(QStringLiteral("스텁: 폴리곤 나누기"), 3000);
#endif
}

void MainWindow::clipOverlappingLayers() {
#if KA_HGIS_HAS_QGIS
  QgsProject* proj = QgsProject::instance();
  if (!proj) return;

  QList<QgsVectorLayer*> allVecs;
  QList<QgsVectorLayer*> polyVecs;
  for (QgsMapLayer* l : proj->mapLayers()) {
    if (auto* v = qobject_cast<QgsVectorLayer*>(l)) {
      if (!v->isValid()) continue;
      allVecs.append(v);
      if (v->geometryType() == Qgis::GeometryType::Polygon) {
        polyVecs.append(v);
      }
    }
  }

  if (allVecs.size() < 2 || polyVecs.isEmpty()) {
    QMessageBox::information(this, QStringLiteral("구간 분리 (클립)"),
                             QStringLiteral("구간을 분리하려면 최소 1개의 폴리곤(바운더리) 레이어와 대상 레이어가 필요합니다."));
    return;
  }

  QDialog dlg(this);
  dlg.setWindowTitle(QStringLiteral("겹치는 구간 분리 (클립)"));
  dlg.resize(420, 200);
  auto* layout = new QVBoxLayout(&dlg);

  auto* infoLab = new QLabel(QStringLiteral("기준 바운더리 레이어와 겹치는 구간만 잘라내어 새 레이어로 분리합니다."), &dlg);
  infoLab->setWordWrap(true);
  layout->addWidget(infoLab);

  auto* form = new QFormLayout();
  auto* targetCombo = new QComboBox(&dlg);
  auto* boundaryCombo = new QComboBox(&dlg);

  QgsVectorLayer* currentLayer = m_layerTree ? qobject_cast<QgsVectorLayer*>(m_layerTree->currentLayer()) : nullptr;

  int targetIdx = 0;
  for (int i = 0; i < allVecs.size(); ++i) {
    targetCombo->addItem(allVecs[i]->name(), QVariant::fromValue(static_cast<void*>(allVecs[i])));
    if (currentLayer && allVecs[i] == currentLayer) targetIdx = i;
  }
  targetCombo->setCurrentIndex(targetIdx);

  int boundaryIdx = 0;
  for (int i = 0; i < polyVecs.size(); ++i) {
    boundaryCombo->addItem(polyVecs[i]->name(), QVariant::fromValue(static_cast<void*>(polyVecs[i])));
    const QString n = polyVecs[i]->name().toLower();
    if (n.contains(QStringLiteral("바운더리")) || n.contains(QStringLiteral("구역")) || n.contains(QStringLiteral("허가"))) {
      boundaryIdx = i;
    }
  }
  boundaryCombo->setCurrentIndex(boundaryIdx);

  form->addRow(QStringLiteral("자를 대상 레이어:"), targetCombo);
  form->addRow(QStringLiteral("기준 바운더리:"), boundaryCombo);
  layout->addLayout(form);

  auto* btnBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
  btnBox->button(QDialogButtonBox::Ok)->setText(QStringLiteral("분리 실행"));
  btnBox->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("취소"));
  connect(btnBox, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
  connect(btnBox, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
  layout->addWidget(btnBox);

  if (dlg.exec() != QDialog::Accepted) return;

  auto* target = static_cast<QgsVectorLayer*>(targetCombo->currentData().value<void*>());
  auto* boundary = static_cast<QgsVectorLayer*>(boundaryCombo->currentData().value<void*>());

  if (!target || !boundary) return;
  if (target == boundary) {
    QMessageBox::warning(this, QStringLiteral("구간 분리"), QStringLiteral("대상 레이어와 기준 바운더리 레이어가 같을 수 없습니다."));
    return;
  }

  QString err;
  QgsVectorLayer* clipped = LayerOps::clipLayerByBoundary(target, boundary, proj, &err);
  if (!clipped) {
    QMessageBox::warning(this, QStringLiteral("구간 분리 실패"), err);
    return;
  }

  if (m_canvas) m_canvas->refresh();
  KaUndoAction act;
  act.type = KaUndoAction::LayerAdded;
  act.layerId = clipped->id();
  act.description = QStringLiteral("구간 분리 레이어 생성");
  m_undoActions.append(act);

  statusBar()->showMessage(QStringLiteral("겹치는 구간을 분리하여 「%1」 레이어를 생성했습니다. (Ctrl+Z로 되돌리기 가능)").arg(clipped->name()), 8000);
  notify(Notice::Success, QStringLiteral("구간 분리 완료"),
         QStringLiteral("「%1」 레이어가 성공적으로 생성되었습니다. (Ctrl+Z로 되돌리기 가능)").arg(clipped->name()),
         QStringLiteral("기준: %1 | 대상: %2").arg(boundary->name(), target->name()));
#else
  statusBar()->showMessage(QStringLiteral("스텁: 구간 분리"), 3000);
#endif
}

void MainWindow::saveEdits() {
#if KA_HGIS_HAS_QGIS
  int n = 0;
  for (auto* l : QgsProject::instance()->mapLayers()) {
    if (auto* v = qobject_cast<QgsVectorLayer*>(l)) {
      if (v->isEditable()) {
        if (v->commitChanges()) ++n;
        else {
          QMessageBox::warning(this, QStringLiteral("저장 실패"),
                               QStringLiteral("%1: %2").arg(v->name(), v->commitErrors().join(QStringLiteral("; "))));
        }
      }
    }
  }
  if (m_canvas) m_canvas->refresh();
  statusBar()->showMessage(QStringLiteral("편집저장 완료 (%1개 레이어)").arg(n), 5000);
  refreshWorkPanel();
#else
  statusBar()->showMessage(QStringLiteral("스텁 저장"), 3000);
#endif
}

void MainWindow::stopEdits() {
#if KA_HGIS_HAS_QGIS
  stopCaptureTool();
  m_editLayer = nullptr;
#endif
  statusBar()->showMessage(QStringLiteral("그리기 종료. 미커밋은 「편집저장」"), 5000);
}

void MainWindow::addControlPoint() {
  QDialog dlg(this);
  dlg.setWindowTitle(QStringLiteral("GPS 기준점"));
  auto* form = new QFormLayout(&dlg);
  auto* id = new QLineEdit(&dlg);
  auto* x = new QLineEdit(&dlg);
  auto* y = new QLineEdit(&dlg);
  auto* datum = new QLineEdit(QStringLiteral("세계측지계"), &dlg);
  auto* ell = new QLineEdit(QStringLiteral("GRS80"), &dlg);
  auto* proj = new QLineEdit(QStringLiteral("TM/UTM-K"), &dlg);
  auto* origin = new QLineEdit(&dlg);
  auto* acc = new QLineEdit(QStringLiteral("1.0"), &dlg);
  auto* pdop = new QLineEdit(QStringLiteral("1.5"), &dlg);
  auto* fix = new QLineEdit(QStringLiteral("RTK"), &dlg);
  auto* axisHint = new QLabel(LayerOps::controlPointAxisHint(), &dlg);
  axisHint->setWordWrap(true);
  form->addRow(axisHint);
  form->addRow(QStringLiteral("점ID"), id);
  form->addRow(QStringLiteral("X (동쪽)"), x);
  form->addRow(QStringLiteral("Y (북쪽)"), y);
  auto* swapAxes = new QCheckBox(QStringLiteral("X·Y 교환 (측량 X=북, Y=동)"), &dlg);
  form->addRow(swapAxes);
  form->addRow(QStringLiteral("측지기준계"), datum);
  form->addRow(QStringLiteral("타원체"), ell);
  form->addRow(QStringLiteral("투영"), proj);
  form->addRow(QStringLiteral("원점"), origin);
  form->addRow(QStringLiteral("accuracy_m"), acc);
  form->addRow(QStringLiteral("PDOP"), pdop);
  form->addRow(QStringLiteral("fix_type"), fix);
  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
  form->addRow(buttons);
  connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
  if (dlg.exec() != QDialog::Accepted) return;
  if (id->text().isEmpty() || x->text().isEmpty() || y->text().isEmpty()) {
    QMessageBox::warning(this, QStringLiteral("입력"), QStringLiteral("점ID/X/Y 필수"));
    return;
  }
  const double xv = x->text().toDouble();
  const double yv = y->text().toDouble();
  bool swap = swapAxes->isChecked();
#if KA_HGIS_HAS_QGIS
  const auto suggestion = LayerOps::suggestControlPointAxisSwap(QgsProject::instance(), xv, yv);
  if (suggestion.ok && !suggestion.summary.isEmpty()) {
    QMessageBox box(this);
    box.setIcon(suggestion.swapSuggested ? QMessageBox::Warning : QMessageBox::Information);
    box.setWindowTitle(QStringLiteral("기준점 축"));
    box.setText(suggestion.summary);
    auto* keep = qobject_cast<QPushButton*>(box.addButton(QStringLiteral("이대로 넣기"), QMessageBox::AcceptRole));
    auto* flip = qobject_cast<QPushButton*>(box.addButton(QStringLiteral("X·Y 교환"), QMessageBox::ActionRole));
    box.addButton(QStringLiteral("취소"), QMessageBox::RejectRole);
    box.setDefaultButton(suggestion.swapSuggested || swap ? flip : keep);
    box.exec();
    if (box.clickedButton() != keep && box.clickedButton() != flip) return;
    swap = box.clickedButton() == flip;
  }
#endif
  const QgsPointXY mapXy = LayerOps::controlPointMapXy(xv, yv, swap);
  m_stubHasMeta = !datum->text().isEmpty() && !ell->text().isEmpty() && !proj->text().isEmpty();
#if KA_HGIS_HAS_QGIS
  auto* layer = ensureDomainLayerForEdit(QStringLiteral("control_points"), QStringLiteral("GPS기준점"));
  if (layer && layer->startEditing()) {
    QgsFeature f(layer->fields());
    f.setAttribute(QStringLiteral("point_id"), id->text());
    f.setAttribute(QStringLiteral("x"), mapXy.x());
    f.setAttribute(QStringLiteral("y"), mapXy.y());
    f.setAttribute(QStringLiteral("datum"), datum->text());
    f.setAttribute(QStringLiteral("ellipsoid"), ell->text());
    f.setAttribute(QStringLiteral("projection"), proj->text());
    f.setAttribute(QStringLiteral("origin"), origin->text());
    f.setAttribute(QStringLiteral("accuracy"), acc->text());
    f.setAttribute(QStringLiteral("accuracy_m"), acc->text().toDouble());
    f.setAttribute(QStringLiteral("pdop"), pdop->text().toDouble());
    f.setAttribute(QStringLiteral("fix_type"), fix->text());
    f.setGeometry(QgsGeometry::fromPointXY(mapXy));
    layer->addFeature(f);
    layer->commitChanges();
    m_stubGcp = layer->featureCount();
  } else
#endif
  { m_stubGcp++; }
  statusBar()->showMessage(QStringLiteral("기준점 등록 (총 추정 %1)").arg(m_stubGcp), 4000);
}

void MainWindow::clearDrawnFeaturesOfCurrentLayer() {
#if KA_HGIS_HAS_QGIS
  auto* vl = m_layerTree ? qobject_cast<QgsVectorLayer*>(m_layerTree->currentLayer()) : nullptr;
  if (!vl || !isProjectSurveyDomainLayer(vl, m_surveyPath)) {
    statusBar()->showMessage(QStringLiteral("현재 조사에서 직접 그린 레이어만 도형을 비울 수 있습니다"), 4000);
    return;
  }
  const long long n = vl->featureCount();
  if (n <= 0) {
    statusBar()->showMessage(QStringLiteral("%1에 지울 도형이 없습니다").arg(vl->name()), 4000);
    return;
  }
  if (QMessageBox::question(
          this, QStringLiteral("그린 도형 삭제"),
          QStringLiteral("%1의 도형 %2개를 지웁니다. Ctrl+Z로 복원할 수 있습니다.\n계속할까요?")
              .arg(vl->name())
              .arg(n),
          QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
    return;

  // 확인 창에 적은 이 레이어만 비운다. deleteSelectedFeatures()는 모든 레이어의 선택을 지워서,
  // 다른 레이어에 남아 있던 선택까지 말없이 함께 지운다.
  const QgsFeatureIds ids = vl->allFeatureIds();
  QString error;
  if (!LayerOps::runEditCommand(vl, QStringLiteral("도형 모두 지우기"), [&]() {
        return vl->deleteFeatures(ids);
      }, &error)) {
    notify(Notice::Warning, QStringLiteral("도형 삭제 확인"), vl->name() + QStringLiteral(": ") + error);
    return;
  }
  QgsProject::instance()->setDirty(true);
  if (m_featureSelectTool) m_featureSelectTool->refreshSelectedGeometry();
  if (m_canvas) LayerOps::refreshCanvasIfIdle(m_canvas);
  refreshWorkPanel();
  statusBar()->showMessage(
      QStringLiteral("도형 %1개를 지웠습니다. Ctrl+Z로 복원할 수 있습니다.").arg(ids.size()), 6000);
#endif
}

