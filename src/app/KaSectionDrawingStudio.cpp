#include "KaSectionDrawingStudio.h"

#include "KaTheme.h"
#include "core/LayerOps.h"
#include "core/LayoutService.h"
#include "core/SectionLayoutService.h"

#include <QBrush>
#include <QCheckBox>
#include <QColor>
#include <QColorDialog>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QFrame>
#include <QGraphicsView>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPainter>
#include <QPen>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QStyle>
#include <QShowEvent>
#include <QSizePolicy>
#include <QSplitter>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

#include <qgis.h>
#include <qgscoordinatereferencesystem.h>
#include <qgslayout.h>
#include <qgslayoutitempage.h>
#include <qgslayoutmanager.h>
#include <qgslayoutpagecollection.h>
#include <qgslayoutrendercontext.h>
#include <qgslayoutview.h>
#include <qgslayoutviewtoolpan.h>
#include <qgslayoutitemscalebar.h>
#include <qgslayoutviewtoolselect.h>
#include <qgsmaplayer.h>
#include <qgsprintlayout.h>
#include <qgsproject.h>
#include <qgsrasterlayer.h>

namespace {
static const QString kSectionSheet = QStringLiteral("section_sheet");
constexpr double kPreviewDpi = 300.0;

QIcon scaleBarPreviewIcon(const char* style) {
    QPixmap pm(72, 40);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(QPen(KaTheme::tokens().border, 1));
    p.setBrush(KaTheme::tokens().bevelLight);
    p.drawRoundedRect(QRectF(1, 4, 70, 32), 5, 5);
    const QString s = QString::fromUtf8(style);
    if (s == QLatin1String("Line Ticks Up")) {
        p.setPen(QPen(KaTheme::tokens().ink, 1.6));
        p.drawLine(QPointF(10, 26), QPointF(62, 26));
        for (int i = 0; i < 5; ++i) {
            const double x = 10.0 + i * 13.0;
            p.drawLine(QPointF(x, 26), QPointF(x, 14));
        }
    } else if (s == QLatin1String("Single Box")) {
        p.setPen(QPen(KaTheme::tokens().ink, 1));
        for (int i = 0; i < 4; ++i) {
            p.setBrush(i % 2 == 0 ? KaTheme::tokens().ink : KaTheme::tokens().bevelLight);
            p.drawRect(QRectF(10 + i * 13, 16, 13, 10));
        }
    } else {
        p.setPen(QPen(KaTheme::tokens().ink, 1));
        for (int row = 0; row < 2; ++row) {
            for (int i = 0; i < 4; ++i) {
                const bool dark = ((i + row) % 2) == 0;
                p.setBrush(dark ? KaTheme::tokens().ink : KaTheme::tokens().bevelLight);
                p.drawRect(QRectF(10 + i * 13, 12 + row * 8, 13, 8));
            }
        }
    }
    return QIcon(pm);
}

QToolButton* makeSampleTile(QWidget* parent, const QIcon& icon, const QString& text,
                            const QString& objectName) {
    auto* b = new QToolButton(parent);
    b->setObjectName(objectName);
    b->setIcon(icon);
    b->setIconSize(QSize(40, 22));
    b->setText(text);
    b->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    b->setAutoRaise(true);
    b->setCursor(Qt::PointingHandCursor);
    b->setToolTip(text);
    b->setProperty("class", QStringLiteral("sampleTile"));
    b->setCheckable(true);
    return b;
}
} // namespace

// ============================================================
// Constructor / Destructor
// ============================================================

KaSectionDrawingStudio::KaSectionDrawingStudio(QgsProject* project, QWidget* parent)
    : QWidget(parent)
    , m_project(project)
{
    buildUi();
    syncCrsComboFromProject();
    rebuildSheet(false);
    refreshLayers();

    if (m_project) {
        connect(m_project, &QgsProject::layersAdded,
                this, &KaSectionDrawingStudio::onLayersAdded);
        connect(m_project, &QgsProject::layersRemoved,
                this, &KaSectionDrawingStudio::onLayersRemoved);
    }
}

KaSectionDrawingStudio::~KaSectionDrawingStudio()
{
    detachLayoutFromView();
}

// ============================================================
// UI construction
// ============================================================

void KaSectionDrawingStudio::buildUi()
{
    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto* splitter = new QSplitter(Qt::Horizontal, this);
    splitter->setChildrenCollapsible(false);

    splitter->addWidget(buildLeftPanel());
    splitter->addWidget(buildCenterPanel());
    splitter->addWidget(buildRightPanel());

    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setStretchFactor(2, 0);

    root->addWidget(splitter);
}

