#pragma once

#include <QFrame>
#include <QList>
#include <QMetaObject>
#include <QPointer>
#include <QString>

#include <qgsfeature.h>
#include <qgsfeatureid.h>

class KaChip;
class QFormLayout;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QToolButton;
class QgsVectorLayer;

// 「선택한 유구」 card: the record of ONE selected feature. A header line (「1호 주거지」
// with the layer kind 「유구 면」 at the right), the editors (번호, 종류, 시대, 메모),
// read-only 「자동 계산」 boxes for 면적·둘레, one state chip (「변경됨 · 아직 저장 안 됨」
// warn / 「저장된 값입니다」 neutral) and 「조사카드 열기」. It never commits: each edit
// goes into the layer's edit buffer as one undoable command, and Ctrl+S stays the only
// save. Kind/period use the preset combos (free text allowed); a repeated number is
// pointed out, never refused. The inspector panel hosts it (KaInspectorPanel).
class KaFeatureCard : public QFrame {
  Q_OBJECT
public:
  explicit KaFeatureCard(QWidget* parent = nullptr);
  ~KaFeatureCard() override;

  // Shows one feature. An invalid layer/feature clears the card.
  void setFeature(QgsVectorLayer* layer, QgsFeatureId fid);
  // Follows the layer's selection: exactly one selected feature is shown, else cleared.
  void showSelection(QgsVectorLayer* layer);
  // False for a record shown directly (a shape just drawn): emptying the selection keeps it.
  bool followsSelection() const { return m_followsSelection; }
  void clear();

  QgsVectorLayer* layer() const;
  QgsFeatureId featureId() const { return m_fid; }
  bool hasFeature() const;

  // Text shown in a row: title, header, layer_kind, number, kind, period, note, area,
  // perimeter, state, note_number.
  QString rowText(const QString& row) const;
  // Writes one field the way the card's editors do (edit buffer only).
  bool setValue(const QString& field, const QString& text, QString* errorOut = nullptr);

  // 「선택한 유구」 for a layer key; the inspector header shows the same words.
  static QString titleFor(const QString& layerKey);
  // 「유구 면」 for a layer: the small label at the right of the header line.
  static QString layerKindFor(const QgsVectorLayer* layer);
  // The inspector has its own title line; the card's title can be hidden there.
  void setTitleVisible(bool visible);
  KaChip* stateChip() const { return m_state; }

signals:
  // After a value went into the edit buffer. `before` is the feature before the change
  // (for the window's undo order); the window marks the survey unsaved and restyles.
  void edited(QgsVectorLayer* layer, QgsFeatureId fid, const QgsFeature& before);
  // The card now shows another feature or nothing (title, header and state follow).
  void featureChanged();
  // 「조사카드 열기」: the window opens the full attribute form for this feature.
  void formRequested(QgsVectorLayer* layer, QgsFeatureId fid);

protected:
  bool eventFilter(QObject* watched, QEvent* event) override;  // 메모 commits on focus-out

private:
  void watchLayer(QgsVectorLayer* layer);
  void refresh();
  void commitEditor(const QString& field, QWidget* editor);
  void buildEditors();  // KaFeatureCardEditors.cpp
  void dropEditors();
  QWidget* measureField(QLabel** valueOut);  // KaFeatureCardEditors.cpp
  void setState(const QString& text, bool changed, bool failed = false);

  QPointer<QgsVectorLayer> m_layer;
  QgsFeatureId m_fid = FID_NULL;
  QList<QMetaObject::Connection> m_connections;
  QString m_numberField;
  bool m_readOnly = false;  // reference/cadastral layer: the editors show the record, never write it
  bool m_followsSelection = false;

  QLabel* m_title = nullptr;
  QWidget* m_header = nullptr;
  QLabel* m_name = nullptr;
  QLabel* m_layerKind = nullptr;
  QLabel* m_empty = nullptr;
  QWidget* m_body = nullptr;
  QFormLayout* m_form = nullptr;
  QLineEdit* m_number = nullptr;
  QToolButton* m_nextNumber = nullptr;
  QLabel* m_numberNote = nullptr;
  QWidget* m_kind = nullptr;
  QWidget* m_period = nullptr;
  QPlainTextEdit* m_note = nullptr;
  QList<QPair<QString, QLineEdit*>> m_nameEdits;  // survey area: 조사명·유적명 (a separate area layer: 이름)
  QLabel* m_area = nullptr;
  QLabel* m_perimeter = nullptr;
  KaChip* m_state = nullptr;
  QPushButton* m_openForm = nullptr;
};
