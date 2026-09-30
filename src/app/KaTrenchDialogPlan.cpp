// KaTrenchDialog preview: debounced, cached, computed off the UI thread (F097), and the
// prompt before a re-generation replaces a hand-adjusted grid (F156).
#include "KaTrenchDialog.h"
#include "core/TrenchPlanCache.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMessageBox>
#include <QMetaObject>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QThreadPool>
#include <QTimer>

#include <utility>

void KaTrenchDialog::schedulePlan() {
  refreshRows();
  ++m_planSeq;  // an answer for the previous input is stale from now on
  m_planPending = true;
  m_planTimer->start();
}

void KaTrenchDialog::refreshRows() {
  const bool fill = autoFill();
  const bool ratio = fill && ratioMode();
  // 지금 쓰지 않는 칸은 회색으로 두지 않고 숨긴다. 자동 배치에서 행·열 「2」가 보이면
  // 트렌치 22개와 어긋나 보였다. 비율 모드의 길이·둑은 아래 결과 칸에 적힌다.
  // 비율 모드의 이름은 조사 종류가 정한다(시굴 Tr-, 표본 Sp-). 접두 칸은 쓰이지 않아 숨긴다.
  if (auto* form = qobject_cast<QFormLayout*>(layout())) {
    form->setRowVisible(m_rows, !fill);
    form->setRowVisible(m_cols, !fill);
    form->setRowVisible(m_size, !ratio);
    form->setRowVisible(m_balk, !ratio);
    form->setRowVisible(m_prefix, !ratio);
  }
  m_az->setEnabled(!useTerrainAzimuth());
}

void KaTrenchDialog::refreshPlan() {
  m_planTimer->stop();
  refreshRows();
  const quint64 seq = ++m_planSeq;
  if (!autoFill()) {
    m_planPending = false;
    const TrenchGridGenerator::Spec sp = spec();
    const double t = sp.rows * sp.cols * sp.trenchWidth * sp.trenchLength;
    setSummaryTone("plain");
    m_ratio->setText(QStringLiteral("수동 배치: %1개 · 총 %2㎡ — 「맵에 찍기」로 맵에서 위치를 정하세요.")
                         .arg(sp.rows * sp.cols)
                         .arg(QLocale().toString(t, 'f', 0)));
    return;
  }
  const QByteArray area = m_areaWkb;
  if (ratioMode()) {
    const double target = targetPct();
    const double az = effectiveAzimuth();
    TrenchGridGenerator::RatioFill known;
    if (TrenchPlanCache::lookup(area, target, 2.0, az, &known)) {
      m_planPending = false;
      showRatioPlan(known, az);
      return;
    }
    m_planPending = true;
    setSummaryTone("plain");
    m_ratio->setText(QStringLiteral("배치를 계산하는 중입니다…"));
    // The pool is drained in the destructor, so `this` outlives the task.
    m_planPool->start([this, seq, area, target, az]() {
      const TrenchGridGenerator::RatioFill plan = TrenchPlanCache::ratioPlan(area, target, 2.0, az);
      QMetaObject::invokeMethod(this, [this, seq, plan, az]() {
        if (seq != m_planSeq) return;
        m_planPending = false;
        showRatioPlan(plan, az);
      }, Qt::QueuedConnection);
    });
    return;
  }
  const TrenchGridGenerator::Spec sp = spec();
  m_planPending = true;
  setSummaryTone("plain");
  m_ratio->setText(QStringLiteral("배치를 계산하는 중입니다…"));
  m_planPool->start([this, seq, area, sp]() {
    QString reason;
    std::vector<TrenchGridGenerator::Cell> cells = TrenchGridGenerator::buildInArea(sp, area, &reason);
    QMetaObject::invokeMethod(this, [this, seq, cells = std::move(cells), reason]() {
      if (seq != m_planSeq) return;
      m_planPending = false;
      showFillPlan(cells, reason);
    }, Qt::QueuedConnection);
  });
}

void KaTrenchDialog::showRatioPlan(const TrenchGridGenerator::RatioFill& plan, double azimuth) {
  if (plan.cells.empty()) {
    setSummaryTone("warn");
    m_ratio->setText(plan.error);
    return;
  }
  {
    const QSignalBlocker blocker(m_balk);
    m_balk->setValue(plan.balk);
  }
  setSummaryTone("ok");
  m_ratio->setText(
      QStringLiteral("%1 · 트렌치 %2개 · 폭 2 m · 최대 길이 %3 m · 둑 %4 m · 총 %5㎡ · 비율 %6% (목표 %7%) · 방위 %8°")
          .arg(surveyKind() == SurveyKind::Trial ? QStringLiteral("시굴조사") : QStringLiteral("표본조사"))
          .arg(plan.cells.size())
          .arg(QLocale().toString(plan.length, 'f', 2))
          .arg(QLocale().toString(plan.balk, 'f', 0))
          .arg(QLocale().toString(TrenchGridGenerator::totalArea(plan.cells), 'f', 2))
          .arg(QLocale().toString(plan.ratioPct, 'f', 1))
          .arg(QLocale().toString(targetPct(), 'f', 0))
          .arg(QLocale().toString(azimuth, 'f', 0)));
}

void KaTrenchDialog::showFillPlan(const std::vector<TrenchGridGenerator::Cell>& cells,
                                  const QString& reason) {
  if (cells.empty()) {
    // Say why (invalid boundary, spec, nothing fits) instead of 「트렌치 0개」.
    setSummaryTone("warn");
    m_ratio->setText(reason.isEmpty()
                         ? QStringLiteral("현재 규격과 방향으로는 구역 안에 들어가는 트렌치가 없습니다.")
                         : reason);
    return;
  }
  const double t = TrenchGridGenerator::totalArea(cells);
  const double pct = m_areaM2 > 0.0 ? t / m_areaM2 * 100.0 : 0.0;
  const bool over = pct > 10.0;
  setSummaryTone(over ? "warn" : "ok");
  m_ratio->setText(QStringLiteral("트렌치 %1개 · 총 %2㎡ · 시굴 비율 %3% (기준: 시굴 10% · 표본 2%)%4")
                       .arg(cells.size())
                       .arg(QLocale().toString(t, 'f', 0))
                       .arg(QLocale().toString(pct, 'f', 1))
                       .arg(over ? QStringLiteral(" — 간격을 키우세요") : QString()));
}

bool KaTrenchDialog::confirmReplaceAdjusted(QWidget* parent, qint64 trenchCount) {
  QMessageBox box(parent);
  box.setIcon(QMessageBox::Question);
  box.setWindowTitle(QStringLiteral("시굴격자"));
  box.setText(QStringLiteral("이미 깔린 시굴격자를 새 격자로 바꿀까요?"));
  box.setInformativeText(
      QStringLiteral("지금 격자(트렌치 %1개)는 손으로 옮기거나 지운 배치까지 새 배치로 바뀝니다.\n"
                     "바꾼 뒤에도 Ctrl+Z로 지금 격자를 되살릴 수 있습니다.")
          .arg(trenchCount));
  QPushButton* replace = box.addButton(QStringLiteral("새 격자로 바꾸기"), QMessageBox::AcceptRole);
  QPushButton* keep = box.addButton(QStringLiteral("그대로 두기"), QMessageBox::RejectRole);
  box.setDefaultButton(keep);
  box.setEscapeButton(keep);
  box.exec();
  return box.clickedButton() == replace;
}
