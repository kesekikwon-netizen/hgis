---
paths:
  - "CMakeLists.txt"
  - "cmake/**"
  - "CMakePresets.json"
  - ".clangd"
  - ".clang-tidy"
  - ".clang-format"
  - "compile_flags.txt"
  - "dev-env.lock.json"
---

# 빌드 설정 규칙

- 옵션은 target 단위(`target_compile_options`, `target_compile_definitions`)로 준다. 전역 `CMAKE_CXX_FLAGS`를 오염시키지 않는다.
- `-march=native`, `/fp:fast`, sanitizer, PGO, LTO는 빌드 정책 변경이다. 사용자 요청과 Qt/QGIS/OSGeo4W 호환 근거 없이 넣지 않는다.
- `CMakePresets.json`의 `vs`(VS 2022 x64, `build/`)와 `compiledb`(Ninja, `build-clangd/`)를 유지한다.
- CMake·SDK를 바꾸면 `scripts/gen-compile-commands.ps1`로 `build/compile_commands.json`을 다시 만들고, `.clangd`·`.clang-tidy`와 실제 빌드가 어긋나지 않게 한다.
- 버전 고정값은 `dev-env.lock.json` 하나에서 관리한다.
