cmake_minimum_required(VERSION 3.16)

project(ProjectDashBoard VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Quick Core5Compat SerialPort)

qt_standard_project_setup(REQUIRES 6.10)

# Content of downloaded design from Figma to Qt or similar
if (EXISTS "${CMAKE_CURRENT_LIST_DIR}/importedcontent/CMakeLists.txt")
    add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/importedcontent)
endif()

qt_add_executable(appProjectDashBoard
    main.cpp
)

qt_add_qml_module(appProjectDashBoard
    URI ProjectDashBoard
    QML_FILES
        Main.qml
        QML_FILES RPM.qml
        QML_FILES RightGauges.qml
        QML_FILES FuelBar.qml
        QML_FILES BackgroundGauges.qml
    RESOURCES
        assets/images/background_dashboard.jpg
        assets/images/background_meter.jpg
        QML_FILES BackgroundDB.qml
        QML_FILES TurnSignal.qml
        QML_FILES ErrorPanel.qml
    SOURCES
        Carbackend.h
        Carbackend.cpp
)

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
set_target_properties(appProjectDashBoard PROPERTIES
#    MACOSX_BUNDLE_GUI_IDENTIFIER com.example.appProjectDashBoard
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

target_link_libraries(appProjectDashBoard
    PRIVATE
        Qt6::Quick
        Qt6::Core5Compat
        Qt6::SerialPort
)

install(TARGETS appProjectDashBoard
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
