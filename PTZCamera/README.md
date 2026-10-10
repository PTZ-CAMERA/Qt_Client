# Mini VMS Client 개발 안내

현재 기능, 구조도, 일반 빌드·실행 방법은 [저장소 README](../README.md)를 참조합니다. 기본 대상 `PTZCamera`의 출력 파일은 `MiniVmsClient`입니다.

## 통신과 영상 구현

- `network/VmsClient`: QWebSocket으로 목록·상태·검색·등록·녹화·PTZ를 요청합니다. 스트림 URI는 QNetworkAccessManager로 VMS REST API에서 조회합니다.
- `camera/CameraViewWidget`: QMediaPlayer → QVideoSink → QImage를 기존 QPainter CameraWidget에 표시합니다. 별도의 libavcodec worker 구현은 현재 없습니다.
- `ui/PtzCommandController`: 버튼·키보드 입력을 합치고 이동 명령을 200ms마다 갱신합니다.
- `playback/PlaybackWidget`: 녹화 검색 결과를 표시하고 완료된 로컬 파일을 Qt Multimedia로 재생합니다.
- `events/EventSearchWidget`: 탐지 샘플/상태 이력과 confidence 필터·페이지네이션을 제공하며 서버 재생 위치를 조회합니다.
- `chat/ChatSearchWidget`: VMS CHAT_SEARCH 응답·결과·추가 질문을 표시합니다. Qt에서 Gemini 키나 API를 직접 사용하지 않습니다.
- `app/MainWindow`: 카메라 선택과 위젯·클라이언트 signal/slot을 연결합니다.

WebSocket 기본 주소는 `ws://127.0.0.1:5000/ws`입니다. HTTP `/api/v1/cameras/{cameraId}/direct-stream?transport=tcp|udp`에서 카메라 URI를 받아 직접 RTSP로 연결하고 계정을 숨깁니다. PTZ·Tracking ON/OFF·채팅은 capability를 확인합니다. 추적 요청과 실제 상태는 구분하며 녹화·검색·재생 위치 조회는 VMS를 사용합니다.

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

## 최신 자동 녹화와 상태 조회

MainWindow의 자동 녹화 체크박스는 SET_AUTO_RECORDING, 진단 패널은 GET_METADATA_STATUS를 사용합니다. 늦게 연결해도 서버 모드와 저장 수를 조회하며 이전 cameraId/requestId 응답을 무시합니다. Qt에 자동 녹화 타이머나 탐지 알고리즘을 중복 구현하지 않습니다.

ChatSearch/Playback은 녹화 중·녹화 없음·기타 재생 실패를 구분하고, 옵션으로 탐지 offset보다 2초 앞에서 동일 파일을 재생합니다. 현재 offset은 추정 시각입니다.

WebRTC는 Web 클라이언트가 VMS 내부 libdatachannel에 연결하는 별도 경로이며 Qt 라이브는 Pi 직접 RTSP입니다. Pi 최신 JSON tracking_set/get과 VMS의 기존 ONVIF Tracking 계약 차이는 저장소 루트 README를 확인하세요.
