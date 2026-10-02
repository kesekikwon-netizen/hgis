#include "KaLineColorDialog.h"

#include "KaTheme.h"
#include "KaUserError.h"
#include "core/CadDrawingLayers.h"
#include "core/LineMapStyle.h"

#include <QColorDialog>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include <qgslayertree.h>
#include <qgslayertreeview.h>
#include <qgsproject.h>
#include <qgsvectorlayer.h>

namespace KaLineColorDialog {
namespace {

void paint(QPushButton* button, const QColor& color) {
  button->setProperty("kaColor", color);
  button->setStyleSheet(KaTheme::colorSwatchStyle(color));
  button->setText(color.name(QColor::HexRgb).toUpper() + QStringLiteral("  ·  클릭해서 색 고르기"));
}

}  // namespace

QgsVectorLayer* layerFor(QgsLayerTreeView* view) {
  if (!view) return nullptr;
  if (auto* layer = qobject_cast<QgsVectorLayer*>(view->currentLayer())) return layer;
  QgsLayerTreeNode* node = view->currentNode();
  if (!QgsLayerTree::isGroup(node)) return nullptr;
  for (QgsLayerTreeNode* child : node->children()) {  // 바로 아래 레이어만: 「참조 지도」 묶음은 도면이 아니다
    const QString drawingId =
        QgsLayerTree::isLayer(child) ? CadDrawingLayers::drawingIdOf(QgsLayerTree::toLayer(child)->layer()) : QString();
    if (!drawingId.isEmpty()) return CadDrawingLayers::alignLayerOf(QgsProject::instance(), drawingId);
  }
  return nullptr;
}

void edit(QWidget* parent, QgsVectorLayer* layer, const std::function<void()>& changed) {
  if (!LineMapStyle::isLineMap(layer)) {
    KaUserError::warn(parent, {QStringLiteral("선 색"),
                               QStringLiteral("「%1」은 색을 바꾸지 않습니다.").arg(layer ? layer->name() : QString()),
                               QStringLiteral("면을 색으로 채운 지도(토양도·지질도)와 점·글자는 공식 범례·도면 표시 그대로 둡니다."),
                               QStringLiteral("도면 선·받은 지적도·등고선처럼 선으로 된 지도나 조사 데이터 레이어를 고르세요.")});
    return;
  }
  QDialog dialog(parent);
  dialog.setObjectName(QStringLiteral("kaLineColorDlg"));
  dialog.setWindowTitle(QStringLiteral("선 색"));
  auto* root = new QVBoxLayout(&dialog);
  auto* name = new QLabel(layer->name(), &dialog);
  name->setWordWrap(true);
  root->addWidget(name);
  auto* form = new QFormLayout();
  auto* color = new QPushButton(&dialog);
  color->setObjectName(QStringLiteral("lineColor"));
  paint(color, LineMapStyle::currentColor(layer));
  QObject::connect(color, &QPushButton::clicked, &dialog, [color] {
    const QColor picked = QColorDialog::getColor(color->property("kaColor").value<QColor>(), color->window(),
                                                 QStringLiteral("선 색 고르기"), QColorDialog::DontUseNativeDialog);
    if (picked.isValid()) paint(color, picked);
  });
  auto* width = new QDoubleSpinBox(&dialog);
  width->setObjectName(QStringLiteral("lineWidth"));
  width->setRange(0.1, 5.0);
  width->setSingleStep(0.1);
  width->setDecimals(2);
  width->setSuffix(QStringLiteral(" mm"));
  width->setValue(LineMapStyle::currentWidthMm(layer));
  form->addRow(QStringLiteral("선 색"), color);
  form->addRow(QStringLiteral("선 굵기"), width);
  root->addLayout(form);

  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
  buttons->button(QDialogButtonBox::Ok)->setText(QStringLiteral("적용"));
  buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("취소"));
  QPushButton* original = buttons->addButton(QStringLiteral("원래 색으로"), QDialogButtonBox::ResetRole);
  original->setObjectName(QStringLiteral("lineColorOriginal"));
  original->setEnabled(LineMapStyle::hasOriginal(layer));
  QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
  QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
  bool restore = false;
  QObject::connect(original, &QPushButton::clicked, &dialog, [&dialog, &restore] {
    restore = true;
    dialog.accept();
  });
  root->addWidget(buttons);
  if (dialog.exec() != QDialog::Accepted) return;

  const bool done = restore ? LineMapStyle::restoreOriginal(layer)
                            : LineMapStyle::apply(layer, color->property("kaColor").value<QColor>(), width->value());
  if (!done) return;
  QgsProject::instance()->setDirty(true);  // 조사를 저장하면 작업공간에 바꾼 색과 원래 모양이 같이 남는다
  if (changed) changed();
}

}  // namespace KaLineColorDialog
