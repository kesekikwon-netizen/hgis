#include "PreparedReferenceMap.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QNetworkReply>
#include <QTimer>
#include <algorithm>
#include <qgsblockingnetworkrequest.h>
#include <qgsfeedback.h>
#include <qgsvectorlayer.h>
#include <qgsvectorfilewriter.h>

bool ReferenceMapPreparation::initializeStorage(PreparedReferenceMap& result,
                                               const QString& requestedBasePath) {
  const QFileInfo requested(requestedBasePath);
  result.storage = std::make_shared<QTemporaryDir>(
      requested.absoluteDir().filePath(QStringLiteral("ka-hgis-reference-XXXXXX")));
  if (!result.storage->isValid()) {
    result.error = QStringLiteral("지도를 저장할 임시 폴더를 만들지 못했습니다. 저장 폴더의 쓰기 권한과 남은 공간을 확인하세요.");
    return false;
  }
  result.gpkgPath = QDir(result.storage->path()).filePath(requested.fileName());
  return true;
}

bool ReferenceMapPreparation::cancelled(PreparedReferenceMap& result, QgsFeedback* feedback) {
  if (!feedback || !feedback->isCanceled()) return false;
  result.status = PreparedReferenceMap::Status::Cancelled;
  result.error = QStringLiteral("지도 내려받기를 취소했습니다. 기존 지도는 유지됩니다.");
  return true;
}

bool ReferenceMapPreparation::download(QNetworkRequest request, QByteArray* body,
                                      QString* error, QgsFeedback* feedback,
                                      const ReferenceDownload& overrideDownload, int timeoutMs) {
  return download(std::move(request), body, error, feedback, overrideDownload,
                  ReferenceTransferLimits{timeoutMs, timeoutMs});
}

bool ReferenceMapPreparation::download(QNetworkRequest request, QByteArray* body,
                                      QString* error, QgsFeedback* feedback,
                                      const ReferenceDownload& overrideDownload,
                                      const ReferenceTransferLimits& limits,
                                      ReferenceTransferFailure* failure) {
  const auto fail = [failure](ReferenceTransferFailure kind) {
    if (failure) *failure = kind;
    return false;
  };
  if (failure) *failure = ReferenceTransferFailure::None;
  if (feedback && feedback->isCanceled()) return fail(ReferenceTransferFailure::Cancelled);
  const int idleMs = std::max(1, limits.idleMs);
  const int totalMs = std::max(idleMs, limits.totalMs);
  // Qt restarts this inactivity timer whenever bytes arrive, so a slow but
  // progressing response is extended; a silent connection still stops early.
  request.setTransferTimeout(idleMs);
  if (overrideDownload) {
    if (overrideDownload(request, body, error, feedback)) return true;
    return fail(feedback && feedback->isCanceled() ? ReferenceTransferFailure::Cancelled
                                                   : ReferenceTransferFailure::Network);
  }
  QgsBlockingNetworkRequest operation;
  QgsFeedback requestFeedback;
  if (feedback)
    QObject::connect(feedback, &QgsFeedback::canceled, &requestFeedback, &QgsFeedback::cancel);
  bool deadlineExceeded = false;
  QTimer deadline;
  deadline.setSingleShot(true);
  QObject::connect(&deadline, &QTimer::timeout, &operation, [&] {
    deadlineExceeded = true;
    requestFeedback.cancel();
  });
  // In a worker QgsBlockingNetworkRequest services this event loop. Unlike the
  // inactivity timeout, this deadline also bounds a server sending trickle data.
  deadline.start(totalMs);
  const auto code = operation.get(request, false, &requestFeedback);
  deadline.stop();
  if (feedback && feedback->isCanceled()) return fail(ReferenceTransferFailure::Cancelled);
  if (deadlineExceeded || code != QgsBlockingNetworkRequest::NoError) {
    // Qt reports its expired inactivity timer as an abort that nobody requested.
    const auto replyError = operation.reply().error();
    const bool idleExpired = !requestFeedback.isCanceled() &&
        (replyError == QNetworkReply::TimeoutError || replyError == QNetworkReply::OperationCanceledError);
    const bool timedOut = deadlineExceeded || idleExpired || code == QgsBlockingNetworkRequest::TimeoutError;
    if (error) *error = timedOut
        ? QStringLiteral("지도 서버의 응답이 늦어 요청을 중단했습니다. 인터넷 연결을 확인한 뒤 다시 내려받으세요.")
        : QStringLiteral("지도 서버에 연결하지 못했습니다. 인터넷 연결을 확인한 뒤 다시 내려받으세요.");
    return fail(timedOut ? ReferenceTransferFailure::Timeout : ReferenceTransferFailure::Network);
  }
  if (body) *body = operation.reply().content();
  return true;
}

