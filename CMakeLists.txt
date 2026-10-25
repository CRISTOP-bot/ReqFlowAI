cmake_minimum_required(VERSION 3.21)
project(ReqFlowAI VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 6.4 REQUIRED COMPONENTS Core Widgets Network Sql)
find_package(yaml-cpp REQUIRED)
find_program(CARGO_EXECUTABLE cargo REQUIRED)

set(RUST_BACKEND_DIR "${CMAKE_CURRENT_SOURCE_DIR}/backend")
set(RUST_BACKEND_TARGET_DIR "${CMAKE_CURRENT_BINARY_DIR}/rust-target")
set(RUST_BACKEND_LIBRARY "${RUST_BACKEND_TARGET_DIR}/release/${CMAKE_STATIC_LIBRARY_PREFIX}reqflow_backend${CMAKE_STATIC_LIBRARY_SUFFIX}")
file(GLOB_RECURSE RUST_BACKEND_SOURCES CONFIGURE_DEPENDS "${RUST_BACKEND_DIR}/src/*.rs")
add_custom_command(
    OUTPUT "${RUST_BACKEND_LIBRARY}"
    COMMAND "${CARGO_EXECUTABLE}" build --manifest-path "${RUST_BACKEND_DIR}/Cargo.toml" --release --target-dir "${RUST_BACKEND_TARGET_DIR}"
    DEPENDS "${RUST_BACKEND_DIR}/Cargo.toml" ${RUST_BACKEND_SOURCES}
    WORKING_DIRECTORY "${RUST_BACKEND_DIR}"
    COMMENT "Building ReqFlow AI Rust backend"
    VERBATIM
)
add_custom_target(reqflow-rust-backend DEPENDS "${RUST_BACKEND_LIBRARY}")

add_executable(reqflow-ai
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
    include/reqflow/testing/assertions.h
    include/reqflow/testing/jsondiff.h
    include/reqflow/ui/mainwindow.h
    include/reqflow/ui/authdialog.h
)
target_include_directories(reqflow-ai PRIVATE include)
add_dependencies(reqflow-ai reqflow-rust-backend)
target_link_libraries(reqflow-ai PRIVATE Qt6::Core Qt6::Widgets Qt6::Network Qt6::Sql yaml-cpp "${RUST_BACKEND_LIBRARY}")
if(UNIX AND NOT APPLE)
    target_link_libraries(reqflow-ai PRIVATE dl pthread m)
endif()

include(CTest)
if(BUILD_TESTING)
    find_package(Qt6 6.4 REQUIRED COMPONENTS Test)
    if(TARGET Qt6::Test)
        add_executable(reqflow-core-tests
            tests/cpp/test_core.cpp
            src/network/httpclient.cpp
            src/data/projectstore.cpp
            src/data/environment.cpp
            src/data/rustbackend.cpp
            src/flow/flowengine.cpp
            src/openapi/openapiimporter.cpp
            src/openapi/yaml.cpp
            src/testing/assertions.cpp
            src/testing/jsondiff.cpp
            include/reqflow/testing/assertions.h
            include/reqflow/testing/jsondiff.h
            include/reqflow/flow/flowengine.h
            include/reqflow/openapi/openapiimporter.h
            include/reqflow/openapi/yaml.h
            include/reqflow/network/httpclient.h
            include/reqflow/data/projectstore.h
            include/reqflow/data/environment.h
        )
        target_include_directories(reqflow-core-tests PRIVATE include)
        add_dependencies(reqflow-core-tests reqflow-rust-backend)
        target_link_libraries(reqflow-core-tests PRIVATE Qt6::Core Qt6::Network Qt6::Sql Qt6::Test yaml-cpp "${RUST_BACKEND_LIBRARY}")
        if(UNIX AND NOT APPLE)
            target_link_libraries(reqflow-core-tests PRIVATE dl pthread m)
        endif()
        add_test(NAME reqflow-core-tests COMMAND reqflow-core-tests)
    endif()
endif()
