cmake_minimum_required(VERSION 3.19)
project(Protoshop LANGUAGES CXX)

set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets)

qt_standard_project_setup()

qt_add_executable(Protoshop
    WIN32 MACOSX_BUNDLE
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
    common.h

    imageres.qrc
    customview.h customview.cpp
    transformablerectitem.h transformablerectitem.cpp
    transformablelineitem.h transformablelineitem.cpp
    transformableellipseitem.h transformableellipseitem.cpp
    transformablepolygonitem.h transformablepolygonitem.cpp
    transformablepathitem.h transformablepathitem.cpp
    transformablecurveitem.h transformablecurveitem.cpp


)

target_link_libraries(Protoshop
    PRIVATE
        Qt::Core
        Qt::Widgets
)

include(GNUInstallDirs)

install(TARGETS Protoshop
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET Protoshop
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})

include_directories(${PROJECT_SOURCE_DIR})
