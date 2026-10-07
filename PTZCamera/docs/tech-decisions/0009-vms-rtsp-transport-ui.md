# 0009: Qt Stream 선택기의 RTSP TCP/UDP 표시

## 문제와 제약

사용자는 Qt에서 쓰지 않는 WebRTC를 Stream 선택기에서 삭제하고 RTSP TCP/UDP를 표시하기를 요청했다. 현재 VMS 릴레이는 RTP/RTCP TCP interleaving만 구현하며 UDP SETUP은 461 Unsupported Transport로 거부한다. UI를 바꿔 실제 UDP 지원이 있는 것처럼 표시하면 안 된다.

## 선택과 사용

WebRTC 항목을 삭제하고 RTSP TCP / RTSP UDP를 표시한다. 기본은 TCP다. 현재 UDP는 서버 미지원으로 비활성화하고 tooltip/사용 도움말에 이유를 표시한다. 전송 방식 변경 시 이전 플레이어와 요청을 정리하며, 비활성 항목이 코드로 선택돼도 재생을 시작하지 않는다. 플레이어 생성 전 선택 transport를 QT_FFMPEG_RTSP_TRANSPORT에 설정한다. 현재 정상 선택 가능한 값은 tcp뿐이다.

## 대안 및 이유

- UDP도 활성화하고 연결 실패만 표시: 서버가 명시적으로 지원하지 않아 미지원 기능 비활성화 원칙에 맞지 않는다.
- TCP만 표시: 실제 서버에는 맞지만 사용자 요청의 TCP/UDP 구분을 보이지 못하므로 두 항목을 표시하되 제한을 명시한다.
- Pi RTSP로 직접 UDP 연결: Qt→VMS→Pi 경계를 우회하므로 제외한다.
- Qt에서 서버 UDP 프로토콜을 가정: 릴레이 구현과 API 계약이 없으므로 서버 확장 후 별도 변경으로 진행해야 한다.

## 제한 및 검증

현재 UDP 재생은 구현되지 않았고 Qt의 환경 변수 설정만으로 서버 지원이 생기지 않는다. C:\PTZ_VMS_Server의 src/stream/RtspRelayServer.cpp와 docs/RTSP_RELAY_API.md에서 TCP 전용/461 거부를 확인했다. Qt 빌드와 UI 테스트로 WebRTC 제거, TCP 기본값, UDP 비활성화 및 재생 시작 거부를 확인한다. 실제 TCP/UDP 종단 간 시험은 Windows 및 서버 UDP 구현 이후 필요하다. 전송 지연에 대한 실측 주장은 하지 않는다.
