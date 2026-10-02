#include "KaGridOriginButton.h"

#include "KaCanvasGridOverlay.h"

#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>

#include <qgsmapcanvas.h>

namespace {

QDoubleSpinBox* coordinateBox(QWidget* parent, const QString& name, double value) {
  auto* box = new QDoubleSpinBox(parent);
  box->setObjectName(name);
  box->setRange(-10000000.0, 10000000.0);
  box->setDecimals(3);
  box->setSuffix(QStringLiteral(" m"));
  box->setValue(value);
  box->setMinimumWidth(150);
  return box;
}

}  // namespace

KaGridOriginButton::KaGridOriginButton(QgsMapCanvas* canvas, std::function<KaCanvasGridOverlay*()> grid,
                                       QWidget* parent)
    : QToolButton(parent), m_canvas(canvas), m_grid(std::move(grid)) {
  setObjectName(QStringLiteral("gridOrigin"));
  setText(QStringLiteral("원점"));
  setToolTip(QStringLiteral("격자 원점: 좌표계 원점(0, 0). 눌러서 조사지 격자 원점을 정합니다."));
  connect(this, &QToolButton::clicked, this, [this] { editOrigin(); });
}

void KaGridOriginButton::setOrigin(double x, double y) {
  KaCanvasGridOverlay* grid = m_grid ? m_grid() : nullptr;
  if (grid) {
    KaCanvasGridOverlay::Config config = grid->config();
    config.originX = x;
    config.originY = y;
    grid->setConfig(config);
  }
  setToolTip(x == 0.0 && y == 0.0
                 ? QStringLiteral("격자 원점: 좌표계 원점(0, 0). 눌러서 조사지 격자 원점을 정합니다.")
                 : QStringLiteral("격자 원점: X(동) %1 m, Y(북) %2 m. 눌러서 바꿉니다.")
                       .arg(x, 0, 'f', 3)
                       .arg(y, 0, 'f', 3));
}

void KaGridOriginButton::editOrigin() {
  KaCanvasGridOverlay* grid = m_grid ? m_grid() : nullptr;
  const KaCanvasGridOverlay::Config current = grid ? grid->config() : KaCanvasGridOverlay::Config();
  QDialog dialog(window());
  dialog.setWindowTitle(QStringLiteral("격자 원점"));
  auto* form = new QFormLayout(&dialog);
  auto* hint = new QLabel(QStringLiteral("격자선 하나가 이 점을 지납니다. 회전한 격자는 이 점을 중심으로 돌고, "
                                         "눈금은 이 점에서 잰 거리로 적습니다."),
                          &dialog);
  hint->setWordWrap(true);
  form->addRow(hint);
  auto* east = coordinateBox(&dialog, QStringLiteral("gridOriginX"), current.originX);
  auto* north = coordinateBox(&dialog, QStringLiteral("gridOriginY"), current.originY);
  form->addRow(QStringLiteral("X (동쪽)"), east);
  form->addRow(QStringLiteral("Y (북쪽)"), north);
  auto* quick = new QHBoxLayout();
  auto* center = new QPushButton(QStringLiteral("지도 가운데"), &dialog);
  auto* zero = new QPushButton(QStringLiteral("좌표계 원점 (0, 0)"), &dialog);
  quick->addWidget(center);
  quick->addWidget(zero);
  form->addRow(quick);
  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
  buttons->button(QDialogButtonBox::Ok)->setText(QStringLiteral("적용"));
  buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("취소"));
  form->addRow(buttons);
  connect(center, &QPushButton::clicked, &dialog, [this, east, north] {
    if (!m_canvas) return;
    const QgsPointXY middle = m_canvas->extent().center();
    east->setValue(middle.x());
    north->setValue(middle.y());
  });
  connect(zero, &QPushButton::clicked, &dialog, [east, north] {
    east->setValue(0.0);
    north->setValue(0.0);
  });
  connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
  if (dialog.exec() == QDialog::Accepted) setOrigin(east->value(), north->value());
}
