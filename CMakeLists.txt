cmake_minimum_required(VERSION 3.20)
project(rtos_spectrum_analyzer_core C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_library(spectrum_analyzer_core STATIC
    core/health_monitor.c
    core/spectrum.c
    core/telemetry.c
)
target_include_directories(spectrum_analyzer_core PUBLIC include)
if(NOT MSVC)
    target_compile_options(spectrum_analyzer_core PRIVATE -Wall -Wextra -Wpedantic)
    target_link_libraries(spectrum_analyzer_core PUBLIC m)
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(core_tests tests/test_core.c)
    target_link_libraries(core_tests PRIVATE spectrum_analyzer_core)
    add_test(NAME core_tests COMMAND core_tests)
endif()
