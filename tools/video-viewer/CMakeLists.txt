# Minimal Qt6/OpenGL video viewer for the skeleton.

find_package(Qt6 QUIET COMPONENTS Widgets OpenGLWidgets OpenGL)

if(NOT Qt6_FOUND)
    message(STATUS "Qt6 not found; skipping video-viewer")
    return()
endif()

qt_standard_project_setup()

add_executable(nxd_video_viewer
    main.cpp)

target_link_libraries(nxd_video_viewer PRIVATE
    nxd_host
    Qt6::Widgets
    Qt6::OpenGLWidgets
    Qt6::OpenGL)

target_include_directories(nxd_video_viewer PRIVATE
    "${CMAKE_SOURCE_DIR}/host/include"
    "${CMAKE_SOURCE_DIR}/sdk/cpp/include"
    "${CMAKE_SOURCE_DIR}/sdk/rust/nxd-abi"
    "${NXD_GENERATED_CPP_DIR}")

target_compile_options(nxd_video_viewer PRIVATE
    -fno-exceptions
    -fno-rtti
    -Wall
    -Wextra
    -Werror)

set_target_properties(nxd_video_viewer PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    AUTOMOC ON)

add_dependencies(nxd_video_viewer nxd_example_virtual_video_cpp nxd_component_descriptors)
