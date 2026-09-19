#include "KaTerrain3dLayoutStudio.h"

#include "KaBeginnerRibbon.h"
#include "KaIcons.h"
#include "KaTheme.h"
#include "core/LayoutService.h"
#include "core/Terrain3dLayoutService.h"

#include <algorithm>

#include <QAbstractItemView>
#include <QAbstractSpinBox>
#include <QAction>
#include <QApplication>
#include <QBrush>
#include <QCheckBox>
#include <QColor>
#include <QDoubleSpinBox>
#include <QEvent>
#include <QFileDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QTimer>
#include <QToolButton>
#include <QUndoStack>
#include <QVBoxLayout>

#include <QGraphicsItem>
#include <qgslayoutitem.h>
#include <qgslayoutitempage.h>
#include <qgsmaplayer.h>
#include <qgslayoutmanager.h>
#include <qgslayoutpagecollection.h>
#include <qgslayoutundostack.h>
#include <qgslayoutview.h>
#include <qgslayoutviewtool.h>
#include <qgslayoutviewtoolpan.h>
#include <qgslayoutviewtoolselect.h>
#include <qgslayertree.h>
#include <qgslayertreemodel.h>
#include <qgslayertreenode.h>
#include <qgslayertreeview.h>
#include <qgsprintlayout.h>
#include <qgsproject.h>

namespace {

QToolButton* makeTile(QWidget* parent, const QIcon& icon, const QString& text, const QSize& iconSize) {
  auto* b = new QToolButton(parent);
  b->setIcon(icon);
  b->setIconSize(iconSize);
  b->setText(KaBeginnerRibbon::twoLine(text));
  b->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
  b->setAutoRaise(true);
  b->setCursor(Qt::PointingHandCursor);
  b->setToolTip(text);
  b->setMinimumHeight(52);
  b->setProperty("class", QStringLiteral("sampleTile"));
  return b;
}

}  // namespace

