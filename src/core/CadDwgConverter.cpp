#include "CadDwgConverter.h"

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QProcess>

namespace CadDwgConverter {
namespace {

constexpr int kPollMs = 100;
// 도구에는 작업 폴더 안의 ASCII 이름만 준다. dwg2dxf 는 인자를 ANSI 코드 페이지로 받아 그 밖의 글자(줄표 – ✓ 등)가
// 든 이름은 열지 못한다(실측: File not found). 인자는 쉘을 거치지 않고 목록으로 넘긴다.
constexpr QLatin1String kWorkDwg("source.dwg");
constexpr QLatin1String kWorkDxf("source.dxf");
#ifdef Q_OS_WIN
constexpr unsigned long kCreateNoWindow = 0x08000000;  // CREATE_NO_WINDOW
#endif

Result failure(const QString& error, const QString& details = QString()) {
  Result result;
  result.error = error;
  result.details = details;
  return result;
}

QString lastNonEmptyLine(const QByteArray& text) {
  const QList<QByteArray> lines = text.split('\n');
  for (auto it = lines.crbegin(); it != lines.crend(); ++it) {
    const QString line = QString::fromUtf8(*it).trimmed();
    if (!line.isEmpty()) return line;
  }
  return {};
}

// 프로세스를 끝내고 파일 핸들이 풀릴 때까지 기다린다. 그래야 뒤이은 파일 지우기가 막히지 않는다.
void stop(QProcess& process) {
  process.kill();
  process.waitForFinished(5000);
}

}  // namespace

QString bundledToolIn(const QString& appDir) {
  const QString tool = QDir(appDir).filePath(QStringLiteral("tools/libredwg/dwg2dxf.exe"));
  return QFileInfo(tool).isFile() ? tool : QString();
}

QString bundledTool() {
  return bundledToolIn(QCoreApplication::applicationDirPath());
}

QString converterLabel() {
  return QStringLiteral("LibreDWG 0.14");
}

Result convert(const QString& dwgPath, const QString& outDir, const QString& tool, int timeoutMs,
               const std::function<bool()>& canceled) {
  const QString readError = QStringLiteral("이 DWG를 읽지 못했습니다.");
  if (!QFileInfo(tool).isFile())
    return failure(QStringLiteral("DWG 변환 도구(LibreDWG)를 찾지 못했습니다."), QDir::toNativeSeparators(tool));

  // 원본은 읽기만 한다. 작업 폴더에 ASCII 이름의 사본을 만들어 그것만 변환한다.
  const QString workDwg = QDir(outDir).filePath(kWorkDwg);
  const QString workDxf = QDir(outDir).filePath(kWorkDxf);
  if (!QDir().mkpath(outDir))
    return failure(readError, QStringLiteral("작업 폴더를 만들지 못했습니다: ") + QDir::toNativeSeparators(outDir));
  if (!QFile::copy(dwgPath, workDwg))
    return failure(readError,
                   QStringLiteral("도면을 작업 폴더로 복사하지 못했습니다: ") + QDir::toNativeSeparators(dwgPath));
  // 읽기 전용 원본(CD·공유 폴더)을 복사하면 그 속성도 따라와 사본을 지울 수 없다.
  QFile::setPermissions(workDwg, QFileDevice::ReadOwner | QFileDevice::WriteOwner);
  // 여기부터는 사본을 우리가 만들었다. 실패·취소·시간 초과로 끝나면 사본과 반쯤 쓴 DXF 를 남기지 않는다.
  const auto fail = [&](const QString& error, const QString& details = QString()) {
    QFile::remove(workDwg);
    QFile::remove(workDxf);
    return failure(error, details);
  };
  // 앞서 돌린 변환의 source.dxf 가 남아 있으면 도구가 아무것도 쓰지 않고 끝나도 결과로 보이므로 먼저 지운다.
  if (QFile::exists(workDxf) && !QFile::remove(workDxf))
    return fail(readError, QStringLiteral("작업 폴더의 이전 결과를 지우지 못했습니다: ") + QDir::toNativeSeparators(workDxf));

  QProcess process;
  process.setProgram(tool);
  // 항상 r2000: 기본 출력 판은 트루컬러(코드 420)를 틀리게 써서 색이 검거나 투명해진다.
  process.setArguments({QStringLiteral("--as"), QStringLiteral("r2000"), QStringLiteral("-y"), QStringLiteral("-o"),
                        QString(kWorkDxf), QString(kWorkDwg)});
  process.setWorkingDirectory(outDir);
  process.setStandardInputFile(QProcess::nullDevice());
  process.setStandardOutputFile(QProcess::nullDevice());
#ifdef Q_OS_WIN
  process.setCreateProcessArgumentsModifier(
      [](QProcess::CreateProcessArguments* args) { args->flags |= kCreateNoWindow; });
#endif
  process.start();
  if (!process.waitForStarted(10000)) return fail(readError, process.errorString());

  // 취소와 시간은 기다리기 전에 먼저 본다. 그래야 처음부터 취소·0 ms 도 프로세스가 끝나기 전에 걸린다.
  QElapsedTimer clock;
  clock.start();
  while (process.state() != QProcess::NotRunning) {
    if (canceled && canceled()) {
      stop(process);
      return fail(QStringLiteral("DWG 변환을 취소했습니다."));
    }
    if (clock.elapsed() >= timeoutMs) {
      stop(process);
      return fail(QStringLiteral("DWG 변환이 너무 오래 걸려 멈췄습니다."));
    }
    process.waitForFinished(kPollMs);
  }

  const int exitCode = process.exitCode();
  const QFileInfo dxf(workDxf);
  if (process.exitStatus() != QProcess::NormalExit || exitCode != 0 || !dxf.isFile() || dxf.size() == 0) {
    const QString lastLine = lastNonEmptyLine(process.readAllStandardError());
    return fail(readError, lastLine.isEmpty() ? QStringLiteral("종료 코드 %1").arg(exitCode) : lastLine);
  }
  QFile::remove(workDwg);
  Result result;
  result.ok = true;
  result.dxfPath = workDxf;
  return result;
}

}  // namespace CadDwgConverter
