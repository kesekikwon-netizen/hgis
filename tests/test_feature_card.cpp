// F041/F044/F045: the optional name/number form after drawing and the 「선택한 유구」 card.
// Kind/period are preset combos (free text allowed), the number starts at the next number
// of that kind, a repeated number is only pointed out, and every edit stays in the edit
// buffer (Ctrl+S is the only save).
#include <QtTest>
#include <QComboBox>
#include <QFocusEvent>
#include <QFrame>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSignalSpy>
#include <QUndoStack>
#include <memory>

#include "app/KaChip.h"
#include "app/KaFeatureCard.h"
#include "app/KaFeatureFormDialog.h"
#include "core/FeaturePresets.h"
#include "core/LayerOps.h"

#include <qgsapplication.h>
#include <qgsfeature.h>
#include <qgsfeatureiterator.h>
#include <qgsfeaturerequest.h>
#include <qgsgeometry.h>
#include <qgsrectangle.h>
#include <qgsvectordataprovider.h>
#include <qgsvectorlayer.h>

namespace {
std::unique_ptr<QgsVectorLayer> surveyFeatures() {
  auto layer = std::make_unique<QgsVectorLayer>(
      QStringLiteral("Polygon?crs=EPSG:5187&field=kind:string&field=period:string&field=feature_no:string"
                     "&field=note:string&field=updated_at:string"),
      QStringLiteral("유구"), QStringLiteral("memory"));
  LayerOps::markSurveyLayer(layer.get(), QStringLiteral("feature_poly"));
  const auto add = [&](const QString& kind, const QString& number, double x) {
    QgsFeature f(layer->fields());
    f.setAttribute(QStringLiteral("kind"), kind);
    f.setAttribute(QStringLiteral("period"), QStringLiteral("청동기"));
    f.setAttribute(QStringLiteral("feature_no"), number);
    f.setGeometry(QgsGeometry::fromRect(QgsRectangle(200000 + x, 450000, 200010 + x, 450005)));
    layer->dataProvider()->addFeature(f);
  };
  add(QStringLiteral("주거지"), QStringLiteral("1호"), 0);
  add(QStringLiteral("주거지"), QStringLiteral("2호"), 20);
  add(QStringLiteral("수혈"), QStringLiteral("1호"), 40);
  return layer;
}

QgsFeatureId featureWith(QgsVectorLayer* layer, const QString& kind, const QString& number) {
  QgsFeatureIterator it = layer->getFeatures();
  QgsFeature f;
  while (it.nextFeature(f)) {
    if (f.attribute(QStringLiteral("kind")).toString() == kind &&
        f.attribute(QStringLiteral("feature_no")).toString() == number)
      return f.id();
  }
  return FID_NULL;
}

QString storedValue(QgsVectorLayer* layer, QgsFeatureId fid, const QString& field) {
  QgsFeature f;
  layer->dataProvider()->getFeatures(QgsFeatureRequest(fid)).nextFeature(f);
  return f.attribute(field).toString();
}
}  // namespace

class TestFeatureCard : public QObject {
  Q_OBJECT
private slots:
  void form_suggestsNextNumberPerKindAndOnlyWarns();
  void form_applyToWritesEditBufferOnly();
  void card_showsOneSelectedFeature();
  void card_editsStayInEditBuffer();
  void card_pointsOutRepeatedNumber();
  void card_headerLineIsKindAndNumber();
  void card_measuresAreReadOnlyBoxes();
  void card_stateIsChipWithTone();
  void card_noteIsTwoLineAndCommitsOnFocusOut();
  void card_openFormEmitsLayerAndFid();
};

