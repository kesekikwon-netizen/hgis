---
name: cpp-dev-loop
description: ka-hgis의 C++(src/, tests/) 코드를 구현·수정·버그 수정할 때 따르는 필수 개발 루프(계획 → Graft → clangd → 수정 → Archify → CMake/CTest → 보고). 한 줄짜리 수정이라도 제품 C++를 바꾸면 이 스킬을 쓴다. 사용자가 구현, 수정, 고쳐, fix, 추가, 바꿔, 적용이라고 말하고 대상이 .cpp/.h이면 연다. 문서·설정만 바꾸는 작업에는 쓰지 않는다.
---

# ka-hgis C++ 개발 루프

사용자는 2026-09-15부터 제품 코드 변경마다 `AGENTS.md + clangd + Graft + Archify + CMake/CTest`를 모두 쓰도록 정했다. 이유는 두 가지다. Graft는 C++ 교차 파일 호출을 10개 중 1개만 찾을 만큼 불완전해서 clangd와 원본으로 확인해야 하고, 테스트 통과만으로는 구조가 어떻게 바뀌었는지 사용자가 볼 수 없어서 Archify 그림이 필요하다. Stop 훅(`.claude/hooks/dev-loop.mjs`)이 네 도구 실행 기록을 확인한다.

## 1. 계획 (짧게)

답이나 메모에 네 줄을 먼저 쓴다: 증상/목표, 가능성 높은 코드·GIS 원인, 고칠 파일, 사용자가 볼 완료 확인. GIS 동작이면 `ka-hgis-gis` 스킬을 함께 연다.

## 2. Graft로 후보 좁히기

- `mcp__hgis_graft__graft_check_freshness` → 인덱스 신선도 확인(필요하면 자동 갱신).
- `mcp__hgis_graft__graft_find_code`로 관련 정의 후보, 알고 있는 파일이면 `mcp__hgis_graft__graft_file_api`.
- 결과의 줄 범위와 시그니처는 Read로 원본을 확인한다. 전체 텍스트와 Qt signal/slot 연결은 `rg`로 찾는다. 검색 결과가 없다고 사용처가 없다고 결론 내리지 않는다.
- MCP가 안 보이면 `node --test scripts/graft-mcp.test.mjs`, 설치 손상이면 `scripts/setup-graft.ps1`(사용자 확인 후) 순으로 복구한다.

## 3. clangd로 선언·정의 확인

실제 호출 위치 하나 이상에 대해 실행한다. 인자는 저장소 상대 경로, 1부터 줄, 1부터 UTF-16 열(심볼 이름 위).

```bash
python scripts/clangd-definition.py src/core/LayerOps.cpp 2517 33
```

`diagnostic_error_count > 0`이면 컴파일 DB부터 확인한다(`scripts/gen-compile-commands.ps1`). clangd-lsp 플러그인이 켜져 있으면 LSP 도구로 참조·진단을 추가로 볼 수 있지만, 기록은 위 스크립트로 남긴다.

## 4. 수정

가장 작은 올바른 변경. 기존 패턴과 헬퍼를 쓰고, 핫스팟(`MainWindow.cpp`, `LayerOps.cpp`)을 키우지 않는다. 바뀐 동작에 대한 회귀 시험을 `tests/`에 추가한다.

## 5. Archify

바뀐 구조나 흐름만 담은 작은 다이어그램을 만들거나, 기존 다이어그램(`docs/architecture/*.json`)을 현재 소스와 대조해 갱신한다. 모든 관계에 현재 소스 근거를 달고, 계획만 된 구성요소는 구분한다.

```bash
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/archify.ps1 validate docs/architecture/<name>.json
```

세부 사용법은 `archify` 스킬. 다이어그램 검증은 C++ 의미를 검증하지 않는다.

## 6. 빌드와 테스트 (Windows에서만)

```bash
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/build-now.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -Command ". .\scripts\dev-env.ps1; ctest --test-dir build -C Release -R '<test_regex>' --output-on-failure"
```

- 시작·UI·메뉴·지도·프로젝트 열기 변경이면 `scripts/run-ka-hgis.ps1 --smoke-quit`까지.
- 실패가 이번 변경 전에도 있던 것인지 구분해 보고한다(같은 함수·메시지인지).
- 사용자가 실행 중인 앱을 끄거나 다시 열지 않는다. 포터블·publish는 요청이 있을 때만.
- Windows가 아닌 환경(클라우드)에서는 빌드·ctest·smoke를 돌리지 않고, 통과했다고 말하지 않는다.

## 7. 보고 형식

```
결론: <한 줄>
변경: <파일:줄 — 무엇을 왜>
증거:
- Graft: <도구, 질의> → <찾은 후보>
- clangd: <file line col> → <선언/정의 위치, diagnostic 수>
- Archify: <파일> validate → <결과>
- CTest: <명령> → <통과/실패, 시간>
미확인: <남은 것>
```

작업이 끝나면 `.codex/NOW.md` 맨 위에 짧은 항목(날짜, 무엇, 검증 결과)을 추가한다.
