cmake_minimum_required(VERSION 3.21)
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" KA_HGIS_VERSION_FILE LIMIT_COUNT 1)
string(STRIP "${KA_HGIS_VERSION_FILE}" KA_HGIS_VERSION)
if(NOT KA_HGIS_VERSION MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+$")
  message(FATAL_ERROR "VERSION must be major.minor.patch, got '${KA_HGIS_VERSION}'")
endif()
project(ka-hgis VERSION ${KA_HGIS_VERSION} LANGUAGES CXX)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

if (MSVC)
  add_compile_options(/MP)
  # /Zc:__cplusplus : 없으면 CMAKE_CXX_STANDARD 20 을 줘도 __cplusplus 가 199711(C++98)로 보고되어
  #                   라이브러리들의 C++20 기능 검출이 조용히 실패한다. (이 머신에서 실측 확인)
  # /utf-8          : 소스/실행 문자셋 UTF-8 고정 (기존에 타깃마다 흩어져 있던 것을 여기서 일괄 적용)
  add_compile_options(/Zc:__cplusplus /utf-8 /permissive-)
  # 경고 수준 상향. 자체 코드는 따옴표 include, Qt/QGIS 는 꺾쇠 include 이므로
  # /external:anglebrackets 로 외부 헤더 경고만 차단하고 우리 코드에는 /W4 를 적용한다.
  add_compile_options(/W4 /external:anglebrackets /external:W0)
endif()

# LTO(링크 타임 최적화): 번역 단위 경계를 넘어 인라이닝한다. Release 에서만.
# check_ipo_supported() 는 build/CMakeFiles 에 임시 테스트 프로젝트를 남겨
# VS 제너레이터의 ALL_BUILD 를 깨뜨리므로(MSB3202) 쓰지 않는다.
# MSVC 2022 는 /GL + /LTCG 를 확실히 지원하므로 바로 켠다.
if(MSVC)
  set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE OFF)
  message(STATUS "LTO: MSVC c2.dll 내부 오류 회피를 위해 OFF")
else()
  include(CheckIPOSupported)
  check_ipo_supported(RESULT KA_IPO_OK OUTPUT KA_IPO_MSG)
  if(KA_IPO_OK)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
  else()
    message(STATUS "LTO 미지원: ${KA_IPO_MSG}")
  endif()
endif()

option(KA_HGIS_BUILD_TESTS "Build tests" ON)
if (EXISTS "C:/OSGeo4W")
  set(OSGEO4W_ROOT_DEFAULT "C:/OSGeo4W")
elseif (EXISTS "D:/OSGeo4W")
  set(OSGEO4W_ROOT_DEFAULT "D:/OSGeo4W")
elseif (EXISTS "A:/OSGeo4W")
  set(OSGEO4W_ROOT_DEFAULT "A:/OSGeo4W")
endif()
set(OSGEO4W_ROOT "${OSGEO4W_ROOT_DEFAULT}" CACHE PATH "OSGeo4W root")
# Keep the prefix under the selected root. A stale cache still pointed tests
# at another PC's qgis-dev, so the WMS provider never loaded.
set(QGIS_PREFIX_PATH "${OSGEO4W_ROOT}/apps/qgis-dev" CACHE PATH "QGIS prefix" FORCE)
list(PREPEND CMAKE_PREFIX_PATH "${OSGEO4W_ROOT}/apps/Qt6" "${OSGEO4W_ROOT}/apps/Qt6/lib/cmake")
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Xml PrintSupport Network Svg Test Multimedia MultimediaWidgets)
find_package(Qt6 REQUIRED COMPONENTS WebEngineWidgets)

set(KA_INCLUDES
  ${CMAKE_SOURCE_DIR}/src
  ${QGIS_PREFIX_PATH}/include
  ${OSGEO4W_ROOT}/apps/gdal-dev/include
  ${OSGEO4W_ROOT}/include
)
set(KA_LIBDIRS
  ${QGIS_PREFIX_PATH}/lib
  ${OSGEO4W_ROOT}/apps/gdal-dev/lib
  ${OSGEO4W_ROOT}/lib
)

add_library(ka_core STATIC
  src/core/LayerLabelControls.cpp
  src/core/CadastralImport.cpp
  src/core/CadastralPortal.cpp
  data/topographic/topographic.qrc
  src/core/TopographicArchive.cpp
  src/core/TopographicSheets.cpp
  src/core/TopographicSourceCrs.cpp
  src/core/TopographicCatalog.cpp
  src/core/TopographicShapefileCache.cpp
  src/core/TopographicSettings.cpp
  src/core/ChecklistEngine.cpp
  src/core/SurveyProjectFactory.cpp
  src/core/SurveyStorage.cpp
  src/core/SurveySession.cpp
  src/core/PreparedReferenceMap.cpp
  src/core/DemDownloadService.cpp
  src/core/ExportService.cpp
  src/core/MapGeoTiffExport.cpp
  src/core/ProjectStateBuilder.cpp
  src/core/LayoutService.cpp
  src/core/LayerOps.cpp
  src/core/BasemapOps.cpp
  src/core/LabelOps.cpp
  src/core/ControlPointCsv.cpp
  src/core/KaPortableRuntime.cpp
  src/core/DemColorRampLegend.cpp
  src/core/DemPresentation.cpp
  src/core/LocationSearch.cpp
  src/core/AdminBoundaryService.cpp
  src/core/KoreaRegionCatalog.cpp
  src/core/HeritageStyle.cpp
  src/core/HeritageSiteLegend.cpp
  src/core/HeritageLayoutNumbers.cpp
  src/core/HeritageRegionResolver.cpp
  src/core/HeritageIntranetSettings.cpp
  src/core/KaSecretStore.cpp
  src/core/KaSessionLog.cpp
  src/core/HeritageIntranetFlow.cpp
  src/core/HeritageImport.cpp
  src/core/SurveyScopeClip.cpp
  src/core/HeritageFormParser.cpp
  src/core/HeritageHttpClient.cpp
  src/core/WorkflowGuide.cpp
  src/core/VworldSettings.cpp
  src/core/GeorefService.cpp
  src/core/FeaturePresets.cpp
  src/core/BufferAnalysis.cpp
  src/core/RecentSurveys.cpp
  src/core/KaSafeQgis.cpp
  src/core/MeasureOps.cpp
  src/core/DemAnalyzer.cpp
  src/core/TrenchGridGenerator.cpp
  src/core/TilePackService.cpp
  src/core/SoilMapService.cpp
  src/core/PaleoLandformService.cpp
  src/core/GeologyMapService.cpp
  src/core/RiverMapService.cpp
  src/core/SectionLayoutService.cpp
  src/core/Terrain3dService.cpp
  src/core/Terrain3dLayoutService.cpp
  src/core/ChecklistEngine.h
  src/core/SurveyProjectFactory.h
  src/core/SurveyStorage.h
  src/core/SurveySession.h
  src/core/ExportService.h
  src/core/ProjectStateBuilder.h
  src/core/LayoutService.h
  src/core/LayerOps.h
  src/core/KaPortableRuntime.h
  src/core/KaSessionLog.h
  src/core/LocationSearch.h
  src/core/AdminBoundaryService.h
  src/core/KoreaRegionCatalog.h
  src/core/HeritageStyle.h
  src/core/HeritageSiteLegend.h
  src/core/HeritageLayoutNumbers.h
  src/core/HeritageRegionResolver.h
  src/core/HeritageIntranetSettings.h
  src/core/HeritageIntranetFlow.h
  src/core/HeritageImport.h
  src/core/SurveyScopeClip.h
  src/core/HeritageFormParser.h
  src/core/HeritageHttpClient.h
  src/core/WorkflowGuide.h
  src/core/VworldSettings.h
  src/core/GeorefService.h
  src/core/FeaturePresets.h
  src/core/BufferAnalysis.h
  src/core/RecentSurveys.h
  src/core/KaSafeQgis.h
  src/core/MeasureOps.h
  src/core/DemAnalyzer.h
  src/core/TrenchGridGenerator.h
  src/core/TilePackService.h
  src/core/SoilMapService.h
  src/core/PaleoLandformService.h
  src/core/GeologyMapService.h
  src/core/RiverMapService.h
  src/core/SectionLayoutService.h
  src/core/Terrain3dService.h
  src/core/Terrain3dLayoutService.h
)
target_include_directories(ka_core PUBLIC ${KA_INCLUDES})
target_link_directories(ka_core PUBLIC ${KA_LIBDIRS})
target_link_libraries(ka_core PUBLIC Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Xml Qt6::PrintSupport Qt6::Network Qt6::Svg qgis_core qgis_gui gdal_i)
if (WIN32)
  target_link_libraries(ka_core PUBLIC crypt32)
