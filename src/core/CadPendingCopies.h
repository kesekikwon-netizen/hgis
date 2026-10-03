#pragma once

#include <QString>

class QgsProject;

// 조사 폴더(가져온자료/도면)에 만든 도면 변환본 가운데 한 번도 작업공간에 저장된 적 없는 것의 목록.
// 저장 없이 닫은 뒤 다시 불러오면 변환본이 test1 (2)~(7) 처럼 쌓이던 일(2026-10-03, 사용자 결정 「저장 안 된 것만
// 지우기」). 작업공간을 쓰는 길(저장·다른 이름으로 저장: SurveyStorage::writeEmbedded,
// kaWriteQgisProjectAtomic)이 그 작업공간이 쓰는 변환본을 목록에서 뺀다. 목록에 없는 파일, 적을 때와 만든 시각이
// 다른 파일(지운 변환본 자리에 나중에 들어온 것)은 지우지 않는다.
namespace CadPendingCopies {

void add(const QString& copyPath);
bool isPending(const QString& copyPath);
// project 가 쓰는 변환본을 목록에서 뺀다. 작업공간을 쓰기 전에 부른다(쓰기에 실패해도 빼 둔다: 덜 지우는 쪽).
// 목록에 적지 못하면 목록을 버린다.
void markSaved(const QgsProject* project);
// 조사를 연 직후 부른다. project(저장된 작업공간에서 읽은 것)가 쓰는 변환본은 목록에서 빼고, 이 조사의
// 가져온자료/도면 에서 목록에 남은 변환본을 지금 지울 수 있으면 지운다. 열려 있어 못 지운 것은 목록에 남아 다음에
// 다시 본다. 지운 수.
int removeUnsaved(const QgsProject* project, const QString& surveyPath);

}  // namespace CadPendingCopies
