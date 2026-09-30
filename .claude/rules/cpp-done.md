---
paths:
  - "src/**/*.{cpp,cxx,cc,h,hpp}"
  - "tests/**/*.{cpp,cxx,cc,h,hpp}"
---

# C++ 변경의 완료 조건과 코드 스타일

이 규칙은 C++ 소스를 읽거나 고칠 때만 올라온다.

## 완료 조건

`src/` 또는 `tests/`의 C++를 고쳤다면, 이번 세션에서 실제로 돌린 다음 네 가지 기록이 있어야 끝이다. 도구 이름을 적는 것은 증거가 아니다. 자세한 순서는 `cpp-dev-loop` 스킬에 있다.

1. Graft — `mcp__hgis_graft__graft_find_code` / `graft_file_api`로 후보를 좁히고 원본으로 확인
2. clangd — `scripts/clangd-definition.py`로 실제 호출 위치의 선언·정의 확인
3. Archify — `scripts/archify.ps1`로 바뀐 흐름을 검토하고 validate
4. CTest — 바뀐 동작에 맞는 `ctest -R` (UI·시작·지도 변경이면 smoke까지)

Stop 훅이 이 기록을 확인하고, 빠진 것이 있으면 이어서 하라고 돌려보낸다. 도구를 쓸 수 없으면 복구를 시도하고, 그래도 안 되면 "검증 미완료"와 이유를 보고한다. 통과를 지어내지 않는다.

## 코드 스타일

- C++20, Qt6, MSVC(`/std:c++20 /Zc:__cplusplus /permissive- /EHsc`). `.clang-format`(Google 기반, 2칸, 120열, `Type* p`)을 따른다.
- 파일 전체를 clang-format으로 재배치하지 않는다. 바뀐 줄만 맞춘다. 무관한 줄이 섞이면 리뷰가 어려워지기 때문이다.
- 사용자에게 보이는 문자열은 기존 `QStringLiteral("…")` 한국어 패턴을 따른다.
- 핫스팟 `src/app/MainWindow.cpp`, `src/core/LayerOps.cpp`를 키우지 않는다. 새 개념은 작은 서비스로 뺀다. 늘어나면 guard 훅이 알린다.
- 새 의존성, drive-by 리팩터링, 관련 없는 정리는 하지 않는다.
