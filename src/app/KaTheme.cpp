#include "KaTheme.h"

#include <QAbstractScrollArea>
#include <QAbstractSpinBox>
#include <QApplication>
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QPainter>
#include <QPainterPath>
#include <QProxyStyle>
#include <QStyleOption>
#include <QWidget>
#include <QtMath>

namespace KaTheme {
namespace {

QColor softenFillSaturation(const QColor& color) {
  // Keep the semantic hue and lightness; reduce only HSL saturation by 20%.
  return QColor::fromHslF(color.hslHueF(), color.hslSaturationF() * 0.8f,
                          color.lightnessF(), color.alphaF()).toRgb();
}

// Strata chrome: flat surfaces, a navy frame and one blue accent. Map symbols,
// page contents and IconPalette are separate. The gloss* stops stay as flat
// aliases of the surface so any selector that still names them paints plainly.
const Tokens kTokens = [] {
  Tokens colors = {
    QColor(0xE3, 0xEE, 0xF8),  // sky0 selection wash
    QColor(0x1F, 0x6F, 0xB2),  // sky1 accent, white text 5.3:1
    QColor(0x18, 0x5E, 0x99),  // sky2 accent hover
    QColor(0x12, 0x55, 0x8D),  // sky3 deep accent, the startup-notice blue
    QColor(0xF4, 0xF6, 0xF8),  // sky4 window face
    QColor(0x1F, 0x6F, 0xB2),  // sky5 selection highlight (= sky1)
    QColor(0x1D, 0x27, 0x33),  // sky6 ink
    QColor(0x1D, 0x27, 0x33),  // ink
    QColor(0x5B, 0x68, 0x75),  // inkMuted, >= 4.5 on every tint below
    QColor(0x59, 0x68, 0x74),  // inkDisabled, readable on disabledSurface
    QColor(0xDC, 0xE3, 0xEA),  // border
    QColor(0xFF, 0xFF, 0xFF),  // bevelLight
    QColor(0xB8, 0xC4, 0xCF),  // bevelDark
    QColor(0xFF, 0xFF, 0xFF),  // canvasNeutral
    QColor(0xF4, 0xF6, 0xF8),  // desk: page background under white cards
    QColor(0xB4, 0x23, 0x18),  // danger
    QColor(0x2E, 0x7D, 0x4F),  // ok
    QColor(0xFF, 0xFF, 0xFF),  // surface
    QColor(0xF7, 0xF9, 0xFB),  // glossMiddle: alternate rows
    QColor(0xFF, 0xFF, 0xFF),  // glossBottom, flat
    QColor(0xEE, 0xF4, 0xFA),  // hoverTop
    QColor(0xEE, 0xF4, 0xFA),  // hoverBottom
    QColor(0xE1, 0xEC, 0xF7),  // pressedTop
    QColor(0xE1, 0xEC, 0xF7),  // pressedBottom
    QColor(0xE6, 0xF0, 0xFA),  // selectedTop
    QColor(0xE6, 0xF0, 0xFA),  // selectedBottom
    QColor(0xEE, 0xF1, 0xF4),  // disabledSurface
    QColor(0x0B, 0x3A, 0x63),  // rail: Strata navy
    QColor(0xFF, 0xFF, 0xFF),  // railText
    QColor(0xD5, 0xE6, 0xF5),  // railMuted, >= 4.5 on rail and deep accent
    QColor(0xEA, 0xF4, 0xEE),  // successSurface
    QColor(0xFB, 0xED, 0xEA),  // dangerSurface
  };
  // Explicit assignments avoid an MSVC /O2 ICE on initializer-list pointers
  // to QColor members of this lambda-local aggregate.
  colors.glossReflection = colors.surface;
  colors.glossShoulder = colors.surface;
  colors.accentReflection = colors.sky1;
  return colors;
}();

// Field chrome shared by combo and spin boxes: one rounded face and a hairline
// edge that turns accent on focus. The style sheet leaves their border native so
// these land here; a QSS border would hand them to QWindowsStyle, which draws no
// arrow beside a styled drop-down and a Win95 bevel around spin buttons.
void drawFieldFace(QPainter* p, const QRectF& rect, const QColor& fill, const QStyleOption* opt) {
  const auto& colors = kTokens;
  const bool enabled = opt->state.testFlag(QStyle::State_Enabled);
  const bool focus = enabled && opt->state.testFlag(QStyle::State_HasFocus);
  const qreal radius = rect.height() < 28 ? 6.0 : 8.0;
  p->save();
  p->setRenderHint(QPainter::Antialiasing, true);
  p->setPen(QPen(focus ? colors.sky1 : colors.border, 1.0));
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
    if (cc == CC_ComboBox && p) {
      if (const auto* cb = qstyleoption_cast<const QStyleOptionComboBox*>(opt)) {
        drawFieldFace(p, QRectF(cb->rect), cb->palette.color(QPalette::Button), cb);
        const QRect arrow = sheet->subControlRect(CC_ComboBox, cb, SC_ComboBoxArrow, w);
        if (!arrow.isEmpty()) {
          const bool enabled = cb->state.testFlag(State_Enabled);
          const bool open = cb->state.testFlag(State_On);
          const QColor ink = !enabled ? kTokens.bevelDark : open ? kTokens.sky1 : kTokens.inkMuted;
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
            if (!enabled || !(sb->stepEnabled & step)) return kTokens.bevelDark;
            if (sb->activeSubControls == part && sb->state.testFlag(State_Sunken)) return kTokens.sky1;
            if (sb->activeSubControls == part && sb->state.testFlag(State_MouseOver)) return kTokens.ink;
            return kTokens.inkMuted;
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
      p->save();
      p->setRenderHint(QPainter::Antialiasing, true);
      QRect box = opt->rect;
      if (box.width() < 14 || box.height() < 14)
        box = QRect(box.center().x() - 8, box.center().y() - 8, 16, 16);
      const QRectF r = QRectF(box).adjusted(1.0, 1.0, -1.0, -1.0);
      const bool on = opt->state.testFlag(State_On);
      const bool part = opt->state.testFlag(State_NoChange);
      const bool dis = !opt->state.testFlag(State_Enabled);
      const bool hover = opt->state.testFlag(State_MouseOver);
      const auto& colors = tokens();
      const QColor fill = colors.sky1;
      const QColor edge = hover ? colors.sky3 : colors.sky2;
      const QColor tickInk = dis ? colors.inkDisabled : colors.surface;
      const QColor stone = hover ? colors.sky2 : colors.bevelDark;
      p->setPen(QPen(dis ? colors.border
                         : ((on || part) ? edge : stone),
                     1.1));
      p->setBrush(dis ? colors.disabledSurface
                      : ((on || part) ? fill : colors.surface));
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
        const QRectF bar = r.adjusted(r.width() * 0.22, r.height() * 0.42,
                                      -r.width() * 0.22, -r.height() * 0.42);
        p->setPen(Qt::NoPen);
        p->setBrush(tickInk);
        p->drawRoundedRect(bar, 1.2, 1.2);
      }
      p->restore();
      return;
    }
    if (pe == PE_IndicatorSpinUp || pe == PE_IndicatorSpinDown || pe == PE_IndicatorArrowUp ||
        pe == PE_IndicatorArrowDown) {
      const bool up = (pe == PE_IndicatorSpinUp || pe == PE_IndicatorArrowUp);
      const bool spin = (pe == PE_IndicatorSpinUp || pe == PE_IndicatorSpinDown);
      drawChevron(p, QRectF(opt->rect).center(), up, spin ? 3.6 : 4.2, spin ? 2.0 : 2.4,
                  opt->state.testFlag(State_Enabled) ? tokens().inkMuted : tokens().bevelDark);
      return;
    }
    if (pe == PE_IndicatorTabClose) {
      // A thin cross; red only under the pointer, not a red square on every tab.
      p->save();
      p->setRenderHint(QPainter::Antialiasing, true);
      const auto& colors = tokens();
      const QRectF r = QRectF(opt->rect).adjusted(1.0, 1.0, -1.0, -1.0);
      const bool hot = opt->state.testFlag(State_Raised) || opt->state.testFlag(State_Sunken);
      if (hot) {
        p->setPen(Qt::NoPen);
        p->setBrush(colors.dangerSurface);
        p->drawRoundedRect(r, 5.0, 5.0);
      }
      const QColor ink = hot ? colors.danger
                       : opt->state.testFlag(State_Selected) ? colors.inkMuted : colors.bevelDark;
      const QPointF c = r.center();
      const qreal a = 3.6;
      p->setPen(QPen(ink, 1.5, Qt::SolidLine, Qt::RoundCap));
      p->drawLine(c + QPointF(-a, -a), c + QPointF(a, a));
      p->drawLine(c + QPointF(-a, a), c + QPointF(a, -a));
      p->restore();
      return;
    }
    QProxyStyle::drawPrimitive(pe, opt, p, w);
  }
};

void setGroup(QPalette& pal, QPalette::ColorGroup g, const Tokens& t, bool disabled) {
  const QColor text = disabled ? t.inkDisabled : t.ink;
  pal.setColor(g, QPalette::Window, t.sky4);
  pal.setColor(g, QPalette::WindowText, text);
  pal.setColor(g, QPalette::Base, disabled ? t.disabledSurface : t.surface);
  pal.setColor(g, QPalette::AlternateBase, t.glossMiddle);
  pal.setColor(g, QPalette::Text, text);
  pal.setColor(g, QPalette::Button, disabled ? t.disabledSurface : t.surface);
  pal.setColor(g, QPalette::ButtonText, text);
  pal.setColor(g, QPalette::BrightText, text);
  pal.setColor(g, QPalette::Highlight, disabled ? t.disabledSurface : t.sky5);
  pal.setColor(g, QPalette::HighlightedText, disabled ? t.inkDisabled : t.surface);
  pal.setColor(g, QPalette::PlaceholderText, disabled ? t.inkDisabled : t.inkMuted);
  pal.setColor(g, QPalette::ToolTipBase, t.surface);
  pal.setColor(g, QPalette::ToolTipText, text);
  pal.setColor(g, QPalette::Light, t.bevelLight);
  pal.setColor(g, QPalette::Midlight, t.glossMiddle);
  pal.setColor(g, QPalette::Mid, t.border);
  pal.setColor(g, QPalette::Dark, t.bevelDark);
  pal.setColor(g, QPalette::Shadow, t.border);
}

}  // namespace

const Tokens& tokens() { return kTokens; }

const IconPalette& iconPalette() {
  static const IconPalette colors = [] {
    IconPalette palette = {
      QColor(0x23, 0x29, 0x30),  // ink: common charcoal outline
      QColor(0x32, 0x6B, 0x9B),  // file: steel blue
      QColor(0x95, 0x60, 0x29),  // record: ochre
      QColor(0x39, 0x73, 0x68),  // map: natural green
      QColor(0x6B, 0x59, 0x96),  // align: muted violet
      QColor(0x24, 0x78, 0x6C),  // output: teal
      QColor(0x1D, 0x6E, 0xB8),  // water: river blue
      QColor(0x93, 0x60, 0x39),  // earth: soil brown
      QColor(0xD8, 0xBB, 0x7B),  // earthLight: sandy layer
      QColor(0x79, 0x6B, 0x62),  // rock: warm stone
      QColor(0x71, 0x82, 0x50),  // vegetation: muted olive
      QColor(0x8F, 0x98, 0xA3),  // disabled: neutralized in icon rendering
      QColor(0x16, 0x3F, 0x59),  // selected: dark blue accent
    };
    palette.file = softenFillSaturation(palette.file);
    palette.record = softenFillSaturation(palette.record);
    palette.map = softenFillSaturation(palette.map);
    palette.align = softenFillSaturation(palette.align);
    palette.output = softenFillSaturation(palette.output);
    palette.water = softenFillSaturation(palette.water);
    palette.earth = softenFillSaturation(palette.earth);
    palette.earthLight = softenFillSaturation(palette.earthLight);
    palette.rock = softenFillSaturation(palette.rock);
    palette.vegetation = softenFillSaturation(palette.vegetation);
    // Keep ink, disabled and selected-outline contrast exactly as before.
    return palette;
  }();
  return colors;
}

const ButtonMetrics& buttonMetrics() {
  static const ButtonMetrics metrics;
  return metrics;
}

QString resolvedStyleSheet(const QString& sheet) {
  const auto& metrics = buttonMetrics();
  const struct { const char* name; int value; } replacements[] = {
      {"ribbonIconSize", metrics.ribbonIconSize},
      {"ribbonFontSize", metrics.ribbonFontSize},
      {"ribbonChipWidth", metrics.ribbonChipWidth},
      {"ribbonMinWidth", metrics.ribbonMinWidth},
      {"ribbonHeight", metrics.ribbonHeight},
      {"buttonPadding", metrics.buttonPadding},
      {"buttonSpacing", metrics.buttonSpacing},
      {"ribbonChipGap", metrics.ribbonChipGap},
      {"ribbonGroupPad", metrics.ribbonGroupPad},
      {"scaleButtonHeight", metrics.scaleButtonHeight},
      {"scaleButtonMinWidth", metrics.scaleButtonMinWidth},
      {"scaleFontSize", metrics.scaleFontSize},
      {"layoutIconSize", metrics.layoutIconSize},
      {"layoutButtonHeight", metrics.layoutButtonHeight},
      {"panelMargin", metrics.panelMargin},
  };
  QString resolved = sheet;
  for (const auto& entry : replacements)
    resolved.replace(QLatin1Char('@') + QString::fromLatin1(entry.name) + QLatin1Char('@'),
                     QString::number(entry.value));
  const auto& colors = tokens();
  const struct { const char* name; QColor value; } colorReplacements[] = {
      {"accent", colors.sky1}, {"accentHover", colors.sky2}, {"accentDeep", colors.sky3},
      {"ink", colors.ink}, {"inkMuted", colors.inkMuted}, {"inkDisabled", colors.inkDisabled},
      {"border", colors.border}, {"edgeLight", colors.bevelLight}, {"edgeDark", colors.bevelDark},
      {"desk", colors.desk}, {"surface", colors.surface},
      {"glossMiddle", colors.glossMiddle}, {"glossBottom", colors.glossBottom},
      {"hoverTop", colors.hoverTop}, {"hoverBottom", colors.hoverBottom},
      {"pressedTop", colors.pressedTop}, {"pressedBottom", colors.pressedBottom},
      {"selectedTop", colors.selectedTop}, {"selectedBottom", colors.selectedBottom},
      {"disabledSurface", colors.disabledSurface},
      {"rail", colors.rail}, {"railText", colors.railText}, {"railMuted", colors.railMuted},
      {"danger", colors.danger}, {"ok", colors.ok},
      {"successSurface", colors.successSurface}, {"dangerSurface", colors.dangerSurface},
      {"glossReflection", colors.glossReflection}, {"glossShoulder", colors.glossShoulder},
      {"accentReflection", colors.accentReflection},
  };
  for (const auto& entry : colorReplacements)
    resolved.replace(QLatin1Char('@') + QString::fromLatin1(entry.name) + QLatin1Char('@'),
                     entry.value.name(QColor::HexRgb));
  return resolved;
}

QPalette palette() {
  QPalette pal;
  setGroup(pal, QPalette::Active, kTokens, false);
  setGroup(pal, QPalette::Inactive, kTokens, false);
  setGroup(pal, QPalette::Disabled, kTokens, true);
  return pal;
}

QString embeddedStyleSheet() {
  return
#include "ka-hgis.qss.inc"
      ;
}

QStringList styleSheetCandidates() {
  const QString appDir = QCoreApplication::applicationDirPath();
  return {
      QDir(appDir).filePath(QStringLiteral("../data/theme/ka-hgis.qss")),
      QDir(appDir).filePath(QStringLiteral("data/theme/ka-hgis.qss")),
      QDir::current().filePath(QStringLiteral("data/theme/ka-hgis.qss")),
  };
}

QString resolveStyleSheetPath() {
  for (const QString& p : styleSheetCandidates()) {
    if (QFile::exists(p))
      return QFileInfo(p).absoluteFilePath();
  }
  return {};
}

QString loadStyleSheet() {
  const QStringList cands = styleSheetCandidates();
  for (const QString& p : cands) {
    qInfo() << "KaTheme QSS candidate:" << p;
    QFile f(p);
    if (!f.exists())
      continue;
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
      qWarning() << "KaTheme QSS unreadable, skipping:" << p << f.errorString();
      continue;
    }
    const QString sheet = QString::fromUtf8(f.readAll());
    if (sheet.trimmed().isEmpty()) {
      qWarning() << "KaTheme QSS empty:" << p;
      continue;
    }
    qInfo() << "KaTheme QSS loaded from" << p;
    return sheet;
  }
  qInfo() << "KaTheme QSS using embedded fallback";
  return embeddedStyleSheet();
}

