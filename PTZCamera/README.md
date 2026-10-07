# Mini VMS Client 개발 안내

현재 기능, 구조도, 일반 빌드·실행 방법은 [저장소 README](../README.md)를 참조합니다. 기본 대상 `PTZCamera`의 출력 파일은 `MiniVmsClient`입니다.

## 통신과 영상 구현

- `network/VmsClient`: QWebSocket으로 목록·상태·검색·등록·녹화·PTZ를 요청합니다. 스트림 URI는 QNetworkAccessManager로 VMS REST API에서 조회합니다.
- `camera/CameraViewWidget`: QMediaPlayer → QVideoSink → QImage를 기존 QPainter CameraWidget에 표시합니다. 별도의 libavcodec worker 구현은 현재 없습니다.
- `ui/PtzCommandController`: 버튼·키보드 입력을 합치고 이동 명령을 200ms마다 갱신합니다.
- `playback/PlaybackWidget`: 녹화 검색 결과를 표시하고 완료된 로컬 파일을 Qt Multimedia로 재생합니다.
- `app/MainWindow`: 카메라 선택과 위젯·클라이언트 signal/slot을 연결합니다.

WebSocket 기본 주소는 `ws://127.0.0.1:5000/ws`입니다. 동일 Host/Port의 HTTP에서 `/api/v1/cameras/{cameraId}/stream`을 조회합니다. 실제 카메라 capability에 따라 PTZ가 활성화되며 Tracking·EVENTS 실기능은 비활성화 상태입니다.

## 이전 직접 연결 화면

Pi 직접 TCP/ONVIF/RTSP/WebRTC 코드는 진단용 legacy target에 보존되어 있습니다. 기본 VMS 실행 파일에는 WebEngine 플레이어가 포함되지 않습니다.

저장소 최상위의 MSVC 환경 터미널에서:

```powershell
cmake -S . -B build-legacy -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH="C:/Qt/6.11.2/msvc2022_64" -DPTZ_BUILD_LEGACY_CLIENT=ON -DPTZ_WITH_WEBENGINE=ON
cmake --build build-legacy --target PTZCameraLegacy --parallel
```

legacy에는 MultimediaWidgets가 추가로 필요하고, WebRTC 진단에는 WebEngineWidgets도 필요합니다. RTSP 전용 진단은 `PTZ_WITH_WEBENGINE=OFF`로 구성합니다.

## 자동 테스트

```powershell
cmake -S . -B build-vms -DPTZ_BUILD_TESTS=ON
cmake --build build-vms --parallel
ctest --test-dir build-vms --output-on-failure
```

`MiniVmsUiTests`는 UI 선택·Dummy·입력·녹화 표시를, `VmsWebSocketTests`는 mock VMS의 연결·요청·오류·PTZ 응답을 검증합니다. 실제 VMS 검사에는 `VMS_SERVER_TEST_EXECUTABLE`, 카메라 설정 검사에는 `VMS_TEST_CAMERA_CONFIG`를 사용할 수 있습니다. 자동 테스트 성공과 Windows·Pi 하드웨어 검증은 구분합니다.

[기술 결정 기록](docs/tech-decisions/) · [아키텍처 기록](../docs/QT_VMS_CLIENT_ARCHITECTURE.md) · [최근 세션](../docs/SESSION_2026-10-08.md)
