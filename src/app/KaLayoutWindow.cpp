#include "KaLayoutWindow.h"
#include "core/LayoutService.h"

#include <QComboBox>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QStatusBar>
#include <QToolBar>
#include <QVBoxLayout>
#include <QWidget>
#include <QAction>

#include <qgsproject.h>
#include <qgsmapcanvas.h>
#include <qgsprintlayout.h>
#include <qgslayoutmanager.h>
#include <qgslayoutview.h>
#include <qgslayoutviewtoolselect.h>
#include <qgslayoutviewtoolpan.h>
#include <qgslayoutviewtoolzoom.h>
#include <qgslayoutexporter.h>
#include <qgsmasterlayoutinterface.h>
#include <qgslayoutguiutils.h>
#include <qgslayoutitemguiregistry.h>
#include <qgsgui.h>

void KaLayoutWindow::ensureLayoutGuiRegistered(QgsMapCanvas* mapCanvas) {
  static bool once = false;
  if (once) return;
  once = true;
  if (mapCanvas)
    QgsLayoutGuiUtils::registerGuiForKnownItemTypes(mapCanvas);
}

KaLayoutWindow::KaLayoutWindow(QgsProject* project, QgsMapCanvas* mapCanvas, QWidget* parent)
    : QMainWindow(parent), m_project(project), m_mapCanvas(mapCanvas) {
  setAttribute(Qt::WA_DeleteOnClose, false);
  setWindowTitle(QStringLiteral("도면 편집"));
  resize(1280, 900);
  ensureLayoutGuiRegistered(mapCanvas);
  buildUi();
  refreshLayoutList();
}

void KaLayoutWindow::buildUi() {
  auto* tb = addToolBar(QStringLiteral("도면"));
  tb->setMovable(false);
  tb->setIconSize(QSize(24, 24));

  m_layoutCombo = new QComboBox(this);
  m_layoutCombo->setMinimumWidth(220);
  m_layoutCombo->setToolTip(QStringLiteral("편집할 도면 선택"));
  connect(m_layoutCombo, QOverload<int>::of(&QComboBox::activated), this, &KaLayoutWindow::onLayoutPicked);
  tb->addWidget(new QLabel(QStringLiteral(" 도면 "), this));
  tb->addWidget(m_layoutCombo);
  tb->addSeparator();

  auto* actSelect = tb->addAction(QStringLiteral("선택"), this, &KaLayoutWindow::useSelectTool);
  actSelect->setToolTip(QStringLiteral("항목 선택·이동 (QGIS 선택 도구)"));
  auto* actPan = tb->addAction(QStringLiteral("이동"), this, &KaLayoutWindow::usePanTool);
  actPan->setToolTip(QStringLiteral("도면 화면 이동"));
  auto* actZoom = tb->addAction(QStringLiteral("확대"), this, &KaLayoutWindow::useZoomTool);
  actZoom->setToolTip(QStringLiteral("드래그 확대"));
  tb->addAction(QStringLiteral("전체보기"), this, &KaLayoutWindow::zoomFull);
  tb->addSeparator();
  tb->addAction(QStringLiteral("목록갱신"), this, &KaLayoutWindow::refreshFromProject);
  tb->addAction(QStringLiteral("PDF보내기"), this, &KaLayoutWindow::exportPdf);

  auto* central = new QWidget(this);
  auto* lay = new QVBoxLayout(central);
  lay->setContentsMargins(4, 4, 4, 4);
  m_view = new QgsLayoutView(central);
  m_view->setObjectName(QStringLiteral("layoutView"));
  lay->addWidget(m_view, 1);
  setCentralWidget(central);

  m_toolSelect = new QgsLayoutViewToolSelect(m_view);
  m_toolPan = new QgsLayoutViewToolPan(m_view);
  m_toolZoom = new QgsLayoutViewToolZoom(m_view);
  m_view->setTool(m_toolSelect);

  m_status = new QLabel(this);
  statusBar()->addWidget(m_status, 1);
  m_status->setText(QStringLiteral(
      "도면 편집: 항목 선택 후 드래그·크기 조절 · PDF보내기로 출력 · 메인 창 레이어/지도가 도면 지도에 반영됩니다"));
}

