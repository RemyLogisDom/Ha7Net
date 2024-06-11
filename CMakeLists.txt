cmake_minimum_required(VERSION 3.16)
project(HA7NetPlugin VERSION 1.0 LANGUAGES C CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOUIC ON)

find_package(QT NAMES Qt5 Qt6 REQUIRED COMPONENTS Core)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Gui Network Widgets)

qt_standard_project_setup()

qt_add_plugin(HA7NetPlugin)
target_sources(HA7NetPlugin PRIVATE
    HA7Net.ui
    HA7NetPlugin.cpp HA7NetPlugin.h
    LCDOW.ui
    LedOW.ui
    ds1820.ui
    ds2406.ui
    ds2408.ui
    ds2413.ui
    ds2423.ui
    ds2438.ui
    ds2450.ui
)
target_link_libraries(HA7NetPlugin PRIVATE
    Qt::Core
    Qt::Gui
    Qt::Network
    Qt::Widgets
)


# Resources:
set(HA7NetPlugin_resource_files
    "images/pause.png"
    "images/play.png"
    "images/trafficlight_green.png"
    "images/trafficlight_off.png"
    "images/trafficlight_on.png"
    "images/trafficlight_red.png"
    "images/trafficlight_red_yellow.png"
    "images/trafficlight_yellow.png"
)

qt_add_resources(HA7NetPlugin "HA7NetPlugin"
    PREFIX
        "/"
    FILES
        ${HA7NetPlugin_resource_files}
)

install(TARGETS HA7NetPlugin
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    FRAMEWORK DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
