#include "KaShellStartup.h"

#include "KaCrashGuard.h"

#include <QDir>

namespace KaShellStartup {

KaUserError::Spec missingQgisSpec(const QString& exeDir) {
  const QString expected = exeDir.isEmpty()
                               ? QStringLiteral("apps\\qgis-dev")
                               : QDir::toNativeSeparators(QDir(exeDir).filePath(QStringLiteral("apps/qgis-dev")));
  KaUserError::Spec spec;
  spec.title = QStringLiteral("앱을 시작할 수 없습니다");
  spec.what = QStringLiteral("지도 엔진(QGIS) 폴더를 찾지 못했습니다.");
  spec.why = QStringLiteral("실행 파일만 다른 곳으로 옮겼거나 폴더 일부만 복사했을 때 생깁니다.\n찾아본 곳: %1")
                 .arg(expected);
  spec.how = QStringLiteral("포터블 폴더 전체를 복사한 뒤 그 안의 ka-hgis.exe로 다시 실행하세요. "
                            "조사 파일은 변경하지 않았습니다.");
  return spec;
}

QStringList requiredProviders() {
  return {QStringLiteral("wms"), QStringLiteral("ogr"), QStringLiteral("gdal")};
}

QStringList missingProviders(const QStringList& available) {
  QStringList missing;
  for (const QString& key : requiredProviders()) {
    if (!available.contains(key)) missing << key;
  }
  return missing;
}

KaUserError::Spec missingProvidersSpec(const QStringList& missing, const QString& prefix) {
  QStringList effects;
  if (missing.contains(QLatin1String("wms")))
    effects << QStringLiteral("위성·지적 같은 배경 지도가 나오지 않습니다.");
  if (missing.contains(QLatin1String("ogr")))
    effects << QStringLiteral("조사 파일(GPKG)과 SHP를 열거나 저장할 수 없습니다.");
  if (missing.contains(QLatin1String("gdal")))
    effects << QStringLiteral("GeoTIFF·DEM 같은 그림 지도를 열 수 없습니다.");
  KaUserError::Spec spec;
  spec.title = QStringLiteral("지도 기능 일부를 쓸 수 없습니다");
  spec.what = effects.join(QLatin1Char('\n'));
  spec.why = QStringLiteral("지도 엔진 폴더에 필요한 모듈(%1)이 없습니다. 폴더 일부만 복사했을 때 생깁니다.\n"
                            "지도 엔진 폴더: %2")
                 .arg(missing.join(QStringLiteral(", ")), QDir::toNativeSeparators(prefix));
  spec.how = QStringLiteral("포터블 폴더 전체를 다시 복사한 뒤 실행하세요. 조사 파일은 변경하지 않았습니다.");
  return spec;
}

void ensureLogFolder() {
  if (KaUserError::logFolder().isEmpty()) KaUserError::setLogFolder(KaCrashGuard::logDir());
}

void showCritical(const KaUserError::Spec& spec) {
  ensureLogFolder();
  KaUserError::critical(nullptr, spec);
}

}  // namespace KaShellStartup
