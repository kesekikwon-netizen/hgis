#include "KaSessionLog.h"
#include "TilePackService.h"

#include <cmath>
#include <algorithm>
#include <limits>
#include <memory>
#include <vector>

#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QSaveFile>
#include <QTemporaryDir>

#include <qgsfeedback.h>

#include <cpl_conv.h>
#include <cpl_string.h>
#include <gdal.h>
#include <gdal_utils.h>

namespace TilePackService {

double webMercatorHalfWorld() { return 20037508.342789244; }

double resolutionAtZoom(int z) {
  if (z < 0) z = 0;
  // 한 변 2^z 타일 × 256 px가 세계를 덮는다.
  return (webMercatorHalfWorld() * 2.0) / (256.0 * std::pow(2.0, z));
}

qint64 tileCount(double minX, double minY, double maxX, double maxY, int minZoom, int maxZoom) {
  if (minZoom > maxZoom) std::swap(minZoom, maxZoom);
  if (minZoom < 0) minZoom = 0;
  if (maxZoom > 22) maxZoom = 22;
  if (!(maxX > minX) || !(maxY > minY)) return 0;

  const double half = webMercatorHalfWorld();
  qint64 total = 0;
  for (int z = minZoom; z <= maxZoom; ++z) {
    const double span = (half * 2.0) / std::pow(2.0, z);  // 타일 한 변(m)
    // XYZ는 위쪽이 원점이라 y는 위에서부터 센다.
    const qint64 c0 = static_cast<qint64>(std::floor((minX + half) / span));
    const qint64 c1 = static_cast<qint64>(std::floor((maxX + half) / span));
    const qint64 r0 = static_cast<qint64>(std::floor((half - maxY) / span));
    const qint64 r1 = static_cast<qint64>(std::floor((half - minY) / span));
    total += (c1 - c0 + 1) * (r1 - r0 + 1);
  }
  return total;
}

QString serviceXml(const Options& opt) {
  const double half = webMercatorHalfWorld();
  // GDAL TMS 미니드라이버는 ${z} ${x} ${y}를 쓴다. QGIS 표기를 바꿔 준다.
  QString url = opt.urlTemplate;
  url.replace(QStringLiteral("{z}"), QStringLiteral("${z}"));
  url.replace(QStringLiteral("{x}"), QStringLiteral("${x}"));
  url.replace(QStringLiteral("{y}"), QStringLiteral("${y}"));
  url.replace(QLatin1Char('&'), QLatin1String("&amp;"));

  QString xml;
  xml += QStringLiteral("<GDAL_WMS>");
  xml += QStringLiteral("<Service name=\"TMS\"><ServerUrl>%1</ServerUrl></Service>").arg(url);
  xml += QStringLiteral("<DataWindow>");
  xml += QStringLiteral("<UpperLeftX>%1</UpperLeftX>").arg(-half, 0, 'f', 6);
  xml += QStringLiteral("<UpperLeftY>%1</UpperLeftY>").arg(half, 0, 'f', 6);
  xml += QStringLiteral("<LowerRightX>%1</LowerRightX>").arg(half, 0, 'f', 6);
  xml += QStringLiteral("<LowerRightY>%1</LowerRightY>").arg(-half, 0, 'f', 6);
  xml += QStringLiteral("<TileLevel>%1</TileLevel>").arg(opt.maxZoom);
  xml += QStringLiteral("<TileCountX>1</TileCountX><TileCountY>1</TileCountY>");
  // XYZ(구글식)는 위가 원점. TMS 기본은 아래라 반드시 지정해야 남북이 안 뒤집힌다.
  xml += QStringLiteral("<YOrigin>top</YOrigin>");
  xml += QStringLiteral("</DataWindow>");
  xml += QStringLiteral("<Projection>EPSG:3857</Projection>");
  xml += QStringLiteral("<BlockSizeX>256</BlockSizeX><BlockSizeY>256</BlockSizeY>");
  xml += QStringLiteral("<BandsCount>%1</BandsCount>").arg(opt.bandCount);
  // 실제 자료가 없는 칸만 비운다. 인증·서버 오류를 빈 타일로 저장하면
  // 실패한 내려받기를 성공으로 보고하게 된다.
  xml += QStringLiteral("<ZeroBlockHttpCodes>204,404</ZeroBlockHttpCodes>");
  xml += QStringLiteral("<ZeroBlockOnServerException>false</ZeroBlockOnServerException>");
  xml += QStringLiteral("<Timeout>30</Timeout><MaxConnections>4</MaxConnections>");
  if (!opt.referer.isEmpty())
    xml += QStringLiteral("<Referer>%1</Referer>").arg(opt.referer);
  xml += QStringLiteral("<Cache/>");
  xml += QStringLiteral("</GDAL_WMS>");
  return xml;
}

bool build(const Options& opt, double minX, double minY, double maxX, double maxY,
           const QString& outPath, QString* errorOut, QgsFeedback* feedback,
           const std::function<bool()>& cancellationRequested) {
  if (errorOut) errorOut->clear();
  auto fail = [&](const QString& m) {
    if (errorOut) *errorOut = m;
    return false;
  };
  struct Progress {
    QgsFeedback* feedback;
    const std::function<bool()>& cancellationRequested;
    double begin = 0.0;
    double span = 85.0;

    bool cancelled() const {
      if (cancellationRequested && cancellationRequested()) {
        if (feedback) feedback->cancel();
        return true;
      }
      return feedback && feedback->isCanceled();
    }

    static int CPL_STDCALL update(double fraction, const char*, void* data) {
      auto* progress = static_cast<Progress*>(data);
      // No exception may cross the GDAL C callback boundary.
      try {
        if (progress->cancelled()) return FALSE;
        if (progress->feedback)
          progress->feedback->setProgress(progress->begin +
              std::clamp(fraction, 0.0, 1.0) * progress->span);
        return TRUE;
      } catch (...) {
        KaSessionLog::line(QStringLiteral("[except] core/TilePackService.cpp:120"));
        return FALSE;
      }
    }
  } progress{feedback, cancellationRequested};
  const auto cancelled = [&]() {
    if (!progress.cancelled()) return false;
    fail(QStringLiteral("배경지도 내려받기를 취소했습니다. 기존 파일은 유지됩니다."));
    return true;
  };
  if (cancelled()) return false;
  if (opt.urlTemplate.isEmpty()) return fail(QStringLiteral("타일 주소가 비었습니다."));
  if (!std::isfinite(minX) || !std::isfinite(minY) || !std::isfinite(maxX) ||
      !std::isfinite(maxY) || !(maxX > minX) || !(maxY > minY))
    return fail(QStringLiteral("범위가 비었습니다. 조사구역을 먼저 그리세요."));
  if (outPath.isEmpty()) return fail(QStringLiteral("저장 경로가 비었습니다."));
  if (opt.minZoom < 0 || opt.maxZoom > 22 || opt.maxZoom < opt.minZoom)
    return fail(QStringLiteral("내려받을 지도 단계가 올바르지 않습니다. 지도 범위를 다시 선택하세요."));
  const double res = resolutionAtZoom(opt.maxZoom);
  const double width = (maxX - minX) / res;
  const double height = (maxY - minY) / res;
  if (!std::isfinite(width) || !std::isfinite(height) ||
      width > std::numeric_limits<int>::max() - 1.0 ||
      height > std::numeric_limits<int>::max() - 1.0)
    return fail(QStringLiteral("내려받을 지도 범위가 너무 큽니다. 조사지역으로 확대하고 다시 시도하세요."));
  const int outW = qMax(1, static_cast<int>(std::lround(width)));
  const int outH = qMax(1, static_cast<int>(std::lround(height)));

  const QFileInfo target(outPath);
  if (!QDir().mkpath(target.absolutePath()))
    return fail(QStringLiteral("저장 폴더를 만들지 못했습니다. 폴더 권한과 남은 공간을 확인하세요."));
  QTemporaryDir storage(QDir(target.absolutePath()).filePath(QStringLiteral("ka-hgis-tilepack-XXXXXX")));
  if (!storage.isValid())
    return fail(QStringLiteral("지도를 받을 임시 폴더를 만들지 못했습니다. 폴더 권한과 남은 공간을 확인하세요."));
  const QString temporaryPath = QDir(storage.path()).filePath(QStringLiteral("map.mbtiles"));

  struct CloseDataset {
    void operator()(void* dataset) const { if (dataset) GDALClose(dataset); }
  };
  using Dataset = std::unique_ptr<void, CloseDataset>;

  GDALAllRegister();
  if (!GDALGetDriverByName("MBTiles"))
    return fail(QStringLiteral("이 GDAL에는 MBTiles 드라이버가 없습니다."));

  const QByteArray xml = serviceXml(opt).toUtf8();
  Dataset src(GDALOpen(xml.constData(), GA_ReadOnly));
  if (cancelled()) return false;
  if (!src)
    return fail(QStringLiteral("배경지도 서버에 연결하지 못했습니다. 인터넷 연결과 배경지도 설정을 확인하세요. 기존 파일은 유지됩니다."));

  char** argv = nullptr;
  argv = CSLAddString(argv, "-of");
  argv = CSLAddString(argv, "MBTiles");
  argv = CSLAddString(argv, "-projwin");
  argv = CSLAddString(argv, QString::number(minX, 'f', 3).toUtf8().constData());
  argv = CSLAddString(argv, QString::number(maxY, 'f', 3).toUtf8().constData());
  argv = CSLAddString(argv, QString::number(maxX, 'f', 3).toUtf8().constData());
  argv = CSLAddString(argv, QString::number(minY, 'f', 3).toUtf8().constData());
  argv = CSLAddString(argv, "-outsize");
  argv = CSLAddString(argv, QByteArray::number(outW).constData());
  argv = CSLAddString(argv, QByteArray::number(outH).constData());
  argv = CSLAddString(argv, "-co");
  argv = CSLAddString(argv, opt.jpeg ? "TILE_FORMAT=JPEG" : "TILE_FORMAT=PNG");
  argv = CSLAddString(argv, "-co");
  argv = CSLAddString(argv, QStringLiteral("MINZOOM=%1").arg(opt.minZoom).toUtf8().constData());
  argv = CSLAddString(argv, "-co");
  argv = CSLAddString(argv, QStringLiteral("MAXZOOM=%1").arg(opt.maxZoom).toUtf8().constData());

  std::unique_ptr<GDALTranslateOptions, decltype(&GDALTranslateOptionsFree)> tOpt(
      GDALTranslateOptionsNew(argv, nullptr), GDALTranslateOptionsFree);
  CSLDestroy(argv);
  if (!tOpt) {
    return fail(QStringLiteral("내려받기 설정을 만들지 못했습니다."));
  }
  GDALTranslateOptionsSetProgress(tOpt.get(), &Progress::update, &progress);

  int usageError = 0;
  CPLErrorReset();
  Dataset out(GDALTranslate(temporaryPath.toUtf8().constData(), src.get(), tOpt.get(), &usageError));
  const bool transferFailed = CPLGetLastErrorType() >= CE_Failure;
  src.reset();
  if (cancelled()) return false;
  if (!out || usageError || transferFailed) {
    return fail(QStringLiteral("배경지도를 끝까지 받지 못했습니다. 인터넷 연결과 저장 공간을 확인하고 다시 시도하세요. 기존 파일은 유지됩니다."));
  }

  // 낮은 줌(멀리 볼 때)은 오버뷰로 채운다. 없으면 줌아웃에서 빈 화면이 된다.
  const int levels = qMax(0, opt.maxZoom - opt.minZoom);
  if (levels > 0) {
    std::vector<int> factors;
    for (int i = 1; i <= levels; ++i) factors.push_back(1 << i);
    progress.begin = 85.0;
    progress.span = 10.0;
    const CPLErr error = GDALBuildOverviews(out.get(), opt.jpeg ? "AVERAGE" : "NEAREST",
        static_cast<int>(factors.size()), factors.data(), 0, nullptr, &Progress::update, &progress);
    if (cancelled()) return false;
    if (error != CE_None)
      return fail(QStringLiteral("축소 지도를 완성하지 못했습니다. 저장 공간을 확인하고 다시 내려받으세요. 기존 파일은 유지됩니다."));
  }
  if (GDALClose(out.release()) != CE_None)
    return fail(QStringLiteral("배경지도 파일을 완전히 저장하지 못했습니다. 저장 공간을 확인하세요. 기존 파일은 유지됩니다."));
  if (cancelled()) return false;

  // Close SQLite first, then reopen the independent file before publishing it.
  {
    Dataset check(GDALOpenEx(temporaryPath.toUtf8().constData(), GDAL_OF_RASTER | GDAL_OF_READONLY,
                            nullptr, nullptr, nullptr));
    if (!check || GDALGetRasterCount(check.get()) < 1 || GDALGetRasterXSize(check.get()) <= 0 ||
        GDALGetRasterYSize(check.get()) <= 0 || QFileInfo(temporaryPath).size() <= 0)
      return fail(QStringLiteral("받은 배경지도 파일을 다시 읽지 못했습니다. 기존 파일은 유지됩니다. 다시 내려받으세요."));
  }

  // Same QSaveFile pattern as project saving: never fall back to overwriting
  // the original in place. Cancellation before commit discards only this copy.
  QFile input(temporaryPath);
  QSaveFile output(target.absoluteFilePath());
  output.setDirectWriteFallback(false);
  if (!input.open(QIODevice::ReadOnly) || !output.open(QIODevice::WriteOnly))
    return fail(QStringLiteral("배경지도 저장 파일을 열지 못했습니다. 폴더 권한과 남은 공간을 확인하세요. 기존 파일은 유지됩니다."));
  const qint64 total = input.size();
  while (!input.atEnd()) {
    if (cancelled()) return false;
    const QByteArray data = input.read(1024 * 1024);
    if (input.error() != QFileDevice::NoError || output.write(data) != data.size())
      return fail(QStringLiteral("받은 지도를 저장하지 못했습니다. 저장 공간을 확인하세요. 기존 파일은 유지됩니다."));
    if (feedback && total > 0)
      feedback->setProgress(95.0 + 4.0 * static_cast<double>(input.pos()) / total);
  }
  if (cancelled()) return false;
  if (!output.commit())
    return fail(QStringLiteral("새 배경지도 파일로 교체하지 못했습니다. 파일을 사용하는 프로그램을 닫고 다시 시도하세요. 기존 파일은 유지됩니다."));
  if (feedback) feedback->setProgress(100.0);
  return true;
}

}  // namespace TilePackService
