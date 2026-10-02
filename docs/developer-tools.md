# HGIS 개발 도구

체크아웃은 클론한 폴더다. SDK는 `scripts/dev-env.ps1`이 `OSGEO4W_ROOT` → `C:\OSGeo4W` → `D:\OSGeo4W` → `A:\OSGeo4W` 순으로 찾는다. 패키지·MSVC·CMake·Windows SDK 판은 `dev-env.lock.json` 하나다. 도구별로 실제 검증된 작업을 맡기고, 원본 코드와 컴파일러·테스트로 결론을 확인한다.

**2026-09-15 사용자 지정 필수 조합 (harness switched to Cursor 2026-09-18): `Cursor Agent + AGENTS.md + clangd + Graft + Archify + CMake/CTest`.** 제품 코드 구현·수정에는 작은 변경도 이 조합을 모두 사용한다. 도구 이름만 언급하지 말고 실제 조회·산출물·검증 결과를 계획 또는 QA 기록에 남긴다.

| 도구 | 실제 개발에서 사용 | 범위 |
| --- | --- | --- |
| Cursor Agent | Agent·Task·MCP·hooks·skills로 조사·계획·수정·검증 실행 | 모델 선택기 고정 모델은 Grok 4.7. Auto·Fast 아님. https://cursor.com/docs/models |
| AGENTS.md | Cursor가 읽는 워크스페이스 규칙. 작업 시작 전 제약·순서·완료 기준 확인 | 현재 상태·handoff·소스와 함께 확인 |
| clangd + 실제 컴파일 DB | 진단, 호출 위치의 선언·정의 확인 | [탐색 명령](clangd-navigation.md), 표본 정의 조회 10/10 |
| Graft MCP | 후보 정의 검색, 파일 구조, 인덱싱된 파일의 문자열 검색 | `src/`, `tests/`; 전체 호출/영향 분석 제외 |
| Archify | 소스로 확인한 구조를 독립 HTML로 검토 | [설치·실행](archify-setup.md), 구조도 품질·브라우저 검증 |
| CMake / CTest | Release 컴파일과 동작 검증 | 기존 프로젝트 스크립트; 도구 결과로 대체하지 않음 |
| ka-hgis-verify | 화면에서 보던 확인을 측정값으로 돌린다. 주변유적 받기는 `파일 수신 시작` 다음 `받음`과 0보다 큰 크기만 통과 | `.agents/skills/ka-hgis-verify/SKILL.md`. 다른 PC는 이 저장소를 받으면 같은 스킬을 쓴다 |

## Cursor 설정

Graft는 저장소 `.cursor/mcp.json`이 아니라 USER-level `%USERPROFILE%\.cursor\mcp.json`의 서버 `hgis_graft`로 Cursor에 연결한다. `.codex/config.toml`은 호환용이며 체크아웃 경로가 들어가므로 커밋하지 않는다. 둘 다 `scripts/setup-dev-paths.ps1`이 이 폴더로 쓴다. 다른 MCP 서버는 유지한다.

clangd는 Cursor clangd 확장과 저장소 `.clangd`의 `CompilationDatabase: build`를 사용한다. Archify는 `scripts/archify.ps1`로 실행한다. CMake는 `CMakePresets.json`의 `vs`(Visual Studio 17 2022 x64)와 기존 `build/`를 유지한다. 워크스페이스 `.vscode/settings.json`은 `cmake.useCMakePresets: always`이고, 폴더를 열 때 자동 구성하지 않는다. `compiledb` 프리셋만 `build-clangd`에 Ninja를 쓴다.

## Graft 설치와 재현

