# HGIS 개발 도구

체크아웃은 클론한 폴더다. SDK는 `scripts/dev-env.ps1`이 `OSGEO4W_ROOT` → `C:\OSGeo4W` → `D:\OSGeo4W` → `A:\OSGeo4W` 순으로 찾는다. 패키지·MSVC·CMake·Windows SDK 판은 `dev-env.lock.json` 하나다. 도구별로 실제 검증된 작업을 맡기고, 원본 코드와 컴파일러·테스트로 결론을 확인한다.

작업 방식과 검증 규칙은 `AGENTS.md`를 따른다(`CLAUDE.md`는 그것을 그대로 불러온다). Cursor·Orca·Archify·Jev와 superpowers 알림 훅, capture-intent 설치기, COMMIT_STATUS Git 훅은 2026-10-16에 걷어 냈다.

| 도구 | 실제 개발에서 사용 | 범위 |
| --- | --- | --- |
| clangd + 실제 컴파일 DB | 진단, 호출 위치의 선언·정의 확인 | [탐색 명령](clangd-navigation.md), 표본 정의 조회 10/10 |
| CMake / CTest | Release 컴파일과 동작 검증 | 기존 프로젝트 스크립트; 도구 결과로 대체하지 않음 |

## 도구 설정

clangd는 저장소 `.clangd`의 `CompilationDatabase: build`를 사용한다. CMake는 `CMakePresets.json`의 `vs`(Visual Studio 17 2022 x64)와 기존 `build/`를 유지한다. 워크스페이스 `.vscode/settings.json`은 `cmake.useCMakePresets: always`이고, 폴더를 열 때 자동 구성하지 않는다. clangd 컴파일 DB는 빌드가 `build/compile_commands.json`으로 만든다(`scripts/compile-commands.mjs`). clangd 헤더 삽입은 `never`다. 변경 줄 clang-tidy는 `scripts/clang-tidy-changed.ps1`다.

## GitHub 자동 검사

Windows CI는 GitHub가 무료로 주는 `windows-2022` 실행 환경에서 돈다(2026-10-02부터, 공개 저장소). OSGeo4W qgis-dev를 `dev-env.lock.json` 꾸러미 목록대로 설치해 주마다 캐시하고, 구성·빌드·CTest·앱 시작 확인·한글 임시 폴더 시험·E2E 점검을 한다. `windows-latest`는 Visual Studio 2026뿐이라 쓰지 않는다. 비밀키는 쓰지 않는다. Linux `Sanity checks (no build)`는 같이 돌지만 빌드 통과가 아니다. 예전 self-hosted runner(`ka-hgis-pc`)는 더 쓰지 않는다. 공개 저장소에서는 지우는 것이 안전하다. GPL 대응 소스는 포터블의 `source/ka-hgis-source.zip` 으로 함께 간다. 이 문서 수정은 제품 C++를 바꾸지 않는다.

## 커밋 게이트

`.claude/hooks/commit-gate.mjs`(Claude Code PreToolUse 훅)는 `src/`·`tests/`·`cmake/`·`data/`·`CMakeLists.txt`를 건드린 커밋을 빌드와 그 뒤 ctest 기록이 있을 때만 통과시키고, 원래 시험을 약하게 바꾼 커밋은 `시험 변경:` 줄이 있어야 통과시킨다. 줄 수로는 막지 않는다(2026-10-16). 시험: `node --test .claude/hooks/commit-gate.test.mjs`.
