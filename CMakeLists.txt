cmake_minimum_required(VERSION 3.16)

project(StockMonitor VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

find_package(Qt5 REQUIRED COMPONENTS Core Widgets Network)
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
target_link_libraries(${PROJECT_NAME} Qt::Core Qt::Widgets Qt::Network OpenSSL::SSL OpenSSL::Crypto qhotkey)

if(WIN32)
    set_target_properties(${PROJECT_NAME} PROPERTIES WIN32_EXECUTABLE TRUE)
    target_compile_options(${PROJECT_NAME} PRIVATE /utf-8)
    target_compile_definitions(${PROJECT_NAME} PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
endif()
