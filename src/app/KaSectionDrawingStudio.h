#pragma once
// KaSectionDrawingStudio: 단면도 작성 전용 3열 독립 QWidget
// 좌: 단면 GeoTIFF 목록 | 중: QgsLayoutView 용지+눈금 | 우: 단면 속성창

#include <QByteArray>
#include <QPointer>
#include <QSet>
#include <QString>
#include <QWidget>

#include "core/SectionLayoutService.h"

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QFrame;
class QLabel;
class QLineEdit;
class QPushButton;
class QScrollArea;
class QShowEvent;
class QTimer;
class QToolButton;
class QTreeWidget;
class QTreeWidgetItem;

class QgsLayoutView;
class QgsLayoutViewToolSelect;
class QgsLayoutViewToolPan;
class QgsPrintLayout;
class QgsProject;
class QgsMapLayer;

class KaSectionDrawingStudio : public QWidget {
    Q_OBJECT
public:
    explicit KaSectionDrawingStudio(QgsProject* project, QWidget* parent = nullptr);
    ~KaSectionDrawingStudio() override;

    void refreshLayers();
    QString selectedCrsAuthId() const;

signals:
    void geoTiffAddRequested(const QString& path);
    // One-line guidance for the main window status bar.
    void statusMessage(const QString& text);

protected:
    void showEvent(QShowEvent* event) override;

private slots:
    void addGeoTiff();
    void moveLayerUp();
    void moveLayerDown();
    void removeFromList();
    void buildSection();
    void exportPdf();
    void onPaperChanged(int idx);
    void onCrsChanged(int idx);
    void onDistanceAutoToggled(bool checked);
    void onReferenceColorClicked();
    void onLayersAdded(const QList<QgsMapLayer*>& layers);
    void onLayersRemoved(const QStringList& ids);
    void onTreeItemChanged(QTreeWidgetItem* item, int col);

private:
    void       buildUi();
    QWidget*   buildLeftPanel();
    QWidget*   buildCenterPanel();
    QWidget*   buildRightPanel();
    // Rebuilds only when the geometry inputs changed (inputSignature); an
    // automatic rebuild keeps the chrome items the user moved.
    void       rebuildSheet(bool interactive);
    void       scheduleRebuild();
    // Title, reference line, tick labels and note change in place.
    void       applyDecorations();
    void       updateScaleNote(const SectionLayoutResult& result);
    void       applySuggestedScale();
    SectionLayoutOptions collectOptions() const;
    void       attachLayoutToView();
    void       detachLayoutFromView();
    void       fitPaperInView();
    QgsPrintLayout* currentLayout() const;
    QList<QgsMapLayer*> checkedLayersInOrder() const;
    void       applySelectedCrsToSectionRasters();
    void       syncCrsComboFromProject();
    void       setStatus(const QString& msg);
    void       applyScaleBarStyle(const QString& style);
    static bool isSectionStudioLayer(const QgsMapLayer* layer);

    QPointer<QgsProject>     m_project;
    QgsLayoutView*           m_view         = nullptr;
    QgsLayoutViewToolSelect* m_toolSelect   = nullptr;
    QgsLayoutViewToolPan*    m_toolPan      = nullptr;
    QTreeWidget*             m_layerTree    = nullptr;
    QComboBox*               m_paperCombo   = nullptr;
    QLineEdit*               m_titleEdit    = nullptr;
    QComboBox*               m_scaleCombo   = nullptr;
    QComboBox*               m_crsCombo     = nullptr;
    QDoubleSpinBox*          m_elevOffsetSpin   = nullptr;
    QDoubleSpinBox*          m_elevIntervalSpin = nullptr;
    QCheckBox*               m_distAutoCheck    = nullptr;
    QDoubleSpinBox*          m_distManualSpin   = nullptr;
    QCheckBox*               m_refLineCheck     = nullptr;
    QDoubleSpinBox*          m_refWidthSpin     = nullptr;
    QPushButton*             m_refColorBtn      = nullptr;
    QPushButton*             m_buildBtn   = nullptr;
    QPushButton*             m_pdfBtn     = nullptr;
    QLabel*                  m_statusLabel = nullptr;
    QComboBox*               m_tickSizeCombo   = nullptr;
    QCheckBox*               m_elevPrefixCheck = nullptr;
    QLineEdit*               m_noteEdit        = nullptr;
    QLabel*                  m_scaleNote       = nullptr;
    QToolButton*             m_scaleSuggestBtn = nullptr;
    QTimer*                  m_rebuildTimer    = nullptr;
    QByteArray               m_lastSignature;
    double                   m_suggestedScale  = 0.0;
    QSet<QString>            m_hiddenLayerIds;
    bool                     m_suppressTreeSignal = false;
    bool                     m_rebuilding         = false;
    QString                  m_scaleBarStyle = QStringLiteral("Double Box");
};
