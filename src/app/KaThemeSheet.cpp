// KaTheme style sheet: token resolution and the embedded-first loader. The
// raw sheet is data/theme/ka-hgis.qss, compiled in as ka-hgis.qss.inc.
#include "KaTheme.h"
#include "KaThemeFonts.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QHash>
#include <QRegularExpression>

namespace KaTheme {
namespace {

QHash<QString, QString> replacementTable() {
  QHash<QString, QString> values;
  const auto& m = buttonMetrics();
  const struct { const char* name; int value; } metrics[] = {
      {"ribbonIconSize", m.ribbonIconSize}, {"ribbonFontSize", m.ribbonFontSize},
      {"ribbonChipWidth", m.ribbonChipWidth}, {"ribbonMinWidth", m.ribbonMinWidth},
      {"ribbonHeight", m.ribbonHeight}, {"buttonPadding", m.buttonPadding},
      {"buttonSpacing", m.buttonSpacing}, {"ribbonChipGap", m.ribbonChipGap},
      {"ribbonGroupPad", m.ribbonGroupPad}, {"scaleButtonHeight", m.scaleButtonHeight},
      {"scaleButtonMinWidth", m.scaleButtonMinWidth}, {"scaleFontSize", m.scaleFontSize},
      {"layoutIconSize", m.layoutIconSize}, {"layoutButtonHeight", m.layoutButtonHeight},
      {"panelMargin", m.panelMargin}, {"uiFontSize", uiFontSize()},
      {"chipHeight", m.chipHeight}, {"chipRadius", m.chipRadius},
      // Ribbon chip box (see QWidget#beginnerRibbon QToolButton): content height and bottom padding.
      {"ribbonLabelTail", m.ribbonLabelTail}, {"ribbonContentHeight", m.ribbonHeight - m.ribbonLabelTail},
      {"ribbonBottomPad", m.buttonPadding + m.ribbonLabelTail},
  };
  for (const auto& entry : metrics) values.insert(QString::fromLatin1(entry.name), QString::number(entry.value));
  const Tokens& c = tokens();
  // Copies, not references or pointers to members: MSVC /O2 once hit an ICE on
  // initializer lists pointing into a QColor aggregate.
  const struct { const char* name; QColor value; } colors[] = {
      {"accent", c.accent}, {"accentHover", c.accentHover}, {"accentDeep", c.accentDeep},
      {"accentWash", c.accentWash}, {"ink", c.ink}, {"inkMuted", c.inkMuted},
      {"inkDisabled", c.inkDisabled}, {"surface", c.surface}, {"desk", c.desk},
      {"altRow", c.altRow}, {"stripe", c.stripe}, {"hover", c.hover}, {"selected", c.selected}, {"pressed", c.pressed},
      {"disabledSurface", c.disabledSurface}, {"border", c.border}, {"borderStrong", c.borderStrong},
      {"edgeLight", c.edgeLight}, {"edgeDark", c.borderStrong}, {"danger", c.danger}, {"ok", c.ok},
      {"successSurface", c.successSurface}, {"dangerSurface", c.dangerSurface},
      {"warn", c.warn}, {"warnSurface", c.warnSurface}, {"rail", c.rail},
      {"railText", c.railText}, {"railMuted", c.railMuted}, {"progressFill", c.progressFill},
      {"focusRing", c.focusRing}, {"ribbonActiveInk", c.ribbonActiveInk}, {"ribbonGroupInk", c.ribbonGroupInk},
      // Legacy placeholders, kept so older sheets still resolve.
      {"glossMiddle", c.glossMiddle}, {"glossBottom", c.glossBottom}, {"hoverTop", c.hoverTop},
      {"hoverBottom", c.hoverBottom}, {"pressedTop", c.pressedTop}, {"pressedBottom", c.pressedBottom},
      {"selectedTop", c.selectedTop}, {"selectedBottom", c.selectedBottom},
      {"glossReflection", c.glossReflection}, {"glossShoulder", c.glossShoulder},
      {"accentReflection", c.accentReflection},
  };
  for (const auto& entry : colors)
    values.insert(QString::fromLatin1(entry.name), entry.value.name(QColor::HexRgb));
  // Font stacks (KaThemeFonts): quoted family lists for the append-only
  // font-family overrides at the end of ka-hgis.qss.
  values.insert(QStringLiteral("uiFont"), quotedFamilies(uiFontStack()));
  values.insert(QStringLiteral("monoFont"), quotedFamilies(monoFontStack()));
  return values;
}

const QRegularExpression& tokenPattern() {
  static const QRegularExpression pattern(QStringLiteral("@([A-Za-z][A-Za-z0-9_]*)@"));
  return pattern;
}

}  // namespace

QString resolvedStyleSheet(const QString& sheet) {
  const QHash<QString, QString> values = replacementTable();
  QString resolved;
  resolved.reserve(sheet.size());
  qsizetype last = 0;
  auto it = tokenPattern().globalMatch(sheet);
  while (it.hasNext()) {
    const auto match = it.next();
    const auto value = values.constFind(match.captured(1));
    if (value == values.constEnd()) continue;
    resolved += QStringView(sheet).mid(last, match.capturedStart() - last);
    resolved += *value;
    last = match.capturedEnd();
  }
  resolved += QStringView(sheet).mid(last);
  return resolved;
}

QStringList unresolvedTokens(const QString& resolved) {
  QStringList names;
  auto it = tokenPattern().globalMatch(resolved);
  while (it.hasNext()) {
    const QString name = it.next().captured(1);
    if (!names.contains(name)) names.append(name);
  }
  return names;
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

bool styleSheetFromDiskRequested() {
  return qEnvironmentVariableIntValue("KA_HGIS_QSS_FROM_DISK") == 1;
}

QString loadStyleSheet() {
  if (!styleSheetFromDiskRequested()) {
    qInfo() << "KaTheme QSS using the embedded sheet";
    return embeddedStyleSheet();
  }
  for (const QString& p : styleSheetCandidates()) {
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
    qInfo() << "KaTheme QSS loaded from disk (KA_HGIS_QSS_FROM_DISK):" << p;
    return sheet;
  }
  qInfo() << "KaTheme QSS no disk copy found; using the embedded sheet";
  return embeddedStyleSheet();
}

QString completeStyleSheet(const QString& raw) {
  const QString sheet = resolvedStyleSheet(raw);
  const QStringList missing = unresolvedTokens(sheet);
  if (missing.isEmpty())
    return sheet;
  qWarning().noquote() << "KaTheme QSS has unknown tokens" << missing.join(QLatin1Char(','))
                       << "- using the embedded sheet";
  return resolvedStyleSheet(embeddedStyleSheet());
}

QString applicationStyleSheet() { return completeStyleSheet(loadStyleSheet()); }

}  // namespace KaTheme