KaTerrain3dLayoutStudio::KaTerrain3dLayoutStudio(QgsProject* project, QWidget* parent)
    : QWidget(parent), m_project(project) {
  setObjectName(QStringLiteral("terrain3dLayoutStudio"));
  setFocusPolicy(Qt::StrongFocus);
  auto* root = new QHBoxLayout(this);
  root->setContentsMargins(0, 0, 0, 0);
  root->setSpacing(0);

  auto* leftCol = new QFrame(this);
  leftCol->setObjectName(QStringLiteral("studioLeftCol"));
  leftCol->setMinimumWidth(220);
  leftCol->setMaximumWidth(280);
  auto* leftColLay = new QVBoxLayout(leftCol);
  leftColLay->setContentsMargins(10, 10, 10, 10);
  leftColLay->setSpacing(6);

  auto* layerBox = new QFrame(leftCol);
  layerBox->setObjectName(QStringLiteral("layersCard"));
  auto* layerLay = new QVBoxLayout(layerBox);
  layerLay->setContentsMargins(8, 8, 8, 8);
  layerLay->setSpacing(4);
  auto* leftCap = new QLabel(QStringLiteral("레이어"), layerBox);
  leftCap->setObjectName(QStringLiteral("cardCaption"));
  m_layerModel = new QgsLayerTreeModel(QgsProject::instance()->layerTreeRoot(), this);
  m_layerModel->setFlag(QgsLayerTreeModel::AllowNodeChangeVisibility, true);
  m_layerTree = new QgsLayerTreeView(layerBox);
  m_layerTree->setObjectName(QStringLiteral("layoutLayerTree"));
  m_layerTree->setModel(m_layerModel);
  m_layerTree->setMinimumWidth(160);
  m_layerTree->setSelectionMode(QAbstractItemView::ExtendedSelection);
  m_overlayTimer = new QTimer(this);
  m_overlayTimer->setSingleShot(true);
  m_overlayTimer->setInterval(250);
  connect(m_overlayTimer, &QTimer::timeout, this, [this]() { emit overlaysChanged(); });
  const auto pingOverlays = [this]() {
    if (m_overlayTimer)
      m_overlayTimer->start();
  };
  connect(m_layerModel, &QAbstractItemModel::dataChanged, this, pingOverlays);
  if (QgsLayerTree* tree = QgsProject::instance()->layerTreeRoot()) {
    connect(tree, &QgsLayerTreeNode::visibilityChanged, this,
            [pingOverlays](QgsLayerTreeNode*) { pingOverlays(); });
  }
  layerLay->addWidget(leftCap);
  layerLay->addWidget(m_layerTree, 1);
  auto* layerEmpty = new QLabel(
      QStringLiteral("이 도면에는 레이어가 없습니다.\n지도에서 켠 레이어가\n여기에 보입니다."),
      layerBox);
  layerEmpty->setObjectName(QStringLiteral("emptyState"));
  layerEmpty->setAlignment(Qt::AlignCenter);
  layerEmpty->setWordWrap(true);
  layerLay->addWidget(layerEmpty);
  auto syncEmpty = [this, layerEmpty]() {
    const bool empty = !QgsProject::instance() || QgsProject::instance()->mapLayers().isEmpty();
    if (layerEmpty)
      layerEmpty->setVisible(empty);
    if (m_layerTree)
      m_layerTree->setVisible(!empty);
  };
  connect(QgsProject::instance(), &QgsProject::layersAdded, this,
          [syncEmpty](const QList<QgsMapLayer*>&) { syncEmpty(); });
  connect(QgsProject::instance(), &QgsProject::layersRemoved, this,
          [syncEmpty](const QStringList&) { syncEmpty(); });
  syncEmpty();
  leftColLay->addWidget(layerBox, 1);

  auto* mid = new QWidget(this);
  auto* midLay = new QVBoxLayout(mid);
  midLay->setContentsMargins(0, 0, 0, 0);
  midLay->setSpacing(0);

  m_view = new QgsLayoutView(mid);
  m_view->setObjectName(QStringLiteral("terrain3dLayoutView"));
  m_view->setBackgroundBrush(QBrush(KaTheme::tokens().desk));
  m_view->setFocusPolicy(Qt::StrongFocus);
  m_toolSelect = new QgsLayoutViewToolSelect(m_view);
  m_toolPan = new QgsLayoutViewToolPan(m_view);
  m_view->installEventFilter(this);
  if (m_view->viewport())
    m_view->viewport()->installEventFilter(this);
  midLay->addWidget(m_view, 1);

  m_status = new QLabel(QStringLiteral("입체지형 조판 — 범례는 「범례」로"), mid);
  m_status->setContentsMargins(8, 4, 8, 4);
  midLay->addWidget(m_status);

  auto* undoAct = new QAction(QStringLiteral("되돌리기"), this);
  if (!parentWidget()) undoAct->setShortcut(QKeySequence::Undo);
  undoAct->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  connect(undoAct, &QAction::triggered, this, &KaTerrain3dLayoutStudio::undoLastChange);
  addAction(undoAct);
  auto* delAct = new QAction(QStringLiteral("지우기"), this);
  if (!parentWidget()) delAct->setShortcut(Qt::Key_Delete);
  delAct->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  connect(delAct, &QAction::triggered, this, &KaTerrain3dLayoutStudio::deleteSelectedItems);
  addAction(delAct);

  auto* side = new QFrame(this);
  side->setObjectName(QStringLiteral("studioRightCol"));
  side->setMinimumWidth(260);
  side->setMaximumWidth(300);
  auto* sideLay = new QVBoxLayout(side);
  sideLay->setContentsMargins(12, 8, 12, 8);
  sideLay->setSpacing(8);

  auto* cardLegend = new QFrame(side);
  cardLegend->setObjectName(QStringLiteral("itemInspector"));
  auto* legendLay = new QVBoxLayout(cardLegend);
  legendLay->setContentsMargins(10, 10, 10, 10);
  legendLay->setSpacing(6);
  auto* legendCap = new QLabel(QStringLiteral("조판 항목"), cardLegend);
  legendCap->setObjectName(QStringLiteral("cardCaption"));
  legendLay->addWidget(legendCap);
  auto* legendRow = new QHBoxLayout;
  legendRow->setSpacing(14);
  auto* legendBtn = makeTile(cardLegend, KaIcons::icon(QStringLiteral("layout_legend")),
                             QStringLiteral("범례"), QSize(22, 22));
  connect(legendBtn, &QToolButton::clicked, this, &KaTerrain3dLayoutStudio::addLegend);
  auto* pdfBtn = makeTile(cardLegend, KaIcons::icon(QStringLiteral("pdf")),
                          QStringLiteral("PDF 내보내기"), QSize(22, 22));
  pdfBtn->setObjectName(QStringLiteral("btnPrimary"));
  connect(pdfBtn, &QToolButton::clicked, this, &KaTerrain3dLayoutStudio::exportPdf);
  legendRow->addWidget(legendBtn, 1);
  legendRow->addWidget(pdfBtn, 1);
  legendLay->addLayout(legendRow);
  legendLay->addWidget(new QLabel(QStringLiteral("제목"), cardLegend));
  m_legendTitle = new QLineEdit(cardLegend);
  m_legendTitle->setPlaceholderText(QStringLiteral("제목을 입력하세요"));
  m_legendTitle->setText(QStringLiteral("범례"));
  connect(m_legendTitle, &QLineEdit::textChanged, this, &KaTerrain3dLayoutStudio::applyLegendStyle);
  legendLay->addWidget(m_legendTitle);
  auto* fontRow = new QHBoxLayout;
  m_legendFont = new QSpinBox(cardLegend);
  m_legendFont->setRange(7, 24);
  m_legendFont->setValue(10);
  m_legendFont->setSuffix(QStringLiteral(" pt"));
  connect(m_legendFont, QOverload<int>::of(&QSpinBox::valueChanged), this,
          &KaTerrain3dLayoutStudio::applyLegendStyle);
  m_legendBold = new QCheckBox(QStringLiteral("B"), cardLegend);
  m_legendBold->setChecked(true);
  m_legendItalic = new QCheckBox(QStringLiteral("I"), cardLegend);
  connect(m_legendBold, &QCheckBox::toggled, this, &KaTerrain3dLayoutStudio::applyLegendStyle);
  connect(m_legendItalic, &QCheckBox::toggled, this, &KaTerrain3dLayoutStudio::applyLegendStyle);
  fontRow->addWidget(new QLabel(QStringLiteral("글자"), cardLegend));
  fontRow->addWidget(m_legendFont, 1);
  fontRow->addWidget(m_legendBold);
  fontRow->addWidget(m_legendItalic);
  legendLay->addLayout(fontRow);
  sideLay->addWidget(cardLegend);

  auto* cardNorth = new QFrame(side);
  cardNorth->setObjectName(QStringLiteral("itemInspector"));
  auto* northLay = new QVBoxLayout(cardNorth);
  northLay->setContentsMargins(10, 10, 10, 10);
  northLay->setSpacing(6);
  northLay->addWidget(new QLabel(QStringLiteral("방위"), cardNorth));
  auto* northRow = new QHBoxLayout;
  northRow->setSpacing(6);
  const char* northTips[] = {"북 글자", "북 화살", "나침반", "바람장미"};
  for (int i = 0; i < 4; ++i) {
    auto* b = makeTile(cardNorth, KaIcons::icon(QStringLiteral("layout_north")),
                       QString::fromUtf8(northTips[i]), QSize(28, 28));
    connect(b, &QToolButton::clicked, this, [this, i]() { applyNorthKind(i); });
    northRow->addWidget(b);
  }
  northLay->addLayout(northRow);
  auto* northSizeRow = new QHBoxLayout;
  northSizeRow->addWidget(new QLabel(QStringLiteral("크기"), cardNorth));
  m_northSize = new QDoubleSpinBox(cardNorth);
  m_northSize->setRange(12.0, 80.0);
  m_northSize->setDecimals(0);
  m_northSize->setSuffix(QStringLiteral(" mm"));
  m_northSize->setValue(28.0);
  connect(m_northSize, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double) {
    applyNorthKind(1);
  });
  northSizeRow->addWidget(m_northSize, 1);
  northLay->addLayout(northSizeRow);
  sideLay->addWidget(cardNorth);

  auto* cardScale = new QFrame(side);
  cardScale->setObjectName(QStringLiteral("itemInspector"));
  auto* scaleLay = new QVBoxLayout(cardScale);
  scaleLay->setContentsMargins(10, 10, 10, 10);
  scaleLay->setSpacing(8);
  scaleLay->addWidget(new QLabel(QStringLiteral("도면 정보"), cardScale));
  m_scaleSpin = new QSpinBox(cardScale);
  m_scaleSpin->setRange(10, 5000000);
  m_scaleSpin->setSingleStep(10);
  m_scaleSpin->setValue(1000);
  m_scaleSpin->setKeyboardTracking(false);
  m_scaleSpin->setGroupSeparatorShown(false);
  auto* applySc = new QPushButton(QStringLiteral("적용"), cardScale);
  connect(applySc, &QPushButton::clicked, this, &KaTerrain3dLayoutStudio::applyScaleNow);
  connect(m_scaleSpin, &QSpinBox::editingFinished, this, &KaTerrain3dLayoutStudio::applyScaleNow);
  auto* scaleTop = new QHBoxLayout;
  scaleTop->addWidget(new QLabel(QStringLiteral("1 :"), cardScale));
  scaleTop->addWidget(m_scaleSpin, 1);
  scaleTop->addWidget(applySc);
  scaleLay->addLayout(scaleTop);
  auto addChipRow = [&](const int* vals, int count) {
    auto* row = new QHBoxLayout;
    row->setSpacing(6);
    for (int i = 0; i < count; ++i) {
      const int n = vals[i];
      auto* chip = new QToolButton(cardScale);
      chip->setObjectName(QStringLiteral("scaleChip"));
      chip->setText(QString::number(n));
      chip->setToolButtonStyle(Qt::ToolButtonTextOnly);
      chip->setCursor(Qt::PointingHandCursor);
      chip->setMinimumSize(48, 26);
      connect(chip, &QToolButton::clicked, this, [this, n]() {
        if (m_scaleSpin)
          m_scaleSpin->setValue(n);
        applyScaleNow();
      });
      row->addWidget(chip);
    }
    row->addStretch(1);
    scaleLay->addLayout(row);
  };
  const int rowA[] = {100, 200, 250};
  const int rowB[] = {300, 400, 500};
  const int rowC[] = {1000, 2000, 5000};
  addChipRow(rowA, 3);
  addChipRow(rowB, 3);
  addChipRow(rowC, 3);
  auto* barRow = new QHBoxLayout;
  struct BarSample {
    const char* style;
    const char* tip;
  };
  const BarSample bars[] = {{"Double Box", "쌍칸"}, {"Single Box", "외칸"}, {"Line Ticks Up", "눈금"}};
  for (const auto& bs : bars) {
    auto* b = makeTile(cardScale, KaIcons::icon(QStringLiteral("layout_scale")),
                       QString::fromUtf8(bs.tip), QSize(40, 22));
    const QString style = QString::fromUtf8(bs.style);
    connect(b, &QToolButton::clicked, this, [this, style]() { applyBarStyle(style); });
    barRow->addWidget(b);
  }
  scaleLay->addLayout(barRow);
  auto* extraRow = new QHBoxLayout;
  auto* scaleLblBtn = makeTile(cardScale, KaIcons::icon(QStringLiteral("layout_scale")),
                               QStringLiteral("축척 글자"), QSize(24, 24));
  connect(scaleLblBtn, &QToolButton::clicked, this, &KaTerrain3dLayoutStudio::addScaleLabel);
  extraRow->addWidget(scaleLblBtn);
  auto* crsBtn = makeTile(cardScale, KaIcons::icon(QStringLiteral("crs")), QStringLiteral("좌표계"),
                          QSize(24, 24));
  connect(crsBtn, &QToolButton::clicked, this, &KaTerrain3dLayoutStudio::addCrsLabel);
  extraRow->addWidget(crsBtn);
  extraRow->addStretch(1);
  scaleLay->addLayout(extraRow);
  sideLay->addWidget(cardScale);
  sideLay->addStretch(1);

  root->addWidget(leftCol, 0);
  root->addWidget(mid, 1);
  root->addWidget(side, 0);
}