QWidget* KaSectionDrawingStudio::buildLeftPanel()
{
    auto* frame = new QFrame(this);
    frame->setObjectName(QStringLiteral("sectionLayersPanel"));
    frame->setMinimumWidth(240);
    frame->setMaximumWidth(300);
    frame->setFrameShape(QFrame::StyledPanel);

    auto* vl = new QVBoxLayout(frame);
    vl->setContentsMargins(4, 4, 4, 4);
    vl->setSpacing(4);

    auto* caption = new QLabel(QStringLiteral("단면 GeoTIFF"), frame);
    caption->setObjectName(QStringLiteral("cardCaption"));
    vl->addWidget(caption);

    auto* btnAddGeo = new QPushButton(QStringLiteral("GeoTIFF 추가"), frame);
    btnAddGeo->setObjectName(QStringLiteral("addGeoTiffBtn"));
    btnAddGeo->setToolTip(QStringLiteral(
        "단면 GeoTIFF만 이 도면에 넣습니다. 위성·지적은 쓰지 않습니다."));
    vl->addWidget(btnAddGeo);

    auto* btnRow = new QHBoxLayout();
    btnRow->setSpacing(2);

    auto makeBtn = [&](const QString& text, const QString& tip) {
        auto* b = new QToolButton(frame);
        b->setText(text);
        b->setToolTip(tip);
        b->setAutoRaise(true);
        return b;
    };

    auto* btnRefresh = makeBtn(QStringLiteral("새로고침"), QStringLiteral("목록 새로고침"));
    auto* btnUp      = makeBtn(QStringLiteral("위"), QStringLiteral("위로 이동"));
    auto* btnDown    = makeBtn(QStringLiteral("아래"), QStringLiteral("아래로 이동"));
    auto* btnRemove  = makeBtn(QStringLiteral("빼기"), QStringLiteral("목록에서 제거 (프로젝트에서는 지우지 않음)"));

    btnRow->addWidget(btnRefresh);
    btnRow->addWidget(btnUp);
    btnRow->addWidget(btnDown);
    btnRow->addStretch();
    btnRow->addWidget(btnRemove);
    vl->addLayout(btnRow);

    m_layerTree = new QTreeWidget(frame);
    m_layerTree->setObjectName(QStringLiteral("sectionLayersTree"));
    m_layerTree->setColumnCount(1);
    m_layerTree->header()->setVisible(false);
    m_layerTree->setDragDropMode(QAbstractItemView::InternalMove);
    m_layerTree->setSelectionMode(QAbstractItemView::SingleSelection);
    vl->addWidget(m_layerTree, 1);

    auto* empty = new QLabel(
        QStringLiteral("단면 사진(GeoTIFF)을 추가하면 이 목록에 나타납니다."), frame);
    empty->setObjectName(QStringLiteral("emptyState"));
    empty->setWordWrap(true);
    empty->setAlignment(Qt::AlignCenter);
    vl->addWidget(empty);

    connect(btnAddGeo,  &QPushButton::clicked, this, &KaSectionDrawingStudio::addGeoTiff);
    connect(btnRefresh, &QToolButton::clicked, this, &KaSectionDrawingStudio::refreshLayers);
    connect(btnUp,      &QToolButton::clicked, this, &KaSectionDrawingStudio::moveLayerUp);
    connect(btnDown,    &QToolButton::clicked, this, &KaSectionDrawingStudio::moveLayerDown);
    connect(btnRemove,  &QToolButton::clicked, this, &KaSectionDrawingStudio::removeFromList);
    connect(m_layerTree, &QTreeWidget::itemChanged,
            this, &KaSectionDrawingStudio::onTreeItemChanged);

    return frame;
}

