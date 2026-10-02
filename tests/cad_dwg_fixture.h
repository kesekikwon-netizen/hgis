// test_cad_dwg.cpp 의 도우미: 파일 쓰기·읽기, 망가진 DWG, GDAL 로 DXF 안 WALL 도형 읽기, 가짜 dwg2dxf.
#pragma once

#include <QByteArray>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStringList>
#include <QTemporaryDir>

#include <random>

#include <gdal.h>
#include <ogr_api.h>

namespace CadDwgFixture {

inline QByteArray sha256Of(const QString& path) {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) return {};
  return QCryptographicHash::hash(file.readAll(), QCryptographicHash::Sha256).toHex();
}

// path 에 bytes 를 쓴다. 폴더가 없으면 만든다.
inline bool writeFile(const QString& path, const QByteArray& bytes) {
  if (!QDir().mkpath(QFileInfo(path).absolutePath())) return false;
  QFile file(path);
  return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size();
}

inline QByteArray readFile(const QString& path) {
  QFile file(path);
  return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

// 4096 바이트 의사난수 "broken.dwg". 씨앗이 고정이라 늘 같은 바이트다.
inline QString writeBrokenDwg(const QTemporaryDir& dir) {
  std::mt19937 random(12345);
  QByteArray junk(4096, 0);
  for (char& byte : junk)
    byte = static_cast<char>(random() & 0xFF);
  const QString path = dir.filePath(QStringLiteral("broken.dwg"));
  return writeFile(path, junk) ? path : QString();
}

// GDAL DXF 드라이버로 열어 Layer 가 WALL 인 도형을 종류별 한 줄씩 돌려준다: 선은 "선", 점은 "글자:<내용>".
// DXF 를 열지 못하면 "GDAL 이 DXF 를 열지 못함" 한 줄만 돌려준다.
inline QStringList wallShapes(const QString& dxfPath) {
  const char* drivers[] = {"DXF", nullptr};
  GDALDatasetH ds =
      GDALOpenEx(dxfPath.toUtf8().constData(), GDAL_OF_VECTOR | GDAL_OF_READONLY, drivers, nullptr, nullptr);
  if (!ds) return {QStringLiteral("GDAL 이 DXF 를 열지 못함")};
  QStringList shapes;
  if (OGRLayerH layer = GDALDatasetGetLayerByName(ds, "entities")) {
    OGR_L_ResetReading(layer);
    while (OGRFeatureH feature = OGR_L_GetNextFeature(layer)) {
      if (QByteArray(OGR_F_GetFieldAsString(feature, OGR_F_GetFieldIndex(feature, "Layer"))) == "WALL") {
        OGRGeometryH geometry = OGR_F_GetGeometryRef(feature);
        const OGRwkbGeometryType type = geometry ? wkbFlatten(OGR_G_GetGeometryType(geometry)) : wkbUnknown;
        if (type == wkbLineString)
          shapes << QStringLiteral("선");
        else if (type == wkbPoint)
          shapes << QStringLiteral("글자:") +
                        QString::fromUtf8(OGR_F_GetFieldAsString(feature, OGR_F_GetFieldIndex(feature, "Text")));
        else
          shapes << QStringLiteral("기타");
      }
      OGR_F_Destroy(feature);
    }
  }
  GDALClose(ds);
  return shapes;
}

// 실패한 변환 뒤에는 작업 폴더에 DXF 도 source.dwg 도 없어야 한다.
inline bool leavesNothingBehind(const QString& outDir) {
  return QDir(outDir).entryList({QStringLiteral("*.dxf")}, QDir::Files).isEmpty() &&
         !QFile::exists(QDir(outDir).filePath(QStringLiteral("source.dwg")));
}

// 시험 프로그램은 가짜 dwg2dxf 노릇도 한다. convert() 가 도구 인자(--as r2000 …)로 이 프로그램을 시작하고 환경 변수
// KA_FAKE_DWG2DXF 가 있으면, 아무것도 쓰지 않고 그 값을 종료 코드로 돌려주며 끝난다. 진짜 도구는 하지 않는 행동
// (아무것도 안 쓰고 0 으로 끝나기, stderr 없이 3 으로 끝나기)을 시험하려는 것이다.
inline bool actsAsFakeTool(int argc, char** argv) {
  return argc >= 2 && qstrcmp(argv[1], "--as") == 0 && qEnvironmentVariableIsSet("KA_FAKE_DWG2DXF");
}

// 이 범위 안에서 시작하는 프로세스만 가짜 도구로 행동한다.
class FakeToolScope {
 public:
  explicit FakeToolScope(int exitCode) { qputenv("KA_FAKE_DWG2DXF", QByteArray::number(exitCode)); }
  ~FakeToolScope() { qunsetenv("KA_FAKE_DWG2DXF"); }
  FakeToolScope(const FakeToolScope&) = delete;
  FakeToolScope& operator=(const FakeToolScope&) = delete;
};

inline QString fakeToolPath() {
  return QCoreApplication::applicationFilePath();
}

}  // namespace CadDwgFixture
