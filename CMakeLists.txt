cmake_minimum_required(VERSION 3.5)

project(qsvn2git VERSION 0.1 LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS
        Widgets
        Xml
)

find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Xml)

add_executable(qsvn2git
    main.cpp
    qmaindialog.cpp
    qmaindialog.h
    qmaindialog.ui
    qconvertorworker.cpp
    qconvertorworker.h
)

target_link_libraries(qsvn2git PRIVATE Qt${QT_VERSION_MAJOR}::Widgets)
target_link_libraries(qsvn2git PRIVATE Qt${QT_VERSION_MAJOR}::Xml)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_project()
endif()
