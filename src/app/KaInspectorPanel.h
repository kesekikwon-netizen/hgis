#pragma once

#include <QFrame>
#include <QPointer>
#include <QString>

class KaChip;
class KaFeatureCard;
class KaInspectorStyle;
class QLabel;
class QStackedWidget;
class QTabBar;
class QToolButton;
class QVBoxLayout;

// The always-present right panel of the map tab: 「선택한 유구」 with 「n개 선택」, a fold
// button, two tabs (속성 · 스타일) and a footer slot for the 「배경 지도」 card.
// 속성 hosts the existing KaFeatureCard (reparented, its own title hidden) and says in
// one sentence when nothing is chosen or while drawing (the card is never hidden for
// drawing). 스타일 changes the chosen layer's colour and width in place (KaInspectorStyle).
// 사진 was dropped: nothing in the app stores photos (user 2026-10-03 「추천진행」).
// The panel decides nothing about layers: P6 attaches it to the splitter, feeds the
// selection count, the drawing flag, the card and the style layer.
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
  KaInspectorStyle* styleEditor() const { return m_style; }
  QWidget* page(int index) const;

  void setDrawing(bool drawing);
  bool isDrawing() const { return m_drawing; }
  void setSelectionCount(int count);
  int selectionCount() const { return m_count; }
  QString title() const;
  // The 속성 sentence now shown; empty while a record is on view and nothing is drawn.
  QString sentence() const;

 signals:
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
  KaInspectorStyle* m_style = nullptr;
  QVBoxLayout* m_footer = nullptr;
  QPointer<KaFeatureCard> m_card;
  bool m_drawing = false;
  int m_count = 0;
};
