#pragma once

#include <QList>
#include <QString>

class QgsProject;

// Read-only description of the reference data a survey currently uses: what it
// is, how large an area it covers, when it was received, its size, and whether it
// still works without a network. Collecting never downloads or changes anything.
namespace ReferenceInventory {

enum class Availability {
  Offline,  // a local file this PC can read without a network
  Online,   // drawn from a web service each time (WMS/XYZ/remote raster)
  Missing,  // a local file that no longer exists
  Unsaved,  // an in-memory layer that is not stored anywhere yet
};

struct Item {
  QString layerId;
  QString name;
  QString kind;
  QString extentText;
  QString receivedText;
  QString location;
  // For online layers: whether 「오프라인 저장」 can keep a copy (provider terms, F059).
  QString offlineNote;
  qint64 bytes = -1;
  Availability availability = Availability::Online;
};

QList<Item> collect(const QgsProject* project);
QString availabilityLabel(Availability availability);
QString sizeLabel(qint64 bytes);
// "이 PC에 있음 N · 인터넷 필요 M · …" for the dialog header.
QString summary(const QList<Item>& items);

}  // namespace ReferenceInventory
