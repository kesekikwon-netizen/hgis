#pragma once

#include <QHash>
#include <QList>
#include <QWidget>

class QAction;
class QFrame;
class QHBoxLayout;
class QToolButton;

// One ribbon size (spec 「리본 크기 단계」). chipWidth 0 = max(tile + 8, label width + 8).
struct RibbonLook {
  int tile;       // icon tile edge in px
  int glyph;      // spec table's rounded glyph edge, documentation only: KaIconsMockupEngine draws tile * 18 / 32
  int chipWidth;  // fixed chip width in px; 0 = follow the label
  bool labels;    // button labels shown under the tile
};

// 초보자용 위 리본. 기존 QAction/QToolButton을 단계 그룹에만 옮긴다.
// The ribbon never folds a group away: every group and every chip is on the row at each size. It draws
// everything at the biggest size that fits its width, so a wide window gets bigger icons and a narrow one
// hides the labels first and then shrinks the icons.
class KaBeginnerRibbon : public QWidget {
  Q_OBJECT
public:
  explicit KaBeginnerRibbon(QWidget* parent = nullptr);

  QFrame* addGroup(const QString& id, const QString& caption);
  QToolButton* addAction(const QString& groupId, QAction* action);
  void addWidget(const QString& groupId, QWidget* widget);
  QFrame* group(const QString& id) const;
  QList<QToolButton*> tabButtons() const;
  void applyTabOrder();
  // 이름은 예전 두 줄 맞춤. 지금은 줄바꿈을 없애 한 줄로 맞춘다.
  static QString twoLine(const QString& text);
  // Prepares one chip (mockup): a one-line label at 13 px that shrinks one pixel at a time only when it is
  // wider than ButtonMetrics::ribbonMaxLabelWidth, never below ButtonMetrics::ribbonMinFontSize; keyboard
  // focus, the hover look, and the chip's full name in its tooltip (the only place the name shows once the
  // labels are hidden). Then draws it at the normal size; a chip inside a ribbon follows the ribbon's size.
  static void applyTwoLine(QToolButton* button);
  // The sizes, biggest first: tile 56, 54 ... 34 with labels (chip = max(tile + 8, label + 8)), tile 32
  // with labels, then tile 32, 24, 20 without labels (chip 40, 30, 26 px). glyph = qRound(tile * 18 / 32.0),
  // except 12 at tile 20 (the spec table).
  static QList<RibbonLook> looks();
  // The first size whose width fits `available` (widths[i] belongs to looks()[i]); the last when none does.
  static int chooseLook(const QList<int>& widths, int available);
  // The size the ribbon draws now.
  RibbonLook look() const;
  // The ribbon width each size needs (row margins included), in looks() order.
  QList<int> lookWidths() const;
  QSize sizeHint() const override;
  // The smallest size (tile 20, no labels): the ribbon is never narrower than this.
  QSize minimumSizeHint() const override;

protected:
  void resizeEvent(QResizeEvent* event) override;
  void showEvent(QShowEvent* event) override;
  bool eventFilter(QObject* watched, QEvent* event) override;

private:
  QHBoxLayout* buttonRow(const QString& groupId) const;
  int groupInsertIndex() const;
  QList<QToolButton*> chips() const;
  int lineHeight() const;
  int rowHeight() const;
  void updateLook();
  void applyLook(int index, bool force = false);

  QHBoxLayout* m_row = nullptr;
  QHash<QString, QFrame*> m_groups;
  QHash<QString, QHBoxLayout*> m_btnRows;
  QStringList m_groupOrder;
  int m_lookIndex = 0;    // looks()[m_lookIndex] is drawn; the constructor starts at the normal size
  int m_appliedLine = 0;  // label line height and total label width the chips were sized for, so a font
  int m_appliedLabels = 0;  // that settles after the first draw (the style sheet) redraws them
  int m_loggedLook = -1;  // the size last written to the session log
  bool m_updatingLook = false;
};
