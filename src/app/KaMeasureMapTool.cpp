#include "KaCrashGuard.h"
#include "core/KaLogExcept.h"
#include "KaMeasureMapTool.h"
#include "core/MeasureOps.h"

#include <cmath>

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QFont>
#include <QFrame>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QMenu>
#include <QPushButton>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidget>

#include <qgscoordinatereferencesystem.h>
#include <qgscoordinatetransformcontext.h>
#include <qgsmapcanvas.h>
#include <qgsproject.h>
#include <qgsrubberband.h>
#include <qgspointlocator.h>
#include <qgssnappingutils.h>
#include <qgsvertexmarker.h>

namespace {
const QColor kTape(30, 103, 198);
}

KaMeasureMapTool::KaMeasureMapTool(QgsMapCanvas* canvas)
    : QgsMapTool(canvas) {
  setCursor(Qt::CrossCursor);
}

KaMeasureMapTool::~KaMeasureMapTool() {
  destroyGraphics();
  if (m_hud)
    m_hud->deleteLater();
}

void KaMeasureMapTool::setMode(Mode mode) {
  if (m_mode == mode)
    return;
  m_mode = mode;
  resetSession();
}

void KaMeasureMapTool::setSnapEnabled(bool on) {
  m_snapEnabled = on;
  if (!on)
    updateSnapMarker(QgsPointXY(), false);
}

void KaMeasureMapTool::resetSession() {
  m_points.clear();
  m_finished = false;
  destroyGraphics();
  refreshHud(nullptr);
  emit statusMessage(m_mode == Mode::Area
                         ? QStringLiteral("면적 줄자: 꼭짓점을 찍고 우클릭 → 마침.")
                         : QStringLiteral("거리 줄자: 점을 찍고 우클릭 → 마침."));
}

QgsMapTool::Flags KaMeasureMapTool::flags() const {
  return QgsMapTool::AllowZoomRect;
}

QgsCoordinateReferenceSystem KaMeasureMapTool::measureCrs() const {
  if (canvas() && canvas()->mapSettings().destinationCrs().isValid())
    return canvas()->mapSettings().destinationCrs();
  if (QgsProject::instance() && QgsProject::instance()->crs().isValid())
    return QgsProject::instance()->crs();
  return QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186"));
}

QgsCoordinateTransformContext KaMeasureMapTool::transformContext() const {
  if (QgsProject::instance())
    return QgsProject::instance()->transformContext();
  return {};
}

void KaMeasureMapTool::ensureHud() {
  if (m_hud || !canvas())
    return;
  auto* host = canvas()->viewport() ? canvas()->viewport() : static_cast<QWidget*>(canvas());
  m_hud = new QFrame(host);
  m_hud->setObjectName(QStringLiteral("measureHud"));
  m_hud->setStyleSheet(
      QStringLiteral("QFrame#measureHud { background: rgba(248,251,255,230); border: 1px solid #94A3B8;"
                     " border-radius: 8px; }"
                     "QLabel { color: #0F172A; }"
                     "QToolButton { padding: 4px 10px; }"
                     "QPushButton { padding: 3px 8px; }"));
  auto* lay = new QVBoxLayout(m_hud);
  lay->setContentsMargins(10, 8, 10, 8);
  lay->setSpacing(4);

  auto* modes = new QHBoxLayout();
  m_btnDist = new QToolButton(m_hud);
  m_btnDist->setText(QStringLiteral("거리"));
  m_btnDist->setCheckable(true);
  m_btnDist->setChecked(true);
  m_btnArea = new QToolButton(m_hud);
  m_btnArea->setText(QStringLiteral("면적"));
  m_btnArea->setCheckable(true);
  modes->addWidget(m_btnDist);
  modes->addWidget(m_btnArea);
  modes->addStretch(1);
  lay->addLayout(modes);

  m_total = new QLabel(QStringLiteral("0 m"), m_hud);
  QFont tf = m_total->font();
  tf.setPointSize(14);
  tf.setBold(true);
  m_total->setFont(tf);
  lay->addWidget(m_total);

  m_detail = new QLabel(QStringLiteral("점을 찍으면 구간이 쌓입니다."), m_hud);
  m_detail->setWordWrap(true);
  lay->addWidget(m_detail);

  auto* row = new QHBoxLayout();
  auto* finishBtn = new QPushButton(QStringLiteral("마침"), m_hud);
  auto* copy = new QPushButton(QStringLiteral("복사"), m_hud);
  auto* undoBtn = new QPushButton(QStringLiteral("한 점 취소"), m_hud);
  auto* again = new QPushButton(QStringLiteral("다시"), m_hud);
  row->addWidget(finishBtn);
  row->addWidget(copy);
  row->addWidget(undoBtn);
  row->addWidget(again);
  lay->addLayout(row);

  m_hint = new QLabel(QStringLiteral("클릭 점 · 우클릭에서 마침 · Backspace 한 점 취소 · Esc 지우기"), m_hud);
  m_hint->setWordWrap(true);
  m_hint->setStyleSheet(QStringLiteral("color:#475569; font-size:11px;"));
  lay->addWidget(m_hint);

  connect(m_btnDist, &QToolButton::clicked, this, [this]() { setMode(Mode::Distance); });
  connect(m_btnArea, &QToolButton::clicked, this, [this]() { setMode(Mode::Area); });
  connect(finishBtn, &QPushButton::clicked, this, [this]() { finish(); });
  connect(copy, &QPushButton::clicked, this, [this]() {
    QApplication::clipboard()->setText(copyText());
    emit statusMessage(QStringLiteral("줄자 숫자를 복사했습니다."));
  });
  connect(undoBtn, &QPushButton::clicked, this, [this]() { undoVertex(); });
  connect(again, &QPushButton::clicked, this, [this]() { resetSession(); });

  m_hud->setFixedWidth(300);
  m_hud->adjustSize();
  placeHud();
  m_hud->show();
  m_hud->raise();
}

