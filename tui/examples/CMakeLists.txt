set(CONFIGWIZARD_FFI_CRATE ${CMAKE_CURRENT_SOURCE_DIR}/../../ffi)
set(CONFIGWIZARD_UI_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../..)
set(CONFIGWIZARD_CARGO_TARGET_DIR ${CONFIGWIZARD_UI_ROOT}/target)

if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR NOT CMAKE_BUILD_TYPE)
    set(CONFIGWIZARD_FFI_PROFILE "")
    set(CONFIGWIZARD_FFI_LIB_DIR ${CONFIGWIZARD_CARGO_TARGET_DIR}/debug)
else()
    set(CONFIGWIZARD_FFI_PROFILE "--release")
    set(CONFIGWIZARD_FFI_LIB_DIR ${CONFIGWIZARD_CARGO_TARGET_DIR}/release)
endif()

set(CONFIGWIZARD_FFI_LIB ${CONFIGWIZARD_FFI_LIB_DIR}/libconfigwizard_ffi.a)

add_custom_command(
    OUTPUT ${CONFIGWIZARD_FFI_LIB}
    COMMAND ${CMAKE_COMMAND} -E env CARGO_TARGET_DIR=${CONFIGWIZARD_CARGO_TARGET_DIR}
        cargo build ${CONFIGWIZARD_FFI_PROFILE} --manifest-path ${CONFIGWIZARD_FFI_CRATE}/Cargo.toml
    WORKING_DIRECTORY ${CONFIGWIZARD_UI_ROOT}
    DEPENDS
        ${CONFIGWIZARD_FFI_CRATE}/src/lib.rs
        ${CONFIGWIZARD_FFI_CRATE}/Cargo.toml
    COMMENT "Building configwizard-ffi"
    VERBATIM
)

add_custom_target(configwizard_ffi DEPENDS ${CONFIGWIZARD_FFI_LIB})

add_executable(configwizard-tui-demo
    demo.cpp
    demo_loader.cpp
)

add_dependencies(configwizard-tui-demo configwizard_ffi)

target_include_directories(configwizard-tui-demo PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CONFIGWIZARD_FFI_CRATE}/include
)

target_link_libraries(configwizard-tui-demo PRIVATE
    configwizard_ui_tui
    nlohmann_json::nlohmann_json
    ${CONFIGWIZARD_FFI_LIB}
    dl
    m
    pthread
)
