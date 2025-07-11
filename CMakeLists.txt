cmake_minimum_required(VERSION 3.16)

project(StockMonitor VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

find_package(Qt5 REQUIRED COMPONENTS Core Widgets Network)
file(GLOB_RECURSE SOURCES src/*.hpp src/*.cpp)
add_executable(${PROJECT_NAME} ${SOURCES} res/main.qrc)
target_link_libraries(${PROJECT_NAME} Qt::Core Qt::Widgets Qt::Network)

if(WIN32)
    set_target_properties(${PROJECT_NAME} PROPERTIES WIN32_EXECUTABLE TRUE)
    target_compile_options(${PROJECT_NAME} PRIVATE /utf-8)
endif()
