#pragma once
#include "CadastralImport.h"
#include <QUrl>

namespace CadastralPortal {
struct Credentials { QString username; QString password; };
struct Resource { QString fileNo; QString name; qint64 sizeKb = 0; QString revision; };
struct District { QString code; QString name; };
struct Request {
  QgsGeometry survey;
  QgsCoordinateReferenceSystem surveyCrs;
  QgsCoordinateReferenceSystem workCrs;
  QgsCoordinateTransformContext context;
  double bufferMeters = 5000.;
  QString directory;
  QString apiKey;
  Credentials credentials;
};
Credentials credentials();
bool saveCredentials(const Credentials& account, QString* error = nullptr);
QList<Resource> parseResources(const QByteArray& html);
QList<Resource> selectResources(const QList<Resource>& resources, const QList<District>& districts, QString* error);
PreparedReferenceMap prepare(const Request& request, const CadastralImport::Cancel& cancel = {},
                             const CadastralImport::Progress& progress = {});
}
