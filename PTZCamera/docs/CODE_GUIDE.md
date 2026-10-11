# Qt 코드 구조와 책임

Qt Creator의 진입 CMakeLists.txt, PTZCamera/ 하위 모듈과 legacy 경로는 유지한다. 관제 페이지 배치와 입력 경계는 [UI_LAYOUT.md](UI_LAYOUT.md)를 참고한다. 기존 backend 계약과 빌드 target은 유지한다.

| 폴더 | 책임 |
|---|---|
| app | MainWindow: 위젯 조립·카메라 선택·상태 배분 |
| network | VmsClient: 비동기 HTTP/WS·requestId·timeout·결과 파싱 |
| camera | 직접 RTSP 표시·영상 위젯·좌표 변환 |
| ui | PTZ 입력·갱신·Tracking 요청·상태 표시 |
| events / chat | 서버 검색 조건·결과·페이지·선택 |
| playback | 완료된 로컬 파일·offset·seek·백엔드 실패 |
| device / log / model | 접속·장치·진단 / 표시 / UI DTO |
| demo / legacy | 모의 데이터 / 이전 진단 구현. 기본 실행과 구분 |

## 먼저 읽을 코드

MainWindow → VmsClient → CameraViewWidget/CameraWidget → PtzCommandController → EventSearchWidget/ChatSearchWidget → PlaybackWidget 순서로 확인한다.

## 유지 규칙

- UI는 ONVIF·SQLite·Gemini를 직접 처리하지 않고 VMS 계약을 사용한다.
- 늦은 응답은 requestId와 cameraId/연결 세대로 필터링한다.
- PTZ 접수/Pi 응답/실제 Tracking metadata는 서로 다른 상태다.
- bbox는 기준 영상 크기에 따라 표시하며 오래된 박스는 제거한다.
- 로컬 녹화 경로의 접근 가능 여부와 서버의 playable 판정은 둘 다 확인한다.
- 추정 offset과 2초 사전 재생은 정확한 센서 프레임 동기화가 아니다.

이 문서는 코드를 읽는 지도다. 구체적인 기능·포트·Pi 버전 차이는 [루트 README](../../README.md), 구현 선택은 [tech-decisions](tech-decisions/)를 기준으로 확인한다.
