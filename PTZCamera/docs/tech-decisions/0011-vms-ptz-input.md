# 0011: Qt 입력을 VMS PTZ API로 연결

2026-10-08. Qt에서 Pi 직접 ONVIF/TCP를 호출하지 않는다. 실제 서버 PtzCommandRouter/WebSocketServer 계약을 확인해 기존 v1 WebSocket을 사용한다.

## 계약과 사용

PTZ_MOVE는 cameraId와 최상위 panVelocity/tiltVelocity(-1..1), PTZ_STOP/PTZ_CENTER는 cameraId를 보낸다. version=1, requestId, command 형식이다. 성공 response의 data.phase=ACCEPTED는 VMS 접수이며 notification event=PTZ_RESULT의 PI_ACKNOWLEDGED가 Pi ONVIF 응답 확인이다. FAILED/REJECTED/SUPERSEDED와 error.code/message를 구분한다. motorArrivalConfirmed=false이므로 실제 각도 도달을 주장하지 않는다.

PtzCommandController는 버튼/키보드 상태를 합치고 최초 즉시, 이후 200ms마다 갱신한다. W/A/S/D 및 기존 방향키, C 및 R 중앙 복귀를 지원한다. 자동 반복의 release/press는 무시한다. 포커스 상실, 창 비활성화, 카메라 변경, 연결 해제, 종료에서 입력 상태/타이머를 초기화하고 기존 cameraId로 Stop을 요청한다. 이미 끊긴 연결에는 Stop 전달을 보장할 수 없다. 서버는 세션 종료 및 600ms 이동 lease 만료에서 Stop을 수행한다.

capabilities.ptz/ptzCenter와 온라인/연결 상태에 따라 조작을 활성화한다. Tracking API는 그대로 미지원이다. PTZ TX, PTZ VMS, PTZ PI, PTZ ERROR 로그를 나눈다. Stop 진행 중 새 Move는 최신 값만 보관하고 Center는 Stop 확인 후 전송한다. 취소하면 보류 명령도 비운다. 오류/응답 타임아웃에는 갱신을 중단한다.

## 대안과 이유

- Pi 직접 SOAP/TCP: VMS 라우팅 원칙에 어긋나 제외한다.
- clicked만 사용: 누르는 동안의 이동과 놓음 정지를 표현하지 못한다.
- 키 자동 반복을 갱신 시계로 사용: 운영체제 반복 주기/가짜 release에 영향을 받아 QTimer를 사용한다.
- ACCEPTED를 Pi 응답으로 표시: 큐 접수와 장치 확인이 달라 구분한다.

## 검증과 제한

클라이언트 빌드와 UI/로컬 WebSocket fixture로 200ms 갱신, 중단, 이전 cameraId 정지, 자동 반복 release, C 입력, 평면 JSON 필드, 접수/장치 확인 로그 분리, Stop 확인 후 Center를 검사한다. 실제 Pi 모터 움직임 및 Windows 포커스 이벤트는 별도 확인해야 한다. 이번 요청에서는 서버 코드를 수정하지 않았다.
