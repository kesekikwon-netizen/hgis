#pragma once
#include <QColor>
#include <QFont>
#include <QLabel>
#include <QSize>
#include <QString>

class QFontMetrics;
class QPainter;
class QRect;

// One status chip shape for the whole chrome. paintChip() draws the pill, glyph
// and text from KaTheme tokens; the KaChip widget and the home table delegate
// both call it, so the two can never drift apart. QSS ([kaChip="true"] in
// ka-hgis.qss) gives the widget only its size and font, never a colour.
// Tone is the only thing that differs between chips. Ok, warn and danger always
// carry a mark (colour alone never speaks); accent and neutral may be text only.
class KaChip : public QLabel {
  Q_OBJECT
 public:
  enum class Tone { Ok, Warn, Danger, Accent, Neutral };

  explicit KaChip(const QString& text = QString(), Tone tone = Tone::Neutral, QWidget* parent = nullptr);

  Tone tone() const { return m_tone; }
  void setTone(Tone tone);
  // An icon id (KaIcons) or one of the built-in marks check/warn/missing/dot.
  // Empty: the tone's own mark for ok/warn/danger, none for accent/neutral.
  QString glyph() const { return m_glyph; }
  void setGlyph(const QString& iconId);
  bool hasGlyph() const;

  QSize sizeHint() const override;
  QSize minimumSizeHint() const override;

  // Paints a chip into rect (vertically centred, chipHeight tall, full width).
  static void paintChip(QPainter& p, const QRect& rect, const QString& text, Tone tone, const QString& glyphId);
  // Width and height a chip needs; fm must measure chipFont().
  static QSize sizeForText(const QFontMetrics& fm, const QString& text, bool hasGlyph);
  // The chip type: base at 11 px, bold.
  static QFont chipFont(QFont base);
  static QColor toneInk(Tone tone);
  static QColor toneSurface(Tone tone);
  // The mark a tone draws when no glyph id is given; empty for accent/neutral.
  static QString defaultGlyph(Tone tone);
  // "ok", "warn", "danger", "accent", "neutral" (the kaTone property and roles).
  static QString toneName(Tone tone);
  static Tone toneFromName(const QString& name);

 protected:
  void paintEvent(QPaintEvent* event) override;

 private:
  void syncProperties();

  Tone m_tone = Tone::Neutral;
  QString m_glyph;
};