void KaMeasureMapTool::placeHud() {
  if (!m_hud || !canvas())
    return;
  auto* host = canvas()->viewport() ? canvas()->viewport() : static_cast<QWidget*>(canvas());
  int x = 10;
  int y = 10;
  if (host) {
    if (auto* rail = host->findChild<QWidget*>(QStringLiteral("layerOpacityRail"))) {
      if (rail->isVisible()) {
        x = rail->geometry().right() + 8;
        y = rail->geometry().top();
      }
    }
  }
  m_hud->move(x, y);
  m_hud->raise();
}

void KaMeasureMapTool::destroyGraphics() {
  if (m_rubber) {
    m_rubber->reset(Qgis::GeometryType::Line);
    delete m_rubber;
    m_rubber = nullptr;
  }
  delete m_snapMark;
  m_snapMark = nullptr;
  qDeleteAll(m_vertexMarks);
  m_vertexMarks.clear();
}

void KaMeasureMapTool::updateSnapMarker(const QgsPointXY& mapPt, bool snapped) {
  QgsMapCanvas* c = canvas();
  if (!c || !m_snapEnabled || !snapped) {
    if (m_snapMark)
      m_snapMark->hide();
    return;
  }
  if (!m_snapMark) {
    m_snapMark = new QgsVertexMarker(c);
    m_snapMark->setIconType(QgsVertexMarker::ICON_CIRCLE);
    m_snapMark->setIconSize(14);
    m_snapMark->setPenWidth(2);
    m_snapMark->setColor(kTape);
    m_snapMark->setFillColor(QColor(255, 255, 255, 220));
  }
  m_snapMark->setCenter(mapPt);
  m_snapMark->show();
}

bool KaMeasureMapTool::mapPointFromEvent(QgsMapMouseEvent* e, QgsPointXY* out, bool* snappedOut) {
  if (!e || !out || !canvas())
    return false;
  bool snapped = false;
  try {
    *out = e->mapPoint();
  } catch (...) {
    KA_LOG_EXCEPT();
    try {
      *out = toMapCoordinates(e->pos());
    } catch (...) {
      KA_LOG_EXCEPT();
      return false;
    }
  }
  if (m_snapEnabled && canvas()->snappingUtils()) {
    const QgsPointLocator::Match hit = canvas()->snappingUtils()->snapToMap(e->pos());
    if (hit.isValid()) {
      *out = hit.point();
      snapped = true;
    }
  }
  if (std::isnan(out->x()) || std::isnan(out->y()))
    return false;
  updateSnapMarker(*out, snapped);
  if (snappedOut)
    *snappedOut = snapped;
  return true;
}

void KaMeasureMapTool::rebuildRubber(const QgsPointXY* cursorOrNull) {
  QgsMapCanvas* c = canvas();
  if (!c)
    return;
  const Qgis::GeometryType gt =
      (m_mode == Mode::Area) ? Qgis::GeometryType::Polygon : Qgis::GeometryType::Line;
  if (!m_rubber) {
    m_rubber = new QgsRubberBand(c, gt);
    m_rubber->setWidth(3);
    m_rubber->setSecondaryStrokeColor(QColor(255, 255, 255, 200));
    m_rubber->setColor(kTape);
    m_rubber->setFillColor(QColor(30, 103, 198, 70));
  }
  m_rubber->reset(gt);
  for (const QgsPointXY& p : m_points)
    m_rubber->addPoint(p, false);
  if (cursorOrNull && !m_finished)
    m_rubber->addPoint(*cursorOrNull, true);
  else if (!m_points.isEmpty())
    m_rubber->addPoint(m_points.last(), true);

  while (m_vertexMarks.size() > m_points.size())
    delete m_vertexMarks.takeLast();
  while (m_vertexMarks.size() < m_points.size()) {
    auto* mk = new QgsVertexMarker(c);
    mk->setIconType(QgsVertexMarker::ICON_CIRCLE);
    mk->setIconSize(9);
    mk->setPenWidth(2);
    mk->setColor(kTape);
    mk->setFillColor(QColor(255, 255, 255));
    m_vertexMarks.append(mk);
  }
  for (int i = 0; i < m_points.size(); ++i)
    m_vertexMarks[i]->setCenter(m_points.at(i));
}

