#pragma once

#include <QColor>
#include <QString>
#include <QVector>
#include <optional>

class QgsVectorLayer;

// 국가유산 GIS통합인트라넷에서 받아 오는 참조 자료(주변유적)의 종류.
// 색·레이어 이름·사이트 탭 코드가 전부 여기서 갈린다. 다른 곳에서 색을 정하지 않는다.
enum class HeritageDataset {
  DesignatedHeritage,       // 지정유산
  AlterationStandard,       // 현상변경허용기준
  BuriedHeritageArea,       // 매장유산유존지역
  HeritageDistributionMap,  // 문화유적분포지도
  SurfaceSurveyArea,        // 지표조사구역
  ExcavationSurveyArea,     // 발굴조사구역
};

struct HeritageStyleResult {
  bool ok = false;
  int categoryCount = 0;
  // 상한을 넘었다. 조용히 단색으로 떨어뜨리지 않고 호출자가 사용자에게 말해야 한다.
  bool overCap = false;
  QString message;
};

// 주변유적 참조 레이어의 색과 기호. 한 종류는 언제나 한 색이다.
class HeritageStyle {
public:
  static QVector<HeritageDataset> allDatasets();

  // 레이어창에 뜨는 이름. 도면 범례의 종류 줄도 이 이름이다.
  static QString layerName(HeritageDataset ds);
  // 사이트 changeTab 코드. 지정유산은 아직 확인하지 못해 빈 문자열이다.
  static QString tabCode(HeritageDataset ds);
  // 종류별 고정 색. 색을 정하는 곳은 여기 하나뿐이다.
  static QColor color(HeritageDataset ds);
  // Upgrade only the old dataset default for layout copies. Keep custom RGB
  // values and the source alpha; this never changes the source layer itself.
  static QColor layoutColor(HeritageDataset ds, const QColor& sourceColor);
  static std::optional<HeritageDataset> fromLayerName(const QString& name);

  // 조사 성과물과 섞이면 안 되는 색. 빨강은 조사구역, 회색은 수치지형도 밑그림이 쓴다.
  static bool isReservedColor(const QColor& c);

  static double outlineWidthMm();       // 조사 도형보다 얇게, 밑그림(0.2)보다는 굵게
  static double nameLabelMinScale();    // 이보다 작은 축척(예: 1:25000)에서는 맵 유적명을 끈다
  static int maxLegendCategories();     // 유적명 줄 수 상한
  static QString unnamedLabel();        // 유적명이 빈 레코드가 들어갈 자리

  // 종류 색 한 줄과 유적명 글자만 건다. 유적마다 분류 심볼을 만들지 않는다.
  // shadeIndex 는 한 ZIP 안의 몇 번째 SHP 인지. 0 이면 종류 대표색을 쓴다.
  static HeritageStyleResult apply(QgsVectorLayer* layer, HeritageDataset ds,
                                   const QString& nameField, int shadeIndex = 0);
};
