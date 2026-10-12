cmake_minimum_required(VERSION 3.16)

project(NightShift VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


find_package(Qt6 REQUIRED COMPONENTS
    Core
    Widgets
    Network
)

qt_add_executable(nightshift
    MANUAL_FINALIZATION
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
    configmanager.cpp
    configmanager.h
    redshiftcontroller.cpp
    redshiftcontroller.h
    resources.qrc
)

target_link_libraries(nightshift PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Network
)

set_target_properties(nightshift PROPERTIES
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)

install(TARGETS nightshift
    BUNDLE  DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

qt_finalize_executable(nightshift)