QWidget* KaSectionDrawingStudio::buildCenterPanel()
{
    auto* container = new QWidget(this);
    auto* vl = new QVBoxLayout(container);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    auto* toolbar = new QToolBar(container);
    toolbar->setIconSize(QSize(16, 16));
    toolbar->setToolButtonStyle(Qt::ToolButtonTextOnly);

    auto* actSelect = toolbar->addAction(QStringLiteral("\uc120\ud0dd"));
    auto* actPan    = toolbar->addAction(QStringLiteral("\uc774\ub3d9"));
    auto* actZoom   = toolbar->addAction(QStringLiteral("\uc804\uccb4\ubcf4\uae30"));

    vl->addWidget(toolbar);

    auto* strip = new QWidget(container);
    strip->setObjectName(QStringLiteral("sampleStrip"));
    auto* stripLay = new QHBoxLayout(strip);
    stripLay->setContentsMargins(10, 4, 10, 4);
    stripLay->setSpacing(8);
    auto* stripCap = new QLabel(QStringLiteral("축척자 샘플"), strip);
    stripCap->setObjectName(QStringLiteral("cardCaption"));
    stripLay->addWidget(stripCap);
    struct BarSample { const char* style; const char* tip; const char* id; };
    const BarSample bars[] = {
        {"Double Box", "쌍칸", "sampleScaleBarDouble"},
        {"Single Box", "외칸", "sampleScaleBarSingle"},
        {"Line Ticks Up", "눈금", "sampleScaleBarTicks"},
    };
    for (const auto& bs : bars) {
        auto* b = makeSampleTile(strip, scaleBarPreviewIcon(bs.style),
                                 QString::fromUtf8(bs.tip), QString::fromUtf8(bs.id));
        const QString style = QString::fromUtf8(bs.style);
        connect(b, &QToolButton::clicked, this, [this, style]() { applyScaleBarStyle(style); });
        stripLay->addWidget(b);
        if (style == QLatin1String("Double Box"))
            b->setChecked(true);
    }
    stripLay->addStretch(1);
    vl->addWidget(strip);

    m_view = new QgsLayoutView(container);
    m_view->setObjectName(QStringLiteral("sectionLayoutView"));
    m_view->setBackgroundBrush(QBrush(KaTheme::tokens().desk));
    vl->addWidget(m_view, 1);

    m_toolSelect = new QgsLayoutViewToolSelect(m_view);
    m_toolPan    = new QgsLayoutViewToolPan(m_view);
    // Do not setTool here. QgsLayoutViewToolSelect::setLayout() creates
    // QgsLayoutMouseHandles and QGraphicsScene::addItem() crashes if the
    // select tool is already active on a view with no layout (0xc0000005).
    // Activate the tool only after setCurrentLayout + setLayout.

    m_statusLabel = new QLabel(container);
    m_statusLabel->setObjectName(QStringLiteral("studioStatus"));
    m_statusLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_statusLabel->setContentsMargins(4, 2, 4, 2);
    vl->addWidget(m_statusLabel);

    connect(actSelect, &QAction::triggered, this, [this]() {
        if (m_view && m_toolSelect) m_view->setTool(m_toolSelect);
    });
    connect(actPan, &QAction::triggered, this, [this]() {
        if (m_view && m_toolPan) m_view->setTool(m_toolPan);
    });
    connect(actZoom, &QAction::triggered, this, [this]() {
        if (!m_view) return;
        auto* ly = currentLayout();
        if (!ly) return;
        auto* pc = ly->pageCollection();
        if (!pc || pc->pageCount() == 0) return;
        auto* pg = pc->page(0);
        if (!pg) return;
        const QRectF pr = pg->mapRectToScene(pg->rect());
        const qreal pad = std::max(pr.width(), pr.height()) * 0.10;
        m_view->fitInView(pr.adjusted(-pad, -pad, pad, pad), Qt::KeepAspectRatio);
    });

    return container;
}