void KaTerrain3dLayoutStudio::attachSheet() {
  attachLayoutToView();
  fitPaper();
}

void KaTerrain3dLayoutStudio::detachSheet() {
  detachLayoutFromView();
}

QgsPrintLayout* KaTerrain3dLayoutStudio::currentLayout() const {
  if (!m_project)
    return nullptr;
  return dynamic_cast<QgsPrintLayout*>(m_project->layoutManager()->layoutByName(
      QString::fromUtf8(Terrain3dLayoutService::kSheetName)));
}

void KaTerrain3dLayoutStudio::attachLayoutToView() {
  auto* ly = currentLayout();
  if (!m_view || !ly || !m_toolSelect)
    return;
  detachLayoutFromView();
  m_view->setCurrentLayout(ly);
  m_toolSelect->setLayout(ly);
  m_view->setTool(m_toolSelect);
}

void KaTerrain3dLayoutStudio::detachLayoutFromView() {
  if (!m_view)
    return;
  if (QgsLayoutViewTool* t = m_view->tool())
    m_view->unsetTool(t);
  m_view->setScene(nullptr);
}

void KaTerrain3dLayoutStudio::fitPaper() {
  auto* ly = currentLayout();
  if (!m_view || !ly)
    return;
  auto* pc = ly->pageCollection();
  if (!pc || pc->pageCount() == 0)
    return;
  auto* pg = pc->page(0);
  if (!pg)
    return;
  const QRectF pr = pg->mapRectToScene(pg->rect());
  const qreal pad = std::max(pr.width(), pr.height()) * 0.08;
  m_view->fitInView(pr.adjusted(-pad, -pad, pad, pad), Qt::KeepAspectRatio);
}

