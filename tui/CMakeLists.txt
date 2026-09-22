cmake_minimum_required(VERSION 3.16)
project(configwizard-ui-tui VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT TUINATOR_ROOT)
    set(TUINATOR_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../../Coditary/core/Tuinator")
endif()

if(NOT EXISTS "${TUINATOR_ROOT}/CMakeLists.txt")
    message(FATAL_ERROR "Tuinator library not found at ${TUINATOR_ROOT}. Set -DTUINATOR_ROOT=...")
endif()

set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
add_subdirectory("${TUINATOR_ROOT}" "${CMAKE_BINARY_DIR}/tuinator-core" EXCLUDE_FROM_ALL)

add_library(configwizard_ui_tui
    src/choice_select.cpp
    src/fuzzy_select.cpp
    src/plain_frontend.cpp
    src/scrollbar.cpp
    src/theme.cpp
    src/tuinator_wizard.cpp
)

add_library(configwizard_ui::tui ALIAS configwizard_ui_tui)

target_include_directories(configwizard_ui_tui
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(configwizard_ui_tui PUBLIC tuinator)

option(CONFIGWIZARD_UI_TUI_BUILD_TESTS "Build configwizard-ui-tui tests" ON)
if(CONFIGWIZARD_UI_TUI_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

option(CONFIGWIZARD_UI_TUI_BUILD_EXAMPLES "Build configwizard-ui-tui examples" ON)
if(CONFIGWIZARD_UI_TUI_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()
