#pragma once
#include <QObject>
#include <QString>
#include <QVector>
#include <QNetworkAccessManager>
#include <QPointer>
#include <QTimer>
#include <memory>

class QNetworkReply;

struct LocationHit {
  QString title;
  QString detail;
  double lon = 0;
  double lat = 0;
  double west = 0, south = 0, east = 0, north = 0;
  bool hasBbox = false;
  QString pnu; // Only populated for an exact parcel-address result.
};

class LocationSearch : public QObject {
  Q_OBJECT
public:
  explicit LocationSearch(QObject* parent = nullptr);
  LocationSearch(std::unique_ptr<QNetworkAccessManager> network, int timeoutMs,
                 QObject* parent = nullptr);
  ~LocationSearch() override;

  void search(const QString& query);
  void searchParcel(const QString& query);
  void cancel();
  static QString vworldApiKey();
  static void setVworldApiKey(const QString& key);

signals:
  void finished(const QVector<LocationHit>& hits);
  void failed(const QString& message);

private:
  void startSearch(const QString& query, bool parcel);
  void searchNominatim(const QString& query);
  void searchVworld(const QString& query, bool parcel = false);
  void handleNominatim(const QByteArray& body);
  void handleVworld(const QByteArray& body, const QString& parcelQuery = {});
  void completeRequest();

  std::unique_ptr<QNetworkAccessManager> m_nam;
  QPointer<QNetworkReply> m_reply;
  QTimer m_deadline;
  int m_timeoutMs;
  bool m_pending = false;
};
