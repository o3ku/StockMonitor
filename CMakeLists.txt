cmake_minimum_required(VERSION 3.16)

project(StockMonitor VERSION 1.6.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

option(STOCKMONITOR_ENABLE_UPX "Compress release executable with UPX" ON)

find_package(Qt5 REQUIRED COMPONENTS Core Widgets Network Svg)
find_package(OpenSSL REQUIRED)

set(QHOTKEY_INSTALL OFF)
set(QHOTKEY_EXAMPLES OFF)
set(BUILD_SHARED_LIBS OFF)
add_subdirectory(Libs/QHotkey)

file(GLOB_RECURSE SOURCES src/*.hpp src/*.cpp)

if(WIN32)
    add_executable(${PROJECT_NAME} ${SOURCES} res/main.qrc res/app.rc)
else()
    add_executable(${PROJECT_NAME} ${SOURCES} res/main.qrc)
endif()
target_link_libraries(${PROJECT_NAME} Qt::Core Qt::Widgets Qt::Network Qt::Svg OpenSSL::SSL OpenSSL::Crypto qhotkey)

if(WIN32)
    set_target_properties(${PROJECT_NAME} PROPERTIES WIN32_EXECUTABLE TRUE)
    target_compile_options(${PROJECT_NAME} PRIVATE /utf-8)
    target_compile_definitions(${PROJECT_NAME} PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
    target_link_options(${PROJECT_NAME} PRIVATE $<$<CONFIG:Release>:/OPT:REF> $<$<CONFIG:Release>:/OPT:ICF> $<$<CONFIG:Release>:/INCREMENTAL:NO>)

    if(CMAKE_BUILD_TYPE STREQUAL "Release" AND STOCKMONITOR_ENABLE_UPX)
        find_program(UPX_EXECUTABLE upx REQUIRED)
        add_custom_command(TARGET ${PROJECT_NAME}
            POST_BUILD
            COMMAND ${UPX_EXECUTABLE} -9 $<TARGET_FILE:${PROJECT_NAME}>
            COMMENT "Compressing StockMonitor with UPX -9"
            VERBATIM
        )
    endif()
endif()