void KaTerrain3dLayoutStudio::warn(const QString& err) {
  if (m_status)
    m_status->setText(err);
  if (!err.isEmpty())
    QMessageBox::information(this, QStringLiteral("입체지형 조판"), err);
}

void KaTerrain3dLayoutStudio::addLegend() {
  QString err;
  if (!Terrain3dLayoutService::ensureLegend(
          m_project, m_legendTitle ? m_legendTitle->text() : QStringLiteral("범례"),
          m_legendFont ? m_legendFont->value() : 10, m_legendBold && m_legendBold->isChecked(),
          m_legendItalic && m_legendItalic->isChecked(), &err)) {
    warn(err);
    return;
  }
  rememberPlaced(Terrain3dLayoutService::kIdLegend);
  selectItemById(Terrain3dLayoutService::kIdLegend);
  if (m_status)
    m_status->setText(QStringLiteral("범례를 넣었습니다. Delete 또는 Ctrl+Z로 지울 수 있습니다."));
  if (m_view) {
    m_view->setFocus(Qt::OtherFocusReason);
    m_view->viewport()->update();
  }
}

void KaTerrain3dLayoutStudio::applyLegendStyle() {
  if (!currentLayout())
    return;
  QString err;
  Terrain3dLayoutService::ensureLegend(
      m_project, m_legendTitle ? m_legendTitle->text() : QStringLiteral("범례"),
      m_legendFont ? m_legendFont->value() : 10, m_legendBold && m_legendBold->isChecked(),
      m_legendItalic && m_legendItalic->isChecked(), &err, false);
}

