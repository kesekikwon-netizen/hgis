# QtTest 실행 결과를 파일로 받아, 실패했을 때만 그 내용을 출력한다.
#
# 이 Qt 빌드(OSGeo4W Qt 6.11)의 QtTest 는 결과를 표준출력으로 내보내지 않는다.
# 콘솔·파이프·cmd 리다이렉트 모두 0줄이고 -o 파일로만 나온다. 그래서 감싸지 않으면
# ctest 로그에 "<end of output>" 만 남아 실패 원인을 알 수 없다.
#
# 사용: cmake -DKA_TEST_EXE=<exe> -DKA_TEST_LOG=<log> [-DKA_TEST_FILTER="fn1 fn2"] -P cmake/run_qtest.cmake
# KA_TEST_FILTER 는 QtTest 함수 이름. https://doc.qt.io/qt-6/qtest-overview.html

if(NOT KA_TEST_EXE)
  message(FATAL_ERROR "KA_TEST_EXE 가 필요하다")
endif()
if(NOT KA_TEST_LOG)
  message(FATAL_ERROR "KA_TEST_LOG 이 필요하다")
endif()

get_filename_component(_ka_log_dir "${KA_TEST_LOG}" DIRECTORY)
file(MAKE_DIRECTORY "${_ka_log_dir}")
file(REMOVE "${KA_TEST_LOG}")

# The app's session log and crash dumps go under the build tree, never into the
# user's %LOCALAPPDATA%\ka-hgis\logs.
get_filename_component(_ka_test_name "${KA_TEST_LOG}" NAME_WE)
set(ENV{KA_HGIS_LOG_DIR} "${_ka_log_dir}/app-logs/${_ka_test_name}")
file(MAKE_DIRECTORY "$ENV{KA_HGIS_LOG_DIR}")

set(_ka_cmd "${KA_TEST_EXE}" -o "${KA_TEST_LOG},txt")
# Qt 6.11 offscreen 기본 화면은 800x800 이다. 그 상태로는 1280 창이
# showEvent 의 화면 맞춤으로 잘린다. 화면 크기는 환경 변수로 넘긴다.
# -platform 인자는 QTEST_GUILESS_MAIN 이 알 수 없는 옵션으로 거부한다.
# https://doc.qt.io/qt-6/qguiapplication.html#supported-command-line-options
get_filename_component(_ka_root "${CMAKE_CURRENT_LIST_DIR}" DIRECTORY)
if(NOT DEFINED ENV{QT_QPA_PLATFORM} OR "$ENV{QT_QPA_PLATFORM}" STREQUAL "offscreen")
  set(ENV{QT_QPA_PLATFORM} "offscreen:configfile=cmake/offscreen-1920.json")
endif()
if(DEFINED KA_TEST_FILTER AND NOT KA_TEST_FILTER STREQUAL "")
  separate_arguments(_ka_filter NATIVE_COMMAND "${KA_TEST_FILTER}")
  list(APPEND _ka_cmd ${_ka_filter})
endif()
execute_process(COMMAND ${_ka_cmd} WORKING_DIRECTORY "${_ka_root}" RESULT_VARIABLE _ka_result)

if(NOT _ka_result EQUAL 0)
  # 실패했을 때만 전체 결과를 남긴다. 통과 로그까지 찍으면 ctest 출력이 파묻힌다.
  if(EXISTS "${KA_TEST_LOG}")
    file(READ "${KA_TEST_LOG}" _ka_output)
    message("${_ka_output}")
  else()
    message("QtTest 로그가 없다. 프로세스가 시작 직후 끝났을 수 있다: ${KA_TEST_LOG}")
  endif()
  message(FATAL_ERROR "테스트 실패 (결과 ${_ka_result}): ${KA_TEST_EXE}")
endif()
