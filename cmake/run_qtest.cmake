# QtTest 실행 결과를 파일로 받아, 실패했을 때만 그 내용을 출력한다.
#
# 이 Qt 빌드(OSGeo4W Qt 6.11)의 QtTest 는 결과를 표준출력으로 내보내지 않는다.
# 콘솔·파이프·cmd 리다이렉트 모두 0줄이고 -o 파일로만 나온다. 그래서 감싸지 않으면
# ctest 로그에 "<end of output>" 만 남아 실패 원인을 알 수 없다.
#
# 사용: cmake -DKA_TEST_EXE=<exe> -DKA_TEST_LOG=<log> -P cmake/run_qtest.cmake

if(NOT KA_TEST_EXE)
  message(FATAL_ERROR "KA_TEST_EXE 가 필요하다")
endif()
if(NOT KA_TEST_LOG)
  message(FATAL_ERROR "KA_TEST_LOG 이 필요하다")
endif()

get_filename_component(_ka_log_dir "${KA_TEST_LOG}" DIRECTORY)
file(MAKE_DIRECTORY "${_ka_log_dir}")
file(REMOVE "${KA_TEST_LOG}")

execute_process(COMMAND "${KA_TEST_EXE}" -o "${KA_TEST_LOG},txt" RESULT_VARIABLE _ka_result)

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
