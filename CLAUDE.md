# ka-hgis — Claude Code 작업 지침

이 파일은 Claude Code가 세션마다 읽는 프로젝트 규칙이다. 제품 규칙의 정본은 `AGENTS.md`이고, 아래에서 그대로 가져온다. 이 파일은 Claude Code에서 그 규칙을 **어떻게 실행하는지**만 덧붙인다.

@AGENTS.md

<harness_override>
2026-09-29 사용자 결정: 이 저장소는 Claude Code로 개발한다. 위 AGENTS.md의 Cursor 전용 문장은 Claude Code 세션에서 다음처럼 바꿔 읽는다. 제품 불변식·GIS 규칙·검증 기준·Git 규칙은 그대로다.

- "Cursor Agent", "Cursor Runtime" → Claude Code. "Task subagents" → `.claude/agents/`의 서브에이전트.
- "Grok 4.7 고정 모델" → 모델은 사용자가 `/model`로 고른다. 모델을 임의로 바꾸라고 권하지 않는다.
- `.cursor/hooks.json` stop 훅 → `.claude/settings.json`의 Stop 훅(`.claude/hooks/dev-loop.mjs`). 같은 네 도구 기록을 확인한다.
- "second harness 금지" 항목은 Claude Code를 막지 않는다. OpenCode·Sisyphus·숨은 자동 push는 여전히 금지다.
- USER-level `%USERPROFILE%\.cursor\mcp.json`의 `hgis_graft` → 프로젝트 `.mcp.json`의 `hgis_graft`. 도구 이름은 `mcp__hgis_graft__graft_find_code` 등이다.
- `.agents/skills/*` 스킬 → `.claude/skills/`의 같은 이름 스킬이 원본 SKILL.md를 읽게 연결한다. 원본은 `.agents/skills/`에만 고친다.
</harness_override>

## 세션 시작

비자명한 제품·GIS 작업 전에 다음을 읽는다. 최근 결정이 과거 문서를 이기기 때문이다.

1. `.codex/NOW.md` 맨 위 몇 항목 (현재 상태, 최근 현장 제약)
2. `docs/HANDOFF.md` (제품 정본)
3. 작업에 해당하는 스킬: GIS 동작은 `ka-hgis-gis`, 조판·정합 회귀는 `ka-hgis-sheet`, 인트라넷 받기는 `heritage-intranet`, 화면 확인은 `ka-hgis-verify`

설명·조사·리뷰 요청은 읽기만 하고 보고한다. 적용·구현·수정·fix 같은 동사가 있을 때만 코드를 바꾼다.

## 셸과 명령 (Windows)

Claude Code의 Bash 도구는 Windows에서 Git Bash로 돈다. 저장소 스크립트는 PowerShell이므로 다음 형태로 부른다. `dev-env.ps1`은 같은 PowerShell 세션 안에서 dot-source해야 SDK 경로가 살아 있다.

```bash
# 빌드 (configure + Release build)
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/build-now.ps1
# 특정 테스트
powershell.exe -NoProfile -ExecutionPolicy Bypass -Command ". .\scripts\dev-env.ps1; ctest --test-dir build -C Release -R '^survey_contour$' --output-on-failure"
# 전체 테스트
powershell.exe -NoProfile -ExecutionPolicy Bypass -Command ". .\scripts\dev-env.ps1; ctest --test-dir build -C Release --output-on-failure"
# 시작·UI·지도 변경 뒤 스모크
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/run-ka-hgis.ps1 --smoke-quit
# clangd 선언/정의 (저장소 상대 파일, 1부터 줄, 1부터 UTF-16 열)
python scripts/clangd-definition.py src/core/ExportService.cpp 149 46
# Archify
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/archify.ps1 validate <diagram.json>
# CMake/SDK가 바뀐 뒤 컴파일 DB 재생성
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/gen-compile-commands.ps1
# 바뀐 줄만 clang-tidy
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/clang-tidy-changed.ps1
```

MSVC 빌드는 수 분 걸린다. 빌드·전체 ctest처럼 출력이 긴 일은 `build-verifier` 서브에이전트에 맡기거나 백그라운드로 돌리고, 끝난 결과만 읽는다. ctest는 기본 16개 병렬이라 부하로만 실패하는 시험이 있다. 실패는 `-j1`로 다시 돌려 확인한다.

## 규칙이 있는 곳

- C++ 변경의 완료 조건(Graft·clangd·Archify·CTest)과 코드 스타일은 `.claude/rules/cpp-done.md`에 있다. `src/`·`tests/`의 C++를 읽을 때 올라온다.
- 반드시 지킬 금지 규칙은 `.claude/hooks/guard.mjs`가 도구 실행 전에 막는다. 막는 것: 강제 push, 숨은 자동 push, 저장소에 키 모양 GUID 쓰기, `src/`의 `removeAllMapLayers()`, 사용자 계정·앱 데이터 수정·삭제. 확인을 받는 것: 포터블·배포·서명, 실행 중인 앱 종료, PowerShell의 커밋·되돌리기 계열 git.
- 절차는 스킬에 있다: 빌드·시험 `/hgis-build`, 워크트리 작업을 바탕화면 아이콘에 연결 `/desktop-connect`.

## 응답

- 한국어로 답한다. 코드·경로·식별자는 영어 그대로 둔다.
- 결론을 먼저 쓰고, 근거(파일:줄, 명령과 결과)를 붙인다. 확인하지 못한 것은 확인하지 못했다고 구분한다.
- 커밋·푸시·포터블 생성(`publish-desktop.ps1`, `make-portable.ps1`)은 사용자가 요청할 때만 한다.

## 개인 설정

이 PC에만 해당하는 메모는 `CLAUDE.local.md`(git 제외)에 둔다.
