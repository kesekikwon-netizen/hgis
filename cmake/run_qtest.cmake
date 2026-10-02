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

# Default runtime environment for every test (F212). Tests registered without an
# ENVIRONMENT property (survey_contour, tile_print, download_ui, ...) died with
# 0xc0000135 outside a dev-env.ps1 shell, and parallel ctest runs shared one TEMP.
# Values already provided by the ctest ENVIRONMENT property or the shell win.
get_filename_component(_ka_bin "${_ka_log_dir}" DIRECTORY)
set(_ka_osgeo "")
if(EXISTS "${_ka_bin}/CMakeCache.txt")
  file(STRINGS "${_ka_bin}/CMakeCache.txt" _ka_osgeo_line REGEX "^OSGEO4W_ROOT:[A-Z]+=")
  if(_ka_osgeo_line)
    string(REGEX REPLACE "^OSGEO4W_ROOT:[A-Z]+=" "" _ka_osgeo "${_ka_osgeo_line}")
  endif()
endif()
if(_ka_osgeo STREQUAL "" AND DEFINED ENV{OSGEO4W_ROOT})
  file(TO_CMAKE_PATH "$ENV{OSGEO4W_ROOT}" _ka_osgeo)
endif()
if(NOT _ka_osgeo STREQUAL "" AND IS_DIRECTORY "${_ka_osgeo}/apps/qgis-dev/bin")
  file(TO_CMAKE_PATH "$ENV{PATH}" _ka_path_now)
  string(TOLOWER "${_ka_path_now}" _ka_path_lower)
  string(TOLOWER "${_ka_osgeo}/apps/qgis-dev/bin" _ka_qgis_bin_lower)
  string(FIND "${_ka_path_lower}" "${_ka_qgis_bin_lower}" _ka_qgis_bin_at)
  if(_ka_qgis_bin_at EQUAL -1)
    set(_ka_bins "${_ka_osgeo}/apps/qgis-dev/bin" "${_ka_osgeo}/apps/Qt6/bin" "${_ka_osgeo}/apps/gdal-dev/bin"
                 "${_ka_osgeo}/apps/pdal-dev/bin" "${_ka_osgeo}/bin")
    string(JOIN ";" _ka_bins_text ${_ka_bins})
    set(ENV{PATH} "${_ka_bins_text};$ENV{PATH}")
  endif()
  if(NOT DEFINED ENV{GDAL_DATA} AND IS_DIRECTORY "${_ka_osgeo}/apps/gdal-dev/share/gdal")
    set(ENV{GDAL_DATA} "${_ka_osgeo}/apps/gdal-dev/share/gdal")
  endif()
  if(NOT DEFINED ENV{PROJ_LIB} AND NOT DEFINED ENV{PROJ_DATA} AND IS_DIRECTORY "${_ka_osgeo}/share/proj")
    set(ENV{PROJ_LIB} "${_ka_osgeo}/share/proj")
  endif()
  if(NOT DEFINED ENV{QGIS_PREFIX_PATH})
    set(ENV{QGIS_PREFIX_PATH} "${_ka_osgeo}/apps/qgis-dev")
  endif()
endif()
# qgis-dev stages project archives as a fixed qgis-project-*.zip inside TEMP, so
# every test process needs its own TEMP. CMake already gives most tests one
# (.../ka-hgis-tests-<id>/<test>); the rest get build/test-tmp/<test> here.
file(TO_CMAKE_PATH "$ENV{TEMP}" _ka_temp_now)
if(NOT _ka_temp_now MATCHES "/ka-hgis-tests-[0-9a-f]+/")
  set(_ka_own_temp "${_ka_bin}/test-tmp/${_ka_test_name}")
  file(MAKE_DIRECTORY "${_ka_own_temp}")
  set(ENV{TEMP} "${_ka_own_temp}")
  set(ENV{TMP} "${_ka_own_temp}")
endif()

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
# 한 실행 파일이 스위트를 둘 돌리면 두 번째는 "<로그>.<이름>.txt" 에 쓴다(test_survey_contour).
# 지난 실행의 것이 섞이지 않게 먼저 지운다.
file(GLOB _ka_extra_logs "${KA_TEST_LOG}.*.txt")
if(_ka_extra_logs)
  file(REMOVE ${_ka_extra_logs})
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
  file(GLOB _ka_extra_logs "${KA_TEST_LOG}.*.txt")
  foreach(_ka_extra IN LISTS _ka_extra_logs)
    file(READ "${_ka_extra}" _ka_output)
    message("---- ${_ka_extra}\n${_ka_output}")
  endforeach()
  message(FATAL_ERROR "테스트 실패 (결과 ${_ka_result}): ${KA_TEST_EXE}")
endif()
