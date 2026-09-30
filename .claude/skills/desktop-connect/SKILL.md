---
name: desktop-connect
description: 워크트리에서 커밋한 작업을 A:\qgis(main)로 fast-forward하고 Release를 다시 빌드해 바탕화면 「고고학 전용 HGIS」 아이콘에 연결한다. 사용자가 /desktop-connect 로 직접 부른다.
disable-model-invocation: true
argument-hint: "[커밋 SHA 또는 브랜치, 비우면 현재 워크트리 HEAD]"
---

# /desktop-connect

바탕화면 아이콘은 `scripts/start-ka-hgis.vbs` → `launch.ps1` → `A:\qgis\build\Release\ka-hgis.exe`를 연다. 워크트리 빌드는 아이콘에 닿지 않는다. 사용자가 부른 경우에만 이 순서대로 한다.

1. 대상 커밋을 정한다. `$ARGUMENTS`가 있으면 그것, 없으면 현재 워크트리의 `git rev-parse HEAD`. 워크트리에 커밋되지 않은 변경이 있으면 멈추고 알린다. 커밋은 사용자가 요청할 때만 Lore 형식으로 한다.
2. `A:\qgis`를 확인한다.
   - `git -C A:/qgis rev-parse HEAD`가 대상 커밋의 조상인가: `git -C A:/qgis merge-base --is-ancestor HEAD <sha>`.
   - `git -C A:/qgis status --porcelain`의 더러운 파일이 대상 커밋이 바꾸는 파일과 겹치지 않는가. 겹치면 멈추고 목록을 보여 준다. 그 작업은 사용자 것이므로 건드리지 않는다.
   - 대상 커밋이 추적하게 된 파일이 `A:\qgis`에 미추적으로 남아 있으면(예: `.claude/`, `CLAUDE.md`) merge가 거부된다. 내용을 비교해 보여 주고, 지워도 되는지 사용자에게 묻는다.
3. `git -C A:/qgis merge --ff-only <sha>`. fast-forward가 안 되면 멈춘다. rebase·강제 이동은 하지 않는다.
4. 사용자가 앱을 실행 중이면 exe가 잠겨 링크가 실패한다. 앱을 닫지 말고, 실행 중인 `build\Release\ka-hgis.exe`를 `ka-hgis.running-<시각>.exe`로 이름만 바꾼 뒤 빌드한다.
5. 빌드: `powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "Set-Location A:\qgis; . .\scripts\dev-env.ps1; cmake --build build --config Release --target ka-hgis"`. 오래 걸리므로 백그라운드로 돌린다.
6. 스모크: `powershell.exe -NoProfile -ExecutionPolicy Bypass -File A:/qgis/scripts/run-ka-hgis.ps1 --smoke-quit`. 종료 코드 0이어야 한다.
7. 아이콘 그림(`data/theme/ka-hgis.ico`)이 바뀌었으면 `SHChangeNotify(SHCNE_ASSOCCHANGED=0x08000000)`로 셸 아이콘 캐시를 갱신한다. `ie4uinit -show`는 이 PC에서 효과가 없었다.
8. 보고한다: fast-forward한 커밋, 빌드·스모크 결과, 남은 더러운 파일. push는 사용자가 한다. 명령만 알려 준다: `git -C A:/qgis push origin main`.

포터블 생성(`make-portable.ps1`, `publish-desktop.ps1`)은 이 절차에 포함되지 않는다. 사용자가 따로 요청할 때만 한다.
