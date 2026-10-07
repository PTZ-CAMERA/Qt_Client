# 0007: 실제 VMS API에 맞춘 Qt 기능 표시

후속 변경: FILE INFO 전용 화면은 사용자의 녹화 영상 재생 요청으로 [0008](0008-local-recording-playback.md)의 동일 PC 로컬 파일 재생으로 확장했다. 원격 재생 API는 추가하지 않았다.

Stream 선택기의 WebRTC 항목은 [0009](0009-vms-rtsp-transport-ui.md)에서 삭제했으며 RTSP TCP/UDP 표시와 현재 TCP 전용 제한을 명시한다.

- 날짜: 2026-10-07
- 상태: Qt 구현, Windows 실서버 종단 간 검증 대기.

## 문제와 제약

기존 Qt Widgets 화면을 유지하면서 C:\PTZ_VMS_Server의 실제 지원 API에 맞춰 동작해야 한다. 서버 문서 일부가 이전 단계 상태여서 현재 실행 코드와 함께 확인했다. 미지원 PTZ/Tracking/EVENTS, Qt WebRTC, 녹화 재생이 구현된 것처럼 보여서는 안 된다.

## 선택과 프로젝트 사용

현재 VMS API를 재사용한다. VmsClient가 JSON과 HTTP를 처리하고 MainWindow는 위젯/신호를 연결한다.

| 기능 | 실제 API | Qt 동작 |
| --- | --- | --- |
| 목록/상태 | WebSocket GET_CAMERA_LIST / GET_CAMERA_STATUS / CAMERA_STATUS | 카메라 및 독립 상태 표시 |
| ONVIF 검색/등록 | WebSocket DISCOVER_CAMERAS / REGISTER_CAMERA | 기존 DEVICE 폼 유지 |
| 실시간 주소 | GET /api/v1/cameras/{cameraId}/stream | ready 상태의 VMS RTSP URI만 표시/복사 |
| 녹화 명령 | WebSocket START_RECORDING / STOP_RECORDING | capabilities.recordings 및 실제 연결 상태로 활성화 |
| 녹화 검색 | WebSocket GET_RECORDINGS | fromMs / toMs / limit=100, 결과 건수/실패 표시 |

GET URL은 주소만 조회하며 기존 영상을 중단하거나 정지된 영상을 시작하지 않는다. 주소는 선택 cameraId에만 적용한다. 카메라 변경과 서버 종료/Dummy 전환 시 초기화한다. 자격 정보가 포함된 URI는 표시하지 않는다. LIVE는 영상 프레임을 받은 뒤에만 활성 표시한다.

검색 시간은 UI의 로컬 시각에서 UTC epoch milliseconds로 변환한다. 서버의 배타적 종료 경계에 맞춰 포함 종료 시각에 1ms를 더한다. 최대 100건이며 전체 건수/페이지 번호를 임의로 만들지 않는다. 조건 변경 후 늦게 도착한 응답은 무효화한다. 실패를 결과 0건으로 표시하지 않는다.

FILE INFO는 서버 파일 경로와 시작/종료 시각만 보여준다. 재생 URI, 파일 다운로드, 녹화 디코딩 API를 가정하지 않는다. 실제 녹화의 Play/Pause/Seek는 비활성화한다. Dummy Mode의 모의 타임라인은 DUMMY로 표시해 유지한다.

실제 VMS 모드에서 PTZ/Tracking 및 키보드 이동/중앙복귀는 비활성화한다. EVENTS 탭은 유지하되 비활성화하고 GET_EVENTS를 보내지 않는다. WebRTC 항목은 선택 불가다. Dummy Mode에서만 기존 모의 PTZ/Tracking/이벤트를 사용한다.

## 후보 및 선택 이유

- 기존 WebSocket 녹화 API: 실제 서버와 호환되므로 선택했다. REST 통합은 별도 서버 계약 확정 후 진행한다.
- 새로운 REST 녹화/재생 endpoint 가정: 서버가 제공하지 않아 제외했다.
- Qt에서 SQLite를 직접 조회: 서버를 통한 조회 원칙 및 다른 PC 실행 계획에 맞지 않아 제외했다.
- 서버 파일 경로를 즉시 재생: 접근 가능 여부와 재생 계약이 없어 파일 정보만 표시한다.
- 클릭 후 NOT_SUPPORTED 응답 표시: 불필요한 요청과 혼동을 줄이도록 미리 비활성화하고 이유를 표시한다.

## 근거 및 남은 검증

서버 코드 기준: src/client/WebSocketServer.cpp의 REST stream/capabilities, src/camera/CameraService.cpp의 명령 분배, src/recording/RecordingManager.cpp의 검색 입력/출력.

Qt Linux 빌드/UI 테스트로 미지원 컨트롤, URI 복사/선택 분리/자격 정보 거부, FILE INFO와 재생 비활성화, 검색 조건 변경/오류를 검사한다. 관리형 환경의 서버 소켓 생성 제한으로 실서버 연결 및 Pi 프레임/녹화 파일은 이번 변경에서 검증하지 못했다. Windows에서 서버 연결, ONVIF 등록, 주소 조회, 실제 REC 상태 및 녹화 종료 후 검색을 확인해야 한다. 지연 성능 수치를 주장하지 않는다.

검증 결과: Linux Qt 6.4.2에서 클라이언트와 두 테스트 실행 파일 빌드 성공. MiniVmsUiTests는 초기화/종료를 포함해 10 passed, 0 failed. 1400×850 실제 VMS 모드의 offscreen 화면도 확인했다. WebSocket 실연결은 이 결과에 포함하지 않는다.
