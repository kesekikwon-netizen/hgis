// CAD 시험용 작은 DXF 를 바이트로 만든다. 그룹 코드와 값을 CRLF 로 잇는 가장 단순한 꼴이고, GDAL DXF 드라이버가
// 그대로 읽는다. 글자(TEXT)는 LibreDWG 출력처럼 정렬점 11/21 을 0,0 으로 적는다: GDAL 은 그때 LABEL 에 엉뚱한
// dx·dy 를 붙이지만 점은 제자리에 둔다(2026-10-02 실측).
#pragma once

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QPointF>
#include <QVector>

namespace CadFixture {

inline QByteArray pair(int code, const QByteArray& value) {
  return QByteArray::number(code) + "\r\n" + value + "\r\n";
}

inline QByteArray num(double value) {
  return QByteArray::number(value, 'f', 6);
}

// HEADER($ACADVER AC1015, $DWGCODEPAGE) + ENTITIES.
inline QByteArray document(const QByteArray& codepage, const QByteArray& entities) {
  return pair(0, "SECTION") + pair(2, "HEADER") + pair(9, "$ACADVER") + pair(1, "AC1015") +
         pair(9, "$DWGCODEPAGE") + pair(3, codepage) + pair(0, "ENDSEC") + pair(0, "SECTION") +
         pair(2, "ENTITIES") + entities + pair(0, "ENDSEC") + pair(0, "EOF");
}

inline QByteArray head(const char* type, const char* layer, int aci, bool paper = false) {
  return pair(0, type) + (paper ? pair(67, "1") : QByteArray()) + pair(8, layer) + pair(62, QByteArray::number(aci));
}

inline QByteArray line(const char* layer, int aci, double x1, double y1, double x2, double y2, bool paper = false,
                       double z = 0) {
  return head("LINE", layer, aci, paper) + pair(10, num(x1)) + pair(20, num(y1)) + pair(30, num(z)) +
         pair(11, num(x2)) + pair(21, num(y2)) + pair(31, num(z));
}

inline QByteArray closedPolyline(const char* layer, int aci, const QVector<QPointF>& pts) {
  QByteArray out = head("LWPOLYLINE", layer, aci) + pair(90, QByteArray::number(pts.size())) + pair(70, "1");
  for (const QPointF& p : pts) out += pair(10, num(p.x())) + pair(20, num(p.y()));
  return out;
}

inline QByteArray text(const char* layer, int aci, double x, double y, double height, double angle,
                       const QByteArray& value) {
  return head("TEXT", layer, aci) + pair(10, num(x)) + pair(20, num(y)) + pair(30, num(0)) + pair(40, num(height)) +
         pair(1, value) + pair(50, num(angle)) + pair(11, num(0)) + pair(21, num(0)) + pair(31, num(0));
}

inline QByteArray point(const char* layer, int aci, double x, double y) {
  return head("POINT", layer, aci) + pair(10, num(x)) + pair(20, num(y)) + pair(30, num(0));
}

// 3~4 꼭짓점. 셋이면 셋째를 넷째로 한 번 더 쓴다(DXF SOLID 는 늘 네 점).
inline QByteArray solid(const char* layer, int aci, const QVector<QPointF>& corners) {
  QByteArray out = head("SOLID", layer, aci);
  for (int i = 0; i < 4; ++i) {
    const QPointF p = corners.value(qMin(i, int(corners.size()) - 1));
    out += pair(10 + i, num(p.x())) + pair(20 + i, num(p.y())) + pair(30 + i, num(0));
  }
  return out;
}

inline bool write(const QString& path, const QByteArray& bytes) {
  if (!QDir().mkpath(QFileInfo(path).absolutePath())) return false;
  QFile file(path);
  return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size();
}

}  // namespace CadFixture
