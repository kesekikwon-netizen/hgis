#pragma once
// SectionLayoutService: 단면도 조판 눈금 계산 및 QGIS 레이아웃 생성

#include <QByteArray>
#include <QHash>
#include <QList>
#include <QPointF>
#include <QSizeF>
#include <QString>
#include <QVector>

#include <qgsrectangle.h>

class QgsMapLayer;
class QgsProject;

/// axisTicks() 반환 구조체. error가 비어 있으면 성공, 아니면 ticks는 항상 비어 있다.
struct AxisTickResult {
    QVector<double> ticks; ///< 성공 시 정렬된 눈금값
    QString         error; ///< 실패 이유 (한국어). 성공 시 isEmpty().
};

/// buildSectionLayout() 입력 옵션
struct SectionLayoutOptions {
    /// 가로 용지. 긴 트렌치 단면은 A2/A1에서 지정 축척 그대로 들어간다.
    enum class Paper { A3, A4, A2, A1 };

    Paper   paper                   = Paper::A3;
    QString titleKo;                             ///< 도면명. 비어 있으면 "단면도" 사용.
    double  elevationOffsetM        = 0.0;       ///< 표고 보정값 (rasterY + offset = 표시값)
    double  elevationIntervalM      = 0.10;      ///< 표고 눈금 간격 (기본 0.10m)
    double  manualDistanceIntervalM = 0.0;       ///< 거리 눈금 간격 (0이면 auto 1-2-5)
    bool    showReferenceLine       = true;      ///< 붉은 점선 기준선 표시 여부
    double  referenceLineWidthMm    = 0.20;      ///< 기준선 굵기 (기본 0.20mm)
    QString referenceLineColor      = QStringLiteral("#D7191C"); ///< 기준선 색상
    /// 지도 축척 분모. 0 = 자동 맞춤(extent가 용지에 꽉 차도록).
    /// >0이면 사용자 지정값을 사용하되, 용지에 들어가지 않으면 자동 최솟값으로 올린다.
    double  scaleDenominator        = 0.0;
    /// 표제에 적는 수평 좌표계(5186/5187). 비어 있으면 EPSG:5187.
    /// 세계 XY로 눕힌 단면은 조판에 이 CRS를 쓰지 않는다(재투영하면 수평이 기운다).
    QString mapCrsAuthId;
    /// Double Box / Single Box / Line Ticks Up. 샘플은 용지가 아니라 스튜디오 스트립.
    QString scaleBarStyle = QStringLiteral("Double Box");
    /// 표고·거리 눈금 글자 크기(pt). 기본 5pt는 예전 도면과 같다.
    double  tickLabelPt = 5.0;
    /// 표고 눈금 앞에 "EL." 을 붙인다(예: EL. 100.20).
    bool    elevationPrefix = false;
    /// 선택 주기 한 줄(예: A–A′ 단면 · 북벽). 비어 있으면 용지에 넣지 않는다.
    QString noteText;
};

/// buildSectionLayout() 반환 결과
struct SectionLayoutResult {
    QString      layoutName;                      ///< 생성된 조판 이름. 오류 시 비어 있음.
    double       appliedScaleDenominator = 0.0;   ///< 적용된 축척 분모
    QgsRectangle appliedExtent;                   ///< 지도 항목에 설정된 범위
    QString      errorKo;                         ///< 오류 메시지 (한국어). 성공 시 isEmpty().
    /// 적용 축척이 표준 축척이 아닐 때, 용지에 들어가는 가장 작은 표준 축척. 아니면 0.
    double       suggestedScaleDenominator = 0.0;
    /// 지정 축척이 용지에 들어가지 않아 올렸을 때의 안내. 아니면 비어 있음.
    QString      warningKo;
};

/// 단면도 눈금 계산 및 QGIS 조판 생성 서비스
class SectionLayoutService {
public:
    // ── Task 1/2: 순수 눈금 계산 (QGIS 의존 없음) ──────────────────────────

