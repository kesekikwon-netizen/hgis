#pragma once

#include <QFrame>
#include <QHash>
#include <QString>
#include <QStringList>

class QLabel;
class QToolButton;

// 「배경 지도」 card at the bottom of the inspector column: one sentence that says what
// really happens (opening a survey brings up satellite and cadastral) and three buttons
// that are the ribbon's 「배경 지도」 buttons again (위성 · 지형 · 옛 지도). The card only
// asks (basemapRequested) and shows (setChecked); it never adds or toggles a layer
// itself, so the legend rules stay in one place (P6 wires it to the window).
class KaBasemapQuickCard : public QFrame {
  Q_OBJECT
 public:
  explicit KaBasemapQuickCard(QWidget* parent = nullptr);

  // "satellite", "terrain", "old_map" in button order.
  static QStringList ids();
  QToolButton* button(const QString& id) const;
  bool isChecked(const QString& id) const;
  QString sentence() const;

 public slots:
  // Mirrors the window's state; the buttons never toggle themselves on click.
  void setChecked(const QString& id, bool on);

 signals:
  void basemapRequested(const QString& id);

 private:
  QHash<QString, QToolButton*> m_buttons;
  QLabel* m_note = nullptr;
};