QWidget* KaSectionDrawingStudio::buildRightPanel()
{
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setMinimumWidth(200);
    scroll->setMaximumWidth(300);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto* panel = new QWidget();
    panel->setObjectName(QStringLiteral("sectionPropertiesPanel"));
    auto* form = new QFormLayout(panel);
    form->setRowWrapPolicy(QFormLayout::WrapLongRows);
    form->setLabelAlignment(Qt::AlignRight);
    form->setContentsMargins(8, 8, 8, 8);
    form->setSpacing(6);

    // Paper
    m_paperCombo = new QComboBox(panel);
    m_paperCombo->setObjectName(QStringLiteral("paperCombo"));
    m_paperCombo->addItem(QStringLiteral("A3"));
    m_paperCombo->addItem(QStringLiteral("A4"));
    form->addRow(QStringLiteral("\uc6a9\uc9c0:"), m_paperCombo);

    // Title
    m_titleEdit = new QLineEdit(panel);
    m_titleEdit->setObjectName(QStringLiteral("titleEdit"));
    m_titleEdit->setText(QStringLiteral("\ub2e8\uba74\ub3c4"));
    m_titleEdit->setPlaceholderText(QStringLiteral("\ub2e8\uba74\ub3c4"));
    form->addRow(QStringLiteral("\ub3c4\uba74\uba85:"), m_titleEdit);

    // Scale
    m_scaleCombo = new QComboBox(panel);
    m_scaleCombo->setObjectName(QStringLiteral("scaleCombo"));
    m_scaleCombo->setEditable(true);
    m_scaleCombo->addItem(QStringLiteral("\uc790\ub3d9 \ub9de\ucda4"));
    m_scaleCombo->addItem(QStringLiteral("1:10"));
    m_scaleCombo->addItem(QStringLiteral("1:20"));
    m_scaleCombo->addItem(QStringLiteral("1:25"));
    m_scaleCombo->addItem(QStringLiteral("1:40"));
    m_scaleCombo->addItem(QStringLiteral("1:50"));
    m_scaleCombo->addItem(QStringLiteral("1:100"));
    m_scaleCombo->addItem(QStringLiteral("1:200"));
    m_scaleCombo->addItem(QStringLiteral("1:250"));
    form->addRow(QStringLiteral("\ucd95\ucca9:"), m_scaleCombo);

    m_crsCombo = new QComboBox(panel);
    m_crsCombo->setObjectName(QStringLiteral("crsCombo"));
    m_crsCombo->addItem(QStringLiteral("EPSG:5187 (동부원점)"), QStringLiteral("EPSG:5187"));
    m_crsCombo->addItem(QStringLiteral("EPSG:5186 (중부원점)"), QStringLiteral("EPSG:5186"));
    m_crsCombo->setCurrentIndex(0);
    form->addRow(QStringLiteral("좌표계:"), m_crsCombo);

    // Elevation offset
    m_elevOffsetSpin = new QDoubleSpinBox(panel);
    m_elevOffsetSpin->setObjectName(QStringLiteral("elevationOffsetSpin"));
    m_elevOffsetSpin->setRange(-9999.0, 9999.0);
    m_elevOffsetSpin->setDecimals(2);
    m_elevOffsetSpin->setValue(0.00);
    m_elevOffsetSpin->setSuffix(QStringLiteral("m"));
    m_elevOffsetSpin->setToolTip(QStringLiteral("\ud45c\uace0 \ubcf4\uc815\uac12 (rasterY + offset = \ud45c\uc2dc \ud45c\uace0)"));
    form->addRow(QStringLiteral("\ud45c\uace0 \ubcf4\uc815:"), m_elevOffsetSpin);

    // Elevation interval
    m_elevIntervalSpin = new QDoubleSpinBox(panel);
    m_elevIntervalSpin->setObjectName(QStringLiteral("elevationIntervalSpin"));
    m_elevIntervalSpin->setRange(0.01, 10.0);
    m_elevIntervalSpin->setDecimals(2);
    m_elevIntervalSpin->setSingleStep(0.01);
    m_elevIntervalSpin->setValue(0.10);
    m_elevIntervalSpin->setSuffix(QStringLiteral("m"));
    form->addRow(QStringLiteral("\ud45c\uace0 \uac04\uaca9:"), m_elevIntervalSpin);

    // Distance auto / manual
    m_distAutoCheck = new QCheckBox(QStringLiteral("\uc790\ub3d9"), panel);
    m_distAutoCheck->setObjectName(QStringLiteral("distanceAutoCheck"));
    m_distAutoCheck->setChecked(true);
    m_distAutoCheck->setToolTip(QStringLiteral("\uac70\ub9ac \ub208\uae08 \uac04\uaca9 \uc790\ub3d9 (1-2-5 \uacc4\uc5f4)"));
    form->addRow(QStringLiteral("\uac70\ub9ac \uac04\uaca9:"), m_distAutoCheck);

    m_distManualSpin = new QDoubleSpinBox(panel);
    m_distManualSpin->setObjectName(QStringLiteral("distanceManualSpin"));
    m_distManualSpin->setRange(0.01, 9999.0);
    m_distManualSpin->setDecimals(2);
    m_distManualSpin->setValue(0.50);
    m_distManualSpin->setSuffix(QStringLiteral("m"));
    m_distManualSpin->setEnabled(false);
    m_distManualSpin->setVisible(false);
    form->addRow(QStringLiteral(""), m_distManualSpin);

    // Reference line
    m_refLineCheck = new QCheckBox(QStringLiteral("\ud45c\uc2dc"), panel);
    m_refLineCheck->setObjectName(QStringLiteral("referenceLineCheck"));
    m_refLineCheck->setChecked(true);
    form->addRow(QStringLiteral("\uae30\uc900\uc120:"), m_refLineCheck);

    m_refWidthSpin = new QDoubleSpinBox(panel);
    m_refWidthSpin->setObjectName(QStringLiteral("referenceLineWidthSpin"));
    m_refWidthSpin->setRange(0.05, 2.0);
    m_refWidthSpin->setDecimals(2);
    m_refWidthSpin->setSingleStep(0.05);
    m_refWidthSpin->setValue(0.20);
    m_refWidthSpin->setSuffix(QStringLiteral("mm"));
    form->addRow(QStringLiteral("\uae30\uc900\uc120 \uad75\uae30:"), m_refWidthSpin);

    m_refColorBtn = new QPushButton(QStringLiteral("#D7191C"), panel);
    m_refColorBtn->setObjectName(QStringLiteral("referenceLineColorBtn"));
    m_refColorBtn->setProperty("lineColor", QStringLiteral("#D7191C"));
    m_refColorBtn->setStyleSheet(QStringLiteral(
        "background:#D7191C; color:white; padding:2px 6px; border-radius:3px;"));
    m_refColorBtn->setToolTip(QStringLiteral("기준선 색상"));
    form->addRow(QStringLiteral("색상:"), m_refColorBtn);

    // Separator
    auto* sep = new QFrame(panel);
    sep->setFrameShape(QFrame::HLine);
    sep->setFrameShadow(QFrame::Sunken);
    form->addRow(sep);

    // Build button
    m_buildBtn = new QPushButton(QStringLiteral("\ub2e8\uba74\ub3c4 \ub9cc\ub4e4\uae30"), panel);
    m_buildBtn->setObjectName(QStringLiteral("btnPrimary"));
    m_buildBtn->setToolTip(QStringLiteral("\uccb4\ud06c\ub41c \ub808\uc774\uc5b4\ub85c \ub2e8\uba74\ub3c4 \uc870\ud310\uc744 \uc0dd\uc131\ud569\ub2c8\ub2e4"));
    form->addRow(m_buildBtn);

    // PDF button
    m_pdfBtn = new QPushButton(QStringLiteral("PDF \uc800\uc7a5"), panel);
    m_pdfBtn->setObjectName(QStringLiteral("pdfSaveBtn"));
    m_pdfBtn->setEnabled(false);
    m_pdfBtn->setToolTip(QStringLiteral("\ub2e8\uba74\ub3c4\ub97c PDF\ub85c \ub0b4\ubcf4\ub0c5\ub2c8\ub2e4 (\ub2e8\uba74\ub3c4 \ub9cc\ub4e4\uae30 \ud6c4 \ud65c\uc131\ud654)"));
    form->addRow(m_pdfBtn);

    scroll->setWidget(panel);
    panel->ensurePolished();
    const int requiredWidth = panel->minimumSizeHint().width() +
        scroll->style()->pixelMetric(QStyle::PM_ScrollBarExtent) + 2 * scroll->frameWidth();
    scroll->setMinimumWidth(std::max(200, requiredWidth));
    scroll->setMaximumWidth(std::max(300, requiredWidth));

    connect(m_paperCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &KaSectionDrawingStudio::onPaperChanged);
    connect(m_crsCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &KaSectionDrawingStudio::onCrsChanged);
    connect(m_distAutoCheck, &QCheckBox::toggled,
            this, &KaSectionDrawingStudio::onDistanceAutoToggled);
    connect(m_refWidthSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [this](double) { rebuildSheet(false); });
    connect(m_refColorBtn, &QPushButton::clicked,
            this, &KaSectionDrawingStudio::onReferenceColorClicked);
    connect(m_buildBtn, &QPushButton::clicked,
            this, &KaSectionDrawingStudio::buildSection);
    connect(m_pdfBtn, &QPushButton::clicked,
            this, &KaSectionDrawingStudio::exportPdf);

    return scroll;
}

