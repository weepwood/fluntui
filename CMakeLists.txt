cmake_minimum_required(VERSION 3.21)
project(FluentSystemInfo VERSION 0.4.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(FSI_BUILD_APP "Build Qt/FluentQt GUI application" ON)
option(FSI_BUILD_TESTS "Build platform-independent unit tests" ON)

if(FSI_BUILD_TESTS)
    enable_testing()
    add_executable(fsi_format_tests tests/format_tests.cpp src/Formatting.cpp)
    target_include_directories(fsi_format_tests PRIVATE src)
    add_test(NAME formatting COMMAND fsi_format_tests)
endif()

if(FSI_BUILD_APP)
    set(CMAKE_AUTOMOC ON)
    set(CMAKE_AUTORCC ON)
    find_package(Qt6 6.2 REQUIRED COMPONENTS Widgets Network)

    # The FluentQt dependency is a native static C++ library. Avoid building its gallery.
    set(FLUENT_QT_BUILD_GALLERY OFF CACHE BOOL "" FORCE)
    set(FLUENT_QT_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(FLUENT_QT_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(FLUENT_QT_INSTALL OFF CACHE BOOL "" FORCE)

    if(DEFINED FLUENTQT_SOURCE_DIR AND NOT FLUENTQT_SOURCE_DIR STREQUAL "")
        add_subdirectory("${FLUENTQT_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/fluentqt")
    else()
        include(FetchContent)
        FetchContent_Declare(fluentqt
            GIT_REPOSITORY https://github.com/calvinhxx/Fluent-Qt.git
            GIT_TAG v1.10.2
            GIT_SHALLOW TRUE
        )
        FetchContent_MakeAvailable(fluentqt)
    endif()

    qt_add_executable(fluent-system-info WIN32
        src/main.cpp src/MainWindow.cpp src/MainWindow.h
        src/SystemCollector.cpp src/SystemCollector.h
        src/TrendGraph.cpp src/TrendGraph.h
        src/Formatting.cpp src/Formatting.h
    )
    target_link_libraries(fluent-system-info PRIVATE FluentQt::FluentQt Qt6::Widgets Qt6::Network)
    target_compile_definitions(fluent-system-info PRIVATE
        FSI_APP_VERSION="${PROJECT_VERSION}"
        UNICODE _UNICODE
    )
    if(MSVC)
        target_compile_options(fluent-system-info PRIVATE /utf-8)
    endif()
    if(WIN32)
        target_compile_definitions(fluent-system-info PRIVATE WINVER=0x0A00 _WIN32_WINNT=0x0A00)
        target_link_libraries(fluent-system-info PRIVATE iphlpapi ws2_32 advapi32 user32)
    endif()
endif()
