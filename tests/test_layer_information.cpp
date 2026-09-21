#include <QtTest>
#include <QAbstractItemModelTester>
#include <QCheckBox>
#include <QComboBox>
#include <QElapsedTimer>
#include <QLabel>
#include <QHeaderView>
#include <QScrollBar>
#include <QScrollArea>
#include <QSplitter>
#include <QToolButton>
#include <QVBoxLayout>
#include <QDir>
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
}
class LayerInformationTest : public QObject {
  Q_OBJECT
private slots:
  void nativeModelSupportsSecondColumnWithoutBreakingTree() {
    QgsProject project;
    auto* group = project.layerTreeRoot()->addGroup(QStringLiteral("참조 지도"));
    auto* layer = add(project, group, QStringLiteral("지적도"));
    KaLayerInformationModel model(&project, false);
    model.setFlag(QgsLayerTreeModel::AllowNodeChangeVisibility);
    QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);
    const auto index = model.node2index(project.layerTreeRoot()->findLayer(layer->id()));
    const auto text = index.siblingAtColumn(1);
    QVERIFY(text.isValid()); QCOMPARE(model.parent(text), model.parent(index));
    QCOMPARE(model.rowCount(text), 0);
    QCOMPARE(model.data(text, Qt::DisplayRole).toString(), QStringLiteral("지번"));
    QCOMPARE(model.data(text, Qt::CheckStateRole).toInt(), int(Qt::Checked));
    QVERIFY(model.setData(text, Qt::Unchecked, Qt::CheckStateRole));
    QVERIFY(!layer->labelsEnabled());
    QVERIFY(project.layerTreeRoot()->findLayer(layer->id())->isVisible());
    project.removeMapLayer(layer);
    QCoreApplication::processEvents();
  }

  void groupBatchPreservesHiddenLayersAndSettlesOnce() {
    QgsProject project; auto* group = project.layerTreeRoot()->addGroup(QStringLiteral("수치지도"));
    QList<QgsVectorLayer*> layers;
    for (int i = 0; i < 200; ++i) layers.append(add(project, group, QString::number(i), i % 2));
    auto* hidden = project.layerTreeRoot()->findLayer(layers.first()->id()); hidden->setItemVisibilityChecked(false);
    KaLayerInformationModel model(&project, false);
    const auto item = model.node2index(group).siblingAtColumn(1);
    QCOMPARE(model.data(item, Qt::CheckStateRole).toInt(), int(Qt::PartiallyChecked));
    QSignalSpy settled(&model, &KaLayerInformationModel::labelsEdited);
    QElapsedTimer elapsed; elapsed.start();
    QVERIFY(model.setData(item, Qt::Checked, Qt::CheckStateRole));
    QVERIFY(model.setData(item, Qt::Unchecked, Qt::CheckStateRole));
    QVERIFY(model.setData(item, Qt::Checked, Qt::CheckStateRole));
    qInfo() << "LABEL_GROUP_200_THREE_TOGGLES_MS" << elapsed.elapsed();
    QVERIFY(!hidden->isVisible());
    for (auto* layer : layers) QVERIFY(layer->labelsEnabled());
    QTRY_COMPARE(settled.count(), 1);
    group->setItemVisibilityChecked(false);
    QVERIFY(!(model.flags(item) & Qt::ItemIsEnabled));
    QVERIFY(!model.setData(item, Qt::Unchecked, Qt::CheckStateRole));
    group->setItemVisibilityChecked(true);
    QCOMPARE(model.data(item, Qt::CheckStateRole).toInt(), int(Qt::Checked));
    QCOMPARE(settled.count(), 1);
  }

  void actualMouseKeyboardAndPanelStayInSync() {
    QgsProject project; auto* group = project.layerTreeRoot()->addGroup(QStringLiteral("참조 지도"));
    auto* layer = add(project, group, QStringLiteral("지적도 · 조사 주변 5km"));
    QWidget host; auto* layout = new QVBoxLayout(&host);
    auto* tree = new KaLayerInformationView(&host);
    KaLayerInformationModel model(&project, false);
    model.setFlag(QgsLayerTreeModel::AllowNodeChangeVisibility);
    tree->setModel(&model); KaLayerInformationModel::configureView(tree);
    layout->addWidget(tree, 1);
    auto* panel = new KaLayerInformationPanel(&model, tree, &host); layout->addWidget(panel);
    host.resize(320, 600); host.show(); tree->expandAll(); tree->setCurrentLayer(layer);
    QVERIFY(QTest::qWaitForWindowExposed(&host));
    const auto index = tree->node2index(project.layerTreeRoot()->findLayer(layer->id())).siblingAtColumn(1);
    QVERIFY(index.isValid());
    auto* checkbox = panel->findChild<QCheckBox*>(QStringLiteral("layerLabelVisible"));
    QVERIFY(checkbox); QTRY_VERIFY(checkbox->isChecked());
    const QRect cell = tree->visualRect(index);
    QVERIFY(!cell.isEmpty());
    QTest::mouseClick(tree->viewport(), Qt::LeftButton, Qt::NoModifier, QPoint(cell.left() + 10, cell.center().y()));
    QTRY_VERIFY(!layer->labelsEnabled()); QTRY_VERIFY(!checkbox->isChecked());
    tree->setCurrentIndex(index); tree->setFocus(); QTest::keyClick(tree, Qt::Key_Space);
    QTRY_VERIFY(layer->labelsEnabled()); QTRY_VERIFY(checkbox->isChecked());
    auto* toggle = panel->findChild<QToolButton*>(QStringLiteral("layerInformationToggle"));
    QVERIFY(toggle);
    QTest::mouseClick(toggle, Qt::LeftButton);
    QTRY_VERIFY(checkbox->isVisible());
    QTest::mouseClick(checkbox, Qt::LeftButton, Qt::NoModifier, QPoint(8, checkbox->height() / 2));
    QTRY_VERIFY(!layer->labelsEnabled());
    LayerOps::setLabelsVisible(layer, true); // Existing context-menu route.
    QTRY_VERIFY(checkbox->isChecked());
    QTRY_VERIFY(tree->columnWidth(0) + tree->columnWidth(1) <= tree->viewport()->width() + 1);
    QCOMPARE(tree->horizontalScrollBar()->value(), 0);
    QVERIFY(tree->visualRect(index.siblingAtColumn(0)).right() > 80);
    QVERIFY(tree->visualRect(index).right() <= tree->viewport()->width());
    const auto path = qEnvironmentVariable("KA_LAYER_UI_QA");
    if (!path.isEmpty()) {
      qInfo() << "LABEL_COLUMNS" << tree->columnWidth(0) << tree->columnWidth(1)
              << tree->header()->sectionResizeMode(0) << tree->header()->sectionResizeMode(1)
              << tree->horizontalScrollBar()->value() << tree->isColumnHidden(0) << tree->isColumnHidden(1);
      QDir().mkpath(path);
      QVERIFY(host.grab().save(QDir(path).filePath(QStringLiteral("layer-panel-%1.png").arg(qEnvironmentVariable("QT_SCALE_FACTOR", "1")))));
    }
    project.layerTreeRoot()->findLayer(layer->id())->setItemVisibilityChecked(false);
    QTRY_VERIFY(!checkbox->isEnabled()); QVERIFY(layer->labelsEnabled());
    QVERIFY(panel->findChild<QLabel*>(QStringLiteral("layerLabelReason"))->text().contains(QStringLiteral("꺼져")));
    project.removeMapLayer(layer); QCoreApplication::processEvents();
    QVERIFY(!checkbox->isEnabled());
  }

  void headerBoundaryCanBeDragged_data() {
    QTest::addColumn<bool>("layoutMode");
    QTest::newRow("map") << false;
    QTest::newRow("layout") << true;
  }

  void headerBoundaryCanBeDragged() {
    QFETCH(bool, layoutMode);
    QgsProject project;
    auto* group = project.layerTreeRoot()->addGroup(QStringLiteral("참조 지도"));
    auto* layer = add(project, group, QStringLiteral("지적도"));
    KaLayerInformationModel model(&project, layoutMode);
    KaLayerInformationView tree;
    tree.setModel(&model); KaLayerInformationModel::configureView(&tree);
    tree.resize(420, 300); tree.show(); tree.expandAll();
    QVERIFY(QTest::qWaitForWindowExposed(&tree));
    QSignalSpy edited(&model, &KaLayerInformationModel::labelsEdited);
    auto* header = tree.header();
    const auto drag = [header](int delta) {
      const QPoint start(header->sectionViewportPosition(0) + header->sectionSize(0) - 1,
                         header->height() / 2);
      QTest::mouseMove(header->viewport(), start);
      QTest::mousePress(header->viewport(), Qt::LeftButton, Qt::NoModifier, start);
      QTest::mouseMove(header->viewport(), start + QPoint(delta, 0), 30);
      QTest::mouseRelease(header->viewport(), Qt::LeftButton, Qt::NoModifier, start + QPoint(delta, 0));
    };
    const int initialWidth = tree.columnWidth(0);
    drag(-60);
    QTRY_VERIFY(tree.columnWidth(0) <= initialWidth - 50);
    const int narrowerWidth = tree.columnWidth(0);
    drag(30);
    QTRY_VERIFY(tree.columnWidth(0) >= narrowerWidth + 20);
    const int chosenWidth = tree.columnWidth(0);
    tree.resize(520, 300);
    QTRY_COMPARE(tree.columnWidth(0), chosenWidth);
    tree.resize(420, 300);
    QTRY_COMPARE(tree.columnWidth(0), chosenWidth);
    model.refreshLabels(); QTest::qWait(150);
    QCOMPARE(tree.columnWidth(0), chosenWidth);
    QCOMPARE(edited.count(), 0);
    QVERIFY(layer->labelsEnabled());
    QVERIFY(project.layerTreeRoot()->findLayer(layer->id())->isVisible());
    QVERIFY(tree.columnWidth(0) + tree.columnWidth(1) <= tree.viewport()->width() + 1);
    drag(400);
    QTRY_VERIFY(tree.columnWidth(1) >= 88);
    QVERIFY(tree.columnWidth(0) + tree.columnWidth(1) <= tree.viewport()->width() + 1);
  }

  void listKeepsFiveRowsAtFieldWindowSizes_data() {
    QTest::addColumn<QSize>("window");
    QTest::newRow("1366x768-100") << QSize(1366, 768);
    QTest::newRow("1920x1080-100") << QSize(1920, 1080);
    QTest::newRow("1920x1080-150") << QSize(1280, 720);
    QTest::newRow("1920x1080-200") << QSize(960, 540);
    QTest::newRow("1366x768-200") << QSize(683, 384);
  }

  void listKeepsFiveRowsAtFieldWindowSizes() {
    QFETCH(QSize, window);
    QgsProject project;
    auto* group = project.layerTreeRoot()->addGroup(QStringLiteral("조사 데이터"));
    for (int i = 0; i < 8; ++i) add(project, group, QStringLiteral("행%1").arg(i));
    QWidget host;
    auto* split = new QSplitter(Qt::Vertical, &host);
    split->setObjectName(QStringLiteral("leftSplit"));
    split->setChildrenCollapsible(false);
    auto* layers = new QFrame(split);
    auto* layersLay = new QVBoxLayout(layers);
    layersLay->setContentsMargins(6, 6, 6, 6);
    auto* cap = new QToolButton(layers);
    cap->setObjectName(QStringLiteral("sidebarFilesToggle"));
    cap->setText(QStringLiteral("파일함"));
    cap->setCheckable(true);
    cap->setChecked(true);
    auto* tree = new KaLayerInformationView(layers);
    KaLayerInformationModel model(&project, false);
    tree->setModel(&model);
    KaLayerInformationModel::configureView(tree);
    tree->expandAll();
    auto* panel = new KaLayerInformationPanel(&model, tree, layers);
    layersLay->addWidget(cap);
    layersLay->addWidget(tree, 1);
    layersLay->addWidget(panel);
    auto* files = new QFrame(split);
    files->setObjectName(QStringLiteral("sidebarFilesScroll"));
    files->setMinimumHeight(200);
    auto* filesLay = new QVBoxLayout(files);
    filesLay->addWidget(new QLabel(QStringLiteral("파일함"), files), 1);
    split->addWidget(layers);
    split->addWidget(files);
    split->setCollapsible(1, true);
    split->setSizes({380, 260});
    connect(cap, &QToolButton::toggled, files, &QWidget::setVisible);
    auto* hostLay = new QVBoxLayout(&host);
    hostLay->setContentsMargins(0, 0, 0, 0);
    hostLay->addWidget(new QLabel(QStringLiteral("ribbon"), &host));
    hostLay->addWidget(split, 1);
    hostLay->addWidget(new QLabel(QStringLiteral("status"), &host));
    host.resize(qMin(360, window.width()), window.height());
    host.show();
    QVERIFY(QTest::qWaitForWindowExposed(&host));
    KaLayerInformationView::protectSidebarList(split, tree, cap, files, panel);
    QVERIFY2(tree->viewport()->height() > 1,
             qPrintable(QStringLiteral("viewport=%1px window=%2x%3")
                            .arg(tree->viewport()->height())
                            .arg(window.width())
                            .arg(window.height())));
    QTRY_VERIFY(tree->viewport()->height() >= tree->minimumListHeight() - tree->header()->height());
    const int row = qMax(22, tree->sizeHintForRow(0));
    QVERIFY2(tree->viewport()->height() >= row * KaLayerInformationView::kMinVisibleRows,
             qPrintable(QStringLiteral("viewport=%1 row=%2 window=%3x%4")
                            .arg(tree->viewport()->height())
                            .arg(row)
                            .arg(window.width())
                            .arg(window.height())));
  }

  void unknownFieldsRequireExplicitChoice() {
    QgsProject project;
    auto* layer = new QgsVectorLayer(QStringLiteral("Point?crs=EPSG:5186&field=opaque_code:string"), QStringLiteral("가져온 자료"), QStringLiteral("memory"));
    project.addMapLayer(layer);
    KaLayerInformationModel model(&project, false);
    QgsLayerTreeView tree; tree.setModel(&model);
    KaLayerInformationPanel panel(&model, &tree);
    tree.setCurrentLayer(layer); QCoreApplication::processEvents();
    auto* fields = panel.findChild<QComboBox*>(QStringLiteral("layerLabelField"));
    QVERIFY(fields->isEnabled()); QVERIFY(!layer->labelsEnabled());
    QCOMPARE(fields->count(), 2); fields->setCurrentIndex(1);
    QVERIFY(QMetaObject::invokeMethod(fields, "activated", Q_ARG(int, 1)));
    QVERIFY(layer->labelsEnabled()); QVERIFY(LayerLabelControls::describe(layer).supported);
  }
};
int main(int argc, char** argv) {
  QgsApplication app(argc, argv, true);
  QgsApplication::setPrefixPath(qEnvironmentVariable("QGIS_PREFIX_PATH", "A:/OSGeo4W/apps/qgis-dev"), true);
  QgsApplication::initQgis();
  LayerInformationTest test; const int result = QTest::qExec(&test, argc, argv);
  QgsApplication::exitQgis(); return result;
}
#include "test_layer_information.moc"
