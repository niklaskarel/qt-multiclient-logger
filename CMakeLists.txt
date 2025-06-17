cmake_minimum_required(VERSION 3.16)

project(EventMonitor VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Network Core PrintSupport)

set(PROJECT_SOURCES
        main.cpp
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(EventMonitor
        ${PROJECT_SOURCES}
        mainwindow.cpp mainwindow.h mainwindow.ui
        eventreceiver.h eventreceiver.cpp
        eventmessage.h
        message_trigger.py
        logger.h logger.cpp
        writer.h writer.cpp
        settings.h settings.cpp settings.ui
        dataprocessor.h dataprocessor.cpp
        3rdPartyLibraries/QCustomPlot/qcustomplot.cpp 3rdPartyLibraries/QCustomPlot/qcustomplot.h
        pythonprocessmanager.h pythonprocessmanager.cpp

    )
# Define target properties for Android with Qt 6 as:
#    set_property(TARGET EventMonitor APPEND PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR
#                 ${CMAKE_CURRENT_SOURCE_DIR}/android)
# For more information, see https://doc.qt.io/qt-6/qt-add-executable.html#target-creation
else()
    if(ANDROID)
        add_library(EventMonitor SHARED
            ${PROJECT_SOURCES}
        )
# Define properties for Android with Qt 5 after find_package() calls as:
#    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
    else()
        add_executable(EventMonitor
            ${PROJECT_SOURCES}
        )
    endif()
endif()

add_definitions(-DQT_STATIC_BUILD)

target_link_libraries(EventMonitor
    PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Network
    Qt${QT_VERSION_MAJOR}::PrintSupport
)

target_include_directories(EventMonitor
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/3rdPartyLibraries/QCustomPlot
)

# Copy the Python script into the build directory
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/message_trigger.py ${CMAKE_CURRENT_BINARY_DIR}/message_trigger.py COPYONLY)

# Install Python script with the executable
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/message_trigger.py DESTINATION ${CMAKE_INSTALL_BINDIR})

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
if(${QT_VERSION} VERSION_LESS 6.1.0)
  set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.EventMonitor)
endif()
set_target_properties(EventMonitor PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS EventMonitor
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
