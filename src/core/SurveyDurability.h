#pragma once

#include <QString>
#include <QtGlobal>

class QFile;

// 조사 파일 세대 교체가 전원이 끊겨도 "이전 세대 아니면 검증된 새 세대"로만 남게 하는 도구.
//
// QFile::flush() 는 Qt 버퍼를 OS 캐시로 넘길 뿐이다. 현장 노트북·태블릿이 그 순간 꺼지면
// 이름 교체는 디스크에 남고 내용은 남지 않을 수 있다. 여기서는 내용을 FlushFileBuffers 로
// 디스크까지 내리고, 이름 교체는 MOVEFILE_WRITE_THROUGH 로 끝까지 기록한 뒤 돌아온다.
// Windows 밖에서는 가능한 범위(fsync 없는 Qt 호출)로만 동작한다.
namespace SurveyDurability {

// 쓰기용으로 열린 파일의 Qt 버퍼와 OS 캐시를 디스크까지 내린다.
bool flushToDisk(QFile& file);

// 닫힌 파일을 쓰기 공유로 잠깐 열어 OS 캐시를 디스크까지 내린다. 없는 파일은 false.
bool flushPathToDisk(const QString& path);

// from 을 to 로 옮기며 기존 to 를 바꾼다(같은 볼륨에서는 원자적 교체).
// 실패하면 nativeError 에 Win32 오류 번호, errorText 에 설명을 넣는다.
bool replaceFileDurably(const QString& from, const QString& to, quint32* nativeError = nullptr,
                        QString* errorText = nullptr);

// Win32 오류 번호가 "다른 프로그램이 잠깐 잡고 있다"(접근 거부·공유 위반·잠금 위반)인가.
bool isTransientLockError(quint32 nativeError);

// source 를 target 으로 1MB 단위로 복사하고 디스크까지 내린다. target 은 새로 만든다.
// 실패하면 target 을 지운다. 원본·대상 파일 모두 사용자 원본이 아닌 임시 파일에만 쓴다.
bool copyFileDurably(const QString& source, const QString& target, QString* errorOut = nullptr);

}  // namespace SurveyDurability
