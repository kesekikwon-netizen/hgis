#pragma once

#include <QColor>
#include <QString>
#include <QStringList>
#include <QVector>

class QgsVectorLayer;
class QgsSymbol;

// Feature kinds (주거지, 수혈 ...) and periods (구석기 ... 조선) offered as choices.
// Kind and period stay free text: the lists only suggest spellings and give a stable
// look. Periods are ordered oldest first and coloured with a lightness-monotonic ramp
// (tells periods apart in black-and-white print and for colour-blind readers); kinds
// are told apart by hatching (polygons), dash pattern (lines) and marker shape (points).
// data/styles/feature_presets.json overrides the built-in table (same values).
// The preset renderer lives in FeatureRecordStyle.cpp.
class FeaturePresets {
public:
  struct Kind {
    QString id;
    QString label;
    QString pattern;  // hatch: none, bdiagonal, fdiagonal, horizontal, vertical, cross, diagcross, dots
    QString line;     // "solid" or a dash list in mm ("3;1.5" = 3 mm dash, 1.5 mm gap)
    QString marker;   // QGIS simple marker name
  };
  struct Period {
    QString id;
    QString label;
    QString color;  // #RRGGBB, or "none" for 미정 (no fill)
  };

  // Stored in ka_hgis/style_mode (the key LayerStyleKinds uses), so the two
  // automatic looks replace each other.
  static constexpr const char* kStyleModePreset = "preset";

  static FeaturePresets& instance();
  // The built-in table used when the JSON is missing (the JSON carries the same values).
  static QVector<Kind> builtInKinds();
  static QVector<Period> builtInPeriods();

  bool load(const QString& jsonPath);
  // The JSON next to the program, else the built-in table. Never fails.
  bool ensureLoaded();
  bool isLoaded() const { return !m_kinds.isEmpty() && !m_periods.isEmpty(); }

  const QVector<Kind>& kinds() const { return m_kinds; }
  const QVector<Period>& periods() const { return m_periods; }
  QStringList kindLabels() const;
  QStringList periodLabels() const;  // oldest first, 미정 last
  QString defaultKindLabel() const;   // 기타: the pattern free-text kinds share

  // Spelling-tolerant keys: "주거 지" -> 주거지, "청동기 시대" / "청동기시대" -> 청동기.
  static QString kindKey(const QString& text);
  static QString periodKey(const QString& text);
  // The same keys computed by a QGIS expression over the "kind" / "period" fields.
  static QString kindKeyExpression();
  static QString periodKeyExpression();
  // The preset with the same key, or nullptr for free text.
  const Kind* matchKind(const QString& text) const;
  const Period* matchPeriod(const QString& text) const;
  // Invalid for 미정, free text and empty values (drawn without fill).
  QColor periodColor(const QString& text) const;
  // Index in the period list (oldest = 0); periods().size() for free text.
  int periodOrder(const QString& text) const;

  // 「시대색·종류 무늬」: one legend class per kind × period present in the layer,
  // plus a 「미분류」 catch-all so free text and new values never disappear.
  // Remembers ka_hgis/style_mode = preset. False when the layer has no kind field.
  bool applyRenderer(QgsVectorLayer* layer);
  static bool canStyle(const QgsVectorLayer* layer);
  static bool isPresetStyled(const QgsVectorLayer* layer);
  // Rebuilds the classes after an edit when the layer uses the preset look.
  static bool refreshIfPresetStyled(QgsVectorLayer* layer);

private:
  FeaturePresets() = default;
  void loadBuiltIn();
  QgsSymbol* symbolFor(int geomType, const Kind& kind, const QColor& periodColor) const;

  QVector<Kind> m_kinds;
  QVector<Period> m_periods;
  bool m_tried = false;
};