void KaLayoutWindow::refreshLayoutList() {
  if (!m_layoutCombo || !m_project) return;
  const QString cur = m_layoutCombo->currentData().toString();
  m_layoutCombo->clear();
  LayoutService::ensureDefaultLayouts(m_project);
  const auto layouts = m_project->layoutManager()->layouts();
  for (QgsMasterLayoutInterface* ml : layouts) {
    if (!ml) continue;
    const QString name = ml->name();
    const QString label = LayoutService::koreanTitle(name) + QStringLiteral("  (") + name + QLatin1Char(')');
    m_layoutCombo->addItem(label, name);
  }
  if (m_layoutCombo->count() == 0) {
    m_status->setText(QStringLiteral("도면이 없습니다. 메인 창에서 「도면 만들기」(Ctrl+L)를 실행하세요."));
    return;
  }
  int idx = 0;
  if (!cur.isEmpty()) {
    const int found = m_layoutCombo->findData(cur);
    if (found >= 0) idx = found;
  }
  m_layoutCombo->setCurrentIndex(idx);
  onLayoutPicked(idx);
}

void KaLayoutWindow::openLayoutByName(const QString& layoutName) {
  refreshLayoutList();
  if (!layoutName.isEmpty()) {
    const int found = m_layoutCombo->findData(layoutName);
    if (found >= 0) {
      m_layoutCombo->setCurrentIndex(found);
      onLayoutPicked(found);
    }
  }
  show();
  raise();
  activateWindow();
}

void KaLayoutWindow::onLayoutPicked(int index) {
  if (!m_project || index < 0 || !m_layoutCombo) return;
  const QString name = m_layoutCombo->itemData(index).toString();
  QgsMasterLayoutInterface* master = m_project->layoutManager()->layoutByName(name);
  auto* printLayout = dynamic_cast<QgsPrintLayout*>(master);
  if (!printLayout) {
    m_status->setText(QStringLiteral("도면을 열 수 없습니다: %1").arg(name));
    return;
  }
  setActiveLayout(printLayout);
  m_status->setText(QStringLiteral("편집 중: %1").arg(LayoutService::koreanTitle(name)));
  setWindowTitle(QStringLiteral("도면 편집 — %1").arg(LayoutService::koreanTitle(name)));
}

void KaLayoutWindow::setActiveLayout(QgsLayout* layout) {
  if (!m_view || !layout) return;
  m_view->setCurrentLayout(layout);
  m_view->setTool(m_toolSelect);
  m_view->zoomFull();
}

void KaLayoutWindow::useSelectTool() {
  if (m_view && m_toolSelect) m_view->setTool(m_toolSelect);
}

void KaLayoutWindow::usePanTool() {
  if (m_view && m_toolPan) m_view->setTool(m_toolPan);
}

void KaLayoutWindow::useZoomTool() {
  if (m_view && m_toolZoom) m_view->setTool(m_toolZoom);
}

void KaLayoutWindow::zoomFull() {
  if (m_view) m_view->zoomFull();
}

void KaLayoutWindow::refreshFromProject() {
  refreshLayoutList();
}

void KaLayoutWindow::exportPdf() {
  if (!m_view || !m_view->currentLayout()) {
    QMessageBox::information(this, QStringLiteral("PDF"), QStringLiteral("열린 도면이 없습니다."));
    return;
  }
  auto* layout = m_view->currentLayout();
  QString name = m_layoutCombo ? m_layoutCombo->currentData().toString() : QString();
  if (name.isEmpty()) {
    if (auto* pl = dynamic_cast<QgsPrintLayout*>(layout))
      name = pl->name();
  }
  if (name.isEmpty()) name = QStringLiteral("layout");
  const QString path = QFileDialog::getSaveFileName(
      this, QStringLiteral("도면 PDF 저장"),
      LayoutService::koreanTitle(name) + QStringLiteral(".pdf"),
      QStringLiteral("PDF (*.pdf)"));
  if (path.isEmpty()) return;

  QgsLayoutExporter exporter(layout);
  QgsLayoutExporter::PdfExportSettings settings;
  settings.dpi = 300;
  const auto result = exporter.exportToPdf(path, settings);
  if (result != QgsLayoutExporter::Success) {
    QMessageBox::warning(this, QStringLiteral("PDF"), QStringLiteral("PDF 내보내기 실패"));
    return;
  }
  m_status->setText(QStringLiteral("PDF 저장: %1").arg(path));
  QMessageBox::information(this, QStringLiteral("PDF"), QStringLiteral("저장 완료:\n%1").arg(path));
}
