#pragma once

#include <QColor>
#include <QString>
#include <QVector>

#include <functional>

#include <qgsgeometry.h>
#include <qgspointxy.h>
#include <qgsrectangle.h>

// DXF 도면 한 장을 GDAL DXF 드라이버로 읽어 모형 공간 도형만 선·면·점·글자로 나눈다.
// 사용자 원본은 읽기만 한다. 좌표는 도면 숫자 그대로다(좌표계는 CadCrsGuess 가 정한다).
enum class CadKind { Line, Fill, Point, Text };

struct CadEntity {
  CadKind kind = CadKind::Line;
  QgsGeometry geometry;   // 2D, 한 조각
  QString cadLayer;
  QColor color;           // 불투명. 밝은 색은 이미 #3C3C3C
  QString text;           // Text 만
  double textHeight = 0;  // 도면 단위(m)
  double textAngle = 0;   // 도, 반시계
  int textAnchor = 1;     // OGR LABEL p: (1~12)
};

struct CadDrawing {
  QVector<CadEntity> entities;  // 모형 공간만
  QgsRectangle robustExtent;    // 도형 중심 X·Y 각각 5~95 %
  int paperSpaceSkipped = 0;
  int gdalWarnings = 0;
  bool readAsUtf8 = false;
};

namespace CadDrawingReader {

// 올바른 UTF-8 이고 0x7F 를 넘는 바이트가 하나 이상이면 참. LibreDWG 출력은 머리글이 ANSI_949 여도 UTF-8 이다.
bool looksUtf8(const QByteArray& bytes);

// OGR 스타일 문자열의 첫 색(PEN·BRUSH·LABEL·SYMBOL). 알파는 무시하고, 휘도 0.9 이상은 #3C3C3C, 색이 없으면 #000000.
QColor colorFromStyle(const QString& ogrStyle);

// 중심점 X·Y 를 각각 정렬해 nearest-rank 로 5~95 % 구간만 남긴 사각형. 멀리 떨어진 몇 개가 범위를 키우지 않는다.
QgsRectangle robustExtent(const QVector<QgsPointXY>& centres);

// 실패하면 false 와 한국어 error, 영어 원문이나 수는 details.
bool read(const QString& dxfPath, CadDrawing* out, QString* error, QString* details,
          const std::function<bool()>& canceled = {});

}  // namespace CadDrawingReader
