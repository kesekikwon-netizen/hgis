#pragma once

#include <QByteArray>
#include <QNetworkAccessManager>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QTimer>
#include <QUrl>
#include <memory>

#include <qgsgeometry.h>
#include <qgspointxy.h>

#include "HeritageFetchPlan.h"

class QNetworkReply;
class QgsCoordinateReferenceSystem;
class QgsProject;

// 조사구역이 어느 시/군에 있는지. 인트라넷 검색은 시/군 하나를 단위로 한다.
struct HeritageRegion {
  QString sido;   // 경상북도
  QString city;   // 안동시
  // 어디서 나온 값인지: "vworld" | "layer" | "manual"
  QString source;
  // 시도 개략 경계 안에 점이 들어가는지 확인했는가. 아니면 사용자가 꼭 확인해야 한다.
  bool insideSidoBounds = false;
  // 조사구역 주변 5km가 걸치는 다른 시/군(판정한 시/군 제외). 확인창이 체크 목록으로 보여 준다.
  // 요청 단위는 그대로 시/군 하나씩이다.
  QList<HeritageCity> nearby;
  // 이웃 시/군을 확인하지 못했거나 일부만 확인했을 때의 설명. 비어 있으면 확인을 마쳤다.
  QString nearbyNote;

  bool ok() const { return !sido.isEmpty() && !city.isEmpty(); }
  QString display() const;  // "경상북도 안동시"
};

// 조사구역 도형 → 시/군.
//
// 기존에 좌표 → 시/군을 해 주는 것이 없다. AdminBoundaryService 는 이름 → 경계 도형이라
// 방향이 반대고, KoreaRegionCatalog 는 정적 목록이다.
// 여기서는 VWorld 좌표→주소를 먼저 쓰고, 실패하면 프로젝트에 올라온 행정경계 레이어로 폴백한다.
class HeritageRegionResolver : public QObject {
  Q_OBJECT
public:
  explicit HeritageRegionResolver(QObject* parent = nullptr);
  HeritageRegionResolver(std::unique_ptr<QNetworkAccessManager> network, int timeoutMs,
                         QObject* parent = nullptr);
  ~HeritageRegionResolver() override;

  // --- 네트워크 없이 검사할 수 있는 부분 ---

  static QUrl buildAddressUrl(const QString& apiKey, double lonDeg, double latDeg);
  static HeritageRegion parseAddress(const QByteArray& body, QString* errorOut = nullptr);
  // "경상북도 안동시 풍천면 ..." → {경상북도, 안동시}
  static HeritageRegion fromAddressText(const QString& text);
  // 조사구역 안쪽의 대표점. 오목한 구역에서도 도형 밖으로 나가지 않는다.
  static QgsPointXY lookupPoint(const QgsGeometry& geometry);
  static QgsPointXY toWgs84(const QgsPointXY& point, const QgsCoordinateReferenceSystem& crs);
  // 프로젝트에 올라온 행정경계 폴리곤으로 점-in-폴리곤. 인터넷 없이 되지만 그 레이어가 항상 있지는 않다.
  static HeritageRegion fromBoundaryLayers(QgsProject* project, const QgsPointXY& wgs84);

  // --- 실제 판정 ---

  void resolve(const QgsGeometry& surveyArea, const QgsCoordinateReferenceSystem& crs,
               QgsProject* fallbackProject = nullptr);
  void cancel();

signals:
  void resolved(const HeritageRegion& region);
  void failed(const QString& message);

private:
  // 판정한 시/군에 주변 5km 이웃 시/군을 붙여 resolved 를 낸다.
  void finishResolved(const HeritageRegion& region, bool allowNetwork);
  void emitWithNearby(HeritageRegion region, const QList<HeritageCity>& found, const QString& note);

  std::unique_ptr<QNetworkAccessManager> m_nam;
  QPointer<QNetworkReply> m_reply;
  QTimer m_deadline;
  int m_timeoutMs;
  bool m_pending = false;
  QPointer<QgsProject> m_fallbackProject;
  QgsPointXY m_lastPoint;
  QgsGeometry m_scope;  // 조사구역 + 5km, EPSG:4326
  HeritageRegion m_primary;
  bool m_nearbyPhase = false;
};
