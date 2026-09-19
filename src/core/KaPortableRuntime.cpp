#include "KaSessionLog.h"
#include "KaPortableRuntime.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QDir>
#include <QProcess>
#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>
#include <QStringList>

#include <cpl_conv.h>
#include <ogr_srs_api.h>

#include <qgscoordinatereferencesystem.h>
#include <qgscoordinatetransform.h>
#include <qgscoordinatetransformcontext.h>
#include <qgspointxy.h>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <stdlib.h>
#endif

#include <cmath>
#include <string>

namespace {
QString g_exeDirOverride;
}

#ifdef Q_OS_WIN
static bool setEnvUtf8AndWide(const char* nameUtf8, const wchar_t* nameW, const QString& value) {
  const QString native = QDir::toNativeSeparators(value);
  qputenv(nameUtf8, native.toUtf8());
  const std::wstring w = native.toStdWString();
  if (_wputenv_s(nameW, w.c_str()) != 0)
    return false;
  return SetEnvironmentVariableW(nameW, w.c_str()) != 0;
}
#else
static bool setEnvUtf8AndWide(const char* nameUtf8, const wchar_t*, const QString& value) {
  return qputenv(nameUtf8, QDir::toNativeSeparators(value).toUtf8());
}
#endif

// ── 한글·비ASCII 경로에서도 QGIS 가 자기 파일을 찾게 한다 ──────────────────
// QGIS 는 접두 경로·플러그인 폴더를 좁은(로컬 코드페이지) 문자열로 다룬다. 그래서
// 폴더를 한글 경로(예: OneDrive\바탕 화면)에 두면 다음이 통째로 실패한다.
//   No dynamic QGIS data provider plugins found in: <경로>/apps/qgis-dev/plugins
//   srs.db does not exist!
// 프로바이더가 하나도 없으니 위성·지적(wms/xyz) 레이어를 아예 만들지 못한다.
// USB(영문 경로)에서는 되고 바탕화면으로 옮기면 안 되던 것이 이것이다.
//
// QGIS 내부는 고칠 수 없으므로, 폴더를 가리키는 순수 ASCII 정션을 만들어 QGIS
// 에게는 그 경로만 보여 준다. 정션은 관리자 권한이 필요 없다. 자료를 복사하지
// 않으므로 용량도 늘지 않는다.
static bool isPureAscii(const QString& s) {
  for (const QChar c : s) {
    if (c.unicode() > 0x7F) return false;
  }
  return true;
}

#ifdef Q_OS_WIN
static QString asciiAliasForBundle(const QString& dir) {
  if (dir.isEmpty() || isPureAscii(dir))
    return dir;
  // 사용자 이름조차 한글일 수 있으므로 반드시 ASCII 인 공용 폴더에 만든다.
  const QString base = QStringLiteral("C:/Users/Public/ka-hgis");
  if (!QDir().mkpath(base))
    return dir;
  const QByteArray h =
      QCryptographicHash::hash(QDir::toNativeSeparators(dir).toUtf8(), QCryptographicHash::Sha1)
          .toHex()
          .left(12);
  const QString link = QDir(base).filePath(QStringLiteral("rt-") + QString::fromLatin1(h));
  // 이미 올바른 정션이 있으면 그대로 쓴다.
  if (QFile::exists(QDir(link).filePath(QStringLiteral("apps/qgis-dev"))))
    return link;
  if (QDir(link).exists())
    QProcess::execute(QStringLiteral("cmd"), {QStringLiteral("/c"), QStringLiteral("rmdir"), link});
  QProcess::execute(QStringLiteral("cmd"),
                    {QStringLiteral("/c"), QStringLiteral("mklink"), QStringLiteral("/J"),
                     QDir::toNativeSeparators(link), QDir::toNativeSeparators(dir)});
  if (QFile::exists(QDir(link).filePath(QStringLiteral("apps/qgis-dev"))))
    return link;
  return dir;  // 못 만들면 원래 경로로 간다(경고는 부팅 점검이 낸다).
}
#endif

