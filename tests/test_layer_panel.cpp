// Layer list model cache, row notes and the label look controls of 표시 설정
// (evaluation F219, F184, F145).
#include <QtTest>
#include <QCheckBox>
#include <QComboBox>
#include <qgsapplication.h>
#include <qgslayertree.h>
#include <qgslayertreeview.h>
#include <qgsproject.h>
#include <qgsvectorlayer.h>
#include "app/KaLayerInformation.h"
#include "core/LayerLabelControls.h"
#include "core/LayerOps.h"

namespace {
QgsVectorLayer* add(QgsProject& project, QgsLayerTreeGroup* group, const QString& name, bool on = true) {
  auto* layer = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5187&field=JIBUN:string"), name, QStringLiteral("memory"));
  LayerOps::applyNameAttributeLabels(layer, QStringLiteral("JIBUN"), 8., false);
  layer->setLabelsEnabled(on);
  project.addMapLayer(layer, false); group->addLayer(layer); return layer;
}
}  // namespace

class LayerPanelTest : public QObject {
  Q_OBJECT
private slots:
  void labelInfoIsCachedAndRefreshedPerLayer() {
    QgsProject project;
    auto* group = project.layerTreeRoot()->addGroup(QStringLiteral("수치지도"));
    QList<QgsVectorLayer*> layers;
    for (int i = 0; i < 20; ++i) layers.append(add(project, group, QString::number(i)));
    KaLayerInformationModel model(&project, false);
    QCoreApplication::processEvents();
    const auto item = model.node2index(project.layerTreeRoot()->findLayer(layers.first()->id())).siblingAtColumn(1);
    const auto groupItem = model.node2index(group).siblingAtColumn(1);
    (void)model.data(groupItem, Qt::CheckStateRole);
    (void)model.data(item, Qt::DisplayRole);
    const int warm = model.describeCallsForTests();
    for (int i = 0; i < 50; ++i) {
      (void)model.data(groupItem, Qt::CheckStateRole);
      (void)model.data(item, Qt::DisplayRole);
    }
    QCOMPARE(model.describeCallsForTests(), warm);
    // One layer's repaint: only its rows (and its group) are announced.
    QSignalSpy changed(&model, &QAbstractItemModel::dataChanged);
    QVERIFY(LayerLabelControls::setVisible(layers.first(), false));
    QCOMPARE(model.data(item, Qt::CheckStateRole).toInt(), int(Qt::Unchecked));  // not stale
    QCOMPARE(model.data(groupItem, Qt::CheckStateRole).toInt(), int(Qt::PartiallyChecked));
    QTRY_VERIFY(!changed.isEmpty());
    QTest::qWait(20);
    // A full refresh would announce all 21 rows; only the layer row and its group are left.
    QVERIFY2(changed.count() < 5, qPrintable(QString::number(changed.count())));
  }

  void fixedDrawingOrderShowsInTheRowTooltip() {
    QgsProject project;
    auto* satellite = add(project, project.layerTreeRoot(), QStringLiteral("위성"));
    auto* plain = add(project, project.layerTreeRoot(), QStringLiteral("참고 면"));
    LayerOps::markReferenceLayer(plain);
    KaLayerInformationModel model(&project, false);
    const auto satRow = model.node2index(project.layerTreeRoot()->findLayer(satellite->id()));
    const auto plainRow = model.node2index(project.layerTreeRoot()->findLayer(plain->id()));
    QVERIFY(model.data(satRow, Qt::ToolTipRole).toString().contains(QStringLiteral("그리기 순서 고정")));
    QVERIFY(!model.data(plainRow, Qt::ToolTipRole).toString().contains(QStringLiteral("그리기 순서 고정")));
  }

  void panelOffersLabelColourAndHalo() {
    QgsProject project;
    auto* layer = add(project, project.layerTreeRoot(), QStringLiteral("유구 번호"));
    KaLayerInformationModel model(&project, false);
    QgsLayerTreeView tree; tree.setModel(&model);
    KaLayerInformationPanel panel(&model, &tree);
    tree.setCurrentLayer(layer); QCoreApplication::processEvents();
    auto* colour = panel.findChild<QComboBox*>(QStringLiteral("layerLabelColor"));
    auto* halo = panel.findChild<QCheckBox*>(QStringLiteral("layerLabelHalo"));
    QVERIFY(colour && halo);
    QVERIFY(colour->isEnabled() && halo->isEnabled());
    QVERIFY(halo->isChecked());
    const QString expression = layer->labeling()->settings().fieldName;
    const int blue = colour->findText(QStringLiteral("진한 파랑"));
    QVERIFY(blue >= 0);
    QVERIFY(QMetaObject::invokeMethod(colour, "activated", Q_ARG(int, blue)));
    QCOMPARE(LayerOps::labelColor(layer), QColor(QStringLiteral("#1e3a8a")));
    halo->click();
    QVERIFY(!LayerOps::labelHalo(layer));
    QCOMPARE(layer->labeling()->settings().fieldName, expression);
    QVERIFY(layer->labelsEnabled());
  }
};

int main(int argc, char** argv) {
  QgsApplication app(argc, argv, true);
  QgsApplication::setPrefixPath(qEnvironmentVariable("QGIS_PREFIX_PATH", "A:/OSGeo4W/apps/qgis-dev"), true);
  QgsApplication::initQgis();
  LayerPanelTest test; const int result = QTest::qExec(&test, argc, argv);
  QgsApplication::exitQgis(); return result;
}
#include "test_layer_panel.moc"