void KaSectionDrawingStudio::applyScaleBarStyle(const QString& style)
{
    m_scaleBarStyle = style.isEmpty() ? QStringLiteral("Double Box") : style;
    auto* ly = currentLayout();
    if (!ly) {
        rebuildSheet(false);
        return;
    }
    auto* sb = qobject_cast<QgsLayoutItemScaleBar*>(
        ly->itemById(QStringLiteral("ka_section_scale_bar")));
    if (!sb) {
        rebuildSheet(false);
        return;
    }
    sb->setStyle(m_scaleBarStyle);
    LayoutService::applySheetScaleBarInk(sb);
    sb->update();
    if (auto* dbl = findChild<QToolButton*>(QStringLiteral("sampleScaleBarDouble")))
        dbl->setChecked(m_scaleBarStyle == QLatin1String("Double Box"));
    if (auto* sgl = findChild<QToolButton*>(QStringLiteral("sampleScaleBarSingle")))
        sgl->setChecked(m_scaleBarStyle == QLatin1String("Single Box"));
    if (auto* tck = findChild<QToolButton*>(QStringLiteral("sampleScaleBarTicks")))
        tck->setChecked(m_scaleBarStyle == QLatin1String("Line Ticks Up"));
    setStatus(QStringLiteral("축척자: %1").arg(
        m_scaleBarStyle == QLatin1String("Single Box") ? QStringLiteral("외칸")
        : m_scaleBarStyle == QLatin1String("Line Ticks Up") ? QStringLiteral("눈금")
                                                          : QStringLiteral("쌍칸")));
}

void KaSectionDrawingStudio::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    QTimer::singleShot(0, this, [this]() { fitPaperInView(); });
}

