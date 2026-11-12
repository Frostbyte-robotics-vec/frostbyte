cmake_minimum_required(VERSION 3.20)
project(frostbyte_tests CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

get_filename_component(FB_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)

# lib/ und hal/sim: reine Logik ohne PROS-Header. Solange es noch keine .cpp gibt, reicht eine Header-Bibliothek.
file(GLOB_RECURSE FB_SOURCES CONFIGURE_DEPENDS "${FB_ROOT}/lib/*.cpp" "${FB_ROOT}/hal/sim/*.cpp")
if(FB_SOURCES)
    add_library(frostbyte_lib STATIC ${FB_SOURCES})
else()
    add_library(frostbyte_lib INTERFACE)
endif()
if(FB_SOURCES)
    set(FB_SCOPE PUBLIC)
else()
    set(FB_SCOPE INTERFACE)
endif()
target_include_directories(frostbyte_lib ${FB_SCOPE} "${FB_ROOT}/lib" "${FB_ROOT}")

# sim/: Welt der Simulation (nur Laptop, ADR-007). sim/main.cpp ist das Programm frostbyte_sim.
file(GLOB FB_SIM_SOURCES CONFIGURE_DEPENDS "${FB_ROOT}/sim/*.cpp")
list(REMOVE_ITEM FB_SIM_SOURCES "${FB_ROOT}/sim/main.cpp")
add_library(frostbyte_simlib STATIC ${FB_SIM_SOURCES})
target_link_libraries(frostbyte_simlib PUBLIC frostbyte_lib)
target_include_directories(frostbyte_simlib PUBLIC "${FB_ROOT}/lib" "${FB_ROOT}")

# Git-Hash für die Kopfzeile der Logs (build=…); ohne git: "-".
execute_process(COMMAND git rev-parse --short HEAD WORKING_DIRECTORY "${FB_ROOT}"
                OUTPUT_VARIABLE FB_BUILD OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
if(NOT FB_BUILD)
    set(FB_BUILD "-")
endif()
add_executable(frostbyte_sim "${FB_ROOT}/sim/main.cpp")
target_link_libraries(frostbyte_sim PRIVATE frostbyte_simlib)
target_compile_definitions(frostbyte_sim PRIVATE FB_BUILD="${FB_BUILD}")

# Erzeugter Routen-Header für test_route_parser.cpp (ADR-023); #include "gen/test_square_route.hpp"
set(FB_GEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/gen_include")
add_custom_command(
    OUTPUT "${FB_GEN_DIR}/gen/test_square_route.hpp"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${FB_GEN_DIR}/gen"
    COMMAND frostbyte_route_export "${FB_ROOT}/field/field.json" "${FB_ROOT}/routes/test_square.route" kRouteTestSquare
            "${FB_GEN_DIR}/gen/test_square_route.hpp"
    DEPENDS frostbyte_route_export "${FB_ROOT}/routes/test_square.route" "${FB_ROOT}/field/field.json"
    VERBATIM)
file(GLOB FB_TESTS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/test_*.cpp")
add_executable(frostbyte_tests main.cpp ${FB_TESTS} "${FB_GEN_DIR}/gen/test_square_route.hpp")
target_include_directories(frostbyte_tests PRIVATE "${FB_GEN_DIR}")
target_include_directories(frostbyte_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/third_party" "${FB_ROOT}/tools")
target_link_libraries(frostbyte_tests PRIVATE frostbyte_lib frostbyte_simlib)
target_compile_definitions(frostbyte_tests PRIVATE FB_TEST_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
                                                   FB_ROOT_DIR="${FB_ROOT}")
# docs/controls.md aus config/ erzeugen: frostbyte_controls_export docs/controls.md
add_executable(frostbyte_controls_export "${FB_ROOT}/tools/controls_export.cpp")
target_link_libraries(frostbyte_controls_export PRIVATE frostbyte_lib)
target_include_directories(frostbyte_controls_export PRIVATE "${FB_ROOT}/tools")

# Route -> C++ für den Roboter: tools/route2cpp.py ruft frostbyte_route_export (ADR-023)
add_executable(frostbyte_route_export "${FB_ROOT}/tools/route_export.cpp")
target_link_libraries(frostbyte_route_export PRIVATE frostbyte_simlib)

foreach(t frostbyte_route_export frostbyte_controls_export frostbyte_tests frostbyte_simlib frostbyte_sim)
    if(MSVC)
        target_compile_options(${t} PRIVATE /W4)
    else()
        target_compile_options(${t} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endforeach()

enable_testing()
add_test(NAME unit COMMAND frostbyte_tests)
add_test(NAME sim_cli_determinism
         COMMAND ${CMAKE_COMMAND} -DSIM=$<TARGET_FILE:frostbyte_sim> -DFIXTURES=${CMAKE_CURRENT_SOURCE_DIR}/fixtures
                 -DOUT=${CMAKE_CURRENT_BINARY_DIR}/cli_determinism -P ${CMAKE_CURRENT_SOURCE_DIR}/cli_determinism.cmake)
