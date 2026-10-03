// The 「선택한 유구」 card also shows and edits 조사 상태·조사자·조사일, and Enter in one of its boxes
// only finishes that box (user 2026-10-03 「선택한 유구의 속성정보를 수정할수있게하라」, answer 「진행」,
// docs/intent/2026-10-03-edit-record-while-drawing.md). tests/test_feature_card.cpp is at its 300-line limit.
#include <QtTest>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QSignalSpy>
#include <memory>

#include "app/KaFeatureCard.h"
#include "core/FeaturePresets.h"
#include "core/LayerOps.h"

#include <qgsapplication.h>
#include <qgsfeature.h>
#include <qgsgeometry.h>
#include <qgsrectangle.h>
#include <qgsvectordataprovider.h>
#include <qgsvectorlayer.h>

namespace {
std::unique_ptr<QgsVectorLayer> recordedFeature(int count = 1) {
  auto layer = std::make_unique<QgsVectorLayer>(
      QStringLiteral("Polygon?crs=EPSG:5187&field=kind:string&field=period:string&field=feature_no:string"
                     "&field=note:string&field=status:string&field=surveyor:string&field=surv_date:string"),
      QStringLiteral("유구"), QStringLiteral("memory"));
  LayerOps::markSurveyLayer(layer.get(), QStringLiteral("feature_poly"));
  QgsFeature f(layer->fields());
  f.setAttribute(QStringLiteral("feature_no"), QStringLiteral("1호"));
  f.setAttribute(QStringLiteral("status"), QStringLiteral("조사 중"));
  f.setAttribute(QStringLiteral("surveyor"), QStringLiteral("권영인"));
  f.setAttribute(QStringLiteral("surv_date"), QStringLiteral("2026-10-03"));
  f.setGeometry(QgsGeometry::fromRect(QgsRectangle(200000, 450000, 200010, 450005)));
  for (int i = 0; i < count; ++i) layer->dataProvider()->addFeature(f);
  return layer;
}

QgsFeatureId firstId(QgsVectorLayer* layer) {
  QgsFeature f;
  layer->getFeatures().nextFeature(f);
  return f.id();
}
}  // namespace

class TestFeatureCardFields : public QObject {
  Q_OBJECT
 private slots:
  void surveyStatusSurveyorAndDateAreOnTheCard() {
    auto layer = recordedFeature();
    KaFeatureCard card;
    card.setFeature(layer.get(), firstId(layer.get()));
    QCOMPARE(card.rowText(QStringLiteral("status")), QStringLiteral("조사 중"));
    QCOMPARE(card.rowText(QStringLiteral("surveyor")), QStringLiteral("권영인"));
    QCOMPARE(card.rowText(QStringLiteral("surv_date")), QStringLiteral("2026-10-03"));
  }

  // Typing in a box and pressing Enter writes that one value into the edit buffer (one undo step);
  // nothing else happens, so a drawing in progress goes on.
  void enterFinishesOnlyThatBox() {
    auto layer = recordedFeature();
    QVERIFY(layer->startEditing());
    KaFeatureCard card;
    card.show();
    card.setFeature(layer.get(), firstId(layer.get()));
    QLineEdit* surveyor = nullptr;
    for (QLineEdit* edit : card.findChildren<QLineEdit*>())
      if (edit->text() == QStringLiteral("권영인")) surveyor = edit;
    QVERIFY(surveyor);
    QSignalSpy edited(&card, &KaFeatureCard::edited);
    QSignalSpy entered(&card, &KaFeatureCard::valueEntered);
    surveyor->setFocus();
    surveyor->setText(QStringLiteral("김조사"));  // typed (QTest cannot type Hangul)
    QTest::keyClick(surveyor, Qt::Key_Return);
    QCOMPARE(edited.count(), 1);
    QCOMPARE(entered.count(), 1);  // while drawing the window hands the keys back to the map
    QCOMPARE(layer->getFeature(firstId(layer.get())).attribute(QStringLiteral("surveyor")).toString(), QStringLiteral("김조사"));
    layer->rollBack();
  }

  // Text still being typed when the card shows another record goes to its own record, never to
  // the next one and never lost (code review: the card is no longer locked while drawing).
  void typedTextStaysWithItsRecord() {
    auto layer = recordedFeature(2);
    QVERIFY(layer->startEditing());
    const QList<QgsFeatureId> ids(layer->allFeatureIds().cbegin(), layer->allFeatureIds().cend());
    QCOMPARE(ids.size(), 2);
    KaFeatureCard card;
    card.show();
    card.setFeature(layer.get(), ids.at(0));
    auto* note = card.findChild<QPlainTextEdit*>(QStringLiteral("kaFeatureCardNote"));
    QLineEdit* surveyor = nullptr;
    for (QLineEdit* edit : card.findChildren<QLineEdit*>())
      if (edit->text() == QStringLiteral("권영인")) surveyor = edit;
    QVERIFY(note && surveyor);
    surveyor->setText(QStringLiteral("김조사"));
    note->setFocus();
    note->setPlainText(QStringLiteral("북벽 교란"));
    card.setFeature(layer.get(), ids.at(1));
    const QgsFeature first = layer->getFeature(ids.at(0));
    const QgsFeature second = layer->getFeature(ids.at(1));
    QCOMPARE(first.attribute(QStringLiteral("note")).toString(), QStringLiteral("북벽 교란"));
    QVERIFY(second.attribute(QStringLiteral("note")).toString().isEmpty());
    QCOMPARE(second.attribute(QStringLiteral("surveyor")).toString(), QStringLiteral("권영인"));
    layer->rollBack();
  }
};

int main(int argc, char** argv) {
  QgsApplication app(argc, argv, true);
  QgsApplication::setPrefixPath(qEnvironmentVariable("QGIS_PREFIX_PATH", QStringLiteral("A:/OSGeo4W/apps/qgis-dev")),
                                true);
  QgsApplication::initQgis();
  FeaturePresets::instance().ensureLoaded();
  TestFeatureCardFields tc;
  const int rc = QTest::qExec(&tc, argc, argv);
  QgsApplication::exitQgis();
  return rc;
}

#include "test_feature_card_fields.moc"