KaPortablePaths KaPortableRuntime::discover(const QString& exeDir) {
  KaPortablePaths out;
  out.exeDir = QDir(exeDir).absolutePath();
#ifdef Q_OS_WIN
  // 한글 경로면 QGIS 에게 보여 줄 ASCII 별칭으로 바꿔 잡는다.
  out.exeDir = QDir(asciiAliasForBundle(out.exeDir)).absolutePath();
#endif
  const QDir app(out.exeDir);
  const QString qgis = app.filePath(QStringLiteral("apps/qgis-dev"));
  if (QDir(qgis).exists())
    out.qgisPrefix = QDir(qgis).absolutePath();

  const QString shareProj = app.filePath(QStringLiteral("share/proj"));
  if (QFile::exists(QDir(shareProj).filePath(QStringLiteral("proj.db"))))
    out.projData = QDir(shareProj).absolutePath();
  else if (QFile::exists(app.filePath(QStringLiteral("proj.db"))))
    out.projData = out.exeDir;

  const QString gdal = app.filePath(QStringLiteral("apps/gdal-dev/share/gdal"));
  if (QDir(gdal).exists())
    out.gdalData = QDir(gdal).absolutePath();

  const QString ca = app.filePath(QStringLiteral("curl-ca-bundle.crt"));
  if (QFile::exists(ca))
    out.caBundle = QFileInfo(ca).absoluteFilePath();

  const QString qtPlug = app.filePath(QStringLiteral("apps/Qt6/plugins"));
  if (QDir(qtPlug).exists())
    out.qtPlugins = QDir(qtPlug).absolutePath();

  if (!out.qgisPrefix.isEmpty()) {
    const QString plug = QDir(out.qgisPrefix).filePath(QStringLiteral("plugins"));
    if (QDir(plug).exists())
      out.qgisPlugins = QDir(plug).absolutePath();
  }
  return out;
}

bool KaPortableRuntime::applyEnvironment(const KaPortablePaths& paths) {
  if (!paths.looksBundled())
    return false;

  bool ok = true;
  ok = setEnvUtf8AndWide("OSGEO4W_ROOT", L"OSGEO4W_ROOT", paths.exeDir) && ok;
  ok = setEnvUtf8AndWide("QGIS_PREFIX_PATH", L"QGIS_PREFIX_PATH", paths.qgisPrefix) && ok;
  if (!paths.projData.isEmpty()) {
    ok = setEnvUtf8AndWide("PROJ_DATA", L"PROJ_DATA", paths.projData) && ok;
    ok = setEnvUtf8AndWide("PROJ_LIB", L"PROJ_LIB", paths.projData) && ok;
  }
  if (!paths.gdalData.isEmpty())
    ok = setEnvUtf8AndWide("GDAL_DATA", L"GDAL_DATA", paths.gdalData) && ok;
  if (!paths.caBundle.isEmpty()) {
    ok = setEnvUtf8AndWide("CURL_CA_BUNDLE", L"CURL_CA_BUNDLE", paths.caBundle) && ok;
    ok = setEnvUtf8AndWide("SSL_CERT_FILE", L"SSL_CERT_FILE", paths.caBundle) && ok;
  }
  if (!paths.qtPlugins.isEmpty()) {
    QCoreApplication::addLibraryPath(paths.qtPlugins);
    ok = setEnvUtf8AndWide("QT_PLUGIN_PATH", L"QT_PLUGIN_PATH", paths.qtPlugins) && ok;
  }
  if (!paths.qgisPlugins.isEmpty())
    ok = setEnvUtf8AndWide("QGIS_PLUGIN_PATH", L"QGIS_PLUGIN_PATH", paths.qgisPlugins) && ok;

  const QDir app(paths.exeDir);
  const QString webProcess = app.filePath(QStringLiteral("QtWebEngineProcess.exe"));
  if (QFileInfo::exists(webProcess)) {
    setEnvUtf8AndWide("QTWEBENGINEPROCESS_PATH", L"QTWEBENGINEPROCESS_PATH", webProcess);
    setEnvUtf8AndWide("QTWEBENGINE_RESOURCES_PATH", L"QTWEBENGINE_RESOURCES_PATH",
                      app.filePath(QStringLiteral("apps/Qt6/resources")));
    setEnvUtf8AndWide("QTWEBENGINE_LOCALES_PATH", L"QTWEBENGINE_LOCALES_PATH",
                      app.filePath(QStringLiteral("apps/Qt6/translations/qtwebengine_locales")));
  }
  const QString qgisPlug = QDir(paths.qgisPrefix).filePath(QStringLiteral("qtplugins"));
  if (QDir(qgisPlug).exists())
    QCoreApplication::addLibraryPath(qgisPlug);

  QStringList prepend;
  const QStringList rels = {
      QString(),
      QStringLiteral("bin"),
      QStringLiteral("apps/qgis-dev/bin"),
      QStringLiteral("apps/Qt6/bin"),
      QStringLiteral("apps/gdal-dev/bin"),
      QStringLiteral("apps/pdal-dev/bin"),
  };
  for (const QString& rel : rels) {
    const QString p = rel.isEmpty() ? paths.exeDir : app.filePath(rel);
    if (QDir(p).exists())
      prepend << QDir::toNativeSeparators(p);
  }
#ifdef Q_OS_WIN
  wchar_t oldPath[32768];
  const DWORD n = GetEnvironmentVariableW(L"PATH", oldPath, 32768);
  const QString old = n > 0 && n < 32768 ? QString::fromWCharArray(oldPath, int(n))
                                         : QString::fromLocal8Bit(qgetenv("PATH"));
#else
  const QString old = QString::fromLocal8Bit(qgetenv("PATH"));
#endif
  ok = setEnvUtf8AndWide("PATH", L"PATH", prepend.join(QLatin1Char(';')) + QLatin1Char(';') + old) &&
       ok;

#ifdef Q_OS_WIN
  // 한글·공백이 든 경로(예: OneDrive\바탕 화면)에 폴더를 두면 provider_wms.dll 이
  // 올라오지 않아 위성·지적 레이어를 아예 만들지 못했다. USB(영문 경로)에서는
  // 되고 바탕화면으로 옮기면 안 되던 것이 이것이다. PATH 는 넣어도 로더가 쓰는
  // 경로 해석에서 깨진다. 로더에 디렉터리를 유니코드 그대로 직접 등록한다.
  // SetDefaultDllDirectories 를 먼저 불러야 AddDllDirectory 가 효력을 갖는다.
  if (SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_DEFAULT_DIRS)) {
    for (const QString& dir : prepend) {
      const std::wstring w = QDir::toNativeSeparators(dir).toStdWString();
      if (!AddDllDirectory(w.c_str()))
        ok = false;
    }
  } else {
    // 이 API 를 못 쓰는 환경에서는 최소한 실행 폴더 하나라도 등록한다.
    const std::wstring w = QDir::toNativeSeparators(paths.exeDir).toStdWString();
    SetDllDirectoryW(w.c_str());
  }
