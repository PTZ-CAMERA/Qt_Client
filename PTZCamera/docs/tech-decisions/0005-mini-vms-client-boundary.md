# 0005: VMS 클라이언트 UI와 기존 Pi 직접 연결 경로 분리

2026-10-07 후속: 통신 보류는 [0006](0006-vms-websocket-api.md)의 사용자 요청으로 해제되었다.
아래 내용은 M1 당시의 결정이며, 현재 기본 UI는 WebSocket으로 VMS 목록/status를 조회할 수 있다.

## 문제와 제약

Qt를 Mini VMS Desktop Client로 바꾸되 기존 화면/플레이어/통신 구현을 보존해야 한다. 최종 Qt 연결 대상은 VMS 서버이며 API가 아직 합의되지 않았다. 이번 범위는 M1 UI와 Dummy Mode다.

## 선택

Qt Widgets/CMake와 기존 CameraWidget/PTZ/Tracking/Connection/로그 구현을 재사용한다. 기본 MainWindow는 VMS UI로 구성하고 기존 창은 opt-in legacy 실행 대상으로 보존한다. DummyDataProvider가 UI용 샘플을 제공하며 VMS 소켓/JSON 프로토콜은 만들지 않는다.

## 후보 비교

- 기존 MainWindow에 VMS/직접 연결 분기를 계속 추가: 직접 Pi 연결이 기본 앱에서 실행될 위험과 Controller 비대화를 줄이기 위해 별도 legacy 창을 선택했다.
- 기존 코드 삭제 후 UI 전체 재작성: 사용자 보존 조건을 충족하지 않고 검증된 플레이어와 QPainter 코드를 잃으므로 제외했다.
- 지금 VMS API를 임의 정의: 서버와 합의되지 않은 형식이 고착되므로 M2로 미룬다.
- 새 QML 또는 native libwebrtc 도입: 현재 Widgets/WebEngine 재사용 조건에 맞지 않아 제외한다.

## 제한과 검증

Dummy LIVE/CONNECTED/REC는 모의 상태로 명시한다. 녹화 영상 실제 재생, 서버 이벤트 검색, VMS 소켓 연결, 실영상 overlay는 후속 단계다. 기존 direct-camera target은 전환 확인용이며 기본 VMS 실행 경로에 포함하지 않는다. Windows Kit 빌드/실행과 가능한 Linux 진단 빌드를 구분해 결과를 기록한다. 지연 성능을 새로 주장하지 않는다.

이번 변경은 Qt 6.11.2 헤더 기반 C++ 번역 단위 24개의 타입/구문 검사를 통과했다. Linux용 Qt 라이브러리 부재와 Windows interop 차단 때문에 실제 실행 파일 링크와 UI 테스트 실행은 확인하지 못했다. 전체 분석/신호 흐름/검증 상황은 [구조 문서](../../../docs/QT_VMS_CLIENT_ARCHITECTURE.md)에 기록한다.
