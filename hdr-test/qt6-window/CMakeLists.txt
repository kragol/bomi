cmake_minimum_required(VERSION 3.21)
project(hdrwindow LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.8 REQUIRED COMPONENTS Gui GuiPrivate Quick OpenGL)
find_package(PkgConfig REQUIRED)
pkg_check_modules(MPV REQUIRED IMPORTED_TARGET mpv)
pkg_check_modules(WAYLAND REQUIRED IMPORTED_TARGET wayland-client)
find_program(WAYLAND_SCANNER wayland-scanner REQUIRED)

set(CM_XML ${QT6_INSTALL_PREFIX}/share/qt6/wayland/protocols/color-management/color-management-v1.xml)
set(CM_HEADER ${CMAKE_CURRENT_BINARY_DIR}/color-management-v1-client-protocol.h)
set(CM_CODE ${CMAKE_CURRENT_BINARY_DIR}/color-management-v1-protocol.c)
add_custom_command(OUTPUT ${CM_HEADER} COMMAND ${WAYLAND_SCANNER} client-header ${CM_XML} ${CM_HEADER} DEPENDS ${CM_XML})
add_custom_command(OUTPUT ${CM_CODE} COMMAND ${WAYLAND_SCANNER} private-code ${CM_XML} ${CM_CODE} DEPENDS ${CM_XML})

qt_standard_project_setup(REQUIRES 6.8)

qt_add_executable(hdrwindow main.cpp mpvitem.cpp mpvitem.h colormanagement.cpp colormanagement.h
    ${CM_HEADER} ${CM_CODE})
target_include_directories(hdrwindow PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
qt_add_qml_module(hdrwindow URI HdrWindow QML_FILES Main.qml)
find_package(Qt6 REQUIRED COMPONENTS ShaderTools)
qt_add_shaders(hdrwindow "shaders" PREFIX "/shaders" BASE ${CMAKE_CURRENT_SOURCE_DIR}
    FILES ui_layer.frag ui_composite.frag)
target_link_libraries(hdrwindow PRIVATE Qt6::Gui Qt6::GuiPrivate Qt6::Quick Qt6::OpenGL PkgConfig::MPV PkgConfig::WAYLAND)
