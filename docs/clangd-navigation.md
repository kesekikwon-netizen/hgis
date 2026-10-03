# C++ 선언·정의 위치 확인

`scripts/clangd-definition.py`는 선택한 코드 위치를 실제 clangd에 질의한다. Graft가 찾은 후보에서 호출 대상을 확인하거나 동명 메서드를 구별할 때 사용한다. 결과는 선언 또는 정의 위치이며 전체 호출자나 변경 영향 범위를 보장하지 않는다.

Python 3.9 이상과 설치된 clangd가 필요하다. Python 추가 패키지는 사용하지 않는다. 컴파일 DB(`build/compile_commands.json`)는 2026-10-03부터 빌드가 만든다(`scripts/compile-commands.mjs`, CMake file API). 없으면 `cmake -S . -B build`를 한 번 돌린 뒤 빌드한다.

```powershell
python .\scripts\clangd-definition.py src/core/ExportService.cpp 149 46
```

인자는 **저장소 상대 파일, 1부터 시작하는 줄, 1부터 시작하는 UTF-16 열**이다. 열은 호출할 심볼 이름 위에 놓는다. 한글과 일반 ASCII 문자는 각각 한 칸, 일부 이모지처럼 보조 평면 문자는 두 칸으로 센다. 출력 위치도 같은 기준이다. 파일을 수정하면 예제의 줄·열이 바뀔 수 있으므로 현재 소스를 먼저 확인한다.

스크립트는 자신이 속한 저장소의 `build/compile_commands.json`을 사용하고, PATH 또는 `Program Files/LLVM/bin/clangd.exe`에서 clangd를 찾는다. DB의 `/vctoolsdir`가 빌드에 쓰는 MSVC를 가리켜, PC에 다른 Visual Studio(18판)가 있어도 그 헤더를 읽지 않는다. Developer PowerShell의 INCLUDE 상속은 필요 없다. 빌드 구성이 바뀌면 다음 빌드가 DB를 다시 쓴다.

출력 JSON의 `locations`는 정확한 파일·줄·열이다. `diagnostic_error_count`가 0보다 크면 현재 파일에 파싱 오류가 있으므로 컴파일 DB와 오류부터 확인한다. 프로젝트 밖 Qt/QGIS/표준 라이브러리 헤더가 대상이면 해당 SDK 경로를 그대로 반환한다. 구현 파일이 아직 인덱싱되지 않은 경우 헤더 선언이 반환될 수 있다.

기본 요청 제한은 45초이며 `--timeout 1`부터 `--timeout 55`까지 지정할 수 있다. 준비 지연·잘못된 입력·대상 없음은 0이 아닌 종료 코드와 오류 메시지를 반환한다. 전체 백그라운드 인덱싱은 켜지 않으며, 성공·실패 모두 스크립트가 시작한 clangd 프로세스를 종료한다. 기존 편집기나 사용자 앱은 조작하지 않는다.