void apply(QApplication* app) {
  if (!app)
    return;
  // Field PCs ship Malgun Gothic. Pretendard is not in data/fonts — do not
  // put it first in QSS or QFont or Hangul falls back to a Latin substitute.
  QFont ui(QStringLiteral("Malgun Gothic"));
  ui.setPixelSize(13);
  ui.setHintingPreference(QFont::PreferFullHinting);
  ui.setStyleStrategy(QFont::PreferAntialias);
  app->setFont(ui);
  app->setStyle(new ChromeStyle);
  app->setPalette(palette());
  app->setStyleSheet(resolvedStyleSheet(loadStyleSheet()));
}

void excludeMapSurface(QWidget* w) {
  if (!w)
    return;
  // Clears the *local* sheet only. Application QSS still applies; GIS exclude
  // selectors in ka-hgis.qss are the real protection.
  w->setStyleSheet(QString());
  w->setAttribute(Qt::WA_StyledBackground, false);
  if (auto* area = qobject_cast<QAbstractScrollArea*>(w)) {
    if (QWidget* vp = area->viewport()) {
      vp->setStyleSheet(QString());
      vp->setAttribute(Qt::WA_StyledBackground, false);
    }
  }
}

QString colorSwatchStyle(const QColor& fill) {
  const QColor use = fill.isValid() ? fill : tokens().surface;
  return QStringLiteral("background-color: %1; border: 1px solid %2; border-radius: 8px;")
      .arg(use.name(), tokens().border.name());
}

}  // namespace KaTheme
