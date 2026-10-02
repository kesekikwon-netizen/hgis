#pragma once

#include <QString>
#include <QStringList>

// 도면 좌표계를 정하는 단서 가운데 도면과 사용 기록에서 읽는 것(CadCrsGuess 가 쓴다).
// 사용자는 도면 좌표계를 모른다고 보고, 도면이 스스로 밝힌 좌표계를 먼저 쓴다(2026-10-03 사용자 목표).
// 원본 도면과 .prj 는 읽기만 한다.
namespace CadCrsHints {

// 도면이 밝힌 좌표계(앞이 먼저): 원본 옆 같은 이름의 .prj, 파일 이름의 EPSG 번호(「…_5187.dxf」),
// 도면 글자의 「EPSG:5186」·「GRS80 동부원점」·「베셀 중부원점」·「UTM-K」. 지번 같은 맨 숫자 글자는 쓰지 않는다.
// 「중부원점」처럼 기준(GRS80·베셀)이 없으면 두 좌표계를 다 낸다. CadCrsGuess::candidateAuthIds 안의 것만 낸다.
QStringList stated(const QString& sourcePath, const QStringList& texts);

// 이름들(파일 이름·조사 이름·도면 글자)에 든 시·도, 시·군·구, 읍·면·동 이름이 가리키는 시·도(겹침 없이).
// 두 글자 이하 이름(「중구」·「동면」)은 여러 곳에 있어 쓰지 않는다.
QStringList provinces(const QStringList& names);

// 같은 원본(SHA256)을 전에 제자리에 올린 좌표계. 없거나 알 수 없는 값이면 "".
QString remembered(const QString& sha256);
// 그 원본의 좌표계를 기억하고 최근 목록 앞에 둔다. authId 가 ""이면 그 원본의 기억만 지운다.
void remember(const QString& sha256, const QString& authId);
// 최근에 맞았던 좌표계(최근 것이 앞, 다섯 개까지).
QStringList recent();

}  // namespace CadCrsHints