SectionLayoutOptions KaSectionDrawingStudio::collectOptions() const
{
    SectionLayoutOptions opts;
    opts.paper = (m_paperCombo && m_paperCombo->currentIndex() == 0)
                 ? SectionLayoutOptions::Paper::A3
                 : SectionLayoutOptions::Paper::A4;
    opts.titleKo = (m_titleEdit && !m_titleEdit->text().isEmpty())
                   ? m_titleEdit->text() : QStringLiteral("단면도");

    if (m_scaleCombo) {
        const int scIdx = m_scaleCombo->currentIndex();
        if (scIdx == 0) {
            opts.scaleDenominator = 0.0;
        } else {
            const QString txt = m_scaleCombo->currentText();
            const int colon = txt.indexOf(QLatin1Char(':'));
            opts.scaleDenominator = (colon >= 0)
                ? txt.mid(colon + 1).toDouble()
                : txt.toDouble();
        }
    }
    if (m_elevOffsetSpin)
        opts.elevationOffsetM = m_elevOffsetSpin->value();
    if (m_elevIntervalSpin)
        opts.elevationIntervalM = m_elevIntervalSpin->value();
    if (m_distAutoCheck && m_distManualSpin)
        opts.manualDistanceIntervalM = m_distAutoCheck->isChecked()
                                       ? 0.0 : m_distManualSpin->value();
    if (m_refLineCheck)
        opts.showReferenceLine = m_refLineCheck->isChecked();
    if (m_refWidthSpin)
        opts.referenceLineWidthMm = m_refWidthSpin->value();
    if (m_refColorBtn) {
        const QString c = m_refColorBtn->property("lineColor").toString();
        opts.referenceLineColor = c.isEmpty() ? QStringLiteral("#D7191C") : c;
    }
    opts.mapCrsAuthId = selectedCrsAuthId();
    opts.scaleBarStyle = m_scaleBarStyle;
    return opts;
}

void KaSectionDrawingStudio::rebuildSheet(bool interactive)
{
    if (m_rebuilding || !m_project) return;
    m_rebuilding = true;
    detachLayoutFromView();
    const auto result = SectionLayoutService::buildSectionLayout(
        m_project, checkedLayersInOrder(), collectOptions());
    attachLayoutToView();
    m_rebuilding = false;

    if (!result.errorKo.isEmpty()) {
        if (interactive) {
            QMessageBox::warning(this, QStringLiteral("단면도 만들기 오류"),
                                 result.errorKo);
        }
        setStatus(QStringLiteral("오류: ") + result.errorKo);
        return;
    }

    const bool hasRaster = !checkedLayersInOrder().isEmpty();
    if (m_pdfBtn)
        m_pdfBtn->setEnabled(hasRaster);
    if (hasRaster) {
        setStatus(QStringLiteral("단면도 완성. 축척 1:%1")
                      .arg(qRound(result.appliedScaleDenominator)));
    } else {
        setStatus(QStringLiteral("용지 눈금 준비. GeoTIFF를 추가하면 표고·거리에 맞춥니다."));
    }
    fitPaperInView();
}

// ============================================================
// View attach / detach
// ============================================================

void KaSectionDrawingStudio::attachLayoutToView()
{
    auto* ly = currentLayout();
    if (!m_view || !ly) return;

    ly->renderContext().setDpi(kPreviewDpi);
    if (m_view->currentLayout() != ly)
        m_view->setCurrentLayout(ly);
    if (m_toolSelect)
        m_toolSelect->setLayout(ly);
    if (m_toolSelect)
        m_view->setTool(m_toolSelect);

    m_view->setBackgroundBrush(QBrush(KaTheme::tokens().desk));
    fitPaperInView();
}

void KaSectionDrawingStudio::fitPaperInView()
{
    if (!m_view) return;
    auto* ly = currentLayout();
    if (!ly) return;
    auto* pc = ly->pageCollection();
    if (!pc || pc->pageCount() == 0) return;
    auto* pg = pc->page(0);
    if (!pg) return;
    const QRectF pr = pg->mapRectToScene(pg->rect());
    const qreal pad = std::max(pr.width(), pr.height()) * 0.10;
    m_view->fitInView(pr.adjusted(-pad, -pad, pad, pad), Qt::KeepAspectRatio);
}

void KaSectionDrawingStudio::detachLayoutFromView()
{
    if (!m_view) return;
    if (auto* t = m_view->tool())
        m_view->unsetTool(t);
    static_cast<QGraphicsView*>(m_view)->setScene(nullptr);
    // QgsLayoutViewToolSelect::setLayout() does layout->addItem() with no
    // null check. Handles are owned by the previous layout and die with it.
}

QgsPrintLayout* KaSectionDrawingStudio::currentLayout() const
{
    if (!m_project) return nullptr;
    return dynamic_cast<QgsPrintLayout*>(
        m_project->layoutManager()->layoutByName(kSectionSheet));
}

// ============================================================
// Layer tree
// ============================================================