void KaTerrain3dLayoutStudio::applyNorthKind(int kind) {
  QString err;
  if (!Terrain3dLayoutService::applyNorth(m_project, kind, m_northSize ? m_northSize->value() : 28.0,
                                          &err)) {
    warn(err);
    return;
  }
  if (m_status)
    m_status->setText(QStringLiteral("방위를 바꿨습니다."));
  if (m_view)
    m_view->viewport()->update();
}

void KaTerrain3dLayoutStudio::applyScaleNow() {
  const int denom = m_scaleSpin ? m_scaleSpin->value() : 1000;
  emit requestScale(denom);
  if (m_status)
    m_status->setText(QStringLiteral("축척 1 : %1 — 입체지형을 맞춥니다").arg(denom));
  if (m_view)
    m_view->viewport()->update();
}

void KaTerrain3dLayoutStudio::applyBarStyle(const QString& style) {
  QString err;
  if (!Terrain3dLayoutService::applyScaleBarStyle(m_project, style, &err)) {
    warn(err);
    return;
  }
  if (m_status)
    m_status->setText(QStringLiteral("축척자 모양을 바꿨습니다."));
  if (m_view)
    m_view->viewport()->update();
}

void KaTerrain3dLayoutStudio::addScaleLabel() {
  QString err;
  if (!Terrain3dLayoutService::ensureScaleLabel(m_project, &err)) {
    warn(err);
    return;
  }
  rememberPlaced(Terrain3dLayoutService::kIdScaleLabel);
  selectItemById(Terrain3dLayoutService::kIdScaleLabel);
  if (m_status)
    m_status->setText(QStringLiteral("축척 글자를 넣었습니다."));
}

