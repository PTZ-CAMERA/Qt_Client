cmake_minimum_required(VERSION 3.21)
project(PTZCamera VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.2 REQUIRED COMPONENTS Widgets Network WebSockets Multimedia)
option(PTZ_BUILD_LEGACY_CLIENT "Build the preserved direct-Pi camera client" OFF)
option(PTZ_WITH_WEBENGINE "Enable WebRTC in the preserved legacy player" ON)
option(PTZ_BUILD_TESTS "Build Mini VMS milestone 1 UI tests" OFF)

set(SHARED_WIDGET_SOURCES
    camera/CameraWidget.h camera/CameraWidget.cpp
    ui/PTZControlWidget.h ui/PTZControlWidget.cpp
    ui/TrackingPanel.h ui/TrackingPanel.cpp
    ui/ConnectionStatusWidget.h ui/ConnectionStatusWidget.cpp
    model/TrackingInfo.h
)
set(VMS_UI_SOURCES
    ${SHARED_WIDGET_SOURCES}
    app/MainWindow.h app/MainWindow.cpp
    network/VmsClient.h network/VmsClient.cpp
    model/VmsTypes.h
    camera/CameraListWidget.h camera/CameraListWidget.cpp
    camera/CameraViewWidget.h camera/CameraViewWidget.cpp
    demo/DummyDataProvider.h demo/DummyDataProvider.cpp
    events/EventSearchWidget.h events/EventSearchWidget.cpp
    playback/PlaybackWidget.h playback/PlaybackWidget.cpp
    device/DeviceInfoWidget.h device/DeviceInfoWidget.cpp
    log/SystemLogWidget.h log/SystemLogWidget.cpp
    ui/PtzKeyboardController.h ui/PtzKeyboardController.cpp
    ui/PtzCommandController.h ui/PtzCommandController.cpp
    ui/StatusIndicatorWidget.h ui/StatusIndicatorWidget.cpp
    ui/HelpDialog.h ui/HelpDialog.cpp
)
function(configure_ptz_ui target)
    target_include_directories(${target} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(${target} PRIVATE Qt6::Widgets Qt6::Network Qt6::WebSockets Qt6::Multimedia)
    if(MSVC)
        target_compile_options(${target} PRIVATE /utf-8)
    endif()
    qt_add_resources(${target} "styles" PREFIX "/" FILES resources/style.qss)
endfunction()

# 기존 target 이름은 유지해 Qt Creator 설정을 재사용한다.
qt_add_executable(PTZCamera WIN32 app/main.cpp ${VMS_UI_SOURCES})
set_target_properties(PTZCamera PROPERTIES OUTPUT_NAME MiniVmsClient)
configure_ptz_ui(PTZCamera)

if(PTZ_BUILD_LEGACY_CLIENT)
    find_package(Qt6 6.2 REQUIRED COMPONENTS Network Multimedia MultimediaWidgets)
    qt_add_executable(PTZCameraLegacy WIN32
        legacy/main.cpp legacy/LegacyMainWindow.h legacy/LegacyMainWindow.cpp
        ${SHARED_WIDGET_SOURCES}
        camera/CameraPlaybackWidget.h camera/CameraPlaybackWidget.cpp
        camera/OnvifPanelWidget.h camera/OnvifPanelWidget.cpp
        camera/rtsp/RtspPlayerWidget.h camera/rtsp/RtspPlayerWidget.cpp
        network/NetworkClient.h network/NetworkClient.cpp
        network/onvif/OnvifClient.h network/onvif/OnvifClient.cpp
    )
    configure_ptz_ui(PTZCameraLegacy)
    target_link_libraries(PTZCameraLegacy PRIVATE Qt6::Network Qt6::Multimedia Qt6::MultimediaWidgets)
    if(PTZ_WITH_WEBENGINE)
        if(MINGW)
            message(FATAL_ERROR "Qt WebEngine requires the Windows MSVC Kit. Set PTZ_WITH_WEBENGINE=OFF for legacy RTSP-only diagnostics.")
        endif()
        find_package(Qt6 6.2 REQUIRED COMPONENTS WebEngineWidgets)
        target_sources(PTZCameraLegacy PRIVATE camera/webrtc/WebRtcPlayerWidget.h camera/webrtc/WebRtcPlayerWidget.cpp)
        target_link_libraries(PTZCameraLegacy PRIVATE Qt6::WebEngineWidgets)
        target_compile_definitions(PTZCameraLegacy PRIVATE PTZ_WITH_WEBENGINE=1)
    endif()
endif()
if(PTZ_BUILD_TESTS)
    find_package(Qt6 6.2 REQUIRED COMPONENTS Test Network)
    enable_testing()
    qt_add_executable(MiniVmsUiTests tests/MiniVmsUiTests.h tests/MiniVmsUiTests.cpp ${VMS_UI_SOURCES})
    configure_ptz_ui(MiniVmsUiTests)
    target_link_libraries(MiniVmsUiTests PRIVATE Qt6::Test Qt6::Network)
    qt_add_executable(VmsWebSocketTests tests/VmsWebSocketTests.h tests/VmsWebSocketTests.cpp ${VMS_UI_SOURCES})
    configure_ptz_ui(VmsWebSocketTests)
    target_link_libraries(VmsWebSocketTests PRIVATE Qt6::Test)
    add_test(NAME VmsWebSocketTests COMMAND VmsWebSocketTests)
    set_tests_properties(VmsWebSocketTests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 60)
    add_test(NAME MiniVmsUiTests COMMAND MiniVmsUiTests)
    set_tests_properties(MiniVmsUiTests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_MEDIA_BACKEND=ffmpeg")
    if(UNIX)
        # 화면 없는 테스트에서는 Vulkan 장치를 노출하지 않고 소프트웨어 디코딩을 사용한다.
        set_property(TEST MiniVmsUiTests APPEND PROPERTY ENVIRONMENT
            "VK_ICD_FILENAMES=${CMAKE_CURRENT_BINARY_DIR}/no-vulkan-driver.json")
    endif()
endif()
