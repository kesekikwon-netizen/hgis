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

namespace ReferenceMapPreparation {
bool initializeStorage(PreparedReferenceMap& result, const QString& requestedBasePath);
bool cancelled(PreparedReferenceMap& result, QgsFeedback* feedback);
bool download(QNetworkRequest request, QByteArray* body, QString* error,
              QgsFeedback* feedback, const ReferenceDownload& overrideDownload = {},
              int timeoutMs = 15000);
bool writeResponse(const QString& path, const QByteArray& body, QString* error);
bool validateFeatureCollection(const QByteArray& body, QString* error);
bool validateCompleteFeatureCollection(const QByteArray& body, QString* error);
bool saveVector(PreparedReferenceMap& result, QgsVectorLayer* layer,
                const QgsCoordinateTransformContext& context, QgsFeedback* feedback);
}