void KaTerrain3dLayoutStudio::addCrsLabel() {
  QString err;
  if (!Terrain3dLayoutService::ensureCrsLabel(m_project, &err)) {
    warn(err);
    return;
  }
  rememberPlaced(Terrain3dLayoutService::kIdCrs);
  selectItemById(Terrain3dLayoutService::kIdCrs);
  if (m_status)
    m_status->setText(QStringLiteral("좌표계를 넣었습니다."));
}

void KaTerrain3dLayoutStudio::exportPdf() {
  auto* ly = currentLayout();
  if (!ly || !m_project) {
    QMessageBox::information(this, QStringLiteral("입체지형 조판"),
                             QStringLiteral("먼저 입체지형 도면출력을 하세요."));
    return;
  }
  const QString path = QFileDialog::getSaveFileName(
      this, QStringLiteral("입체지형 PDF"), QStringLiteral("입체지형_조판.pdf"),
      QStringLiteral("PDF (*.pdf)"));
  if (path.isEmpty())
    return;
  QString err;
  const QString saved = LayoutService::exportLayoutPdf(
      m_project, QString::fromUtf8(Terrain3dLayoutService::kSheetName), path, &err);
  if (saved.isEmpty()) {
    QMessageBox::warning(this, QStringLiteral("입체지형 조판"),
                         err.isEmpty() ? QStringLiteral("PDF를 쓰지 못했습니다.") : err);
    return;
  }
  if (m_status)
    m_status->setText(QStringLiteral("PDF 저장 · %1").arg(saved));
}

bool KaTerrain3dLayoutStudio::editingText() const {
  QWidget* focus = QApplication::focusWidget();
  return qobject_cast<QLineEdit*>(focus) || qobject_cast<QAbstractSpinBox*>(focus);
}

void KaTerrain3dLayoutStudio::rememberPlaced(const char* id) {
  if (!id)
    return;
  m_placeUndo.append(QString::fromUtf8(id));
}

void KaTerrain3dLayoutStudio::selectItemById(const char* id) {
  auto* ly = currentLayout();
  if (!ly || !id)
    return;
  for (QGraphicsItem* gi : ly->items()) {
    if (auto* it = dynamic_cast<QgsLayoutItem*>(gi)) {
      if (it->id() == QLatin1String(id)) {
        ly->setSelectedItem(it);
        return;
      }
    }
  }
}

