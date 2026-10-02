// KaTheme::ChromeStyle: the Fusion proxy style behind ka-hgis.qss. It paints the
// parts a style sheet cannot draw without image files (url() is not allowed).
#include "KaTheme.h"

#include <QAbstractSpinBox>
#include <QPainter>
#include <QPainterPath>
#include <QProxyStyle>
#include <QPushButton>
#include <QStyleOption>
#include <QWidget>

namespace KaTheme {
namespace {

// Field chrome shared by combo and spin boxes: one rounded face and a hairline
// edge that turns accent on focus. The style sheet leaves their border native so
// these land here; a QSS border would hand them to QWindowsStyle, which draws no
// arrow beside a styled drop-down and a Win95 bevel around spin buttons.
void drawFieldFace(QPainter* p, const QRectF& rect, const QColor& fill, const QStyleOption* opt) {
  const auto& colors = tokens();
  const bool enabled = opt->state.testFlag(QStyle::State_Enabled);
  const bool focus = enabled && opt->state.testFlag(QStyle::State_HasFocus);
  const qreal radius = rect.height() < 28 ? 6.0 : 8.0;
  p->save();
  p->setRenderHint(QPainter::Antialiasing, true);
  p->setPen(QPen(focus ? colors.focusRing : colors.border, 1.0));
  p->setBrush(fill);
  p->drawRoundedRect(rect.adjusted(0.5, 0.5, -0.5, -0.5), radius, radius);
  p->restore();
}

// One thin chevron for combo, spin and menu arrows.
void drawChevron(QPainter* p, const QPointF& c, bool up, qreal half, qreal rise, const QColor& ink) {
  p->save();
  p->setRenderHint(QPainter::Antialiasing, true);
  const qreal tip = up ? -rise : rise;
  QPainterPath path;
  path.moveTo(c.x() - half, c.y() - tip);
  path.lineTo(c.x(), c.y() + tip);
  path.lineTo(c.x() + half, c.y() - tip);
  p->setPen(QPen(ink, 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
  p->setBrush(Qt::NoBrush);
  p->drawPath(path);
  p->restore();
}

void drawCheckIndicator(const QStyleOption* opt, QPainter* p) {
  p->save();
  p->setRenderHint(QPainter::Antialiasing, true);
  QRect box = opt->rect;
  if (box.width() < 14 || box.height() < 14)
    box = QRect(box.center().x() - 8, box.center().y() - 8, 16, 16);
  const QRectF r = QRectF(box).adjusted(1.0, 1.0, -1.0, -1.0);
  const bool on = opt->state.testFlag(QStyle::State_On);
  const bool part = opt->state.testFlag(QStyle::State_NoChange);
  const bool dis = !opt->state.testFlag(QStyle::State_Enabled);
  const bool hover = opt->state.testFlag(QStyle::State_MouseOver);
  const auto& colors = tokens();
  const QColor edge = hover ? colors.accentDeep : colors.accentHover;
  const QColor tickInk = dis ? colors.inkDisabled : colors.surface;
  const QColor stone = hover ? colors.accentHover : colors.borderStrong;
  p->setPen(QPen(dis ? colors.border : ((on || part) ? edge : stone), 1.1));
  p->setBrush(dis ? colors.disabledSurface : ((on || part) ? colors.accent : colors.surface));
  p->drawRoundedRect(r, 3.5, 3.5);
  if (on) {
    QPainterPath tick;
    tick.moveTo(r.left() + r.width() * 0.22, r.center().y() + r.height() * 0.02);
    tick.lineTo(r.left() + r.width() * 0.40, r.bottom() - r.height() * 0.28);
    tick.lineTo(r.right() - r.width() * 0.20, r.top() + r.height() * 0.26);
    p->setPen(QPen(tickInk, 1.85, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p->setBrush(Qt::NoBrush);
    p->drawPath(tick);
  } else if (part) {
    const QRectF bar = r.adjusted(r.width() * 0.22, r.height() * 0.42, -r.width() * 0.22, -r.height() * 0.42);
    p->setPen(Qt::NoPen);
    p->setBrush(tickInk);
    p->drawRoundedRect(bar, 1.2, 1.2);
  }
  p->restore();
}

// The radio twin of the check box: the same edge colours, an accent dot when on.
void drawRadioIndicator(const QStyleOption* opt, QPainter* p) {
  p->save();
  p->setRenderHint(QPainter::Antialiasing, true);
  const QRectF r = QRectF(opt->rect).adjusted(1.0, 1.0, -1.0, -1.0);
  const bool on = opt->state.testFlag(QStyle::State_On);
  const bool dis = !opt->state.testFlag(QStyle::State_Enabled);
  const bool hover = opt->state.testFlag(QStyle::State_MouseOver);
  const auto& colors = tokens();
  const QColor edge = dis ? colors.border : on ? colors.accent : hover ? colors.accentHover : colors.borderStrong;
  p->setPen(QPen(edge, on ? 1.6 : 1.1));
  p->setBrush(dis ? colors.disabledSurface : colors.surface);
  p->drawEllipse(r);
  if (on) {
    p->setPen(Qt::NoPen);
    p->setBrush(dis ? colors.inkDisabled : colors.accent);
    const qreal d = r.width() * 0.26;
    p->drawEllipse(r.adjusted(d, d, -d, -d));
  }
  p->restore();
}

// A thin cross; red only under the pointer, not a red square on every tab.
void drawTabClose(const QStyleOption* opt, QPainter* p) {
  p->save();
  p->setRenderHint(QPainter::Antialiasing, true);
  const auto& colors = tokens();
  const QRectF r = QRectF(opt->rect).adjusted(1.0, 1.0, -1.0, -1.0);
  const bool hot = opt->state.testFlag(QStyle::State_Raised) || opt->state.testFlag(QStyle::State_Sunken);
  if (hot) {
    p->setPen(Qt::NoPen);
    p->setBrush(colors.dangerSurface);
    p->drawRoundedRect(r, 5.0, 5.0);
  }
  const QColor ink = hot ? colors.danger
                         : opt->state.testFlag(QStyle::State_Selected) ? colors.inkMuted : colors.borderStrong;
  const QPointF c = r.center();
  const qreal a = 3.6;
  p->setPen(QPen(ink, 1.5, Qt::SolidLine, Qt::RoundCap));
  p->drawLine(c + QPointF(-a, -a), c + QPointF(a, a));
  p->drawLine(c + QPointF(-a, a), c + QPointF(a, -a));
  p->restore();
}

class ChromeStyle : public QProxyStyle {
public:
  ChromeStyle() : QProxyStyle(QStringLiteral("Fusion")) {}

  int pixelMetric(PixelMetric metric, const QStyleOption* opt, const QWidget* w) const override {
    if (metric == PM_IndicatorWidth || metric == PM_IndicatorHeight ||
        metric == PM_ExclusiveIndicatorWidth || metric == PM_ExclusiveIndicatorHeight)
      return 16;
    // The style sheet counts this frame into combo and spin box height. One
    // hairline for both keeps fields in a form the same height.
    if (metric == PM_SpinBoxFrameWidth || metric == PM_ComboBoxFrameWidth)
      return 1;
    // A 20 px close target instead of Qt's 16 px square.
    if (metric == PM_TabCloseIndicatorWidth || metric == PM_TabCloseIndicatorHeight)
      return 20;
    return QProxyStyle::pixelMetric(metric, opt, w);
  }

  void drawComplexControl(ComplexControl cc, const QStyleOptionComplex* opt, QPainter* p,
                          const QWidget* w) const override {
    // Ask the application style for sub-control rectangles so the chevrons sit
    // where the style sheet hit-tests clicks.
    const QStyle* sheet = w ? w->style() : this;
    const auto& colors = tokens();
    if (cc == CC_ComboBox && p) {
      if (const auto* cb = qstyleoption_cast<const QStyleOptionComboBox*>(opt)) {
        drawFieldFace(p, QRectF(cb->rect), cb->palette.color(QPalette::Button), cb);
        const QRect arrow = sheet->subControlRect(CC_ComboBox, cb, SC_ComboBoxArrow, w);
        if (!arrow.isEmpty()) {
          const bool enabled = cb->state.testFlag(State_Enabled);
          const bool open = cb->state.testFlag(State_On);
          const QColor ink = !enabled ? colors.borderStrong : open ? colors.accent : colors.inkMuted;
          const QPointF c(qMin<qreal>(arrow.center().x(), cb->rect.right() - 13.0),
                          cb->rect.center().y() + 0.5);
          drawChevron(p, c, open, 4.2, 2.4, ink);
        }
        return;
      }
    }
    if (cc == CC_SpinBox && p) {
      if (const auto* sb = qstyleoption_cast<const QStyleOptionSpinBox*>(opt)) {
        drawFieldFace(p, QRectF(sb->rect), sb->palette.color(QPalette::Base), sb);
        if (sb->buttonSymbols != QAbstractSpinBox::NoButtons) {
          const bool enabled = sb->state.testFlag(State_Enabled);
          const QRect up = sheet->subControlRect(CC_SpinBox, sb, SC_SpinBoxUp, w);
          const qreal x = qMin<qreal>(up.center().x(), sb->rect.right() - 12.0);
          const qreal mid = sb->rect.center().y() + 0.5;
          const auto inkFor = [&](SubControl part, QAbstractSpinBox::StepEnabledFlag step) {
            if (!enabled || !(sb->stepEnabled & step)) return colors.borderStrong;
            if (sb->activeSubControls == part && sb->state.testFlag(State_Sunken)) return colors.accent;
            if (sb->activeSubControls == part && sb->state.testFlag(State_MouseOver)) return colors.ink;
            return colors.inkMuted;
          };
          drawChevron(p, QPointF(x, mid - 4.5), true, 3.6, 2.0,
                      inkFor(SC_SpinBoxUp, QAbstractSpinBox::StepUpEnabled));
          drawChevron(p, QPointF(x, mid + 4.5), false, 3.6, 2.0,
                      inkFor(SC_SpinBoxDown, QAbstractSpinBox::StepDownEnabled));
        }
        return;
      }
    }
    QProxyStyle::drawComplexControl(cc, opt, p, w);
  }

  void drawPrimitive(PrimitiveElement pe, const QStyleOption* opt, QPainter* p,
                     const QWidget* w) const override {
    if (!opt || !p) {
      QProxyStyle::drawPrimitive(pe, opt, p, w);
      return;
    }
    if (pe == PE_IndicatorCheckBox || pe == PE_IndicatorItemViewItemCheck) {
      drawCheckIndicator(opt, p);
      return;
    }
    if (pe == PE_IndicatorRadioButton) {
      drawRadioIndicator(opt, p);
      return;
    }
    if (pe == PE_IndicatorSpinUp || pe == PE_IndicatorSpinDown || pe == PE_IndicatorArrowUp ||
        pe == PE_IndicatorArrowDown) {
      const bool up = (pe == PE_IndicatorSpinUp || pe == PE_IndicatorArrowUp);
      const bool spin = (pe == PE_IndicatorSpinUp || pe == PE_IndicatorSpinDown);
      drawChevron(p, QRectF(opt->rect).center(), up, spin ? 3.6 : 4.2, spin ? 2.0 : 2.4,
                  opt->state.testFlag(State_Enabled) ? tokens().inkMuted : tokens().borderStrong);
      return;
    }
    if (pe == PE_IndicatorTabClose) {
      drawTabClose(opt, p);
      return;
    }
    if (pe == PE_FrameFocusRect) {
      // A thin accent ring after keyboard navigation (Tab, shortcuts), as on
      // Windows; a mouse click shows none. Push buttons carry their ring in the
      // style sheet (:focus), and item views opt out with outline: none.
      if (!opt->state.testFlag(State_KeyboardFocusChange) || qobject_cast<const QPushButton*>(w))
        return;
      p->save();
      p->setRenderHint(QPainter::Antialiasing, true);
      p->setPen(QPen(tokens().focusRing, 1.5));
      p->setBrush(Qt::NoBrush);
      p->drawRoundedRect(QRectF(opt->rect).adjusted(0.75, 0.75, -0.75, -0.75), 4.0, 4.0);
      p->restore();
      return;
    }
    QProxyStyle::drawPrimitive(pe, opt, p, w);
  }
};

}  // namespace

QStyle* createChromeStyle() { return new ChromeStyle; }

}  // namespace KaTheme
