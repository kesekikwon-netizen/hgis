---
name: hgis-build
description: ka-hgis Release 빌드와 CTest를 실행하고 결과를 요약한다. /hgis-build [테스트정규식] 으로 사용자가 직접 부른다.
disable-model-invocation: true
argument-hint: "[ctest -R 정규식, 비우면 전체]"
---

# /hgis-build

1. Release 빌드를 백그라운드로 실행하고 끝날 때까지 기다린다:
   `powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/build-now.ps1`
2. 빌드가 실패하면 첫 번째 컴파일 오류(파일:줄, 메시지)와 원인 후보를 보고하고 멈춘다. 고치지는 않는다.
3. 성공하면 테스트를 실행한다. 인자 `$ARGUMENTS`가 있으면 `-R '$ARGUMENTS'`를 붙이고, 없으면 전체를 돈다:
   `powershell.exe -NoProfile -ExecutionPolicy Bypass -Command ". .\scripts\dev-env.ps1; ctest --test-dir build -C Release --output-on-failure"`
4. 결과를 표로 요약한다: 통과/실패 수, 실패한 테스트 이름과 첫 실패 메시지, 소요 시간. `.codex/NOW.md`에 기록된 기존 실패(예: perf_engine, storage_safety, cadastral, save_open_portable)와 같은지 표시한다.
