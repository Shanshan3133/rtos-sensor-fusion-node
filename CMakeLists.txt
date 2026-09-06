cmake_minimum_required(VERSION 3.20)
project(sensor_node_core C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_library(sensor_node_core STATIC
    core/calibration.c
    core/fusion.c
    core/health_monitor.c
    core/telemetry.c
    drivers/sensor_codec.c
)
target_include_directories(sensor_node_core PUBLIC include)
if(NOT MSVC)
    target_compile_options(sensor_node_core PRIVATE -Wall -Wextra -Wpedantic)
    target_link_libraries(sensor_node_core PUBLIC m)
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(core_tests tests/test_core.c)
    target_link_libraries(core_tests PRIVATE sensor_node_core)
    add_test(NAME core_tests COMMAND core_tests)
endif()