검증한 upstream은 [NanoNets/Graft](https://github.com/NanoNets/Graft/tree/f9e65396e638e517aecae0d731017f53084d70ed), 버전 표기 `0.18.0`, 고정 커밋 `f9e65396e638e517aecae0d731017f53084d70ed`다. npm 최신 패키지와 동일한 소스라는 뜻은 아니다.

```powershell
.\scripts\setup-dev-paths.ps1
.\scripts\setup-graft.ps1
node --test scripts/graft-mcp.test.mjs
# 설치된 의존성/생성 파일 손상 시 고정 소스를 유지한 재빌드
# .\scripts\setup-graft.ps1 -Rebuild
```

Git, Node.js/npm, Python, VS C++ 도구가 필요하다. 설치 스크립트는 `build/tooling/graft-source`에 고정 소스를 받아 lockfile 의존성을 설치하고 필요한 native parser만 빌드한다. 기존 소스가 다른 커밋이거나 수정되어 있으면 중단한다. 인덱스는 `build/tooling/graft-index`에 생성한다. 재실행은 설치된 빌드를 재사용하며 인덱스를 새로 만든다. 두 폴더 모두 기존 build ignore 정책에 포함된다.

Cursor 연결은 USER-level `%USERPROFILE%\.cursor\mcp.json`의 `hgis_graft`다. 경로는 `scripts/setup-dev-paths.ps1`이 이 체크아웃으로 넣는다. 전역 설정·모델 선택은 바꾸지 않는다. Graft는 잠금의 Node 주 판이 같다. 이미 진행 중인 턴의 도구 목록 갱신은 별개이므로, Cursor에서 서버가 보이지 않으면 Cursor를 다시 열어 USER MCP를 로드한다.

서버는 `scripts/graft-mcp.mjs`를 실행한다. upstream MCP 조회 함수와 동일한 줄 단위 JSON-RPC 전송 방식을 사용하며 네 가지 도구만 노출한다.

- `graft_find_code`: 관련 정의 후보를 좁힌다.
- `graft_file_api`: 한 파일의 정의 목록을 살핀다.
- `graft_find_all`: 인덱싱된 소스/테스트에서 문자열을 찾는다.
- `graft_check_freshness`: 필요하면 구조 인덱스를 갱신하고 현재 파일 지문과 일치하는지 확인한다.

조회 전에 내용 해시까지 비교하여 구조 인덱스를 갱신하고, 갱신 실패·누락·조회 중 소스 변경은 오류로 반환한다. 생성물은 로컬 build 아래에만 쓴다. 최초 설치·의존성 다운로드에는 네트워크를 쓰지만 이후 구조 인덱싱과 조회에는 모델 API를 쓰지 않는다. upstream 자동 초기화, 자동 업데이트, 전역 hook, 토큰 절감 홍보 문구는 연결하지 않는다.

Graft의 기본 추출은 실제 호출 표본 10개 중 1개, Windows clangd 탐지만 보정한 실험도 6개만 찾았다. 따라서 `graft_trace_calls`와 graph 기반 영향 범위 판단을 제외했다. 파일·줄 범위와 오버로드는 원본 또는 clangd로 확인한다. 검색 결과가 없다는 이유로 사용처가 없다고 판단하지 않는다.

## 작업 순서

1. Cursor Agent에서 `AGENTS.md`, `.codex/NOW.md`, 관련 handoff·소스·테스트를 확인하고 필요한 작업 계획을 기록한다.
2. Graft로 관련 정의/파일을 조회하고 `rg`와 원본으로 확인한다. 작은 알려진 파일 수정도 해당 파일의 API 조회로 근거를 남긴다.
3. 실제 컴파일 DB를 사용하는 clangd로 구체적 호출의 선언/정의와 진단을 확인한다.
4. Archify로 변경 대상의 구조 또는 흐름을 검토한다. 변경 범위에 맞는 작은 도표를 사용한다. 기존 도표도 현재 소스 근거를 대조하고 필요한 부분을 갱신한 뒤 `validate` → `deliver` → `visual-check`를 실행한다.
5. 최소 변경을 구현하고 관련 CMake 빌드와 CTest를 실행한다. UI/지도 변경의 시작 smoke와 바로가기 연결은 `AGENTS.md`의 검증 규칙을 따른다. 변경 후 코드·도표 근거가 여전히 일치하는지 확인한다.
6. 각 도구의 입력 위치·산출물·결과와 남은 실패를 기록한다. 도구 부재 시 문서의 복구 절차를 시도하고, 불가능한 필수 검증은 미완료로 명시한다. 도구 결과가 소스·컴파일러·실제 동작 검증을 대신하지 않는다.

제품 코드나 C++ 빌드 동작을 바꾸지 않는 문서·규칙만의 수정은 기존 `AGENTS.md` 지침대로 파일 읽기·diff·대상 텍스트 검사로 검증한다. 포터블 생성은 계속 별도의 명시적 배포 요청이 있어야 한다.

## 검증 기록과 남은 사항

이번 설정의 실행 로그는 `build/qa/dev-tools-setup-20260914/`, 이전 전체 제품 빌드·테스트 기록은 `build/qa/tooling-validation-20260914/REPORT.md`에 있다. 로컬 QA 결과는 저장소에 커밋되지 않는다.

2026-09-14의 43개 중 39개 실패 기록은 현재 기준이 아니다. 2026-09-20에 `save_open_drawing`과 `heritage_style`은 통과했고 smoke는 0이었다. 사용자 Cursor의 Orca 훅과 `build/`용 Ninja 설정은 2026-09-22에 제거했다. clangd 헤더 삽입은 `never`다. 변경 줄 clang-tidy는 `scripts/clang-tidy-changed.ps1`다.

Grok이 여섯 도구를 빠뜨리던 이유는 지시가 `AGENTS.md` 안에만 있고, 빼먹어도 턴이 끝났기 때문이다. `src/`·`tests/` C++를 고친 Windows 세션은 `.cursor/hooks.json`의 `stop`이 Graft·clangd·Archify·ctest 기록이 없을 때 follow-up을 보낸다. 훅은 Node라서 Ubuntu 클라우드에서도 실행된다. https://cursor.com/docs/hooks

기록은 명령 이름이 아니라 **출력에 남은 결과**로만 세운다(2026-09-29, `.cursor/hooks/dev-loop.mjs`). Cursor 는 `afterShellExecution` 에 `command`·`output` 만 주고 종료 코드는 주지 않는다. 그래서:

| 도구 | 통과로 치는 증거 | 통과가 아닌 경우 |
| --- | --- | --- |
| ctest | `N% tests passed, 0 tests failed out of M`(M>0). `--output-junit` 을 썼으면 그 파일이 있고 `failures`·`errors` 가 0 | 실패 1개 이상, 0개 실행, 요약 없음, JUnit 없음 |
| clangd | `clangd-definition.py` 출력에 `"locations"` 와 `"file"` | `clangd-definition:` 오류, 파이썬 Traceback, 위치 없음 |
| Archify | `archify.ps1 validate`/`deliver`/`visual-check` 가 FAIL·Error 없이 끝나고 명령에 적은 `.html`/`.json` 이 있음 | 다른 하위 명령, 실패 문구, 없는 산출물 |
| Graft | `hgis_graft` MCP 호출(오류 없음) 또는 서버 스크립트 응답 | 오류 응답 |

실패는 `state/<도구>.failed` 에 이유를 남기고, `stop` follow-up 이 그 이유를 그대로 보여 준다(예: `ctest (ctest: 3 of 30 tests failed)`). `loop_limit` 2 뒤에는 멈추므로 끝까지 통과하지 못한 도구는 보고에 "미완료"로 적는다. 여섯 도구 필수 정책 자체는 그대로다.

클라우드 에이전트는 Ubuntu다. 설정은 `.cursor/environment.json`과 `.cursor/Dockerfile`이다. https://cursor.com/docs/cloud-agent/setup 그 VM에는 OSGeo4W와 Visual Studio가 없다. Release·CTest·smoke는 이 Windows PC에서만 한다. 클라우드에 비밀키를 파일로 넣지 않는다. 대시보드 Secrets를 쓴다. https://cursor.com/dashboard/cloud-agents#environments

Windows CI는 GitHub가 무료로 주는 `windows-2022` 실행 환경에서 돈다(2026-10-02부터, 공개 저장소). OSGeo4W qgis-dev를 `dev-env.lock.json` 꾸러미 목록대로 설치해 주마다 캐시하고, 구성·빌드·CTest·앱 시작 확인·한글 임시 폴더 시험·E2E 점검을 한다. `windows-latest`는 Visual Studio 2026뿐이라 쓰지 않는다. 비밀키는 쓰지 않는다. Linux `Sanity checks (no build)`는 같이 돌지만 빌드 통과가 아니다. 예전 self-hosted runner(`ka-hgis-pc`)는 더 쓰지 않는다. 공개 저장소에서는 지우는 것이 안전하다. GPL 대응 소스는 포터블의 `source/ka-hgis-source.zip` 으로 함께 간다. 이 문서 수정은 제품 C++를 바꾸지 않는다.

과거 Codex 설정 근거(호환): [프로젝트 MCP 설정](https://learn.chatgpt.com/docs/extend/mcp?surface=cli), [프로젝트 스킬](https://learn.chatgpt.com/docs/build-skills). Cursor는 USER MCP와 위 Cursor 설정을 우선한다. 실제 이 PC의 유효 설정과 실행 결과를 우선한다.

## 슈퍼파워 알림 장치

사용자 메시지마다 superpowers 스킬 표(CLAUDE.md 「superpowers 사용 방식」과 같은 내용)를 붙이는 UserPromptSubmit 훅이다. 긴 대화에서 스킬 사용이 빠지지 않게 한다. Strata 저장소(작업 폴더 포함) 안에서만 붙는다. 설치는 사용자가 직접 한다(대화 세션은 `~/.claude`를 고칠 수 없다). 설치 전 설정을 `~/.claude/_reset_backup/`에 백업하고, 다른 훅은 건드리지 않는다.

```powershell
node --test scripts/claude-hooks/superpowers-reminder.test.mjs
node scripts/claude-hooks/install-superpowers-reminder.mjs           # 설치
node scripts/claude-hooks/install-superpowers-reminder.mjs --remove  # 제거
```
