#pragma once
#include <QDialog>
#include <QPointer>
#include <QHash>
#include <QSet>
#include <memory>
#include <vector>
#include <qgspointxy.h>
#include "core/TopographicCatalog.h"

class QgsMapCanvas;
class QgsTask;
class QgsVectorLayer;
class QTableWidget;
class QLabel;
class QProgressBar;
class QTimer;
class QPushButton;

// Explicitly opened reference-map inbox. Never runs during application startup.
class KaTopographicImportDialog final : public QDialog {
  Q_OBJECT
public:
  explicit KaTopographicImportDialog(QgsMapCanvas* canvas, QWidget* parent = nullptr);
  ~KaTopographicImportDialog() override;
  void scanFolder(const QString& folder);
  void setMapsEnabled(bool enabled);
  // Folder for the merged sheet GPKG (normally <survey>/지형도/합침). Empty uses a
  // file-backed source folder or the app data folder, never the working directory.
  void setMergeDirectory(const QString& directory);
  void setAutomaticLoadingEnabled(bool enabled);
  bool isAutomaticLoading() const { return m_automaticLoading; }
  // Only the trusted download/sheet resolver calls this. Every record must carry
  // a valid resolved CRS and source sheet; validation is atomic before any add.
  bool importVerified(const QList<TopographicCatalog::Record>& records, QString* error = nullptr);
  // Retain unresolved source items in the inbox only; this never approves a CRS
  // or adds a map layer. A successful explicit manual add releases this hold.
  void retainForReview(const QList<TopographicCatalog::Record>& records, const QString& reason);
signals:
  void referenceLayersChanged();
  void automaticLoadingChanged(bool loading);
  void automaticLoadingAttention(const QString& message);
  void automaticLoadingProgressChanged(int loadedLayers, int pendingLayers);
private:
  void updateAutomaticLoading();
  void populate(const TopographicCatalog::ScanResult& result);
  void previewCurrent();
  void importSelected();
  void mergeConfirmed(const QList<TopographicCatalog::Record>& records);
  void updateCoverage();
  void loadNext();
  void publishPrepared();
  void clearPreview();
  QString recordKey(const TopographicCatalog::Record& record) const;
  QPointer<QgsMapCanvas> m_canvas;
  QPointer<QgsTask> m_task;
  QTableWidget* m_table = nullptr;
  QLabel* m_status = nullptr;
  QProgressBar* m_progress = nullptr;
  QPushButton* m_import = nullptr;
  QgsMapCanvas* m_preview = nullptr;
  QgsVectorLayer* m_previewLayer = nullptr;
  QTimer* m_panTimer = nullptr;
  QTimer* m_loadTimer = nullptr;
  QList<TopographicCatalog::Record> m_records;
  QList<TopographicCatalog::Record> m_confirmed;
  QHash<QString, QString> m_loaded;
  QHash<QString, QString> m_styles;
  QHash<QString, bool> m_visibility;
  QSet<QString> m_userDeleted;
  QSet<QString> m_ignored;
  QgsPointXY m_settledCenter;
  bool m_hasSettledCoverage = false;
  QSet<QString> m_automaticRecords;
  QSet<QString> m_manualRecords;
  QHash<QString, QString> m_reviewReasons;
  QList<TopographicCatalog::Record> m_pending;
  struct PreparedLayer {
    QString key;
    std::unique_ptr<QgsVectorLayer> layer;
    bool visible = false;
    TopographicCatalog::Category category = TopographicCatalog::Category::Unknown;
  };
  std::vector<PreparedLayer> m_prepared;
  QStringList m_loadErrors;
  QString m_mergeDirectory;
  bool m_mapsEnabled = true;
  bool m_updatingCoverage = false;
  bool m_automaticLoadingEnabled = true;
  bool m_automaticLoading = false;
  int m_reportedAutomaticLoaded = -1;
  int m_reportedAutomaticPending = -1;
  quint64 m_generation = 0;
  quint64 m_coverageGeneration = 0;
};
