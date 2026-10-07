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
    app/main.cpp
    app/MainWindow.h app/MainWindow.cpp
    camera/CameraWidget.h camera/CameraWidget.cpp
    camera/CameraPlaybackWidget.h camera/CameraPlaybackWidget.cpp
    camera/OnvifPanelWidget.h camera/OnvifPanelWidget.cpp
    camera/rtsp/RtspPlayerWidget.h camera/rtsp/RtspPlayerWidget.cpp
    network/NetworkClient.h network/NetworkClient.cpp
    network/onvif/OnvifClient.h network/onvif/OnvifClient.cpp
    ui/PTZControlWidget.h ui/PTZControlWidget.cpp
    ui/TrackingPanel.h ui/TrackingPanel.cpp
    ui/ConnectionStatusWidget.h ui/ConnectionStatusWidget.cpp
    model/TrackingInfo.h
)

qt_add_resources(PTZCamera "styles"
    PREFIX "/"
    FILES resources/style.qss
)

target_include_directories(PTZCamera PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(PTZCamera PRIVATE Qt6::Widgets Qt6::Network Qt6::Multimedia Qt6::MultimediaWidgets)
if(PTZ_WITH_WEBENGINE)
    target_sources(PTZCamera PRIVATE
        camera/webrtc/WebRtcPlayerWidget.h camera/webrtc/WebRtcPlayerWidget.cpp
    )
    target_link_libraries(PTZCamera PRIVATE Qt6::WebEngineWidgets)
    target_compile_definitions(PTZCamera PRIVATE PTZ_WITH_WEBENGINE=1)
endif()
