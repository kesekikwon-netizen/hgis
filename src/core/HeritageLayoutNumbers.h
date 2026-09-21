#pragma once

#include <QColor>
#include <QMap>
#include <QString>
#include <QVector>
#include <QObject>
#include <QHash>
#include <QSet>
#include <QPointer>
#include <memory>

class QgsLayoutItemMap;
class QgsLayoutItemLegend;
class QgsVectorLayer;
class QgsLabelingResults;

// Numbered heritage presentation belongs only to this layout's style overrides.
// Source layers, provider data and the main map's labels are never changed.
class HeritageLayoutNumbers : public QObject {
  Q_OBJECT
public:
  explicit HeritageLayoutNumbers(QObject* parent = nullptr) : QObject(parent) {}
  ~HeritageLayoutNumbers() override;
  struct Entry {
    QString layerId;
    QString dataset;
    QString name;
    int number = 0;  // Current displayed number; zero for omitted entries after compaction.
    int legendIndex = 0;
    QColor color;
    QSet<qint64> featureIds;
  };

  bool update(QgsLayoutItemMap* map, bool force = false);
  void raiseAboveGeometries(QgsLayoutItemMap* map);
  static QgsLayoutItemMap* numbersMapOf(QgsLayoutItemMap* base);
  void applyLegend(QgsLayoutItemLegend* legend) const;
  const QVector<Entry>& entries() const { return m_entries; }
  // One point per on-page number. Circles are drawn here, not by the labeling engine.
  QgsVectorLayer* numberLayer() const { return m_numberLayer; }
  const QMap<QString, QString>& overrides() const { return m_overrides; }
  const QString& error() const { return m_error; }
  quint64 revision() const { return m_revision; }
  void followRenderedLabels(QgsLayoutItemMap* map);
  static HeritageLayoutNumbers* forMap(QgsLayoutItemMap* map);
  bool acceptRenderedLabels(QgsLayoutItemMap* map, const QgsLabelingResults* results);
  const QSet<QString>& visibleKeys() const { return m_visibleKeys; }
  // Numbers actually drawn on the map. Same set as the sheet legend.
  QSet<QString> legendKeys() const;
  static QString entryKey(const QString& layerId, int number);
  bool exportPdf(QgsLayoutItemMap* map, QgsLayoutItemLegend* legend,
                 const QString& path, double dpi, QString* error = nullptr, bool forceVectorOutput = false);

signals:
  void visibleEntriesChanged();

private:
  struct NumberPin {
    double x = 0.;
    double y = 0.;
    double originX = 0.;
    double originY = 0.;
    double size = 0.;
    int number = 0;
    qint64 sourceId = 0;
    QString name;
    QString layerId;
    QString fill;
    QString ink;
  };
  void publishNumberPins(QgsLayoutItemMap* map, const QVector<NumberPin>& pins);
  QByteArray renderSignature(QgsLayoutItemMap* map, bool includeStyles = true) const;
  QSet<QString> placedKeys(QgsLayoutItemMap* map, const QgsLabelingResults* results) const;
  bool compactRenderedNumbers(QgsLayoutItemMap* map, const QgsLabelingResults* results, const QSet<QString>& keys);
  bool shouldRestoreCandidates(QgsLayoutItemMap* map) const;
  void restoreCandidates(QgsLayoutItemMap* map);
  void applyBaseStyleOverrides(QgsLayoutItemMap* map);
  void connectNumberPreview(QgsLayoutItemMap* map);
  QVector<Entry> m_candidateEntries;
  QMap<QString, QString> m_candidateOverrides;
  QByteArray m_pinnedSignature;
  double m_pinnedPaperArea = 0.;
  bool m_compacted = false;
  bool m_restorePending = false;
  int m_restoreSkips = 0;
  bool m_legendPending = false;
  QPointer<QgsLayoutItemMap> m_followedMap;
  QVector<QMetaObject::Connection> m_renderConnections;
  QSet<QString> m_visibleKeys;
  QByteArray m_previewSignature;
  quint64 m_placementRevision = 0;
  bool m_tracking = false;
  bool m_previewBusy = false;
  bool m_previewDirty = false;
  bool m_exporting = false;
  QString m_signature;
  QByteArray m_contentSignature;
  QString m_error;
  QVector<Entry> m_entries;
  QMap<QString, QString> m_overrides;
  QHash<QString, quint64> m_layerRevisions;
  struct DrawingSource {
    quint64 revision = 0;
    std::shared_ptr<QgsVectorLayer> layer;
  };
  QHash<QString, DrawingSource> m_drawingSources;
  QgsVectorLayer* m_numberLayer = nullptr;
  quint64 m_revision = 0;
  mutable bool m_applying = false;
};
