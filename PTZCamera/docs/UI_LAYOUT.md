# Live 중심 UI 구성

## 변경 범위

Qt 6 Widgets와 기존 dark theme을 유지한다. VMS/Pi/Web의 구현이나 API, Qt의
VmsClient 요청·응답 처리, RTSP transport, 녹화 정책, Tracking 명령, Gemini 검색은 변경하지 않는다.
새로운 모의 기능이나 서버 capability를 추가하지 않는다. 기존 Dummy 모드는 Settings로 이동하며
사용 중에는 상단 DEMO 표시를 유지한다. 시작 페이지는 LIVE다.

## 코드와 연결

| 화면 | 기존 클래스 | 기존 신호 경로 |
|---|---|---|
| LIVE 카메라 목록 | CameraListWidget | cameraSelected → MainWindow::selectCamera |
| LIVE 영상·REC toolbar | CameraViewWidget | start/stopRequested 및 recording 요청 → MainWindow → VmsClient |
| LIVE PTZ·Tracking | PTZControlWidget / TrackingPanel | PtzCommandController 및 MainWindow → VmsClient |
| PLAYBACK | PlaybackWidget | searchRequested → VmsClient; 완료 로컬 파일은 Qt Multimedia |
| EVENTS | EventSearchWidget | recordSearchRequested / recordPlaybackRequested → 기존 VMS API |
| SEARCH | ChatSearchWidget | searchRequested → chatSearch; 결과 → 기존 Playback 경로 |
| SETTINGS 연결·장치 | ConnectionStatusWidget / DeviceInfoWidget | connect/disconnect, discover/register/list/refresh → 기존 MainWindow 연결 |
| SETTINGS 상세 진단·로그 | 기존 metadataDiagnostics / SystemLogWidget | 기존 상태 갱신 및 addLog |

조립 코드는 app/MainWindow.cpp에 유지한다. EVENTS의 활성 여부는 고정 탭 번호 대신
indexOf(m_events)로 결정한다. 이벤트·자연어 검색에서 녹화 열기는 기존 setCurrentWidget(m_playback)을 유지한다.
탭 이동은 위젯을 삭제하거나 재생기를 다시 만들지 않는다.

PLAYBACK의 왼쪽 QListView는 기존 recordingResults QComboBox의 모델을 그대로 사용한다.
선택을 기존 combo와 동기화하고 기존 openSelectedRecording으로 연결한다.
콤보는 숨겨서 두 개의 목록이 보이지 않게 한다. 파일 경로·offset·오류·seek 처리 코드는 그대로다.

## UI 입력 경계

Live를 떠날 때 기존 requestStop으로 진행 중인 수동 PTZ 입력을 정리한다.
검색/재생/설정에서는 PTZ 단축키를 비활성화한다. 카메라 상태 알림이 와도 이 조건을 유지한다.
수동 입력이 없으면 기존 PtzCommandController::cancel은 Stop을 보내지 않으므로
단순 탭 이동으로 자동 Tracking을 중단시키지 않는다. 녹화·RTSP 중단 요청도 추가하지 않는다.

Camera Name은 기존 상태 조회 값으로 표시한다. 기존 등록 API에 없는 이름 수정이나
수동 RTSP URI 등록은 추가하지 않았다. ONVIF 주소·계정·비밀번호·Profile 등록과 URI 조회를 재사용한다.
Events의 기존 TIME/CAMERA/TYPE/CONFIDENCE/PAN/TILT 열을 유지해 정보를 누락하지 않는다.

## 검증 구분

- Windows MSVC/Qt 6.11.2 및 Linux Qt 6.4.2 빌드.
- UI 테스트: 5개 페이지·기본 Live·설정 분리·수동 PTZ 중단·비관제 페이지 단축키 차단.
- 기존 테스트 서버: Connect/Disconnect, Discovery, Camera Add 요청, 상태 알림,
  REC 요청 연결, PTZ 응답 단계, Tracking 확인, Event/Search 및 Playback 전환.
- 실제 VMS 실행 파일을 임시 포트/DB로 실행: WS/직접 RTSP URI 조회, 이벤트 검색,
  합성 녹화 파일 연결과 offsetMs 조회. 운영 DB나 실제 Pi를 사용하지 않는다.
- 로컬 합성 H.264 파일: 새 목록 클릭 후 실제 프레임 표시, Pause/Stop, seek/offset.
- 실제 Pi의 Discovery·ONVIF 응답·TCP/UDP 프레임·모터 동작·녹화와 실제 Gemini API는
  이번 UI 회귀 검증의 실물 시험에 포함하지 않는다. 이 항목은 장비 연결 후 별도로 확인한다.

테스트 더블과 합성 영상은 기존 테스트 환경이며 제품 UI에 새 mock 기능을 추가한 것이 아니다.
`VMS_UI_LAYOUT_PREVIEW`를 지정해 UI 테스트를 실행하면 화면별 미리보기를 저장할 수 있다.
이 이미지는 DEMO 표시를 포함한 배치 확인용이며 실물 동작 증빙이 아니다.

2026-10-11 결과: 두 플랫폼 빌드 성공. Linux CTest 2/2 통과(UI 17,
통신 12개 성공; init/cleanup 포함). Windows UI 17개, 통신 11개 성공,
실제 C++ VMS 실행 테스트 1개는 Windows에서 생략하고 Linux에서 실행했다.
Windows 기본 플랫폼으로 5개 화면을 캡처해 배치를 확인했다.
Windows 실행 파일: `build/metadata-chat-msvc/PTZCamera/MiniVmsClient.exe`.
