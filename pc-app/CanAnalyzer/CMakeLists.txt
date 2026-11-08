cmake_minimum_required(VERSION 3.16)

project(CanAnalyzer VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Widgets : l'interface graphique
# SerialPort : la liaison serie avec le CP2102 / la STM32F4
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets SerialPort)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets SerialPort)

set(PROJECT_SOURCES
    main.cpp
    mainwindow.cpp
    mainwindow.h
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(CanAnalyzer
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
    )
else()
    add_executable(CanAnalyzer
        ${PROJECT_SOURCES}
    )
endif()

target_link_libraries(CanAnalyzer PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::SerialPort
)

set_target_properties(CanAnalyzer PROPERTIES
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS CanAnalyzer
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(CanAnalyzer)
endif()