endif()
if (MSVC)
  target_compile_options(ka_core PRIVATE /openmp /utf-8)
endif()
target_compile_definitions(ka_core PUBLIC KA_HGIS_HAS_QGIS=1 "CORE_EXPORT=__declspec(dllimport)" "GUI_EXPORT=__declspec(dllimport)")

set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
  "${CMAKE_SOURCE_DIR}/data/theme/ka-hgis.qss")
file(READ "${CMAKE_SOURCE_DIR}/data/theme/ka-hgis.qss" KA_QSS_CONTENT)
# CMake string lengths are UTF-8 byte counts. Bound each MSVC raw literal
# to 8 KiB and split only after a newline, preserving Korean byte sequences.
set(_ka_qss_remaining "${KA_QSS_CONTENT}")
set(KA_QSS_LITERALS "")
while(TRUE)
  string(LENGTH "${_ka_qss_remaining}" _ka_qss_length)
  if(_ka_qss_length GREATER 8192)
    string(SUBSTRING "${_ka_qss_remaining}" 0 8192 _ka_qss_prefix)
    string(FIND "${_ka_qss_prefix}" "\n" _ka_qss_split REVERSE)
    if(_ka_qss_split LESS 0)
      message(FATAL_ERROR "ka-hgis.qss has a line longer than 8192 UTF-8 bytes; wrap that line before embedding it.")
    endif()
    math(EXPR _ka_qss_chunk_length "${_ka_qss_split} + 1")
  else()
    set(_ka_qss_chunk_length ${_ka_qss_length})
  endif()
  string(SUBSTRING "${_ka_qss_remaining}" 0 ${_ka_qss_chunk_length} _ka_qss_chunk)
  string(FIND "${_ka_qss_chunk}" ")KAQSS\"" _ka_qss_delimiter)
  if(NOT _ka_qss_delimiter LESS 0)
    message(FATAL_ERROR "ka-hgis.qss contains the reserved raw-string delimiter )KAQSS\".")
  endif()
  string(APPEND KA_QSS_LITERALS "R\"KAQSS(${_ka_qss_chunk})KAQSS\"\n")
  if(_ka_qss_length LESS_EQUAL 8192)
    break()
  endif()
  string(SUBSTRING "${_ka_qss_remaining}" ${_ka_qss_chunk_length} -1 _ka_qss_remaining)
endwhile()
configure_file(
  "${CMAKE_SOURCE_DIR}/src/app/ka-hgis.qss.inc.in"
  "${CMAKE_BINARY_DIR}/generated/ka-hgis.qss.inc"
  @ONLY
  NEWLINE_STYLE UNIX
)

add_executable(ka-hgis
  data/theme/icons.qrc
  data/providers/ngii.qrc
  src/app/KaTopographicScopePanel.cpp
  src/app/KaTopographicScopePanel.h
  src/app/app.rc
  src/app/main.cpp
  src/app/MainWindow.cpp
    src/app/MainWindowContextMenus.cpp
    src/app/MainWindowUndo.cpp
    src/app/MainWindowDownloads.cpp
    src/app/MainWindowExport.cpp
    src/app/MainWindowEditing.cpp
    src/app/MainWindowSession.cpp
    src/app/MainWindowTopographic.cpp
    src/app/MainWindowCadastral.cpp
  src/app/KaLayerInformation.cpp
  src/app/KaLayerInformation.h
  src/app/KaTopographicBrowser.cpp
  src/app/KaTopographicBrowser.h
  src/app/KaHeritageBrowser.cpp
  src/app/KaHeritageBrowser.h
  src/app/KaHeritageSetupDialogs.cpp
  src/app/KaHeritageSetupDialogs.h
  src/app/KaTopographicAccountDialog.cpp
  src/app/KaTopographicAccountDialog.h
  src/app/KaTopographicImportDialog.cpp
  src/app/KaTopographicImportDialog.h
  src/app/KaBeginnerRibbon.cpp
  src/app/KaApplication.cpp
  src/app/KaStartupSplash.cpp
  src/app/KaStartupSplash.h
  src/app/KaSplashPalette.h
  src/app/KaSplashCredits.cpp
  src/app/KaSplashCredits.h
  src/app/KaSplashArt.cpp
  src/app/KaSplashArt.h
  src/app/KaDownloadUi.cpp
  src/app/KaDownloadUi.h
  src/app/KaReferenceDownloadJob.cpp
  src/app/KaTheme.cpp
  src/app/KaUserError.cpp
  src/app/KaIcons.cpp
  src/app/KaCaptureMapTool.cpp
  src/app/KaAttributeMapTool.cpp
  src/app/KaLayoutWindow.cpp
  src/app/KaDrawingStudio.cpp
  src/app/KaSectionDrawingStudio.cpp
  src/app/KaTerrain3dStudio.cpp
  src/app/KaTerrain3dLayoutStudio.cpp
  src/app/KaTerrain3dView.cpp
  src/app/KaAlignMapTool.cpp
  src/app/KaImageView.cpp
  src/app/KaStartPage.cpp
  src/app/KaStartHero.cpp
  src/app/KaAppBar.cpp
  src/app/KaMapControls.cpp
  src/app/KaCoordPointMapTool.cpp
  src/app/KaMeasureMapTool.cpp
  src/app/KaAboveLabelsOverlay.cpp
  src/app/KaCanvasGridOverlay.cpp
  src/app/KaTrenchMoveTool.cpp
  src/app/KaVertexEditTool.cpp
  src/app/KaFeatureSelectTool.cpp
  src/app/KaFoundLocationMark.cpp
  src/app/KaTrenchDialog.cpp
  src/app/KaDemClassDialog.cpp
  src/app/KaSurveyAreaDialog.cpp
  src/app/KaStatusBar.cpp
  src/app/KaCrashGuard.cpp
  src/app/KaGdalErrorLog.cpp
  src/app/KaRegionLocator.cpp
  src/app/KaFileBrowserPanel.cpp
  src/app/KaLayerOpacityRail.cpp
  src/app/KaSnapSettingsWidget.cpp
  src/app/KaSnapSettingsWidget.h
  src/app/KaFeatureFormDialog.cpp
  src/app/KaFeatureFormDialog.h
  src/app/MainWindow.h
  src/app/KaSurveyAreaDialog.h
  src/app/KaFileBrowserPanel.h
  src/app/KaLayerOpacityRail.h
  src/app/KaBeginnerRibbon.h
  src/app/KaApplication.h
  src/app/KaTheme.h
  src/app/KaUserError.h
  src/app/KaIcons.h
  src/app/KaCaptureMapTool.h
  src/app/KaAttributeMapTool.h
  src/app/KaLayoutWindow.h
  src/app/KaDrawingStudio.h
  src/app/KaSectionDrawingStudio.h
  src/app/KaTerrain3dStudio.h
  src/app/KaTerrain3dLayoutStudio.h
  src/app/KaTerrain3dView.h
  src/app/KaAlignMapTool.h
  src/app/KaImageView.h
  src/app/KaStartPage.h
  src/app/KaStartHero.h
  src/app/KaAppBar.h
  src/app/KaMapControls.h
  src/app/KaCoordPointMapTool.h
  src/app/KaMeasureMapTool.h
  src/app/KaAboveLabelsOverlay.h
  src/app/KaCanvasGridOverlay.h
  src/app/KaTrenchMoveTool.h
  src/app/KaVertexEditTool.h
  src/app/KaFeatureSelectTool.h
  src/app/KaFoundLocationMark.h
  src/app/KaTrenchDialog.h
  src/app/KaDemClassDialog.h
  src/app/KaStatusBar.h
  src/app/KaRegionLocator.h
  src/app/KaCrashGuard.h
  src/app/KaGdalErrorLog.h
)
target_include_directories(ka-hgis PRIVATE ${KA_INCLUDES} ${CMAKE_BINARY_DIR}/generated)
target_link_directories(ka-hgis PRIVATE ${KA_LIBDIRS})
target_link_libraries(ka-hgis PRIVATE ka_core Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Xml Qt6::PrintSupport Qt6::Network Qt6::Svg Qt6::WebEngineWidgets Qt6::Multimedia Qt6::MultimediaWidgets qgis_core qgis_gui)
target_compile_definitions(ka-hgis PRIVATE
  KA_HGIS_HAS_QGIS=1
  "CORE_EXPORT=__declspec(dllimport)"
  "GUI_EXPORT=__declspec(dllimport)"
  "KA_HGIS_VERSION=\"${PROJECT_VERSION}\"")
