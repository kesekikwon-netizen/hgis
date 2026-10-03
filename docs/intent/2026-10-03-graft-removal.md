# 요청 기록: 쓰이지 않는 Graft 코드 검색 도구와 Cursor 시절 설명 지우기

- 날짜: 2026-10-03
- 상태: 완료
- 관련 문서: docs/developer-tools.md, README.md

## 요청 원문
> graft-mcp 는있는게좋은가?
> jev선택 (Claude 추천: 지우기 → Jev 선택: 지우기, 확신도 1.0)

2026-10-03 덧붙임 (Claude 보고 「이번에 손대지 않은 것: docs/developer-tools.md와 README에는 이미 걷어 낸 Cursor 시절 작업 방식 설명이 아직 남아 있습니다」에 대해):
> 이것도지워

## 문제
- Graft(코드 검색 MCP 도구)는 2026-09-15 Cursor로 개발하던 때 Cursor 전용으로 넣었다. 지금은 Cursor 연결 설정이 이 PC에 없고 Claude·Codex 설정에도 연결돼 있지 않아 아무 데서도 쓰지 않는다.
- 같은 일(정의 찾기, 파일 안 목록, 글자 검색)은 Claude 대화에서 clangd와 내장 검색이 더 정확하게 한다. Graft 자기 설명서에도 실제 호출 10개 중 1개만 찾았다고 적혀 있다.
- 그런데도 설치본 약 570MB(A:\qgis\build\tooling), 설치 안 된 작업 폴더에서 늘 실패하는 시험 1개, 「필수 도구」로 적힌 설명서가 남아 헷갈린다.
- docs/developer-tools.md와 README는 Cursor·Codex 하네스를 걷어 낸 뒤에도(커밋 28280fa) Cursor 시절 작업 방식(필수 도구 조합, AGENTS.md, Cursor 훅·클라우드 에이전트, Codex 설정, 그 시절 작업 순서)을 지금 규칙처럼 설명한다. 가리키는 AGENTS.md, .codex/NOW.md, .agents/skills, .cursor 는 이미 없다.

## 바라는 결과
- 저장소에서 Graft 스크립트 4개(graft-mcp.mjs, 그 시험, setup-graft.ps1, Graft 경로만 쓰는 setup-dev-paths.ps1)가 없어지고, 설명서·설치 안내·환경 비교 스크립트(dev-env-lock.ps1)에서 Graft 단계가 빠진다.
- A:\qgis\build\tooling 의 Graft 설치본과 색인(graft-source, graft-index)을 지운다.
- 저장소의 node 시험을 모두 돌리면 Graft 때문에 실패하는 시험이 없다.
- docs/developer-tools.md와 README에서 Cursor 시절 작업 방식 설명이 빠진다. 지금도 맞는 내용(clangd·Archify·CMake 설정, 컴파일 DB, clang-tidy 스크립트, GitHub 자동 검사, 슈퍼파워 알림 장치)은 남기고, 규칙은 CLAUDE.md, 프로젝트 스킬은 .claude/skills 를 가리킨다.

## 영향받는 곳
- 개발 도구와 설명서만. 앱 기능·빌드·GitHub 검사는 바뀌지 않는다.

## 지켜야 할 것
- 지난 작업 기록(docs/HANDOFF.md, docs/archive)은 역사이므로 고치지 않는다.
- 설치 폴더는 안에 다른 곳으로 이어지는 연결(정션·심볼릭 링크)이 없는지 먼저 보고, 연결을 따라가지 않는 방법으로 지운다. 같은 폴더의 Graft가 아닌 파일은 그대로 둔다.
- 다시 필요하면 git 기록의 setup-graft.ps1로 다시 설치할 수 있다.
- 기술 선택(Claude가 정함): `.codex/config.toml` 무시 규칙은 남긴다(이 PC 경로가 든 설정이 실수로 커밋되지 않게). Node 판 비교는 다른 도구도 쓰므로 남긴다. 설명서에는 걷어 낸 사실을 한 줄로 남긴다(옛 작업 기록을 읽는 사람을 위해).

## 이번에 하지 않는 것
- Archify 도구 자체와 그 설치 안내(docs/archify-setup.md) 정리.
- 요청한 두 파일 밖의 Cursor 언급(다른 문서)은 보고에 한 줄로만 알린다.

## 미정 질문
- 없음(지울지는 사용자가 Jev에 맡김 → Jev: 지우기. Cursor 시절 설명은 사용자가 지우라고 함).

## 확인 방법
- 파일 탐색기에서 A:\qgis\build\tooling 안에 graft-source·graft-index 폴더가 없다.
- 저장소 node 시험을 모두 돌리면 실패가 없다.
- GitHub 저장소 첫 화면(README)에 Cursor·AGENTS.md 이야기가 없다.

## 결과
- 2026-10-03 완료. 커밋: 33f31ba(Graft 스크립트 4개와 그 설명 정리), 97b7cbf(developer-tools.md·README의 Cursor 시절 설명 정리). 기록 b197d78, 2421a12.
- 확인한 것: A:\qgis\build\tooling 에 graft-source·graft-index 없음(지우기 전 안의 연결 0개 확인, 553MB), 같은 폴더의 다른 파일은 그대로. 저장소 node 시험 101/101 통과(Graft 시험 실패 없음). dev-env-lock.ps1 출력은 Graft 경고 두 줄만 빠짐. README·developer-tools.md 에 Cursor 시절 표현 0줄(걷어 낸 사실 한 줄 제외), 없는 파일을 가리키는 링크 0개.
- 바꾼 시험: scripts/graft-mcp.test.mjs(시험 2개)를 시험 대상 graft-mcp.mjs 와 함께 지웠다(커밋 메시지에 「시험 변경:」 줄).
- 남긴 것(사용자 확인 필요): build/qa/tooling-validation-20260914 의 옛 Graft 사본(약 0.9GB, 09-14 검증 기록)과 build/tooling/tmp 의 Graft 시험 임시 폴더 7개(약 0.5MB). 요청한 두 파일 밖의 Cursor 언급(PROJECT.md, docs/README.md 등).
