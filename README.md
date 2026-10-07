# PTZ Camera Qt Client

Raspberry Pi 카메라 영상과 객체 추적 상태를 표시하고 PTZ 명령을 보내는 Qt 6 Widgets 클라이언트입니다. Windows PC에서 실행합니다.

## 현재 기능

- WebRTC, RTSP TCP, RTSP UDP 영상 재생 선택 (기본 WebRTC)
- Qt 화면에서 ONVIF 카메라 검색 또는 서비스 주소 입력 후 RTSP URL 조회
- PTZ 버튼, 방향키, `R` 키를 통한 원위치 명령
- 추적 상태·객체 정보·연결 상태·시스템 로그 UI
- Raspberry Pi 제어 서버에 연결하는 TCP 클라이언트 초안

영상 재생 주소와 TCP 제어 주소는 별개입니다. 현재 기본값은 Pi `192.168.0.92`이며, 자세한 주소와 기능 설명은 [PTZCamera/README.md](PTZCamera/README.md)에 있습니다.

## 빌드

Qt 6.11.2 MSVC 2022 x64와 `WebEngineWidgets`, `Multimedia`, `MultimediaWidgets`, `WebChannel`, `Positioning` 모듈이 필요합니다. Qt Creator에서 저장소 최상위의 `CMakeLists.txt`를 열고 MSVC Kit을 선택하세요. 기존 MinGW 산출물과 분리된 새 빌드 디렉터리를 사용합니다.

이 작업 환경에서는 Windows 실행 파일 호출이 막혀 있어 현재 코드의 MSVC 구성·빌드·실행은 아직 검증되지 않았습니다.

## 문서

- [상세 사용법과 빌드 방법](PTZCamera/README.md)
- [기술 선택 기록](PTZCamera/docs/tech-decisions/)
- [작업 기록](PTZCamera/docs/session-logs/)