bool ReferenceMapPreparation::writeResponse(const QString& path, const QByteArray& body,
                                           QString* error) {
  QFile file(path);
  if (!file.open(QIODevice::WriteOnly) || file.write(body) != body.size() || !file.flush()) {
    if (error) *error = QStringLiteral("받은 지도 데이터를 저장하지 못했습니다. 저장 공간과 폴더 권한을 확인하세요.");
    return false;
  }
  return true;
}

bool ReferenceMapPreparation::validateFeatureCollection(const QByteArray& body, QString* error) {
  const QJsonDocument document = QJsonDocument::fromJson(body);
  if (document.isObject() && document.object().value(QStringLiteral("type")) == QLatin1String("FeatureCollection") &&
      document.object().value(QStringLiteral("features")).isArray()) return true;
  if (error) *error = QStringLiteral("지도 서버가 올바른 지도 데이터를 보내지 않았습니다. 잠시 후 다시 내려받으세요.");
  return false;
}

bool ReferenceMapPreparation::validateCompleteFeatureCollection(const QByteArray& body, QString* error) {
  if (!validateFeatureCollection(body, error)) return false;
  const auto object = QJsonDocument::fromJson(body).object();
  const qsizetype received = object.value(QStringLiteral("features")).toArray().size();
  for (const auto& key : {QStringLiteral("numberMatched"), QStringLiteral("totalFeatures")}) {
    const auto value = object.value(key);
    bool valid = false;
    const qlonglong expected = value.isDouble() ? value.toInteger(-1) : value.toString().toLongLong(&valid);
    if ((value.isDouble() || valid) && expected > received) {
      if (error) *error = QStringLiteral("지도 데이터 일부만 받았습니다. 기존 지도는 유지됩니다. 범위를 좁혀 다시 내려받으세요.");
      return false;
    }
  }
  return true;
}

bool ReferenceMapPreparation::saveVector(PreparedReferenceMap& result, QgsVectorLayer* layer,
                                        const QgsCoordinateTransformContext& context,
                                        QgsFeedback* feedback) {
  if (cancelled(result, feedback)) return false;
  QgsVectorFileWriter::SaveVectorOptions options;
  options.driverName = QStringLiteral("GPKG");
  options.layerName = result.tableName;
  options.fileEncoding = QStringLiteral("UTF-8");
  QString details;
  const auto code = QgsVectorFileWriter::writeAsVectorFormatV3(
      layer, result.gpkgPath, context, options, &details);
  if (cancelled(result, feedback)) return false;
  if (code != QgsVectorFileWriter::NoError) {
    result.error = QStringLiteral("새 지도 파일을 저장하지 못했습니다. 저장 공간과 폴더 권한을 확인하세요. 기존 지도는 유지됩니다.");
    return false;
  }
  QgsVectorLayer check(result.gpkgPath + QStringLiteral("|layername=") + result.tableName,
                       QStringLiteral("verify"), QStringLiteral("ogr"));
  if (!check.isValid() || check.featureCount() != layer->featureCount()) {
    result.error = QStringLiteral("저장한 지도를 다시 확인하지 못했습니다. 기존 지도는 유지됩니다. 다른 저장 폴더에서 다시 내려받으세요.");
    return false;
  }
  result.status = PreparedReferenceMap::Status::Ready;
  return true;
}
