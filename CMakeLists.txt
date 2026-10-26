cmake_minimum_required(VERSION 3.21)
project(ReqFlowAI VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 6.4 REQUIRED COMPONENTS Core Widgets Network Sql)

if(ANDROID)
    # yaml-cpp is built for the selected Android ABI as part of this cross-build.
    include(FetchContent)
    set(YAML_CPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(YAML_CPP_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
    set(YAML_CPP_INSTALL OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(yaml-cpp
        URL https://github.com/jbeder/yaml-cpp/archive/refs/tags/0.8.0.tar.gz
        # B5: el tarball se verifica por hash para no depender de un contenido
        # mutable servido bajo la misma URL. Hash calculado con sha256sum del
        # tarball descargado (1 017 151 bytes, gzip verificado con gzip -t).
        URL_HASH SHA256=fbe74bbdcee21d656715688706da3c8becfd946d92cd44705cc6098bb23b3a16
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
    FetchContent_MakeAvailable(yaml-cpp)
else()
    find_package(yaml-cpp REQUIRED)
endif()

find_program(CARGO_EXECUTABLE cargo REQUIRED)

set(RUST_BACKEND_DIR "${CMAKE_CURRENT_SOURCE_DIR}/backend")
set(RUST_BACKEND_TARGET_DIR "${CMAKE_CURRENT_BINARY_DIR}/rust-target")
set(RUST_CARGO_TARGET_ARGS "")
set(RUST_CARGO_COMMAND "${CARGO_EXECUTABLE}")
set(RUST_ANDROID_LINKER_ENV "")
if(ANDROID)
    if(CMAKE_ANDROID_ARCH_ABI STREQUAL "arm64-v8a")
        set(RUST_ANDROID_TARGET "aarch64-linux-android")
        set(RUST_ANDROID_LINKER "${CMAKE_ANDROID_NDK}/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android28-clang")
        set(RUST_ANDROID_LINKER_ENV "CARGO_TARGET_AARCH64_LINUX_ANDROID_LINKER=${RUST_ANDROID_LINKER}")
    elseif(CMAKE_ANDROID_ARCH_ABI STREQUAL "armeabi-v7a")
        set(RUST_ANDROID_TARGET "armv7-linux-androideabi")
        set(RUST_ANDROID_LINKER "${CMAKE_ANDROID_NDK}/toolchains/llvm/prebuilt/linux-x86_64/bin/armv7a-linux-androideabi28-clang")
        set(RUST_ANDROID_LINKER_ENV "CARGO_TARGET_ARMV7_LINUX_ANDROIDEABI_LINKER=${RUST_ANDROID_LINKER}")
    else()
        message(FATAL_ERROR "Unsupported Android ABI for Rust backend: ${CMAKE_ANDROID_ARCH_ABI}")
    endif()
    set(RUST_CARGO_TARGET_ARGS --target ${RUST_ANDROID_TARGET})
    set(RUST_CARGO_COMMAND ${CMAKE_COMMAND} -E env "${RUST_ANDROID_LINKER_ENV}" "${CARGO_EXECUTABLE}")
    set(RUST_BACKEND_LIBRARY "${RUST_BACKEND_TARGET_DIR}/${RUST_ANDROID_TARGET}/release/${CMAKE_STATIC_LIBRARY_PREFIX}reqflow_backend${CMAKE_STATIC_LIBRARY_SUFFIX}")
else()
    set(RUST_BACKEND_LIBRARY "${RUST_BACKEND_TARGET_DIR}/release/${CMAKE_STATIC_LIBRARY_PREFIX}reqflow_backend${CMAKE_STATIC_LIBRARY_SUFFIX}")
endif()
file(GLOB_RECURSE RUST_BACKEND_SOURCES CONFIGURE_DEPENDS "${RUST_BACKEND_DIR}/src/*.rs")
add_custom_command(
    OUTPUT "${RUST_BACKEND_LIBRARY}"
    COMMAND ${RUST_CARGO_COMMAND} build --manifest-path "${RUST_BACKEND_DIR}/Cargo.toml" --release ${RUST_CARGO_TARGET_ARGS} --target-dir "${RUST_BACKEND_TARGET_DIR}"
    DEPENDS "${RUST_BACKEND_DIR}/Cargo.toml" ${RUST_BACKEND_SOURCES}
    WORKING_DIRECTORY "${RUST_BACKEND_DIR}"
    COMMENT "Building ReqFlow AI Rust backend${RUST_ANDROID_TARGET}"
    VERBATIM
)
add_custom_target(reqflow-rust-backend DEPENDS "${RUST_BACKEND_LIBRARY}")

qt_add_executable(reqflow-ai
    src/main.cpp
    src/network/httpclient.cpp
    src/data/projectstore.cpp
    src/data/environment.cpp
    src/data/rustbackend.cpp
    src/flow/flowengine.cpp
    src/flow/floweditor.cpp
    src/openapi/openapiimporter.cpp
    src/openapi/yaml.cpp
    src/testing/assertions.cpp
    src/testing/jsondiff.cpp
    src/ui/mainwindow.cpp
    src/ui/authdialog.cpp
    include/reqflow/network/httpclient.h
    include/reqflow/data/projectstore.h
    include/reqflow/data/environment.h
    include/reqflow/data/rustbackend.h
    include/reqflow/flow/flowengine.h
    include/reqflow/flow/floweditor.h
    include/reqflow/flow/flowgraphics.h
    include/reqflow/openapi/openapiimporter.h
    include/reqflow/openapi/yaml.h
    include/reqflow/security/secrets.h
    include/reqflow/testing/assertions.h
    include/reqflow/testing/jsondiff.h
    include/reqflow/ui/mainwindow.h
    include/reqflow/ui/authdialog.h
)
target_include_directories(reqflow-ai PRIVATE include)
add_dependencies(reqflow-ai reqflow-rust-backend)
target_link_libraries(reqflow-ai PRIVATE Qt6::Core Qt6::Widgets Qt6::Network Qt6::Sql yaml-cpp::yaml-cpp "${RUST_BACKEND_LIBRARY}")
if(UNIX AND NOT APPLE AND NOT ANDROID)
    target_link_libraries(reqflow-ai PRIVATE dl pthread m)
endif()
if(ANDROID)
    set_property(TARGET reqflow-ai PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
    set_property(TARGET reqflow-ai PROPERTY QT_ANDROID_PACKAGE_NAME "org.reqflow.ai")
    set_property(TARGET reqflow-ai PROPERTY QT_ANDROID_VERSION_CODE 1)
    set_property(TARGET reqflow-ai PROPERTY QT_ANDROID_VERSION_NAME "${PROJECT_VERSION}")
endif()

include(CTest)
if(BUILD_TESTING AND NOT ANDROID)
    find_package(Qt6 6.4 REQUIRED COMPONENTS Test)
    if(TARGET Qt6::Test)
        add_executable(reqflow-core-tests
            tests/cpp/test_core.cpp src/network/httpclient.cpp src/data/projectstore.cpp
            src/data/environment.cpp src/data/rustbackend.cpp src/flow/flowengine.cpp
            src/openapi/openapiimporter.cpp src/openapi/yaml.cpp src/testing/assertions.cpp
            src/testing/jsondiff.cpp include/reqflow/testing/assertions.h
            include/reqflow/testing/jsondiff.h include/reqflow/flow/flowengine.h
            include/reqflow/openapi/openapiimporter.h include/reqflow/openapi/yaml.h
            include/reqflow/network/httpclient.h include/reqflow/data/projectstore.h
            include/reqflow/data/environment.h include/reqflow/security/secrets.h)
        target_include_directories(reqflow-core-tests PRIVATE include)
        add_dependencies(reqflow-core-tests reqflow-rust-backend)
        target_link_libraries(reqflow-core-tests PRIVATE Qt6::Core Qt6::Network Qt6::Sql Qt6::Test yaml-cpp::yaml-cpp "${RUST_BACKEND_LIBRARY}")
        if(UNIX AND NOT APPLE)
            target_link_libraries(reqflow-core-tests PRIVATE dl pthread m)
        endif()
        add_test(NAME reqflow-core-tests COMMAND reqflow-core-tests)
    endif()
endif()