void KaMeasureMapTool::addVertex(const QgsPointXY& pt) {
  if (m_finished)
    resetSession();
  m_points.append(pt);
  m_finished = false;
  rebuildRubber(nullptr);
  refreshHud(nullptr);
}

void KaMeasureMapTool::undoVertex() {
  if (m_points.isEmpty())
    return;
  m_finished = false;
  m_points.removeLast();
  rebuildRubber(nullptr);
  refreshHud(nullptr);
}

void KaMeasureMapTool::finish() {
  if (m_mode == Mode::Distance && m_points.size() < 2) {
    emit statusMessage(QStringLiteral("거리를 마치려면 점을 두 개 이상 찍으세요."));
    return;
  }
  if (m_mode == Mode::Area && m_points.size() < 3) {
    emit statusMessage(QStringLiteral("면적을 마치려면 꼭짓점을 세 개 이상 찍은 뒤 마침을 고르세요."));
    return;
  }
  m_finished = true;
  rebuildRubber(nullptr);
  refreshHud(nullptr);
  emit statusMessage(m_total ? m_total->text() : QStringLiteral("측정했습니다."));
}

void KaMeasureMapTool::showTapeMenu(const QPoint& globalPos) {
  QMenu menu;
  QAction* finishAct = menu.addAction(QStringLiteral("마침"));
  const bool canFinish = (m_mode == Mode::Area) ? (m_points.size() >= 3) : (m_points.size() >= 2);
  finishAct->setEnabled(canFinish);
  QFont f = finishAct->font();
  f.setBold(true);
  finishAct->setFont(f);
  QAction* undoAct = menu.addAction(QStringLiteral("한 점 취소"));
  undoAct->setEnabled(!m_points.isEmpty());
  menu.addSeparator();
  QAction* againAct = menu.addAction(QStringLiteral("다시"));
  QAction* picked = menu.exec(globalPos);
  if (picked == finishAct)
    finish();
  else if (picked == undoAct)
    undoVertex();
  else if (picked == againAct)
    resetSession();
}

QString KaMeasureMapTool::copyText() const {
  const auto crs = measureCrs();
  const auto ctx = transformContext();
  QStringList lines;
  if (m_mode == Mode::Area) {
    lines << QStringLiteral("면적 %1").arg(
        MeasureOps::formatAreaM2(MeasureOps::polygonAreaSquareMeters(m_points, crs, ctx)));
    if (m_points.size() >= 3)
      lines << QStringLiteral("둘레 %1").arg(
          MeasureOps::formatLengthM(MeasureOps::polygonPerimeterMeters(m_points, crs, ctx)));
  } else {
    lines << QStringLiteral("거리 %1").arg(
        MeasureOps::formatLengthM(MeasureOps::lineLengthMeters(m_points, crs, ctx)));
    const auto segs = MeasureOps::segmentLengthsMeters(m_points, crs, ctx);
    for (int i = 0; i < segs.size(); ++i)
      lines << QStringLiteral("%1구간 %2 · 도북 방위 %3")
                   .arg(i + 1)
                   .arg(MeasureOps::formatLengthM(segs[i]))
                   .arg(MeasureOps::formatBearing(
                       MeasureOps::gridBearingDegrees(m_points.at(i), m_points.at(i + 1))));
  }
  if (crs.isValid())
    lines << QStringLiteral("작업좌표 %1 평면").arg(crs.authid());
  return lines.join(QLatin1Char('\n'));
}

