cmake_minimum_required(VERSION 3.20)

project(WindTool VERSION 0.1.0 LANGUAGES CXX)

if(NOT WIN32)
    message(FATAL_ERROR "WindTool targets MikuMikuDance on 64-bit Windows.")
endif()

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "WindTool must be built for x64.")
endif()

option(WINDTOOL_BUILD_TESTS "Build WindTool test fixtures" ON)
option(WINDTOOL_BUILD_FORWARDER "Build the MMEffect forwarding loader" ON)
option(WINDTOOL_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

set(WINDTOOL_INCLUDE_DIR "${CMAKE_CURRENT_LIST_DIR}/include")
set(WINDTOOL_SDK_DIR "${CMAKE_CURRENT_LIST_DIR}/sdk")

function(windtool_configure_target target)
    target_compile_features(${target} PRIVATE cxx_std_17)
    target_include_directories(
        ${target}
        PRIVATE
            "${WINDTOOL_INCLUDE_DIR}"
            "${WINDTOOL_SDK_DIR}")

    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8)
        if(WINDTOOL_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE /WX)
        endif()
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
        if(WINDTOOL_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    endif()
endfunction()

add_library(
    windtool_core STATIC
    src/core.cpp
    src/physics_track.cpp
    src/track_json.cpp
    src/wind.cpp)
target_compile_features(windtool_core PUBLIC cxx_std_17)
target_include_directories(
    windtool_core
    PUBLIC
        "${WINDTOOL_INCLUDE_DIR}"
        "${WINDTOOL_SDK_DIR}")
windtool_configure_target(windtool_core)

add_library(MmdPhysicsControlStudio SHARED src/host_plugin.cpp)
windtool_configure_target(MmdPhysicsControlStudio)
target_compile_definitions(
    MmdPhysicsControlStudio
    PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
target_link_libraries(
    MmdPhysicsControlStudio
    PRIVATE windtool_core bcrypt comctl32 dwmapi uxtheme)
set_target_properties(
    MmdPhysicsControlStudio
    PROPERTIES PREFIX "" OUTPUT_NAME "MmdPhysicsControlStudio")

if(WINDTOOL_BUILD_FORWARDER)
    add_library(
        PhysicsMMEffectForwarder SHARED
        forwarder/physics_mme_forwarder.cpp
        forwarder/mme_forwarder.def)
    windtool_configure_target(PhysicsMMEffectForwarder)
    target_compile_definitions(
        PhysicsMMEffectForwarder PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
    set_target_properties(
        PhysicsMMEffectForwarder
        PROPERTIES PREFIX "" OUTPUT_NAME "MMEffect")
endif()

if(MINGW)
    target_link_options(
        MmdPhysicsControlStudio PRIVATE -static -static-libgcc -static-libstdc++)
    if(WINDTOOL_BUILD_FORWARDER)
        target_link_options(
            PhysicsMMEffectForwarder
            PRIVATE -static -static-libgcc -static-libstdc++)
    endif()
endif()

if(WINDTOOL_BUILD_TESTS)
    include(CTest)

    add_library(MmdPhysicsControlStudioSurrogate SHARED src/host_plugin.cpp)
    windtool_configure_target(MmdPhysicsControlStudioSurrogate)
    target_compile_definitions(
        MmdPhysicsControlStudioSurrogate
        PRIVATE
            WIN32_LEAN_AND_MEAN
            NOMINMAX
            UNICODE
            _UNICODE
            PCS_SURROGATE_HOST)
    target_link_libraries(
        MmdPhysicsControlStudioSurrogate
        PRIVATE windtool_core bcrypt comctl32 dwmapi uxtheme)
    set_target_properties(
        MmdPhysicsControlStudioSurrogate
        PROPERTIES PREFIX "" OUTPUT_NAME "MmdPhysicsControlStudioSurrogate")

    add_executable(windtool_core_fixture tests/physics_control_fixture.cpp)
    windtool_configure_target(windtool_core_fixture)
    target_link_libraries(windtool_core_fixture PRIVATE windtool_core)

    add_executable(
        windtool_host_contract_fixture
        tests/physics_host_contract_fixture.cpp)
    windtool_configure_target(windtool_host_contract_fixture)

    add_executable(
        windtool_ui_surrogate
        tests/physics_inspector_surrogate.cpp)
    windtool_configure_target(windtool_ui_surrogate)

    if(MINGW)
        target_link_options(
            MmdPhysicsControlStudioSurrogate
            PRIVATE -static -static-libgcc -static-libstdc++)
        target_link_options(
            windtool_host_contract_fixture
            PRIVATE -static -static-libgcc -static-libstdc++)
        target_link_options(
            windtool_ui_surrogate
            PRIVATE -static -static-libgcc -static-libstdc++)
    endif()

    add_test(NAME windtool_core_fixture COMMAND windtool_core_fixture)
    add_test(
        NAME windtool_host_contract_fixture
        COMMAND windtool_host_contract_fixture $<TARGET_FILE:MmdPhysicsControlStudio>)
    add_test(
        NAME windtool_ui_surrogate
        COMMAND windtool_ui_surrogate $<TARGET_FILE:MmdPhysicsControlStudioSurrogate>)
endif()

install(
    TARGETS MmdPhysicsControlStudio
    RUNTIME DESTINATION PhysicsControlStudio
    LIBRARY DESTINATION PhysicsControlStudio)

if(WINDTOOL_BUILD_FORWARDER)
    install(
        TARGETS PhysicsMMEffectForwarder
        RUNTIME DESTINATION .
        LIBRARY DESTINATION .)
endif()