# Desktop GUI. QGIS/Qt debug text must not open a console behind the window.
set_target_properties(ka-hgis PROPERTIES WIN32_EXECUTABLE TRUE)

# 릴리스에도 PDB를 만들어 크래시 로그(KaCrashGuard)가 함수명·줄번호까지 심볼화하게 한다.
# /FS: 병렬 컴파일에서 같은 PDB에 안전하게 쓰도록(C1041 방지).
if(MSVC)
  target_compile_options(ka_core PRIVATE "$<$<CONFIG:Release>:/Zi>" "$<$<CONFIG:Release>:/FS>")
  target_compile_options(ka-hgis PRIVATE /utf-8 "$<$<CONFIG:Release>:/Zi>" "$<$<CONFIG:Release>:/FS>")
  target_link_options(ka-hgis PRIVATE
    "$<$<CONFIG:Release>:/DEBUG>" "$<$<CONFIG:Release>:/OPT:REF>" "$<$<CONFIG:Release>:/OPT:ICF>")
endif()


add_custom_command(TARGET ka-hgis POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:ka-hgis>/data/rules"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_SOURCE_DIR}/data/rules/drawing_checklist.v1.json"
    "$<TARGET_FILE_DIR:ka-hgis>/data/rules/drawing_checklist.v1.json"
  COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:ka-hgis>/data/styles"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_SOURCE_DIR}/data/styles/feature_presets.json"
    "$<TARGET_FILE_DIR:ka-hgis>/data/styles/feature_presets.json"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_SOURCE_DIR}/data/korea_dongs.json"
    "$<TARGET_FILE_DIR:ka-hgis>/data/korea_dongs.json"
  COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:ka-hgis>/data/theme"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_SOURCE_DIR}/data/theme/ka-hgis.qss"
    "$<TARGET_FILE_DIR:ka-hgis>/data/theme/ka-hgis.qss"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_SOURCE_DIR}/data/theme/ka-hgis.ico"
    "$<TARGET_FILE_DIR:ka-hgis>/data/theme/ka-hgis.ico"
  COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:ka-hgis>/data/fonts"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_SOURCE_DIR}/ka-hgis-실행.bat"
    "$<TARGET_FILE_DIR:ka-hgis>/ka-hgis-실행.bat"
)

