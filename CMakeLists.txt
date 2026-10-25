cmake_minimum_required(VERSION 3.21)
project(ReqFlowAI VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 6.4 REQUIRED COMPONENTS Core Widgets Network Sql)

add_executable(reqflow-ai
    src/main.cpp
    src/network/httpclient.cpp
    src/data/projectstore.cpp
    src/data/environment.cpp
    src/flow/flowengine.cpp
    src/flow/floweditor.cpp
    src/openapi/openapiimporter.cpp
    src/testing/assertions.cpp
    src/ui/mainwindow.cpp
    include/reqflow/network/httpclient.h
    include/reqflow/data/projectstore.h
    include/reqflow/data/environment.h
    include/reqflow/flow/flowengine.h
    include/reqflow/flow/floweditor.h
    include/reqflow/flow/flowgraphics.h
    include/reqflow/openapi/openapiimporter.h
    include/reqflow/testing/assertions.h
    include/reqflow/ui/mainwindow.h
)
target_include_directories(reqflow-ai PRIVATE include)
target_link_libraries(reqflow-ai PRIVATE Qt6::Core Qt6::Widgets Qt6::Network Qt6::Sql)

include(CTest)
if(BUILD_TESTING)
    find_package(Qt6 6.4 QUIET COMPONENTS Test)
    if(TARGET Qt6::Test)
        add_executable(reqflow-core-tests
            tests/cpp/test_core.cpp
            src/network/httpclient.cpp
            src/data/projectstore.cpp
            src/data/environment.cpp
            src/flow/flowengine.cpp
            src/openapi/openapiimporter.cpp
            src/testing/assertions.cpp
            include/reqflow/testing/assertions.h
            include/reqflow/flow/flowengine.h
            include/reqflow/openapi/openapiimporter.h
            include/reqflow/network/httpclient.h
            include/reqflow/data/projectstore.h
            include/reqflow/data/environment.h
        )
        target_include_directories(reqflow-core-tests PRIVATE include)
        target_link_libraries(reqflow-core-tests PRIVATE Qt6::Core Qt6::Network Qt6::Sql Qt6::Test)
        add_test(NAME reqflow-core-tests COMMAND reqflow-core-tests)
    endif()
endif()