void KaSectionDrawingStudio::refreshLayers()
{
    if (!m_project || !m_layerTree) return;

    QMap<QString, Qt::CheckState> prevState;
    for (int i = 0; i < m_layerTree->topLevelItemCount(); ++i) {
        auto* it = m_layerTree->topLevelItem(i);
        prevState.insert(it->data(0, Qt::UserRole).toString(), it->checkState(0));
    }

    m_suppressTreeSignal = true;
    m_layerTree->clear();

    const auto layers = m_project->mapLayers();
    for (auto it = layers.cbegin(); it != layers.cend(); ++it) {
        auto* layer = it.value();
        if (!layer || m_hiddenLayerIds.contains(layer->id())) continue;
        if (!isSectionStudioLayer(layer)) continue;

        auto* item = new QTreeWidgetItem();
        item->setText(0, layer->name());
        item->setData(0, Qt::UserRole, layer->id());
        item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable
                       | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled);

        Qt::CheckState cs = prevState.contains(layer->id())
            ? prevState.value(layer->id()) : Qt::Checked;
        item->setCheckState(0, cs);
        m_layerTree->addTopLevelItem(item);
    }

    m_suppressTreeSignal = false;
    if (auto* hint = findChild<QLabel*>(QStringLiteral("emptyState")))
        hint->setVisible(m_layerTree->topLevelItemCount() == 0);
}

// ============================================================
// Layer button slots
// ============================================================

void KaSectionDrawingStudio::addGeoTiff()
{
    const QString path = QFileDialog::getOpenFileName(
        this,
        QStringLiteral("GeoTIFF \ud30c\uc77c \uc5f4\uae30"),
        QString(),
        QStringLiteral("GeoTIFF \ud30c\uc77c (*.tif *.tiff);;\ubaa8\ub4e0 \ud30c\uc77c (*)"));
    if (path.isEmpty()) return;

    emit geoTiffAddRequested(path);
}

void KaSectionDrawingStudio::moveLayerUp()
{
    if (!m_layerTree) return;
    auto* item = m_layerTree->currentItem();
    if (!item) return;
    const int row = m_layerTree->indexOfTopLevelItem(item);
    if (row <= 0) return;
    m_suppressTreeSignal = true;
    m_layerTree->takeTopLevelItem(row);
    m_layerTree->insertTopLevelItem(row - 1, item);
    m_layerTree->setCurrentItem(item);
    m_suppressTreeSignal = false;
}

void KaSectionDrawingStudio::moveLayerDown()
{
    if (!m_layerTree) return;
    auto* item = m_layerTree->currentItem();
    if (!item) return;
    const int row  = m_layerTree->indexOfTopLevelItem(item);
    const int last = m_layerTree->topLevelItemCount() - 1;
    if (row >= last) return;
    m_suppressTreeSignal = true;
    m_layerTree->takeTopLevelItem(row);
    m_layerTree->insertTopLevelItem(row + 1, item);
    m_layerTree->setCurrentItem(item);
    m_suppressTreeSignal = false;
}

void KaSectionDrawingStudio::removeFromList()
{
    if (!m_layerTree) return;
    auto* item = m_layerTree->currentItem();
    if (!item) return;
    const QString id = item->data(0, Qt::UserRole).toString();
    m_hiddenLayerIds.insert(id);
    delete m_layerTree->takeTopLevelItem(
        m_layerTree->indexOfTopLevelItem(item));
}

// ============================================================
// Build / Export
// ============================================================

void KaSectionDrawingStudio::buildSection()
{
    applySelectedCrsToSectionRasters();
    rebuildSheet(true);
}

void KaSectionDrawingStudio::exportPdf()
{
    const QString path = QFileDialog::getSaveFileName(
        this,
        QStringLiteral("PDF \uc800\uc7a5"),
        QString(),
        QStringLiteral("PDF \ud30c\uc77c (*.pdf)"));
    if (path.isEmpty()) return;

    QString outPath = path;
    if (!outPath.endsWith(QStringLiteral(".pdf"), Qt::CaseInsensitive))
        outPath += QStringLiteral(".pdf");

    QString errMsg;
    const QString result = SectionLayoutService::exportSectionPdf(m_project, outPath, &errMsg);
    if (result.isEmpty()) {
        QMessageBox::warning(this,
            QStringLiteral("PDF \uc800\uc7a5 \uc624\ub958"),
            errMsg.isEmpty()
                ? QStringLiteral("\uc54c \uc218 \uc5c6\ub294 \uc624\ub958\uac00 \ubc1c\uc0dd\ud588\uc2b5\ub2c8\ub2e4.")
                : errMsg);
        setStatus(QStringLiteral("PDF \uc800\uc7a5 \uc2e4\ud328"));
    } else {
        QMessageBox::information(this,
            QStringLiteral("PDF \uc800\uc7a5 \uc644\ub8cc"),
            QStringLiteral("PDF\ub97c \uc800\uc7a5\ud588\uc2b5\ub2c8\ub2e4:\n%1").arg(result));
        setStatus(QStringLiteral("PDF \uc800\uc7a5 \uc644\ub8cc: %1")
                      .arg(QFileInfo(result).fileName()));
    }
}

