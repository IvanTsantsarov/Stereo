cmake_minimum_required(VERSION 3.19)
project(Stereo LANGUAGES CXX)

# Find Qt 6 packages
find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets Gui) # Added Gui component for QImage handling

# ----------------------------------------------------------------------
# OPENCV SETUP
# ----------------------------------------------------------------------
# If CMake cannot find OpenCV, uncomment the line below and point it
# to the folder containing "OpenCVConfig.cmake"
# set(OpenCV_DIR "C:/path/to/opencv/build")

set(OpenCV_DIR "C:/msys64/mingw64/lib/cmake/opencv4")
find_package(OpenCV REQUIRED)
# ----------------------------------------------------------------------

qt_standard_project_setup()

qt_add_executable(StereoDepth
    WIN32 MACOSX_BUNDLE
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
    stereo.h stereo.cpp
    trash.txt
)

target_link_libraries(StereoDepth
    PRIVATE
        Qt::Core
        Qt::Widgets
        Qt::Gui
        ${OpenCV_LIBS} # Link all compiled OpenCV module libraries
)

# Map the include paths so your source files can resolve #include <opencv2/...>
target_include_directories(StereoDepth PRIVATE ${OpenCV_INCLUDE_DIRS})

install(TARGETS StereoDepth
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET StereoDepth
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})
