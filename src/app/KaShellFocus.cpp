#include "KaShellFocus.h"

#include <QSplitter>
#include <QWidget>
#include <QtGlobal>

KaShellFocus::KaShellFocus(QSplitter* split, QObject* parent) : QObject(parent), m_split(split) {}

void KaShellFocus::setChrome(const QList<QWidget*>& chrome) {
  m_chrome.clear();
  for (QWidget* w : chrome) {
    if (w) m_chrome.append(w);
  }
}

int KaShellFocus::defaultLeftPanelWidth(int totalWidth) {
  const int cap = totalWidth < kCompactSplitWidth ? kCompactLeftWidth : kMaxLeftWidth;
  return qBound(kMinLeftWidth, qRound(totalWidth * 0.22), cap);
}

QList<int> KaShellFocus::sizesWithLeft(const QSplitter* split, int left) {
  if (!split || split->count() < 2) return {};
  const QList<int> now = split->sizes();
  // The current sizes add up to the space between the handles; the widget width is the
  // fallback before the first layout, when every size is still 0.
  int total = 0;
  for (int size : now) total += size;
  if (total <= 0) total = split->width();
  left = qMax(1, left);
  if (split->count() < 3) return {left, qMax(1, total - left)};
  const QWidget* rightWidget = split->widget(2);
  if (rightWidget && rightWidget->isHidden()) return {left, qMax(1, total - left), 0};
  const int right = now.value(2) > 0 ? now.value(2) : kDefaultRightWidth;
  return {left, qMax(1, total - left - right), right};
}

void KaShellFocus::applyDefaultWidthOnce() {
  if (m_widthDecided || !m_split || m_split->count() < 2 || m_leftCollapsed) return;
  const int total = m_split->width();
  if (total < 300) return;  // not laid out yet; try again on the next show
  m_widthDecided = true;
  m_split->setSizes(sizesWithLeft(m_split, defaultLeftPanelWidth(total)));
}

// After a pane hides, hand its width to the map (index 1) instead of letting the splitter
// share it out in proportion, which would also widen the pane on the other side.
void KaShellFocus::foldInto(int foldedIndex, const QList<int>& before) {
  if (!m_split || before.size() != m_split->count() || before.size() < 3) return;
  QList<int> sizes = before;
  sizes[1] += sizes.value(foldedIndex);
  sizes[foldedIndex] = 0;
  m_split->setSizes(sizes);
}

// A pane that was hidden when the sizes were saved is stored as 0; if it is visible now
// it keeps its current width rather than collapsing to nothing.
void KaShellFocus::restoreSizes(const QList<int>& saved) {
  if (!m_split || saved.size() != m_split->count()) return;
  QList<int> sizes = saved;
  const QList<int> now = m_split->sizes();
  for (int i = 0; i < sizes.size(); ++i) {
    const QWidget* w = m_split->widget(i);
    if (sizes[i] > 0 || !w || w->isHidden()) continue;
    sizes[i] = now.value(i) > 0 ? now.value(i) : (i == 2 ? kDefaultRightWidth : 1);
  }
  m_split->setSizes(sizes);
}

void KaShellFocus::applyLeftCollapsed(bool collapsed) {
  if (!m_split || m_split->count() < 2 || collapsed == m_leftCollapsed) return;
  QWidget* left = m_split->widget(0);
  if (!left) return;
  if (collapsed) {
    m_savedSizes = m_split->sizes();
    left->hide();
    foldInto(0, m_savedSizes);
  } else {
    left->show();
    restoreSizes(m_savedSizes);
  }
  m_leftCollapsed = collapsed;
}

void KaShellFocus::applyRightCollapsed(bool collapsed) {
  if (!m_split || m_split->count() < 3 || collapsed == m_rightCollapsed) return;
  QWidget* right = m_split->widget(2);
  if (!right) return;
  if (collapsed) {
    m_savedRightSizes = m_split->sizes();
    right->hide();
    foldInto(2, m_savedRightSizes);
  } else {
    right->show();
    restoreSizes(m_savedRightSizes);
  }
  m_rightCollapsed = collapsed;
}

void KaShellFocus::setLeftPanelCollapsed(bool collapsed) {
  if (collapsed == m_leftCollapsed) return;
  applyLeftCollapsed(collapsed);
  if (m_leftCollapsed != collapsed) return;
  emit changed(collapsed
                   ? QStringLiteral("왼쪽 패널을 접었습니다. %1 키를 누르면 다시 펼칩니다.").arg(leftPanelKey())
                   : QStringLiteral("왼쪽 패널을 펼쳤습니다."));
}

void KaShellFocus::toggleLeftPanel() { setLeftPanelCollapsed(!m_leftCollapsed); }

void KaShellFocus::setRightPanelCollapsed(bool collapsed) {
  if (collapsed == m_rightCollapsed) return;
  applyRightCollapsed(collapsed);
  if (m_rightCollapsed != collapsed) return;
  emit changed(collapsed
                   ? QStringLiteral("오른쪽 패널을 접었습니다. %1 키를 누르면 다시 펼칩니다.").arg(rightPanelKey())
                   : QStringLiteral("오른쪽 패널을 펼쳤습니다."));
}

void KaShellFocus::toggleRightPanel() { setRightPanelCollapsed(!m_rightCollapsed); }

void KaShellFocus::setMapFocused(bool focused) {
  if (focused == m_focused) return;
  if (focused) {
    m_leftCollapsedBeforeFocus = m_leftCollapsed;
    m_hiddenChrome.clear();
    for (const QPointer<QWidget>& w : m_chrome) {
      if (!w || w->isHidden()) continue;
      w->hide();
      m_hiddenChrome.append(w);
    }
    applyLeftCollapsed(true);
    m_focused = true;
    emit changed(QStringLiteral("지도 넓게 보기 — %1 키를 다시 누르면 리본과 왼쪽 패널이 돌아옵니다.")
                     .arg(mapFocusKey()));
    return;
  }
  for (const QPointer<QWidget>& w : m_hiddenChrome) {
    if (w) w->show();
  }
  m_hiddenChrome.clear();
  if (!m_leftCollapsedBeforeFocus) applyLeftCollapsed(false);
  m_focused = false;
  emit changed(QStringLiteral("지도 넓게 보기를 끝냈습니다."));
}

void KaShellFocus::toggleMapFocus() { setMapFocused(!m_focused); }

void KaShellFocus::restoreAll() {
  setMapFocused(false);
  applyLeftCollapsed(false);
  applyRightCollapsed(false);
}