void KaTerrain3dLayoutStudio::deleteSelectedItems() {
  if (editingText())
    return;
  auto* ly = currentLayout();
  if (!ly)
    return;
  const QList<QgsLayoutItem*> sel = ly->selectedLayoutItems(false);
  int n = 0;
  for (QgsLayoutItem* it : sel) {
    if (!it || dynamic_cast<QgsLayoutItemPage*>(it))
      continue;
    ly->removeLayoutItem(it);
    ++n;
  }
  if (m_status) {
    if (n > 0)
      m_status->setText(QStringLiteral("선택한 항목 %1개를 지웠습니다.").arg(n));
    else
      m_status->setText(QStringLiteral("지울 항목을 먼저 선택하세요."));
  }
  if (m_view)
    m_view->viewport()->update();
}

void KaTerrain3dLayoutStudio::undoLastChange() {
  if (editingText())
    return;
  auto* ly = currentLayout();
  if (ly && ly->undoStack() && ly->undoStack()->stack() && ly->undoStack()->stack()->canUndo()) {
    ly->undoStack()->stack()->undo();
    if (m_status)
      m_status->setText(QStringLiteral("한 단계 되돌렸습니다."));
    if (m_view)
      m_view->viewport()->update();
    return;
  }
  while (!m_placeUndo.isEmpty()) {
    const QString id = m_placeUndo.takeLast();
    if (!ly)
      break;
    for (QGraphicsItem* gi : ly->items()) {
      auto* it = dynamic_cast<QgsLayoutItem*>(gi);
      if (!it || it->id() != id || dynamic_cast<QgsLayoutItemPage*>(it))
        continue;
      ly->removeLayoutItem(it);
      if (m_status)
        m_status->setText(QStringLiteral("방금 넣은 항목을 되돌렸습니다."));
      if (m_view)
        m_view->viewport()->update();
      return;
    }
  }
  if (m_status)
    m_status->setText(QStringLiteral("되돌릴 것이 없습니다."));
}

void KaTerrain3dLayoutStudio::redoLastChange() {
  if (editingText())
    return;
  auto* ly = currentLayout();
  if (ly && ly->undoStack() && ly->undoStack()->stack() && ly->undoStack()->stack()->canRedo()) {
    ly->undoStack()->stack()->redo();
    if (m_status)
      m_status->setText(QStringLiteral("다시 실행했습니다."));
    if (m_view)
      m_view->viewport()->update();
    return;
  }
  if (m_status)
    m_status->setText(QStringLiteral("다시 실행할 것이 없습니다."));
}

bool KaTerrain3dLayoutStudio::eventFilter(QObject* watched, QEvent* event) {
  const bool onView = m_view && event && (watched == m_view || watched == m_view->viewport());
  if (onView && event->type() == QEvent::KeyPress) {
    auto* ke = static_cast<QKeyEvent*>(event);
    if (ke->matches(QKeySequence::Undo) ||
        ((ke->modifiers() & Qt::ControlModifier) && ke->key() == Qt::Key_Z)) {
      undoLastChange();
      return true;
    }
    if (ke->matches(QKeySequence::Redo) ||
        ((ke->modifiers() & Qt::ControlModifier) && ke->key() == Qt::Key_Y)) {
      redoLastChange();
      return true;
    }
    if (ke->key() == Qt::Key_Delete || ke->key() == Qt::Key_Backspace) {
      deleteSelectedItems();
      return true;
    }
  }
  return QWidget::eventFilter(watched, event);
}

void KaTerrain3dLayoutStudio::keyPressEvent(QKeyEvent* event) {
  if (event && (event->matches(QKeySequence::Undo) ||
                ((event->modifiers() & Qt::ControlModifier) && event->key() == Qt::Key_Z))) {
    undoLastChange();
    event->accept();
    return;
  }
  if (event && (event->matches(QKeySequence::Redo) ||
                ((event->modifiers() & Qt::ControlModifier) && event->key() == Qt::Key_Y))) {
    redoLastChange();
    event->accept();
    return;
  }
  if (event && (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace)) {
    deleteSelectedItems();
    event->accept();
    return;
  }
  QWidget::keyPressEvent(event);
}
