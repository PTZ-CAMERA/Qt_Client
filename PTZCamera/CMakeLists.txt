cmake_minimum_required(VERSION 3.21)
project(PTZCamera VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.2 REQUIRED COMPONENTS Widgets Network Multimedia MultimediaWidgets)
option(PTZ_WITH_WEBENGINE "Enable embedded WebRTC playback (requires Qt WebEngine Widgets)" ON)
if(PTZ_WITH_WEBENGINE)
    if(MINGW)
        message(FATAL_ERROR "Qt WebEngine is unavailable with MinGW on Windows. Select a Qt MSVC Kit, or set PTZ_WITH_WEBENGINE=OFF for an RTSP-only diagnostic build.")
    endif()
    find_package(Qt6 6.2 REQUIRED COMPONENTS WebEngineWidgets)
endif()

qt_add_executable(PTZCamera WIN32
    main.cpp
    MainWindow.h MainWindow.cpp
    CameraWidget.h CameraWidget.cpp
    PTZControlWidget.h PTZControlWidget.cpp
    TrackingPanel.h TrackingPanel.cpp
    ConnectionStatusWidget.h ConnectionStatusWidget.cpp
    NetworkClient.h NetworkClient.cpp
    CameraPlaybackWidget.h CameraPlaybackWidget.cpp
    TrackingInfo.h
)

qt_add_resources(PTZCamera "styles"
    PREFIX "/"
    FILES resources/style.qss
)

target_link_libraries(PTZCamera PRIVATE Qt6::Widgets Qt6::Network Qt6::Multimedia Qt6::MultimediaWidgets)
if(PTZ_WITH_WEBENGINE)
    target_link_libraries(PTZCamera PRIVATE Qt6::WebEngineWidgets)
    target_compile_definitions(PTZCamera PRIVATE PTZ_WITH_WEBENGINE=1)
endif()