#endif
  return ok;
}

bool KaPortableRuntime::bindProjSearchPaths(const QString& projDataDir) {
  if (projDataDir.trimmed().isEmpty() || !QDir(projDataDir).exists())
    return false;
  const QByteArray utf8 = QDir::toNativeSeparators(QDir(projDataDir).absolutePath()).toUtf8();
  // PROJ 7+ / GDAL: UTF-8 경로. search_paths가 있으면 컴파일 타임 설치 경로
  // (개발 PC의 D:\OSGeo4W\share\proj)는 쓰지 않는다.
  // https://proj.org/en/latest/resource_files.html
  // https://gdal.org/en/stable/api/ogr_srs_api.html
  const char* list[] = {utf8.constData(), nullptr};
  OSRSetPROJSearchPaths(list);
  CPLSetConfigOption("PROJ_DATA", utf8.constData());
  CPLSetConfigOption("PROJ_LIB", utf8.constData());
  QgsCoordinateReferenceSystem::invalidateCache();
  QgsCoordinateTransform::invalidateCache();
  return true;
}

// QtWebEngine 은 Chromium 샌드박스가 뜨지 못하면 모든 페이지 로드를
// loadFinished(false) 로 끝낸다. 창은 뜨는데 아무것도 안 나오므로 원인을 찾기 어렵다.
// 2026-09-11 이 PC에서 A/B 로 확인: --no-sandbox 만 있으면 되고 --disable-gpu 는 무관했다.
// 대신 원격 페이지 렌더링의 프로세스 격리가 약해진다. 이 앱이 여는 곳은
// 국토정보플랫폼과 국가유산 인트라넷 두 곳뿐이라 그 범위에서 받아들인다.
bool KaPortableRuntime::applyWebEngineFlags() {
  const QByteArray existing = qgetenv("QTWEBENGINE_CHROMIUM_FLAGS");
  if (!existing.trimmed().isEmpty()) return false;  // 사용자가 정한 값을 덮지 않는다
  qputenv("QTWEBENGINE_CHROMIUM_FLAGS", QByteArray("--no-sandbox"));
  return true;
}