void TestFeatureCard::form_suggestsNextNumberPerKindAndOnlyWarns() {
  auto layer = surveyFeatures();
  QVERIFY(layer->startEditing());
  QgsFeature drawn(layer->fields());
  drawn.setGeometry(QgsGeometry::fromRect(QgsRectangle(200100, 450000, 200110, 450005)));
  QVERIFY(layer->addFeature(drawn));
  KaFeatureFormDialog form(layer.get(), nullptr, drawn.id());
  auto* kind = form.findChild<QComboBox*>(QStringLiteral("kaFeatureFormKind"));
  auto* period = form.findChild<QComboBox*>(QStringLiteral("kaFeatureFormPeriod"));
  auto* number = form.findChild<QLineEdit*>(QStringLiteral("kaFeatureFormNumber"));
  auto* note = form.findChild<QLabel*>(QStringLiteral("kaFeatureFormNumberNote"));
  QVERIFY(kind && period && number && note);
  QVERIFY(kind->isEditable() && period->isEditable());  // free text stays allowed
  QCOMPARE(kind->itemText(0), QStringLiteral("주거지"));
  QCOMPARE(period->itemText(0), QStringLiteral("구석기"));
  QCOMPARE(kind->currentText(), QString());  // nothing is pre-chosen
  QCOMPARE(number->text(), QStringLiteral("1호"));
  kind->setEditText(QStringLiteral("주거지"));
  QCOMPARE(number->text(), QStringLiteral("3호"));
  kind->setEditText(QStringLiteral("수혈"));
  QCOMPARE(number->text(), QStringLiteral("2호"));
  // A typed number wins over the suggestion; a repeated one is pointed out, not refused.
  // QLineEdit::insert() takes the user-input path (textEdited); QTest::keyClicks cannot
  // type Korean (it asserts on a character outside Latin-1).
  number->selectAll();
  number->insert(QStringLiteral("1호"));
  QCOMPARE(number->text(), QStringLiteral("1호"));
  QVERIFY(!note->isHidden());
  QVERIFY(note->text().contains(QStringLiteral("이미 있는 번호")));
  kind->setEditText(QStringLiteral("구"));
  QCOMPARE(number->text(), QStringLiteral("1호"));  // kept: the user typed it
  QVERIFY(note->isHidden());
  layer->rollBack();
}

void TestFeatureCard::form_applyToWritesEditBufferOnly() {
  auto layer = surveyFeatures();
  QVERIFY(layer->startEditing());
  QgsFeature drawn(layer->fields());
  drawn.setGeometry(QgsGeometry::fromRect(QgsRectangle(200100, 450000, 200110, 450005)));
  QVERIFY(layer->addFeature(drawn));
  const long long before = layer->dataProvider()->featureCount();
  KaFeatureFormDialog form(layer.get(), nullptr, drawn.id());
  form.findChild<QComboBox*>(QStringLiteral("kaFeatureFormKind"))->setEditText(QStringLiteral("주거 지"));
  form.findChild<QComboBox*>(QStringLiteral("kaFeatureFormPeriod"))->setEditText(QStringLiteral("원삼국"));
  QCOMPARE(form.nameText(), QStringLiteral("주거지"));  // the survey's spelling
  QCOMPARE(form.numberText(), QStringLiteral("3호"));
  QString error;
  QVERIFY2(LayerOps::runEditCommand(layer.get(), QStringLiteral("이름·번호"),
                                    [&]() { return form.applyTo(layer.get(), &error); }, &error),
           qPrintable(error));
  const QgsFeature written = layer->getFeature(drawn.id());
  QCOMPARE(written.attribute(QStringLiteral("kind")).toString(), QStringLiteral("주거지"));
  QCOMPARE(written.attribute(QStringLiteral("period")).toString(), QStringLiteral("원삼국"));
  QCOMPARE(written.attribute(QStringLiteral("feature_no")).toString(), QStringLiteral("3호"));
  QVERIFY(!written.attribute(QStringLiteral("updated_at")).toString().isEmpty());
  QVERIFY(layer->isModified());
  QCOMPARE(layer->dataProvider()->featureCount(), before);  // nothing committed
  layer->rollBack();
}

void TestFeatureCard::card_showsOneSelectedFeature() {
  auto layer = surveyFeatures();
  KaFeatureCard card;
  const QgsFeatureId house = featureWith(layer.get(), QStringLiteral("주거지"), QStringLiteral("1호"));
  layer->selectByIds({house});
  card.showSelection(layer.get());
  QVERIFY(card.hasFeature());
  QCOMPARE(card.featureId(), house);
  QCOMPARE(card.rowText(QStringLiteral("title")), QStringLiteral("선택한 유구"));
  QCOMPARE(card.rowText(QStringLiteral("number")), QStringLiteral("1호"));
  QCOMPARE(card.rowText(QStringLiteral("kind")), QStringLiteral("주거지"));
  QCOMPARE(card.rowText(QStringLiteral("period")), QStringLiteral("청동기"));
  QCOMPARE(card.rowText(QStringLiteral("state")), QStringLiteral("저장된 값입니다"));  // the state chip's text
  // Two selected: the card shows none of them.
  layer->selectByIds({house, featureWith(layer.get(), QStringLiteral("수혈"), QStringLiteral("1호"))});
  card.showSelection(layer.get());
  QVERIFY(!card.hasFeature());
}