if(KA_HGIS_BUILD_TESTS)
  enable_testing()
  # QtTest 결과는 이 Qt 빌드에서 표준출력으로 나오지 않는다. 래퍼로 실행해
  # 실패했을 때만 원인을 ctest 로그에 남긴다. 자세한 사정은 cmake/run_qtest.cmake 참고.
  function(ka_add_qtest test_name target_name)
    ka_add_qtest_filter(${test_name} ${target_name})
  endfunction()
  function(ka_add_qtest_filter test_name target_name)
    string(JOIN " " _ka_filter ${ARGN})
    add_test(NAME ${test_name} COMMAND ${CMAKE_COMMAND}
      -DKA_TEST_EXE=$<TARGET_FILE:${target_name}>
      -DKA_TEST_LOG=${CMAKE_BINARY_DIR}/test-logs/${test_name}.txt
      "-DKA_TEST_FILTER=${_ka_filter}"
      -P ${CMAKE_SOURCE_DIR}/cmake/run_qtest.cmake
      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
  endfunction()
  set(KA_TEST_DEFS KA_HGIS_HAS_QGIS=1 "CORE_EXPORT=__declspec(dllimport)" "GUI_EXPORT=__declspec(dllimport)")
  set(KA_TEST_LIBS ka_core Qt6::Test Qt6::Core Qt6::Xml Qt6::Gui Qt6::Widgets Qt6::PrintSupport qgis_core qgis_gui)

  add_executable(ka_hgis_tests tests/test_checklist.cpp)
  target_link_libraries(ka_hgis_tests PRIVATE ${KA_TEST_LIBS})
  target_include_directories(ka_hgis_tests PRIVATE ${KA_INCLUDES})
  target_link_directories(ka_hgis_tests PRIVATE ${KA_LIBDIRS})
  target_compile_definitions(ka_hgis_tests PRIVATE ${KA_TEST_DEFS})
  ka_add_qtest(checklist_engine ka_hgis_tests)

  add_executable(ka_workflow_tests tests/test_workflow.cpp
    src/app/KaCaptureMapTool.cpp src/app/KaCaptureMapTool.h
    src/app/KaCrashGuard.cpp src/app/KaCrashGuard.h)
  target_link_libraries(ka_workflow_tests PRIVATE ${KA_TEST_LIBS})
  target_include_directories(ka_workflow_tests PRIVATE ${KA_INCLUDES})
  target_link_directories(ka_workflow_tests PRIVATE ${KA_LIBDIRS})
  target_compile_definitions(ka_workflow_tests PRIVATE ${KA_TEST_DEFS})
  if (MSVC)
    target_compile_options(ka_workflow_tests PRIVATE /utf-8)
  endif()
  ka_add_qtest(workflow_engine ka_workflow_tests)

  add_executable(ka_georef_tests tests/test_georef.cpp)
  target_link_libraries(ka_georef_tests PRIVATE ${KA_TEST_LIBS})
  target_include_directories(ka_georef_tests PRIVATE ${KA_INCLUDES})
  target_link_directories(ka_georef_tests PRIVATE ${KA_LIBDIRS})
  target_compile_definitions(ka_georef_tests PRIVATE ${KA_TEST_DEFS})
  ka_add_qtest(georef_engine ka_georef_tests)

  add_executable(ka_preset_tests tests/test_presets.cpp)
  target_link_libraries(ka_preset_tests PRIVATE ${KA_TEST_LIBS})
  target_include_directories(ka_preset_tests PRIVATE ${KA_INCLUDES})
  target_link_directories(ka_preset_tests PRIVATE ${KA_LIBDIRS})
  target_compile_definitions(ka_preset_tests PRIVATE ${KA_TEST_DEFS})
  ka_add_qtest(feature_presets ka_preset_tests)

  add_executable(ka_buffer_tests tests/test_buffer.cpp)
  target_link_libraries(ka_buffer_tests PRIVATE ${KA_TEST_LIBS})
  target_include_directories(ka_buffer_tests PRIVATE ${KA_INCLUDES})
  target_link_directories(ka_buffer_tests PRIVATE ${KA_LIBDIRS})
  target_compile_definitions(ka_buffer_tests PRIVATE ${KA_TEST_DEFS})
  ka_add_qtest(buffer_ring ka_buffer_tests)

  add_executable(ka_measure_tests tests/test_measure.cpp)
  target_link_libraries(ka_measure_tests PRIVATE ${KA_TEST_LIBS})
  target_include_directories(ka_measure_tests PRIVATE ${KA_INCLUDES})
  target_link_directories(ka_measure_tests PRIVATE ${KA_LIBDIRS})
  target_compile_definitions(ka_measure_tests PRIVATE ${KA_TEST_DEFS})
  ka_add_qtest(measure_tape ka_measure_tests)

  add_executable(ka_dem_trench_tests tests/test_dem_trench.cpp)
  target_link_libraries(ka_dem_trench_tests PRIVATE ${KA_TEST_LIBS})
  target_include_directories(ka_dem_trench_tests PRIVATE ${KA_INCLUDES})
  target_link_directories(ka_dem_trench_tests PRIVATE ${KA_LIBDIRS})
  target_compile_definitions(ka_dem_trench_tests PRIVATE ${KA_TEST_DEFS})
  if (MSVC)
    target_compile_options(ka_dem_trench_tests PRIVATE /utf-8)
  endif()
  ka_add_qtest(dem_trench_engine ka_dem_trench_tests)

  add_executable(ka_section_layout_tests tests/test_section_layout.cpp)
  target_link_libraries(ka_section_layout_tests PRIVATE ${KA_TEST_LIBS})
  target_include_directories(ka_section_layout_tests PRIVATE ${KA_INCLUDES})
  target_link_directories(ka_section_layout_tests PRIVATE ${KA_LIBDIRS})
  target_compile_definitions(ka_section_layout_tests PRIVATE ${KA_TEST_DEFS})
  ka_add_qtest(section_layout_engine ka_section_layout_tests)

  add_executable(ka_terrain_3d_tests tests/test_terrain_3d.cpp)
  target_link_libraries(ka_terrain_3d_tests PRIVATE ${KA_TEST_LIBS})
  target_include_directories(ka_terrain_3d_tests PRIVATE ${KA_INCLUDES})
  target_link_directories(ka_terrain_3d_tests PRIVATE ${KA_LIBDIRS})
  target_compile_definitions(ka_terrain_3d_tests PRIVATE ${KA_TEST_DEFS})
  if (MSVC)
    target_compile_options(ka_terrain_3d_tests PRIVATE /utf-8)
  endif()
  ka_add_qtest(terrain_3d_engine ka_terrain_3d_tests)

  add_executable(ka_section_studio_tests
    tests/test_section_studio.cpp
    src/app/KaSectionDrawingStudio.cpp
    src/app/KaTheme.cpp
  )
  target_link_libraries(ka_section_studio_tests PRIVATE ${KA_TEST_LIBS})
  target_include_directories(ka_section_studio_tests PRIVATE ${KA_INCLUDES} ${CMAKE_BINARY_DIR}/generated)
  target_link_directories(ka_section_studio_tests PRIVATE ${KA_LIBDIRS})
  target_compile_definitions(ka_section_studio_tests PRIVATE ${KA_TEST_DEFS})
  ka_add_qtest(section_studio_engine ka_section_studio_tests)

  # Console subsystem, otherwise QtTest PASS/FAIL lines never reach ctest.
  set_target_properties(ka_hgis_tests ka_workflow_tests ka_georef_tests ka_preset_tests
                        ka_buffer_tests ka_measure_tests ka_dem_trench_tests
                        ka_section_layout_tests ka_section_studio_tests
                        ka_terrain_3d_tests
                        PROPERTIES WIN32_EXECUTABLE FALSE)


  add_executable(ka_theme_tests
    data/theme/icons.qrc
    tests/test_theme.cpp
    src/app/KaTheme.cpp
    src/app/KaBeginnerRibbon.cpp
    src/app/KaIcons.cpp
    src/app/KaAppBar.cpp
    src/app/KaSplashArt.cpp
  )
  set_target_properties(ka_theme_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  target_link_libraries(ka_theme_tests PRIVATE Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets)
  target_include_directories(ka_theme_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_BINARY_DIR}/generated
  )
  ka_add_qtest(theme_qss ka_theme_tests)

  add_executable(ka_user_error_tests
    tests/test_user_error.cpp
    src/app/KaUserError.cpp
  )
  set_target_properties(ka_user_error_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  target_link_libraries(ka_user_error_tests PRIVATE Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets)
  target_include_directories(ka_user_error_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
  ka_add_qtest(user_error ka_user_error_tests)

  add_executable(ka_region_locator_tests tests/test_region_locator.cpp
    src/app/KaRegionLocator.cpp src/app/KaBeginnerRibbon.cpp src/app/KaTheme.cpp
    src/core/KoreaRegionCatalog.cpp)
  set_target_properties(ka_region_locator_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  target_link_libraries(ka_region_locator_tests PRIVATE Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets)
  target_include_directories(ka_region_locator_tests PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_BINARY_DIR}/generated)
  ka_add_qtest(region_locator_popup ka_region_locator_tests)

  add_executable(ka_heritage_import_tests tests/test_heritage_import.cpp)
  add_executable(ka_gdal_log_tests tests/test_gdal_error_log.cpp
    src/app/KaGdalErrorLog.cpp src/app/KaGdalErrorLog.h src/app/KaCrashGuard.cpp src/app/KaCrashGuard.h)
  target_include_directories(ka_gdal_log_tests PRIVATE src ${KA_INCLUDES})
  target_link_directories(ka_gdal_log_tests PRIVATE ${KA_LIBDIRS})
  target_link_libraries(ka_gdal_log_tests PRIVATE ka_core Qt6::Test)
  set_target_properties(ka_gdal_log_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  ka_add_qtest(gdal_error_log ka_gdal_log_tests)
  add_executable(ka_catch_log_tests tests/test_catch_log.cpp src/core/KaSessionLog.cpp)
  set_target_properties(ka_catch_log_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  target_include_directories(ka_catch_log_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
  target_link_libraries(ka_catch_log_tests PRIVATE Qt6::Test Qt6::Core)
  ka_add_qtest(catch_log ka_catch_log_tests)
  add_executable(ka_export_survey_areas_tests tests/test_export_survey_areas.cpp)
  target_link_libraries(ka_export_survey_areas_tests PRIVATE ka_core Qt6::Test)
  set_target_properties(ka_export_survey_areas_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  ka_add_qtest(export_survey_areas ka_export_survey_areas_tests)
  get_target_property(KA_PERF_SOURCES ka-hgis SOURCES)
  list(REMOVE_ITEM KA_PERF_SOURCES src/app/main.cpp src/app/KaApplication.cpp src/app/app.rc)
  add_executable(ka_perf_tests tests/test_perf.cpp ${KA_PERF_SOURCES})
  set_target_properties(ka_perf_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  target_link_libraries(ka_perf_tests PRIVATE ka_core Qt6::Test Qt6::Svg Qt6::WebEngineWidgets)
  target_include_directories(ka_perf_tests PRIVATE ${KA_INCLUDES} ${CMAKE_BINARY_DIR}/generated)
  target_compile_options(ka_perf_tests PRIVATE /utf-8 /FS)
  ka_add_qtest(perf_engine ka_perf_tests)
  set_tests_properties(perf_engine PROPERTIES TIMEOUT 300)
  add_executable(ka_map_geotiff_export_tests tests/test_map_geotiff_export.cpp)
  target_link_libraries(ka_map_geotiff_export_tests PRIVATE ka_core Qt6::Test)
  set_target_properties(ka_map_geotiff_export_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  ka_add_qtest(map_geotiff_export ka_map_geotiff_export_tests)
  target_link_libraries(ka_heritage_import_tests PRIVATE ka_core Qt6::Test)
  set_target_properties(ka_heritage_import_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  ka_add_qtest(heritage_import ka_heritage_import_tests)
  add_executable(ka_survey_scope_clip_tests tests/test_survey_scope_clip.cpp)
  target_link_libraries(ka_survey_scope_clip_tests PRIVATE ka_core Qt6::Test)
  set_target_properties(ka_survey_scope_clip_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  ka_add_qtest(survey_scope_clip ka_survey_scope_clip_tests)
  add_executable(ka_heritage_download_retry_tests tests/test_heritage_download_retry.cpp
    src/app/KaHeritageBrowser.cpp src/app/KaHeritageBrowser.h)
  target_link_libraries(ka_heritage_download_retry_tests PRIVATE ka_core Qt6::Test Qt6::WebEngineWidgets)
  set_target_properties(ka_heritage_download_retry_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  ka_add_qtest(heritage_download_retry ka_heritage_download_retry_tests)
  add_executable(ka_heritage_form_tests tests/test_heritage_form.cpp)
  target_link_libraries(ka_heritage_form_tests PRIVATE ka_core Qt6::Test Qt6::Network)
  set_target_properties(ka_heritage_form_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  ka_add_qtest(heritage_form ka_heritage_form_tests)
  add_executable(ka_heritage_style_tests tests/test_heritage_style.cpp)
  target_link_libraries(ka_heritage_style_tests PRIVATE ka_core Qt6::Test)
  set_target_properties(ka_heritage_style_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  ka_add_qtest(heritage_style ka_heritage_style_tests)
  add_executable(ka_heritage_flow_tests tests/test_heritage_flow.cpp)
  target_link_libraries(ka_heritage_flow_tests PRIVATE ka_core Qt6::Test)
  set_target_properties(ka_heritage_flow_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  ka_add_qtest(heritage_flow ka_heritage_flow_tests)
  add_executable(ka_webengine_smoke_tests tests/test_webengine_smoke.cpp)
  set_target_properties(ka_webengine_smoke_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  target_link_libraries(ka_webengine_smoke_tests PRIVATE ka_core Qt6::Test Qt6::Widgets Qt6::WebEngineWidgets)
  ka_add_qtest(webengine_smoke ka_webengine_smoke_tests)
  add_executable(ka_heritage_region_tests tests/test_heritage_region.cpp)
  target_link_libraries(ka_heritage_region_tests PRIVATE ka_core Qt6::Test Qt6::Network)
  set_target_properties(ka_heritage_region_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  ka_add_qtest(heritage_region ka_heritage_region_tests)
  add_executable(ka_layer_state_tests tests/test_layer_state.cpp)
  add_executable(ka_layer_label_tests tests/test_layer_labels.cpp)
  target_link_libraries(ka_layer_label_tests PRIVATE ka_core Qt6::Test)
  ka_add_qtest(layer_labels ka_layer_label_tests)
  add_executable(ka_layer_information_tests tests/test_layer_information.cpp
    src/app/KaLayerInformation.cpp src/app/KaLayerInformation.h)
  target_link_libraries(ka_layer_information_tests PRIVATE ka_core Qt6::Test)
  ka_add_qtest(layer_information ka_layer_information_tests)
  set_tests_properties(layer_labels layer_information PROPERTIES TIMEOUT 90)
  set_target_properties(ka_layer_label_tests ka_layer_information_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  add_executable(ka_dem_legend_tests tests/test_dem_legend.cpp)
  add_executable(ka_topographic_catalog_tests tests/test_topographic_catalog.cpp)
  add_executable(ka_topographic_archive_tests tests/test_topographic_archive.cpp)
  add_executable(ka_topographic_source_crs_tests tests/test_topographic_source_crs.cpp)
  add_executable(ka_topographic_shapefile_tests tests/test_topographic_shapefile.cpp)
  target_link_libraries(ka_topographic_shapefile_tests PRIVATE ka_core Qt6::Test)
  set_target_properties(ka_topographic_shapefile_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  ka_add_qtest(topographic_shapefile ka_topographic_shapefile_tests)
  add_executable(ka_topographic_settings_tests tests/test_topographic_settings.cpp)
  target_link_libraries(ka_topographic_settings_tests PRIVATE ka_core Qt6::Test)
  set_target_properties(ka_topographic_settings_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  ka_add_qtest(topographic_settings ka_topographic_settings_tests)
  add_executable(ka_topographic_account_dialog_tests tests/test_topographic_account_dialog.cpp
    src/app/KaTopographicAccountDialog.cpp src/app/KaTopographicAccountDialog.h)
  target_link_libraries(ka_topographic_account_dialog_tests PRIVATE ka_core Qt6::Test)
  set_target_properties(ka_topographic_account_dialog_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  ka_add_qtest(topographic_account_dialog ka_topographic_account_dialog_tests)
  add_executable(ka_heritage_setup_dialogs_tests tests/test_heritage_setup_dialogs.cpp
    src/app/KaHeritageSetupDialogs.cpp src/app/KaHeritageSetupDialogs.h)
  target_link_libraries(ka_heritage_setup_dialogs_tests PRIVATE ka_core Qt6::Test)
  set_target_properties(ka_heritage_setup_dialogs_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  ka_add_qtest(heritage_setup_dialogs ka_heritage_setup_dialogs_tests)
  target_link_libraries(ka_topographic_source_crs_tests PRIVATE ka_core Qt6::Test)
  set_target_properties(ka_topographic_source_crs_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  ka_add_qtest(topographic_source_crs ka_topographic_source_crs_tests)
  add_executable(ka_topographic_sheets_tests tests/test_topographic_sheets.cpp)
  target_link_libraries(ka_topographic_sheets_tests PRIVATE ka_core Qt6::Test)
  set_target_properties(ka_topographic_sheets_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  ka_add_qtest(topographic_sheets ka_topographic_sheets_tests)
  target_link_libraries(ka_topographic_archive_tests PRIVATE ka_core Qt6::Test)
  set_target_properties(ka_topographic_archive_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  ka_add_qtest(topographic_archive ka_topographic_archive_tests)
  target_link_libraries(ka_topographic_catalog_tests PRIVATE ka_core Qt6::Test)
  ka_add_qtest(topographic_catalog ka_topographic_catalog_tests)
  add_executable(ka_topographic_browser_tests tests/test_topographic_browser.cpp
    data/providers/ngii.qrc
    src/app/KaTopographicBrowser.cpp src/app/KaTopographicBrowser.h)
  target_include_directories(ka_topographic_browser_tests PRIVATE src)
  set_target_properties(ka_topographic_browser_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  target_link_libraries(ka_topographic_browser_tests PRIVATE ka_core Qt6::Test Qt6::WebEngineWidgets Qt6::Network)
  if(MSVC)
    target_compile_options(ka_topographic_browser_tests PRIVATE /utf-8)
  endif()
  ka_add_qtest(topographic_browser ka_topographic_browser_tests)
  add_executable(ka_topographic_import_tests tests/test_topographic_import.cpp
    src/app/KaTopographicImportDialog.cpp src/app/KaTopographicImportDialog.h
    src/app/KaCrashGuard.cpp src/app/KaCrashGuard.h)
  target_link_libraries(ka_topographic_import_tests PRIVATE ka_core Qt6::Test)
  set_target_properties(ka_topographic_import_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  ka_add_qtest(topographic_import ka_topographic_import_tests)
  add_executable(ka_topographic_scope_tests tests/test_topographic_scope.cpp
    data/providers/ngii.qrc
    src/app/KaTopographicScopePanel.cpp src/app/KaTopographicScopePanel.h
    src/app/KaTopographicBrowser.cpp src/app/KaTopographicBrowser.h
    src/app/KaTopographicImportDialog.cpp src/app/KaTopographicImportDialog.h
    src/app/KaCrashGuard.cpp src/app/KaCrashGuard.h)
  target_link_libraries(ka_topographic_scope_tests PRIVATE ka_core Qt6::Test Qt6::WebEngineWidgets)
  set_target_properties(ka_topographic_scope_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  ka_add_qtest(topographic_scope ka_topographic_scope_tests)
  foreach(_ka_download_target IN ITEMS ka_heritage_download_retry_tests ka_topographic_browser_tests ka_topographic_scope_tests)
    target_sources(${_ka_download_target} PRIVATE src/app/KaDownloadUi.cpp src/app/KaDownloadUi.h)
  endforeach()

  add_executable(ka_startup_splash_tests tests/test_startup_splash.cpp
    data/theme/icons.qrc
    src/app/KaStartupSplash.cpp src/app/KaStartupSplash.h
    src/app/KaSplashPalette.h src/app/KaSplashCredits.cpp src/app/KaSplashCredits.h src/app/KaSplashArt.cpp src/app/KaSplashArt.h)
  target_include_directories(ka_startup_splash_tests PRIVATE src)
  target_link_libraries(ka_startup_splash_tests PRIVATE Qt6::Test Qt6::Widgets)
  set_target_properties(ka_startup_splash_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  ka_add_qtest(startup_splash ka_startup_splash_tests)
  set_tests_properties(startup_splash PROPERTIES TIMEOUT 45)

  add_executable(ka_download_ui_tests tests/test_download_ui.cpp
    src/app/KaDownloadUi.cpp src/app/KaDownloadUi.h
    src/app/KaTopographicBrowser.cpp src/app/KaTopographicBrowser.h
    src/app/KaHeritageBrowser.cpp src/app/KaHeritageBrowser.h)
  target_link_libraries(ka_download_ui_tests PRIVATE ka_core Qt6::Test Qt6::WebEngineWidgets)
  set_target_properties(ka_download_ui_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  ka_add_qtest(download_ui ka_download_ui_tests)
  set_tests_properties(download_ui PROPERTIES TIMEOUT 90)
  add_executable(ka_dem_rendering_tests tests/test_dem_rendering.cpp)
  add_executable(ka_dem_presentation_tests tests/test_dem_presentation.cpp src/app/KaDemClassDialog.cpp src/app/KaDemClassDialog.h)
  target_link_libraries(ka_dem_presentation_tests PRIVATE ka_core Qt6::Test)
  ka_add_qtest(dem_presentation ka_dem_presentation_tests)
  target_link_libraries(ka_dem_rendering_tests PRIVATE ka_core Qt6::Test)
  ka_add_qtest(dem_rendering ka_dem_rendering_tests)
  target_link_libraries(ka_dem_legend_tests PRIVATE ka_core Qt6::Test)
  ka_add_qtest(dem_legend ka_dem_legend_tests)
  set_target_properties(ka_layer_state_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  target_link_libraries(ka_layer_state_tests PRIVATE ka_core Qt6::Test)
  ka_add_qtest(layer_state_regressions ka_layer_state_tests)

  add_executable(ka_map_render_tests tests/test_map_render.cpp)
  set_target_properties(ka_map_render_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  target_link_libraries(ka_map_render_tests PRIVATE ka_core Qt6::Test Qt6::Network)
  ka_add_qtest(map_render ka_map_render_tests)
  add_executable(ka_parallel_render_tests tests/test_parallel_render.cpp)
  set_target_properties(ka_parallel_render_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  target_link_libraries(ka_parallel_render_tests PRIVATE ka_core Qt6::Test Qt6::Network)
  ka_add_qtest(parallel_render ka_parallel_render_tests)
  set_tests_properties(region_locator_popup layer_state_regressions map_render PROPERTIES TIMEOUT 60)
  set_tests_properties(parallel_render PROPERTIES TIMEOUT 90)

  add_executable(ka_hgis_above_labels_tests tests/test_above_labels.cpp src/app/KaAboveLabelsOverlay.cpp)
  set_target_properties(ka_hgis_above_labels_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  target_link_libraries(ka_hgis_above_labels_tests PRIVATE ka_core Qt6::Test)
  ka_add_qtest(above_labels ka_hgis_above_labels_tests)
  ka_add_qtest(above_labels_125 ka_hgis_above_labels_tests)
  ka_add_qtest(above_labels_150 ka_hgis_above_labels_tests)
  ka_add_qtest(above_labels_200 ka_hgis_above_labels_tests)
  set_tests_properties(above_labels above_labels_125 above_labels_150 above_labels_200 PROPERTIES TIMEOUT 60)

  add_executable(ka_recent_tests
    tests/test_recent.cpp
    src/core/RecentSurveys.cpp
  )
  set_target_properties(ka_recent_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  target_link_libraries(ka_recent_tests PRIVATE Qt6::Test Qt6::Core)
  target_include_directories(ka_recent_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
  ka_add_qtest(recent_surveys ka_recent_tests)

  add_executable(ka_network_tests tests/test_network_services.cpp)
  target_link_libraries(ka_network_tests PRIVATE ka_core Qt6::Test Qt6::Network)
  ka_add_qtest(network_services ka_network_tests)
  set_tests_properties(network_services PROPERTIES TIMEOUT 30)

  add_executable(ka_storage_safety_tests tests/test_storage_safety.cpp)
  target_link_libraries(ka_storage_safety_tests PRIVATE ka_core Qt6::Test)
  ka_add_qtest(storage_safety ka_storage_safety_tests)
  set_tests_properties(storage_safety PROPERTIES TIMEOUT 120)

  add_executable(ka_reference_download_tests tests/test_reference_download.cpp
    src/app/KaReferenceDownloadJob.cpp
    src/app/KaCrashGuard.cpp src/app/KaCrashGuard.h)
  target_link_libraries(ka_reference_download_tests PRIVATE ka_core Qt6::Test Qt6::Network)
  ka_add_qtest(reference_download ka_reference_download_tests)
  set_tests_properties(reference_download PROPERTIES TIMEOUT 60)

  add_executable(ka_cadastral_tests tests/test_cadastral.cpp)
  target_link_libraries(ka_cadastral_tests PRIVATE ka_core Qt6::Test Qt6::Network)
  ka_add_qtest(cadastral ka_cadastral_tests)
  set_tests_properties(cadastral PROPERTIES TIMEOUT 90)

  # Exercise the actual window's save/open slots, including project signal callbacks.
  get_target_property(KA_SAVE_OPEN_SOURCES ka-hgis SOURCES)
  list(REMOVE_ITEM KA_SAVE_OPEN_SOURCES src/app/main.cpp src/app/KaApplication.cpp src/app/app.rc)
  add_executable(ka_save_open_tests tests/test_save_open.cpp ${KA_SAVE_OPEN_SOURCES})
  set_target_properties(ka_save_open_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  target_link_libraries(ka_save_open_tests PRIVATE ka_core Qt6::Test Qt6::Svg Qt6::WebEngineWidgets)
  target_include_directories(ka_save_open_tests PRIVATE ${KA_INCLUDES} ${CMAKE_BINARY_DIR}/generated)
  target_compile_definitions(ka_save_open_tests PRIVATE "KA_HGIS_VERSION=\"${PROJECT_VERSION}\"")
  if(MSVC)
    target_compile_options(ka_save_open_tests PRIVATE /utf-8 /FS)
  endif()
  # One exe, several CTest names. QtTest function filter:
  # https://doc.qt.io/qt-6/qtest-overview.html
  ka_add_qtest_filter(save_open_window ka_save_open_tests
    compactLayerListKeepsHierarchyAndMapLabelSize
    layerInformationControlsIntegrateWithMapAndDrawingStudio
    geoTiffButtonOnlyEnabledOnMapTab
    windowGeometryFitsLogicalMonitors
    mainWindowFitsAvailableScreenOnShow
    homePageShowsStrataHeroWithoutOpeningASurvey
    mapControlsZoomTheCanvasAndShowAScaleBar
    drawingToolsGetTheirOwnRowBelowTheRibbon
    provinceChipMovesMapWithoutLoadingLayers)
  ka_add_qtest_filter(save_open_topo ka_save_open_tests
    topographicActualWindowRemainsStableAfterCompletion
    topographicDownloadKeepsMapVisibleAndUsesSurveyFolder)
  ka_add_qtest_filter(save_open_drawing ka_save_open_tests
    drawingStudio_decorationsMoveAndShrink
    drawingStudio_northArrowRendersOnOpen
    drawingStudio_fieldPageGrowsA4LandscapeByOneCentimetre
    drawingStudio_heritageRefreshRequestsCoalesceAndReuseOnRevisit
    drawingStudioAboveLabelsMapDrawsGeometryWithoutDuplicateNumberLabels
    drawingStudio_largeScaleChipsApplyToMapAndInput
    drawingInspectorUsesHeightWithoutOverlapping
    scaleControlFollowsActiveDrawingWithoutChangingMapCanvas
    scaleControl_isSingleWidgetAcceptingBothForms)
  ka_add_qtest_filter(save_open_edit ka_save_open_tests
    layerContextMenu_matchesLayerKind
    changingLabelFontKeepsEveryLayerAndItsVisibility
    trenchPresetRespectsAreaAndDimensions
    layerDeleteKeyPreservesSourceAndUndoRestoresPendingEdits
    layerDeleteKeyRemovesReferenceFromMap
    layerDeleteKeyRemovesCadastralAndKeepsItGone
    cadastralGroupDeleteKeyRemovesChildren
    surveyAreaDialogContinuesTheExistingArea
    cadastralGroupContextMenuCanRemove
    referenceGroupMenuRemovesEveryRowAndCtrlZRestores
    emptyGroupDeleteKeyRemovesRowAndCtrlZRestores
    ctrlZRestoresVertexEditsAndGroupedFeatureDeletion
    layerContextMenu_usesClickedRowAndLeavesSourceIntact
    layerContextMenu_moveToBottomPreservesRegisteredLayer
    mapContextMenu_onlyOffersRecordEditingForSurveyFeatures)
  ka_add_qtest_filter(save_open_roundtrip ka_save_open_tests
    saveAndReopen_keepsTreeGeometryAttributesAndStyle
    saveReopenSubmit_preservesWorkAndPackage
    save_keepsDrawnSurveyArea
    save_fieldPackageKeepsSurveyAreaReadable)
  ka_add_qtest_filter(save_open_open ka_save_open_tests
    open_repairsRegistryOnlyLayers
    startupHome_ignoresRememberedSurveyUntilUserOpensIt
    open_blocksNestedOpenAndAutosave
    emptyGpkgWithoutWorkspace_opensWithoutDomainLayers)
  ka_add_qtest_filter(save_open_open_invalid ka_save_open_tests
    invalidSurvey_doesNotReplaceCurrentWork
    openWhileDrawing_resetsCaptureBeforeReplacingLayers
    open_unsavedWorkRespectsAnswer)
  ka_add_qtest_filter(save_open_commit ka_save_open_tests
    partialCommit_preservesRemainingBufferAndCreatesIndependentRecovery
    failedCommit_doesNotOverwriteSavedWorkspace)
  ka_add_qtest_filter(save_open_saveas ka_save_open_tests
    newSurvey_selectedCrsSurvivesSaveAndOpen
    embeddedOpenRestoresDemInActualTree
    saveAs_preservesActualTablesAndReopensCopy
    newSurvey_existingNamePreservesUnsavedCurrentSurvey
    closeSave_preservesMemoryReferenceVectorOnReopen
    saveAs_embeddedFailureRestoresSourcesAndRemainsUnsaved
    saveAs_existingDestinationIsNotOverwritten
    save_qgisExceptionStaysInsideSlot
    save_companionFailureKeepsEmbeddedWorkspaceAndWarns
    fieldCopy_restoresAllValidLayersAndSavesTree)
  set(KA_SAVE_OPEN_TESTS
    save_open_window save_open_topo save_open_drawing
    save_open_edit save_open_roundtrip save_open_open
    save_open_open_invalid save_open_commit save_open_saveas)
  set_tests_properties(${KA_SAVE_OPEN_TESTS} PROPERTIES TIMEOUT 180 RESOURCE_LOCK ka_save_open)

  add_executable(ka_e2e_tests tests/test_e2e_opaque.cpp)
  set_target_properties(ka_e2e_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  target_link_libraries(ka_e2e_tests PRIVATE ${KA_TEST_LIBS})
  target_include_directories(ka_e2e_tests PRIVATE ${KA_INCLUDES})
  target_link_directories(ka_e2e_tests PRIVATE ${KA_LIBDIRS})
  target_compile_definitions(ka_e2e_tests PRIVATE ${KA_TEST_DEFS})
  if (MSVC)
    target_compile_options(ka_e2e_tests PRIVATE /utf-8 /W4)
  endif()
  ka_add_qtest(e2e_opaque_suite ka_e2e_tests)

  # Windows: ctest without OSGeo4W PATH → 0xc0000135 (DLL not found).
  # ENVIRONMENT is a CMake list, so unescaped ';' would truncate PATH to the
  # first bin dir. Escape path separators; keep ';' only between VAR=value.
  if(WIN32 AND EXISTS "${OSGEO4W_ROOT}/apps/qgis-dev/bin")
    set(_ka_path
      "${OSGEO4W_ROOT}/apps/qgis-dev/bin"
      "${OSGEO4W_ROOT}/apps/Qt6/bin"
      "${OSGEO4W_ROOT}/apps/gdal-dev/bin"
      "${OSGEO4W_ROOT}/apps/pdal-dev/bin"
      "${OSGEO4W_ROOT}/bin"
    )
    string(JOIN "\\;" _ka_path_esc ${_ka_path})
    set_tests_properties(layer_labels layer_information checklist_engine workflow_engine georef_engine feature_presets buffer_ring measure_tape dem_trench_engine theme_qss user_error recent_surveys section_layout_engine section_studio_engine terrain_3d_engine ${KA_SAVE_OPEN_TESTS} e2e_opaque_suite network_services storage_safety reference_download region_locator_popup layer_state_regressions map_render above_labels above_labels_125 above_labels_150 above_labels_200 PROPERTIES
      ENVIRONMENT "GDAL_DATA=${OSGEO4W_ROOT}/apps/gdal-dev/share/gdal;PATH=${_ka_path_esc};QGIS_PREFIX_PATH=${QGIS_PREFIX_PATH};QT_QPA_PLATFORM=offscreen")
    set_tests_properties(dem_legend PROPERTIES ENVIRONMENT "GDAL_DATA=${OSGEO4W_ROOT}/apps/gdal-dev/share/gdal;PATH=${_ka_path_esc};QGIS_PREFIX_PATH=${QGIS_PREFIX_PATH};QT_QPA_PLATFORM=offscreen" TIMEOUT 60)
    set_tests_properties(topographic_catalog PROPERTIES ENVIRONMENT "GDAL_DATA=${OSGEO4W_ROOT}/apps/gdal-dev/share/gdal;PATH=${_ka_path_esc};QGIS_PREFIX_PATH=${QGIS_PREFIX_PATH};QT_QPA_PLATFORM=offscreen" TIMEOUT 60)
    set_tests_properties(topographic_archive PROPERTIES ENVIRONMENT "GDAL_DATA=${OSGEO4W_ROOT}/apps/gdal-dev/share/gdal;PATH=${_ka_path_esc};QGIS_PREFIX_PATH=${QGIS_PREFIX_PATH};QT_QPA_PLATFORM=offscreen" TIMEOUT 90)
    set_tests_properties(topographic_sheets PROPERTIES ENVIRONMENT "GDAL_DATA=${OSGEO4W_ROOT}/apps/gdal-dev/share/gdal;PATH=${_ka_path_esc};QGIS_PREFIX_PATH=${QGIS_PREFIX_PATH};QT_QPA_PLATFORM=offscreen" TIMEOUT 60)
    set_tests_properties(topographic_source_crs PROPERTIES ENVIRONMENT "GDAL_DATA=${OSGEO4W_ROOT}/apps/gdal-dev/share/gdal;PATH=${_ka_path_esc};QGIS_PREFIX_PATH=${QGIS_PREFIX_PATH};QT_QPA_PLATFORM=offscreen" TIMEOUT 60)
    set_tests_properties(topographic_shapefile topographic_settings topographic_account_dialog heritage_setup_dialogs PROPERTIES ENVIRONMENT "GDAL_DATA=${OSGEO4W_ROOT}/apps/gdal-dev/share/gdal;PATH=${_ka_path_esc};QGIS_PREFIX_PATH=${QGIS_PREFIX_PATH};QT_QPA_PLATFORM=offscreen" TIMEOUT 90)
    set_tests_properties(topographic_scope PROPERTIES ENVIRONMENT "GDAL_DATA=${OSGEO4W_ROOT}/apps/gdal-dev/share/gdal;PATH=${_ka_path_esc};QGIS_PREFIX_PATH=${QGIS_PREFIX_PATH};QT_QPA_PLATFORM=offscreen;QTWEBENGINEPROCESS_PATH=${OSGEO4W_ROOT}/apps/Qt6/bin/QtWebEngineProcess.exe;QTWEBENGINE_RESOURCES_PATH=${OSGEO4W_ROOT}/apps/Qt6/resources;QTWEBENGINE_LOCALES_PATH=${OSGEO4W_ROOT}/apps/Qt6/translations/qtwebengine_locales" TIMEOUT 90)
    set_tests_properties(topographic_import PROPERTIES ENVIRONMENT "GDAL_DATA=${OSGEO4W_ROOT}/apps/gdal-dev/share/gdal;PATH=${_ka_path_esc};QGIS_PREFIX_PATH=${QGIS_PREFIX_PATH};QT_QPA_PLATFORM=offscreen" TIMEOUT 90)
    set_tests_properties(heritage_flow PROPERTIES ENVIRONMENT "PATH=${_ka_path_esc};QT_QPA_PLATFORM=offscreen" TIMEOUT 60)
    set_tests_properties(heritage_style PROPERTIES ENVIRONMENT "GDAL_DATA=${OSGEO4W_ROOT}/apps/gdal-dev/share/gdal;PATH=${_ka_path_esc};QGIS_PREFIX_PATH=${QGIS_PREFIX_PATH};QT_QPA_PLATFORM=offscreen" TIMEOUT 180)
    set_tests_properties(heritage_form PROPERTIES ENVIRONMENT "PATH=${_ka_path_esc};QT_QPA_PLATFORM=offscreen" TIMEOUT 60)
    set_tests_properties(heritage_download_retry PROPERTIES ENVIRONMENT "GDAL_DATA=${OSGEO4W_ROOT}/apps/gdal-dev/share/gdal;PATH=${_ka_path_esc};QGIS_PREFIX_PATH=${QGIS_PREFIX_PATH};QT_QPA_PLATFORM=offscreen;QTWEBENGINEPROCESS_PATH=${OSGEO4W_ROOT}/apps/Qt6/bin/QtWebEngineProcess.exe;QTWEBENGINE_RESOURCES_PATH=${OSGEO4W_ROOT}/apps/Qt6/resources;QTWEBENGINE_LOCALES_PATH=${OSGEO4W_ROOT}/apps/Qt6/translations/qtwebengine_locales" TIMEOUT 90)
    set_tests_properties(gdal_error_log PROPERTIES ENVIRONMENT "PATH=${_ka_path_esc};QT_QPA_PLATFORM=offscreen" TIMEOUT 60)
    set_tests_properties(catch_log PROPERTIES ENVIRONMENT "PATH=${_ka_path_esc};QT_QPA_PLATFORM=offscreen" TIMEOUT 60)
    set_tests_properties(cadastral PROPERTIES ENVIRONMENT "GDAL_DATA=${OSGEO4W_ROOT}/apps/gdal-dev/share/gdal;PROJ_LIB=${OSGEO4W_ROOT}/share/proj;PATH=${_ka_path_esc};QGIS_PREFIX_PATH=${QGIS_PREFIX_PATH};QT_QPA_PLATFORM=offscreen" TIMEOUT 90)
    set_tests_properties(export_survey_areas PROPERTIES ENVIRONMENT "GDAL_DATA=${OSGEO4W_ROOT}/apps/gdal-dev/share/gdal;PROJ_LIB=${OSGEO4W_ROOT}/share/proj;PATH=${_ka_path_esc};QGIS_PREFIX_PATH=${QGIS_PREFIX_PATH};QT_QPA_PLATFORM=offscreen" TIMEOUT 120)
    set_tests_properties(perf_engine PROPERTIES ENVIRONMENT "GDAL_DATA=${OSGEO4W_ROOT}/apps/gdal-dev/share/gdal;PROJ_LIB=${OSGEO4W_ROOT}/share/proj;PATH=${_ka_path_esc};QGIS_PREFIX_PATH=${QGIS_PREFIX_PATH};QT_QPA_PLATFORM=offscreen" TIMEOUT 300)
    set_tests_properties(parallel_render PROPERTIES ENVIRONMENT "GDAL_DATA=${OSGEO4W_ROOT}/apps/gdal-dev/share/gdal;PATH=${_ka_path_esc};QGIS_PREFIX_PATH=${QGIS_PREFIX_PATH};QT_QPA_PLATFORM=offscreen" TIMEOUT 90)
    set_tests_properties(heritage_import map_geotiff_export PROPERTIES ENVIRONMENT "GDAL_DATA=${OSGEO4W_ROOT}/apps/gdal-dev/share/gdal;PROJ_LIB=${OSGEO4W_ROOT}/share/proj;PATH=${_ka_path_esc};QGIS_PREFIX_PATH=${QGIS_PREFIX_PATH};QT_QPA_PLATFORM=offscreen" TIMEOUT 180)
    set_tests_properties(heritage_region PROPERTIES ENVIRONMENT "GDAL_DATA=${OSGEO4W_ROOT}/apps/gdal-dev/share/gdal;PROJ_LIB=${OSGEO4W_ROOT}/share/proj;PATH=${_ka_path_esc};QGIS_PREFIX_PATH=${QGIS_PREFIX_PATH};QT_QPA_PLATFORM=offscreen" TIMEOUT 60)
    set_tests_properties(webengine_smoke PROPERTIES ENVIRONMENT "PATH=${_ka_path_esc};QT_QPA_PLATFORM=offscreen;QTWEBENGINEPROCESS_PATH=${OSGEO4W_ROOT}/apps/Qt6/bin/QtWebEngineProcess.exe;QTWEBENGINE_RESOURCES_PATH=${OSGEO4W_ROOT}/apps/Qt6/resources;QTWEBENGINE_LOCALES_PATH=${OSGEO4W_ROOT}/apps/Qt6/translations/qtwebengine_locales" TIMEOUT 120)
    set_tests_properties(topographic_browser PROPERTIES ENVIRONMENT "GDAL_DATA=${OSGEO4W_ROOT}/apps/gdal-dev/share/gdal;PATH=${_ka_path_esc};QT_QPA_PLATFORM=offscreen;QTWEBENGINEPROCESS_PATH=${OSGEO4W_ROOT}/apps/Qt6/bin/QtWebEngineProcess.exe;QTWEBENGINE_RESOURCES_PATH=${OSGEO4W_ROOT}/apps/Qt6/resources;QTWEBENGINE_LOCALES_PATH=${OSGEO4W_ROOT}/apps/Qt6/translations/qtwebengine_locales" TIMEOUT 150)
    # 기본 검사는 로컬 HTTP fixture에서 전송·취소·로그인 세션을 검증한다.
    # 실제 사이트 3개 검사는 KA_HGIS_LIVE_NGII_* opt-in 없이는 skip한다.
    set_tests_properties(dem_rendering PROPERTIES ENVIRONMENT "GDAL_DATA=${OSGEO4W_ROOT}/apps/gdal-dev/share/gdal;PATH=${_ka_path_esc};QGIS_PREFIX_PATH=${QGIS_PREFIX_PATH};QT_QPA_PLATFORM=offscreen" TIMEOUT 180)
    set_tests_properties(dem_presentation PROPERTIES ENVIRONMENT "GDAL_DATA=${OSGEO4W_ROOT}/apps/gdal-dev/share/gdal;PATH=${_ka_path_esc};QGIS_PREFIX_PATH=${QGIS_PREFIX_PATH};QT_QPA_PLATFORM=offscreen" TIMEOUT 120)
    set_property(TEST above_labels APPEND PROPERTY ENVIRONMENT "QT_SCALE_FACTOR=1")
    set_property(TEST above_labels_125 APPEND PROPERTY ENVIRONMENT "QT_SCALE_FACTOR=1.25")
    set_property(TEST above_labels_150 APPEND PROPERTY ENVIRONMENT "QT_SCALE_FACTOR=1.5")
    set_property(TEST above_labels_200 APPEND PROPERTY ENVIRONMENT "QT_SCALE_FACTOR=2")
    # This qgis-dev uses a fixed qgis-project-XXXXXX.zip inside TEMP for project
    # archives. Separate test processes must never share that staging file.
    file(TO_CMAKE_PATH "$ENV{TEMP}" _ka_test_temp_root)
    if(NOT IS_DIRECTORY "${_ka_test_temp_root}")
      set(_ka_test_temp_root "${CMAKE_BINARY_DIR}/test-tmp")
    endif()
    string(SHA256 _ka_test_temp_id "${CMAKE_BINARY_DIR}")
    string(SUBSTRING "${_ka_test_temp_id}" 0 12 _ka_test_temp_id)
    foreach(_ka_test IN ITEMS checklist_engine workflow_engine georef_engine feature_presets
        buffer_ring measure_tape dem_trench_engine theme_qss recent_surveys section_layout_engine
        section_studio_engine terrain_3d_engine ${KA_SAVE_OPEN_TESTS} e2e_opaque_suite network_services
        storage_safety reference_download region_locator_popup layer_state_regressions map_render
        above_labels above_labels_125 above_labels_150 above_labels_200 dem_presentation
        export_survey_areas perf_engine parallel_render catch_log cadastral)
      set(_ka_test_temp "${_ka_test_temp_root}/ka-hgis-tests-${_ka_test_temp_id}/${_ka_test}")
      file(MAKE_DIRECTORY "${_ka_test_temp}")
      set_property(TEST ${_ka_test} APPEND PROPERTY ENVIRONMENT
        "TEMP=${_ka_test_temp}" "TMP=${_ka_test_temp}")
    endforeach()
  endif()
endif()