// 설치된 OSGeo4W 로 도는 개발 빌드에서 PATH 를 채운다.
// QGIS_PREFIX_PATH 또는 OSGEO4W_ROOT 로 뿌리를 찾고, 그 아래 bin 폴더들을 앞에 붙인다.
// 이미 들어 있으면 다시 넣지 않는다.
bool KaPortableRuntime::prependOsgeoPath() {
#ifdef Q_OS_WIN
  QString root = QString::fromUtf8(qgetenv("OSGEO4W_ROOT"));
  if (root.isEmpty() || !QDir(root).exists()) {
    const QString prefix = QString::fromUtf8(qgetenv("QGIS_PREFIX_PATH"));
    if (!prefix.isEmpty())
      root = QDir(prefix).filePath(QStringLiteral("../.."));
  }
  if (root.isEmpty() || !QDir(root).exists()) {
    for (const QString& cand : {QStringLiteral("C:/OSGeo4W"), QStringLiteral("D:/OSGeo4W")}) {
      if (QDir(cand).exists()) { root = cand; break; }
    }
  }
  if (root.isEmpty() || !QDir(root).exists()) return false;
  const QDir app(QDir(root).absolutePath());
  QStringList prepend;
  for (const QString& rel : {QStringLiteral("bin"), QStringLiteral("apps/qgis-dev/bin"),
                             QStringLiteral("apps/Qt6/bin"), QStringLiteral("apps/gdal-dev/bin"),
                             QStringLiteral("apps/pdal-dev/bin")}) {
    const QString p = app.filePath(rel);
    if (QDir(p).exists()) prepend << QDir::toNativeSeparators(p);
  }
  if (prepend.isEmpty()) return false;

  wchar_t oldPath[32768];
  const DWORD n = GetEnvironmentVariableW(L"PATH", oldPath, 32768);
  const QString old = n > 0 && n < 32768 ? QString::fromWCharArray(oldPath, int(n)) : QString();
  QStringList add;
  for (const QString& p : prepend) {
    if (!old.contains(p, Qt::CaseInsensitive)) add << p;
  }
  if (add.isEmpty()) return true;  // 이미 다 들어 있다
  const QString merged = add.join(QLatin1Char(';')) + QLatin1Char(';') + old;
  const std::wstring w = merged.toStdWString();
  SetEnvironmentVariableW(L"PATH", w.c_str());
  qputenv("PATH", merged.toUtf8());
  return true;
#else
  return false;
#endif
}

void KaPortableRuntime::isolateUserState(const KaPortablePaths& paths) {
  if (!paths.looksBundled())
    return;
  const QString cfg = QDir(paths.exeDir).filePath(QStringLiteral("config"));
  QDir().mkpath(cfg);
  // 다른 PC에 남은 이전 설치 AppData/레지스트리를 쓰지 않는다.
  QSettings::setDefaultFormat(QSettings::IniFormat);
  QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, QDir(cfg).absolutePath());
  QSettings::setPath(QSettings::IniFormat, QSettings::SystemScope, QDir(cfg).absolutePath());
  const QString qgisProfile = QDir(cfg).filePath(QStringLiteral("qgis-profile"));
  QDir().mkpath(qgisProfile);
  setEnvUtf8AndWide("QGIS_CUSTOM_CONFIG_PATH", L"QGIS_CUSTOM_CONFIG_PATH", qgisProfile);
}

QString KaPortableRuntime::resolvedExeDir() {
  if (!g_exeDirOverride.isEmpty())
    return QDir(g_exeDirOverride).absolutePath();
  if (QCoreApplication::instance())
    return QCoreApplication::applicationDirPath();
  return {};
}

void KaPortableRuntime::setExeDirOverride(const QString& exeDir) {
  g_exeDirOverride = exeDir.trimmed();
}

QString KaPortableRuntime::userConfigDir() {
  const KaPortablePaths paths = discover(resolvedExeDir());
  if (paths.looksBundled()) {
    const QString cfg = QDir(paths.exeDir).filePath(QStringLiteral("config"));
    QDir().mkpath(cfg);
    return QDir(cfg).absolutePath();
  }
  const QString base = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
  if (!base.isEmpty()) {
    QDir().mkpath(base);
    return QDir(base).absolutePath();
  }
  const QString fallback = QDir::temp().filePath(QStringLiteral("ka-hgis-config"));
  QDir().mkpath(fallback);
  return QDir(fallback).absolutePath();
}

bool KaPortableRuntime::koreaWorkAndWebCrsValid() {
  const QgsCoordinateReferenceSystem work(QStringLiteral("EPSG:5186"));
  const QgsCoordinateReferenceSystem east(QStringLiteral("EPSG:5187"));
  const QgsCoordinateReferenceSystem web(QStringLiteral("EPSG:3857"));
  if (!work.isValid() || !east.isValid() || !web.isValid())
    return false;
  try {
    QgsCoordinateTransform xf(web, work, QgsCoordinateTransformContext());
    xf.setBallparkTransformsAreAppropriate(true);
    if (!xf.isValid())
      return false;
    const QgsPointXY out = xf.transform(QgsPointXY(14135000.0, 4510000.0));
    return std::isfinite(out.x()) && std::isfinite(out.y());
  } catch (...) {
    KaSessionLog::line(QStringLiteral("[except] core/KaPortableRuntime.cpp:351"));
    return false;
  }
}