void TestFeatureCard::card_editsStayInEditBuffer() {
  auto layer = surveyFeatures();
  KaFeatureCard card;
  const QgsFeatureId house = featureWith(layer.get(), QStringLiteral("주거지"), QStringLiteral("2호"));
  card.setFeature(layer.get(), house);
  QSignalSpy edited(&card, &KaFeatureCard::edited);
  QString error;
  QVERIFY2(card.setValue(QStringLiteral("note"), QStringLiteral("북벽 교란"), &error), qPrintable(error));
  QCOMPARE(edited.count(), 1);
  QVERIFY(layer->isEditable() && layer->isModified());
  QCOMPARE(layer->getFeature(house).attribute(QStringLiteral("note")).toString(), QStringLiteral("북벽 교란"));
  QCOMPARE(storedValue(layer.get(), house, QStringLiteral("note")), QString());  // file untouched
  QVERIFY(card.rowText(QStringLiteral("state")).contains(QStringLiteral("아직 저장 안 됨")));
  QVERIFY(!layer->getFeature(house).attribute(QStringLiteral("updated_at")).toString().isEmpty());
  const int undoSteps = layer->undoStack()->count();
  QVERIFY(card.setValue(QStringLiteral("note"), QStringLiteral("북벽 교란")));  // unchanged: no new step
  QCOMPARE(edited.count(), 1);
  QCOMPARE(layer->undoStack()->count(), undoSteps);
  QVERIFY(card.setValue(QStringLiteral("kind"), QStringLiteral("주거 지")));  // same key: no change
  QCOMPARE(edited.count(), 1);
  QVERIFY(card.setValue(QStringLiteral("kind"), QStringLiteral("수 혈")));
  QCOMPARE(layer->getFeature(house).attribute(QStringLiteral("kind")).toString(), QStringLiteral("수혈"));
  QCOMPARE(edited.count(), 2);
  layer->undoStack()->undo();  // Ctrl+Z takes one card edit back
  QCOMPARE(layer->getFeature(house).attribute(QStringLiteral("kind")).toString(), QStringLiteral("주거지"));
  QCOMPARE(layer->getFeature(house).attribute(QStringLiteral("note")).toString(), QStringLiteral("북벽 교란"));
  layer->rollBack();
  QCOMPARE(card.rowText(QStringLiteral("note")), QString());
}

void TestFeatureCard::card_pointsOutRepeatedNumber() {
  auto layer = surveyFeatures();
  KaFeatureCard card;
  const QgsFeatureId second = featureWith(layer.get(), QStringLiteral("주거지"), QStringLiteral("2호"));
  card.setFeature(layer.get(), second);
  QCOMPARE(card.rowText(QStringLiteral("note_number")), QString());
  QVERIFY(card.setValue(QStringLiteral("feature_no"), QStringLiteral("01호")));
  QVERIFY2(card.rowText(QStringLiteral("note_number")).contains(QStringLiteral("같은 번호가 1개 더")),
           qPrintable(card.rowText(QStringLiteral("note_number"))));
  QCOMPARE(layer->getFeature(second).attribute(QStringLiteral("feature_no")).toString(), QStringLiteral("01호"));
  // 수혈 1호 is another kind: not a repeat.
  card.setFeature(layer.get(), featureWith(layer.get(), QStringLiteral("수혈"), QStringLiteral("1호")));
  QCOMPARE(card.rowText(QStringLiteral("note_number")), QString());
  layer->rollBack();
}

void TestFeatureCard::card_headerLineIsKindAndNumber() {
  auto layer = surveyFeatures();
  KaFeatureCard card;
  card.setFeature(layer.get(), featureWith(layer.get(), QStringLiteral("주거지"), QStringLiteral("1호")));
  QCOMPARE(card.rowText(QStringLiteral("header")), QStringLiteral("1호 주거지"));
  QCOMPARE(card.rowText(QStringLiteral("layer_kind")), QStringLiteral("유구 면"));
  QVERIFY(card.setValue(QStringLiteral("feature_no"), QString()));
  QCOMPARE(card.rowText(QStringLiteral("header")), QStringLiteral("주거지"));  // no number: the kind alone
  layer->rollBack();
}

