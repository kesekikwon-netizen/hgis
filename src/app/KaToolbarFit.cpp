#include "KaToolbarFit.h"

#include <QEvent>
#include <QLayout>
#include <QObject>
#include <QResizeEvent>
#include <QScopedValueRollback>
#include <QTimer>
#include <QToolBar>

namespace {

// The stages, biggest first. The first keeps the bar's own tool button style (labels beside or under the
// icon), the others show the icons alone, then smaller.
struct Stage {
  bool keepLabels;
  int icon;
};
constexpr Stage kStages[] = {{true, 20}, {false, 20}, {false, 16}};

class Fit final : public QObject {
public:
  explicit Fit(QToolBar* bar) : QObject(bar), m_bar(bar), m_labelStyle(bar->toolButtonStyle()) {
    setObjectName(QStringLiteral("kaToolbarFit"));
    bar->installEventFilter(this);
  }

  bool eventFilter(QObject* watched, QEvent* event) override {
    if (watched != m_bar) return false;
    switch (event->type()) {
      case QEvent::Resize: {
        // Only a new width changes what fits (our own stages change the height). Fitting here, before the
        // bar's layout handles the resize, means the » button never shows for a frame.
        const int width = static_cast<QResizeEvent*>(event)->size().width();
        if (width != m_width) {
          m_width = width;
          fit();
        }
        break;
      }
      case QEvent::Show:
      case QEvent::ActionAdded:
      case QEvent::ActionRemoved:
      case QEvent::ActionChanged:  // the bar builds its buttons after this filter has run: fit once it has
        fitLater();
        break;
      default:
        break;
    }
    return false;
  }

private:
  void fitLater() {
    if (m_queued) return;
    m_queued = true;
    QTimer::singleShot(0, this, [this] {
      m_queued = false;
      fit();
    });
  }

  void apply(const Stage& stage) {
    const Qt::ToolButtonStyle style = stage.keepLabels ? m_labelStyle : Qt::ToolButtonIconOnly;
    if (m_bar->toolButtonStyle() != style) m_bar->setToolButtonStyle(style);
    if (m_bar->iconSize() != QSize(stage.icon, stage.icon)) m_bar->setIconSize(QSize(stage.icon, stage.icon));
  }

  // The first stage whose buttons fit the bar's width; the last one when none does. Nothing is painted
  // between the trials, so the bar only ever shows the stage that stays.
  void fit() {
    if (m_busy || !m_bar->isVisible()) return;
    const QScopedValueRollback<bool> busy(m_busy, true);
    for (const Stage& stage : kStages) {
      apply(stage);
      if (m_bar->sizeHint().width() <= m_bar->width()) break;
    }
  }

  QToolBar* m_bar;
  Qt::ToolButtonStyle m_labelStyle;
  int m_width = -1;
  bool m_queued = false;
  bool m_busy = false;
};

}  // namespace

namespace KaToolbarFit {

void install(QToolBar* bar) {
  if (!bar || bar->findChild<QObject*>(QStringLiteral("kaToolbarFit"), Qt::FindDirectChildrenOnly)) return;
  new Fit(bar);
}

}  // namespace KaToolbarFit
