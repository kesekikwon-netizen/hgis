#pragma once

#include <QByteArray>
#include <QColor>
#include <QHash>
#include <QNetworkRequest>
#include <QStringList>
#include <QTemporaryDir>
#include <functional>
#include <memory>

class QgsFeedback;
class QgsVectorLayer;
class QgsCoordinateTransformContext;

// Only values and independently owned files cross the worker/GUI boundary.
struct PreparedReferenceMap {
  enum class Status { Ready, Cancelled, Failed };
  Status status = Status::Failed;
  // A completed atomic file replacement cannot be undone by a late Cancel click.
  bool outputCommitted = false;
  // The site rejected the saved login; the GUI offers to re-enter it and retry.
  bool accountRejected = false;
  QString gpkgPath;
  QString tableName;
  QString rasterUri;
  QString error;
  QStringList warnings;
  QHash<QString, QColor> officialColors;
  std::shared_ptr<QTemporaryDir> storage;

  bool isReady() const { return status == Status::Ready; }
  void retainFiles() const { if (storage) storage->setAutoRemove(false); }
};

using ReferenceDownload = std::function<bool(
    const QNetworkRequest&, QByteArray*, QString*, QgsFeedback*)>;

// idleMs: abort when no byte arrives for this long; every received chunk restarts it.
// totalMs: absolute cap, which also bounds a server trickling a few bytes forever.
struct ReferenceTransferLimits {
  int idleMs = 15000;
  int totalMs = 15000;
};

// Why a transfer failed, so a caller can split a slow piece or retry a dropped one.
enum class ReferenceTransferFailure { None, Cancelled, Timeout, Network };

namespace ReferenceMapPreparation {
// Large WFS/WMS payloads on field LTE: a transfer that keeps progressing may take
// up to three minutes, while a silent connection still fails after 15 seconds.
inline constexpr ReferenceTransferLimits kLargeTransfer{15000, 180000};
bool initializeStorage(PreparedReferenceMap& result, const QString& requestedBasePath);
bool cancelled(PreparedReferenceMap& result, QgsFeedback* feedback);
// Compatibility form: one value is both the idle and the absolute limit.
bool download(QNetworkRequest request, QByteArray* body, QString* error,
              QgsFeedback* feedback, const ReferenceDownload& overrideDownload = {},
              int timeoutMs = 15000);
bool download(QNetworkRequest request, QByteArray* body, QString* error,
              QgsFeedback* feedback, const ReferenceDownload& overrideDownload,
              const ReferenceTransferLimits& limits,
              ReferenceTransferFailure* failure = nullptr);
bool writeResponse(const QString& path, const QByteArray& body, QString* error);
bool validateFeatureCollection(const QByteArray& body, QString* error);
bool validateCompleteFeatureCollection(const QByteArray& body, QString* error);
bool saveVector(PreparedReferenceMap& result, QgsVectorLayer* layer,
                const QgsCoordinateTransformContext& context, QgsFeedback* feedback);
}