// ============================================================
// Property panel slots
// ============================================================

void KaSectionDrawingStudio::onPaperChanged(int /*idx*/)
{
    rebuildSheet(false);
}

void KaSectionDrawingStudio::onCrsChanged(int /*idx*/)
{
    applySelectedCrsToSectionRasters();
    rebuildSheet(false);
}

void KaSectionDrawingStudio::onDistanceAutoToggled(bool checked)
{
    if (m_distManualSpin) {
        m_distManualSpin->setEnabled(!checked);
        m_distManualSpin->setVisible(!checked);
    }
}

void KaSectionDrawingStudio::onReferenceColorClicked()
{
    if (!m_refColorBtn) return;
    QColor cur(m_refColorBtn->property("lineColor").toString());
    if (!cur.isValid())
        cur = QColor(QStringLiteral("#D7191C"));
    const QColor picked = QColorDialog::getColor(
        cur, this, QStringLiteral("기준선 색상"));
    if (!picked.isValid()) return;
    const QString hex = picked.name(QColor::HexRgb).toUpper();
    m_refColorBtn->setProperty("lineColor", hex);
    m_refColorBtn->setText(hex);
    m_refColorBtn->setStyleSheet(QStringLiteral(
        "background:%1; color:white; padding:2px 6px; border-radius:3px;").arg(hex));
    rebuildSheet(false);
}

void KaSectionDrawingStudio::onLayersAdded(const QList<QgsMapLayer*>& layers)
{
    bool sectionAdded = false;
    for (QgsMapLayer* layer : layers) {
        if (isSectionStudioLayer(layer)) {
            sectionAdded = true;
            break;
        }
    }
    refreshLayers();
    if (sectionAdded) {
        applySelectedCrsToSectionRasters();
        rebuildSheet(false);
    }
}

void KaSectionDrawingStudio::onLayersRemoved(const QStringList& /*ids*/)
{
    refreshLayers();
}

void KaSectionDrawingStudio::onTreeItemChanged(QTreeWidgetItem* /*item*/, int col)
{
    if (m_suppressTreeSignal || col != 0) return;
    rebuildSheet(false);
}

// ============================================================
// Helpers
// ============================================================

QList<QgsMapLayer*> KaSectionDrawingStudio::checkedLayersInOrder() const
{
    QList<QgsMapLayer*> result;
    if (!m_project || !m_layerTree) return result;
    for (int i = 0; i < m_layerTree->topLevelItemCount(); ++i) {
        auto* item = m_layerTree->topLevelItem(i);
        if (item->checkState(0) != Qt::Checked) continue;
        const QString id = item->data(0, Qt::UserRole).toString();
        if (auto* layer = m_project->mapLayer(id))
            result.append(layer);
    }
    return result;
}

QString KaSectionDrawingStudio::selectedCrsAuthId() const
{
    if (!m_crsCombo) return QStringLiteral("EPSG:5187");
    const QString auth = m_crsCombo->currentData().toString();
    return auth.isEmpty() ? QStringLiteral("EPSG:5187") : auth;
}

void KaSectionDrawingStudio::syncCrsComboFromProject()
{
    if (!m_crsCombo || !m_project) return;
    const QString auth = m_project->crs().authid();
    const int idx = m_crsCombo->findData(auth);
    if (idx >= 0)
        m_crsCombo->setCurrentIndex(idx);
}

void KaSectionDrawingStudio::applySelectedCrsToSectionRasters()
{
    // 원본 GeoTIFF에 setCrs 하지 않는다. 조판 VRT·표제만 선택 CRS를 쓴다.
}

bool KaSectionDrawingStudio::isSectionStudioLayer(const QgsMapLayer* layer)
{
    if (!layer) return false;
    const auto* rl = qobject_cast<const QgsRasterLayer*>(layer);
    if (!rl || !rl->isValid()) return false;
    if (rl->customProperty(QStringLiteral("ka_hgis/section_display")).toBool())
        return false;
    if (rl->customProperty(QStringLiteral("ka_hgis/section_raster")).toBool())
        return true;
    if (LayerOps::isBasemapLayer(rl))
        return false;
    const QString n = rl->name();
    if (n.contains(QStringLiteral("위성")) || n.startsWith(QStringLiteral("지적"))
        || n.contains(QStringLiteral("VWorld")) || n.contains(QStringLiteral("OSM"))
        || n.contains(QStringLiteral("Carto")) || n.contains(QStringLiteral("Google")))
        return false;
    return rl->providerType() == QLatin1String("gdal");
}

void KaSectionDrawingStudio::setStatus(const QString& msg)
{
    if (m_statusLabel)
        m_statusLabel->setText(msg);
}
