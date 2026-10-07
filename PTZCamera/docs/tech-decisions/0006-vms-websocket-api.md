# 0006: Qt ↔ VMS WebSocket JSON API

2026-10-07 갱신: 아래 내용은 최초 WebSocket 단계 기록이다. 현재 ONVIF 등록, VMS RTSP URI 조회, 녹화 명령/검색 연동과 미지원 UI 정책은 [0007](0007-supported-vms-ui.md)을 따른다.

2026-10-07. 사용자가 WebSocket 구현을 요청하여 0005의 통신 보류를 해제한다.

## 문제와 제약

현재 Qt UI는 Dummy Mode이고 VMS는 C++17 FFmpeg RTSP ingest를 구현했다.
Qt/Web client가 같은 control/status protocol을 사용할 수 있어야 하고,
server는 Qt GUI/QtNetwork에 의존하지 않아야 한다. 이번 범위에 아직 없는
PTZ/recording/events/live backend를 성공한 것처럼 표시하면 안 된다.

## 선택과 사용

VMS는 Boost.Beast/Asio와 nlohmann_json을 사용한 async WebSocket server,
Qt는 Qt Network/WebSockets의 QWebSocket adapter `network/VmsClient`를 사용한다.
`ws://127.0.0.1:5000/ws`, version=1 JSON, string requestId 기반 request/response,
CAMERA_STATUS notification을 양쪽에 맞춰 구현했다. 지원 command는 GET_CAMERA_LIST와
GET_CAMERA_STATUS다. 나머지 command는 NOT_SUPPORTED이며 실연결의 PTZ/tracking은
비활성화된다. Dummy Mode는 기존대로 유지하고 실연결 전환 때 정리한다.

통신 상세 형식은 인접 VMS repository의 `docs/WEBSOCKET_API.md`를 기준으로 한다.
Qt UI는 CameraInfo로 변환된 데이터를 받고 protocol parsing을 MainWindow에 넣지 않는다.
ONVIF/RTSP camera credentials를 Qt에 전달하지 않는다. Stream Receiving (VMS)는 ingest
상태이고 frame display/LIVE와 다르다. 실제 frame transport는 후속 단계다.

## 대안과 선택 이유

- 자체 TCP: 기존 NetworkClient는 Pi용 line protocol이며 browser가 직접 접속할 수 없다.
  추가 framing/browser bridge를 만드는 대신 native WebSocket을 선택했다.
- HTTP-only polling: camera status push를 위해 별도 polling이 필요하다. 현재 control/status
  하나의 양방향 연결을 우선 구현하고 REST는 필요 시 추가한다.
- QtNetwork server: server의 독립 C++/FFmpeg 요구와 의존성 최소화 조건에 맞춰 Beast를 선택했다.
- libwebsockets/다른 networking SDK: 현재 architecture가 Asio/Beast를 후보로 두고 있고,
  Beast는 async HTTP upgrade/WebSocket과 Windows/Linux를 같은 모델로 처리한다.
- JSON 수동 parsing: escape/type/size validation을 직접 만드는 대신 nlohmann_json을 사용한다.
  추가 dependency는 header-only package이고 server에만 링크한다.

## 제한, 가정, 검증

기본 loopback bind, 인증/TLS 없음, browser UI 없음. 미구현 backend를 실제 연결하지 않는다.
자동 재접속은 없고 Connect로 다시 연결한다. Connection/request timeout은 5초,
Qt ping은 5초, activity timeout 15초, inbound 64KiB/pending 64개 제한이다.
Server의 socket 처리 thread는 하나이며 camera snapshot만 mutex로 공유한다.
상태 snapshot push 주기는 1초다. 종단 간 영상 latency 수치를 주장하지 않는다.

Linux Qt 6.4.2에서 앱과 Qt tests를 실제 링크/실행하고, C++ VMS와 실제 Pi의 상태
전달을 검사한다. Windows Qt 6.11.2 MSVC SDK의 WebSockets module 존재는 확인했으나
Windows native 실행은 별도 검증이 필요하다. 변경 파일 외 기존 dirty working tree는 유지한다.

## References

- [Qt QWebSocket](https://doc.qt.io/qt-6/qwebsocket.html)
- [Boost.Beast WebSocket](https://www.boost.org/latest/libs/beast/doc/html/beast/using_websocket.html)

검증 결과: VMS tests 3/3 및 ASan/UBSan/leak 검사 통과. Qt CTest 2/2 통과.
실제 Pi를 연결한 QWebSocket/MainWindow integration에서 H264 1280x720 30FPS,
packet counter 1→49 증가, server 종료 시 disconnect/화면 정리를 확인했다.
기존 UI test의 window teardown signal assertion도 수정하고 통과했다.