    /// [minVal, maxVal] 범위에서 interval 배수인 눈금값을 반환한다.
    ///
    /// 부동소수점 누적 대신 정수 인덱스 곱셈(i * interval)을 사용하므로
    /// 0.10m 간격 표고 눈금의 오차가 누적되지 않는다.
    ///
    /// 실패 조건 (error 설정, ticks 비어 있음 반환):
    ///   - interval <= 0 또는 비유한값
    ///   - minVal 또는 maxVal 비유한값
    ///   - minVal >= maxVal
    ///   - 눈금 수 > 500
    static AxisTickResult axisTicks(double minVal, double maxVal, double interval);

    /// 주어진 span에 대해 1-2-5 계열 중 targetTickCount 이하 눈금이 나오는
    /// 가장 작은 간격을 반환한다. 잘못된 인수는 1.0을 반환한다.
    static double niceDistanceInterval(double span, int targetTickCount = 7);

    // ── Task 3: QGIS 조판 생성 ──────────────────────────────────────────────

    /// 단면도 조판("section_sheet")을 프로젝트에 (재)생성한다.
    ///
    /// layers: 체크된 단면 GeoTIFF 레이어 목록 (체크 순서 보존).
    ///         비어 있으면 10m×2m 빈 용지와 표고·거리 눈금만 만든다.
    ///         회전된 지도 GT는 버리고 픽셀 가로=거리·세로=표고로 펼친다.
    ///         mapCrsAuthId는 표제·표시 CRS(재투영 없음).
    static SectionLayoutResult buildSectionLayout(
        QgsProject*                project,
        const QList<QgsMapLayer*>& layers,
        const SectionLayoutOptions& options = SectionLayoutOptions{});

    /// "section_sheet" 조판을 벡터 PDF로 내보낸다.
    /// 도면 PDF와 같은 KaPdfExport::sheetSettings()(300 DPI, 벡터, 글자는 가능한 한 글자).
    /// 화면 DPI는 내보내기 후 복원한다.
    /// 성공 시 pdfPath, 실패 시 빈 문자열 (errorOut에 한국어 오류 설정).
    static QString exportSectionPdf(
        QgsProject*    project,
        const QString& pdfPath,
        QString*       errorOut = nullptr);

    // ── 조판 유지: 재생성 건너뛰기와 제자리 반영 (SectionSheetDecor.cpp) ──

    /// 가로 용지 크기(mm).
    static QSizeF paperSizeMm(SectionLayoutOptions::Paper paper);
    /// 용지 모양을 바꾸는 입력(레이어·파일 버전·용지·축척·표고·거리·좌표계)의 지문.
    /// 같으면 조판을 다시 만들 필요가 없다. 도면명·기준선·눈금 글자·주기는 넣지 않는다.
    static QByteArray inputSignature(const QList<QgsMapLayer*>& layers,
                                     const SectionLayoutOptions& options);
    /// 도면명·기준선·축척자 모양·눈금 글자·주기를 조판을 지우지 않고 반영한다.
    /// 사용자가 옮긴 항목은 그 자리에 둔다. section_sheet가 없으면 false.
    static bool applyDecorationOptions(QgsProject* project, const SectionLayoutOptions& options);
    /// 사용자가 만들어진 자리에서 옮긴 표제·축척·좌표계·주기 항목의 현재 위치(mm).
    static QHash<QString, QPointF> userMovedItems(QgsProject* project);
    /// 다시 만든 조판에 userMovedItems() 위치를 되돌려 놓는다.
    static void restoreUserMovedItems(QgsProject* project, const QHash<QString, QPointF>& moved);
    /// 표시 래스터는 조판이 가지며 조사 파일에 저장되지 않는다. 다시 연 조사에서
    /// 조판에 남긴 평면값과 원본 GeoTIFF로 다시 만든다(사진 전체 검사 없음).
    /// 이미 모두 있거나 원본이 없으면 아무것도 하지 않고 false.
    static bool restoreDisplayLayers(QgsProject* project);
};