void TestFeatureCard::card_measuresAreReadOnlyBoxes() {
  auto layer = surveyFeatures();
  KaFeatureCard card;
  card.setFeature(layer.get(), featureWith(layer.get(), QStringLiteral("주거지"), QStringLiteral("1호")));
  const auto boxes = card.findChildren<QFrame*>(QStringLiteral("kaMeasureField"));
  QCOMPARE(boxes.size(), 2);  // 면적 and 둘레 of a polygon
  for (QFrame* box : boxes) {
    QCOMPARE(box->findChild<QLabel*>(QStringLiteral("kaMeasureAuto"))->text(), QStringLiteral("자동 계산"));
    QVERIFY(box->findChild<QLabel*>(QStringLiteral("kaMeasureValue")));
    QVERIFY(box->findChildren<QLineEdit*>().isEmpty());
  }
  QCOMPARE(card.rowText(QStringLiteral("area")), QStringLiteral("50.00 m²"));
  QCOMPARE(card.rowText(QStringLiteral("perimeter")), QStringLiteral("30.00 m"));
}

void TestFeatureCard::card_stateIsChipWithTone() {
  auto layer = surveyFeatures();
  KaFeatureCard card;
  card.setFeature(layer.get(), featureWith(layer.get(), QStringLiteral("주거지"), QStringLiteral("2호")));
  KaChip* state = card.stateChip();
  QVERIFY(state && state->objectName() == QStringLiteral("kaFeatureCardState"));
  QCOMPARE(state->tone(), KaChip::Tone::Neutral);
  QCOMPARE(state->text(), QStringLiteral("저장된 값입니다"));
  QVERIFY(card.setValue(QStringLiteral("note"), QStringLiteral("교란")));
  QCOMPARE(state->tone(), KaChip::Tone::Warn);
  QCOMPARE(state->text(), QStringLiteral("변경됨 · 아직 저장 안 됨"));
  layer->rollBack();
  QCOMPARE(state->tone(), KaChip::Tone::Neutral);
}

void TestFeatureCard::card_noteIsTwoLineAndCommitsOnFocusOut() {
  auto layer = surveyFeatures();
  KaFeatureCard card;
  card.setFeature(layer.get(), featureWith(layer.get(), QStringLiteral("수혈"), QStringLiteral("1호")));
  auto* note = card.findChild<QPlainTextEdit*>(QStringLiteral("kaFeatureCardNote"));
  QVERIFY(note);
  QVERIFY(note->minimumHeight() >= 56 && note->maximumHeight() <= 72);
  QSignalSpy edited(&card, &KaFeatureCard::edited);
  note->setFocus();
  note->setPlainText(QStringLiteral("북서쪽 모서리 바닥 다짐 확인 필요"));
  QCOMPARE(edited.count(), 0);  // typing alone writes nothing
  QFocusEvent leave(QEvent::FocusOut, Qt::MouseFocusReason);  // offscreen windows never get real focus
  QCoreApplication::sendEvent(note, &leave);  // leaving the box commits
  QTRY_COMPARE(edited.count(), 1);
  QCOMPARE(layer->getFeature(card.featureId()).attribute(QStringLiteral("note")).toString(),
           QStringLiteral("북서쪽 모서리 바닥 다짐 확인 필요"));
  layer->rollBack();
}

void TestFeatureCard::card_openFormEmitsLayerAndFid() {
  auto layer = surveyFeatures();
  KaFeatureCard card;
  auto* open = card.findChild<QPushButton*>(QStringLiteral("kaFeatureCardOpenForm"));
  QVERIFY(open && open->isHidden());
  QgsVectorLayer* askedLayer = nullptr; QgsFeatureId askedFid = FID_NULL; int asked = 0;  // no QSignalSpy: QgsVectorLayer*
  connect(&card, &KaFeatureCard::formRequested, [&](QgsVectorLayer* l, QgsFeatureId f) { askedLayer = l; askedFid = f; ++asked; });
  const QgsFeatureId fid = featureWith(layer.get(), QStringLiteral("주거지"), QStringLiteral("1호"));
  card.setFeature(layer.get(), fid);
  QVERIFY(!open->isHidden());
  open->click();
  QCOMPARE(asked, 1);
  QCOMPARE(askedLayer, layer.get());
  QCOMPARE(askedFid, fid);
}

#include "test_feature_card.moc"

int main(int argc, char** argv) {
  QgsApplication app(argc, argv, true);
  QgsApplication::setPrefixPath(qEnvironmentVariable("QGIS_PREFIX_PATH", QStringLiteral("A:/OSGeo4W/apps/qgis-dev")),
                                true);
  QgsApplication::initQgis();
  FeaturePresets::instance().ensureLoaded();
  TestFeatureCard tc;
  const int rc = QTest::qExec(&tc, argc, argv);
  QgsApplication::exitQgis();
  return rc;
}