void KaMeasureMapTool::refreshHud(const QgsPointXY* cursorOrNull) {
  ensureHud();
  if (m_btnDist)
    m_btnDist->setChecked(m_mode == Mode::Distance);
  if (m_btnArea)
    m_btnArea->setChecked(m_mode == Mode::Area);

  QVector<QgsPointXY> live = m_points;
  if (cursorOrNull && !m_finished)
    live.append(*cursorOrNull);
  const auto crs = measureCrs();
  const auto ctx = transformContext();

  QString total;
  QStringList bits;
  if (m_mode == Mode::Area) {
    const double area = MeasureOps::polygonAreaSquareMeters(live, crs, ctx);
    total = MeasureOps::formatAreaM2(area);
    if (live.size() >= 3)
      bits << QStringLiteral("둘레 %1").arg(
          MeasureOps::formatLengthM(MeasureOps::polygonPerimeterMeters(live, crs, ctx)));
    if (m_finished)
      bits << QStringLiteral("면적만 산출");
    else if (live.size() < 3)
      bits << QStringLiteral("세 점 이상 찍고 우클릭 → 마침");
    else
      bits << QStringLiteral("우클릭에서 마침을 고르면 면적만 확정");
  } else {
    const double len = MeasureOps::lineLengthMeters(live, crs, ctx);
    total = MeasureOps::formatLengthM(len);
    if (live.size() < 2)
      bits << QStringLiteral("두 점 이상 찍고 우클릭 → 마침");
    else {
      const auto segs = MeasureOps::segmentLengthsMeters(live, crs, ctx);
      for (int i = 0; i < segs.size() && i < 8; ++i)
        bits << QStringLiteral("%1구간 %2").arg(i + 1).arg(MeasureOps::formatLengthM(segs[i]));
      // Grid bearing of the segment being drawn (or the last one once finished).
      bits << QStringLiteral("도북 방위 %1").arg(MeasureOps::formatBearing(
                  MeasureOps::gridBearingDegrees(live.at(live.size() - 2), live.last())));
      if (!m_finished)
        bits << QStringLiteral("우클릭 → 마침");
    }
  }
  bits << QStringLiteral("작업좌표 %1 평면").arg(crs.authid());

  if (m_total)
    m_total->setText(total);
  if (m_detail)
    m_detail->setText(bits.join(QStringLiteral(" · ")));
  if (m_hud) {
    m_hud->adjustSize();
    placeHud();
    m_hud->show();
  }
}

void KaMeasureMapTool::canvasPressEvent(QgsMapMouseEvent* e) {
  if (!e)
    return;
  if (e->button() == Qt::RightButton) {
    showTapeMenu(e->globalPosition().toPoint());
    return;
  }
  if (e->button() != Qt::LeftButton)
    return;
  QgsPointXY pt;
  if (!mapPointFromEvent(e, &pt))
    return;
  addVertex(pt);
}

void KaMeasureMapTool::canvasMoveEvent(QgsMapMouseEvent* e) {
  QgsPointXY pt;
  if (!mapPointFromEvent(e, &pt))
    return;
  if (m_points.isEmpty() || m_finished)
    return;
  rebuildRubber(&pt);
  refreshHud(&pt);
}

void KaMeasureMapTool::canvasDoubleClickEvent(QgsMapMouseEvent* e) {
  if (e && e->button() == Qt::LeftButton && !m_points.isEmpty()) {
    // Double-click also fires press; drop the duplicate last vertex if identical.
    if (m_points.size() >= 2) {
      const double px = canvas() ? canvas()->mapSettings().mapUnitsPerPixel() : 1.0;
      const double tol = std::max(0.05, px * 10.0);
      if (m_points[m_points.size() - 1].sqrDist(m_points[m_points.size() - 2]) < tol * tol)
        m_points.removeLast();
    }
  }
  finish();
}

void KaMeasureMapTool::keyPressEvent(QKeyEvent* e) {
  if (!e)
    return;
  if (e->key() == Qt::Key_Escape) {
    resetSession();
    e->accept();
    return;
  }
  if (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) {
    finish();
    e->accept();
    return;
  }
  if (e->key() == Qt::Key_Backspace || e->key() == Qt::Key_Z) {
    undoVertex();
    e->accept();
    return;
  }
  if (e->key() == Qt::Key_D) {
    setMode(Mode::Distance);
    e->accept();
    return;
  }
  if (e->key() == Qt::Key_A) {
    setMode(Mode::Area);
    e->accept();
    return;
  }
  QgsMapTool::keyPressEvent(e);
}

void KaMeasureMapTool::activate() {
  if (canvas()) {
    canvas()->setMouseTracking(true);
    m_savedMenuPolicy = canvas()->contextMenuPolicy();
    canvas()->setContextMenuPolicy(Qt::PreventContextMenu);
  }
  QgsMapTool::activate();
  setCursor(Qt::CrossCursor);
  ensureHud();
  if (m_hud)
    m_hud->show();
  if (!m_points.isEmpty())
    rebuildRubber(nullptr);
  refreshHud(nullptr);
  emit statusMessage(QStringLiteral("줄자: 점을 찍고 우클릭에서 마침을 고르세요."));
}

void KaMeasureMapTool::deactivate() {
  if (m_hud)
    m_hud->hide();
  destroyGraphics();
  if (canvas())
    canvas()->setContextMenuPolicy(m_savedMenuPolicy);
  QgsMapTool::deactivate();
}
