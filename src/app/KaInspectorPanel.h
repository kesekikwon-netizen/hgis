#pragma once

#include <QFrame>
#include <QPointer>
#include <QString>

class KaChip;
class KaFeatureCard;
class QLabel;
class QPushButton;
class QStackedWidget;
class QTabBar;
class QToolButton;
class QVBoxLayout;

// The always-present right panel of the map tab: 「선택한 유구」 with 「n개 선택」, a fold
// button, three tabs (속성 · 스타일 · 사진) and a footer slot for the 「배경 지도」 card.
// 속성 hosts the existing KaFeatureCard (reparented, its own title hidden) and says in
// one sentence when nothing is chosen or while drawing (the card is never hidden for
// drawing). 스타일 is a sentence plus 「레이어 스타일 편집…」; 사진 is a sentence only.
// The panel decides nothing about layers: P6 attaches it to the splitter, feeds the
// selection count, the drawing flag and the card, and answers the two signals.
class KaInspectorPanel : public QFrame {
  Q_OBJECT
 public:
  static constexpr int kMinimumWidth = 260;
  static constexpr int kMaximumWidth = 380;

  explicit KaInspectorPanel(QWidget* parent = nullptr);

  // 272 on a window narrower than 1500 px (1366: map stays >= 760 px), else 300.
  static int preferredWidth(int totalWidth);

  // Where the window builds the feature card (setupFeatureCard(host, layout)).
  QWidget* attributeHost() const { return m_attributePage; }
  QVBoxLayout* attributeLayout() const { return m_cardSlot; }
  // Adopts a card: reparents it into the 속성 tab and follows its feature state.
  void setFeatureCard(KaFeatureCard* card);
  KaFeatureCard* featureCard() const { return m_card.data(); }
  // Below the tabs: the 「배경 지도」 card goes here.
  QVBoxLayout* footerLayout() const { return m_footer; }

  QTabBar* tabs() const { return m_tabs; }
  QWidget* page(int index) const;

  void setDrawing(bool drawing);
  bool isDrawing() const { return m_drawing; }
  void setSelectionCount(int count);
  int selectionCount() const { return m_count; }
  QString title() const;
  // The 속성 sentence now shown; empty while a record is on view and nothing is drawn.
  QString sentence() const;

 signals:
  void styleEditRequested();
  void collapseRequested();

 private:
  void syncAttributeState();

  QLabel* m_title = nullptr;
  KaChip* m_countChip = nullptr;
  QToolButton* m_collapse = nullptr;
  QTabBar* m_tabs = nullptr;
  QStackedWidget* m_stack = nullptr;
  QWidget* m_attributePage = nullptr;
  QLabel* m_sentence = nullptr;
  QVBoxLayout* m_cardSlot = nullptr;
  QPushButton* m_styleEdit = nullptr;
  QVBoxLayout* m_footer = nullptr;
  QPointer<KaFeatureCard> m_card;
  bool m_drawing = false;
  int m_count = 0;
};
