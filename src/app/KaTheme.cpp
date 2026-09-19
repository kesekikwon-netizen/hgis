#include "KaTheme.h"

#include <QAbstractScrollArea>
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

QColor blendSurface(const QColor& color, const QColor& surface, double fraction) {
  // Channel-wise sRGB interpolation, with explicit integer rounding.
  return QColor(qRound(color.red() * (1.0 - fraction) + surface.red() * fraction),
                qRound(color.green() * (1.0 - fraction) + surface.green() * fraction),
                qRound(color.blue() * (1.0 - fraction) + surface.blue() * fraction),
                color.alpha());
}

// Shared chrome colors. Map symbols, page contents and IconPalette are separate.
const Tokens kTokens = [] {
  Tokens colors = {
    QColor(0xD6, 0xF0, 0xFC),  // sky0 Bloom selection wash
    QColor(0x00, 0x78, 0xD4),  // sky1 Windows accent https://fluent2.microsoft.design/color
    QColor(0x00, 0x67, 0xC0),  // sky2 accent hover (darker so white text stays >= 4.5)
    QColor(0x00, 0x5A, 0x9E),  // sky3 deep accent
    QColor(0xF3, 0xF3, 0xF3),  // sky4 Mica light fallback
    QColor(0x00, 0x78, 0xD4),  // sky5 selection highlight (= sky1)
    QColor(0x20, 0x28, 0x31),  // sky6 ink
    QColor(0x20, 0x28, 0x31),  // ink
    QColor(0x52, 0x60, 0x6D),  // inkMuted
    QColor(0x59, 0x68, 0x74),  // inkDisabled, readable on disabledSurface
    QColor(0xCB, 0xD3, 0xDB),  // border
    QColor(0xFF, 0xFF, 0xFF),  // bevelLight
    QColor(0x9B, 0xB4, 0xC6),  // bevelDark
    QColor(0xFF, 0xFF, 0xFF),  // canvasNeutral
    QColor(0xE8, 0xF1, 0xF8),  // desk Bloom mica tint
    QColor(0xA3, 0x3A, 0x2E),  // danger
    QColor(0x32, 0x6B, 0x4A),  // ok
    QColor(0xFF, 0xFF, 0xFF),  // surface
    QColor(0xF0, 0xF7, 0xFB),  // glossMiddle
    QColor(0xD7, 0xE8, 0xF4),  // glossBottom
    QColor(0xFF, 0xFF, 0xFF),  // hoverTop
    QColor(0xCD, 0xE6, 0xF5),  // hoverBottom
    QColor(0xB9, 0xD9, 0xEE),  // pressedTop
    QColor(0xD4, 0xE8, 0xF4),  // pressedBottom
    QColor(0xE7, 0xF5, 0xFC),  // selectedTop
    QColor(0xC5, 0xE7, 0xF6),  // selectedBottom
    QColor(0xE8, 0xEE, 0xF2),  // disabledSurface
    QColor(0x00, 0x67, 0xC0),  // rail Bloom blue (not softened)
    QColor(0xFF, 0xFF, 0xFF),  // railText
    QColor(0xF3, 0xFB, 0xFF),  // railMuted, >= 4.5 on accent and rail
    QColor(0xEA, 0xF2, 0xEC),  // successSurface
    QColor(0xF9, 0xE9, 0xE5),  // dangerSurface
  };
  // Explicit assignments avoid an MSVC /O2 ICE on initializer-list pointers
  // to QColor members of this lambda-local aggregate.
  colors.sky1 = softenFillSaturation(colors.sky1);
  colors.sky2 = softenFillSaturation(colors.sky2);
  colors.sky3 = softenFillSaturation(colors.sky3);
  colors.sky5 = softenFillSaturation(colors.sky5);
  colors.danger = softenFillSaturation(colors.danger);
  colors.ok = softenFillSaturation(colors.ok);
  colors.sky0 = blendSurface(colors.sky0, colors.surface, 0.2);
  colors.sky4 = blendSurface(colors.sky4, colors.surface, 0.2);
  colors.desk = blendSurface(colors.desk, colors.surface, 0.2);
  colors.glossMiddle = blendSurface(colors.glossMiddle, colors.surface, 0.2);
  colors.glossBottom = blendSurface(colors.glossBottom, colors.surface, 0.2);
  colors.hoverTop = blendSurface(colors.hoverTop, colors.surface, 0.2);
  colors.hoverBottom = blendSurface(colors.hoverBottom, colors.surface, 0.2);
  colors.pressedTop = blendSurface(colors.pressedTop, colors.surface, 0.2);
  colors.pressedBottom = blendSurface(colors.pressedBottom, colors.surface, 0.2);
  colors.selectedTop = blendSurface(colors.selectedTop, colors.surface, 0.2);
  colors.selectedBottom = blendSurface(colors.selectedBottom, colors.surface, 0.2);
  colors.disabledSurface = blendSurface(colors.disabledSurface, colors.surface, 0.2);
  colors.successSurface = blendSurface(colors.successSurface, colors.surface, 0.2);
  colors.dangerSurface = blendSurface(colors.dangerSurface, colors.surface, 0.2);
  // Text, outlines, disabled ink and the map-neutral surface stay unchanged.
  colors.glossReflection = colors.surface;
  colors.glossShoulder = blendSurface(colors.glossMiddle, colors.surface, 0.5);
  // Do not lift the accent toward white: #0078D4 + 8% white drops below 4.5:1
  // on surface text. Keep the reflection stop equal to the softened accent.
  colors.accentReflection = colors.sky1;
  return colors;
}();

class ChromeStyle : public QProxyStyle {
public:
  ChromeStyle() : QProxyStyle(QStringLiteral("Fusion")) {}

  int pixelMetric(PixelMetric metric, const QStyleOption* opt, const QWidget* w) const override {
    if (metric == PM_IndicatorWidth || metric == PM_IndicatorHeight ||
        metric == PM_ExclusiveIndicatorWidth || metric == PM_ExclusiveIndicatorHeight)
      return 16;
    return QProxyStyle::pixelMetric(metric, opt, w);
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
      p->save();
      p->setRenderHint(QPainter::Antialiasing, true);
      const QRect r = opt->rect;
      const bool up = (pe == PE_IndicatorSpinUp || pe == PE_IndicatorArrowUp);
      const qreal cx = r.center().x();
      const qreal cy = r.center().y();
      QPainterPath path;
      if (up) {
        path.moveTo(cx, cy - 3.6);
        path.lineTo(cx + 5.2, cy + 2.6);
        path.lineTo(cx - 5.2, cy + 2.6);
      } else {
        path.moveTo(cx, cy + 3.6);
        path.lineTo(cx + 5.2, cy - 2.6);
        path.lineTo(cx - 5.2, cy - 2.6);
      }
      path.closeSubpath();
      p->setPen(Qt::NoPen);
      p->setBrush(opt->state.testFlag(State_Enabled) ? tokens().ink : tokens().inkDisabled);
      p->drawPath(path);
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
