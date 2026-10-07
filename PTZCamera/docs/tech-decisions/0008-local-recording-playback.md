# 0008: 동일 PC의 VMS 녹화 파일 재생과 사용 도움말

- 날짜: 2026-10-07
- 상태: 구현, Linux 재생/UI 검증 완료. Windows 실서버 파일 재생 검증 대기.
- 이전 결정: [0007](0007-supported-vms-ui.md)의 FILE INFO 전용 동작을 로컬 파일 재생으로 확장한다.

## 문제와 제약

사용자는 녹화 검색에서 파일을 선택해 실제 영상을 보고 싶다. Qt와 VMS는 같은 PC에서 실행되며 VMS GET_RECORDINGS는 완료된 파일의 절대 경로와 시작/종료 시각을 반환한다. 원격 재생/파일 전송 API는 현재 없으므로 이를 임의로 가정하지 않는다. 기존 탭과 화면 배치를 유지한다.

## 선택한 기술과 사용

기존 Qt Multimedia의 QMediaPlayer/QVideoSink를 재사용한다. QFile을 읽기 전용으로 열어 setSourceDevice로 전달하며, 파일은 플레이어가 소유해 디코더보다 먼저 파괴되지 않게 한다. QVideoFrame을 QImage로 변환해 기존 CameraWidget에 표시한다. 새 FFmpeg SDK/WebEngine/MultimediaWidgets 의존성은 추가하지 않는다.

녹화 검색은 계속 VMS API를 사용한다. 결과는 처음에 선택 없음으로 표시하며 사용자가 항목을 선택하면 실제 재생을 시작한다. OPEN으로 선택한 파일을 다시 열고 OPEN FILE로 로컬 파일을 직접 고를 수 있다. Play/Pause/Stop, seek 가능한 파일의 ±10초/타임라인을 제공한다. 파일이 없거나 아직 .part 상태면 재생하지 않고 오류를 표시한다.

VMS Host가 localhost 또는 loopback인 경우에만 API의 서버 파일 경로를 자동으로 연다. Windows에서 같은 PC의 WSL /mnt/{drive}/ 경로는 Windows 드라이브 경로로 변환한다. 원격 VMS의 경로는 로컬 파일로 해석하지 않고 로컬 복사본을 OPEN FILE로 선택하도록 안내한다. UI가 SQLite를 직접 열지 않는다.

비동기 플레이어/프레임 콜백에 세대 번호를 적용해 이전 파일의 늦은 프레임을 무시한다. 파일 열기 실패가 동기적으로 전달돼 플레이어가 정리된 경우 다시 play를 호출하지 않는다. 파일 변경/clear/종료 시 플레이어를 정지·정리한다. 영상 로드 15초 타임아웃을 표시한다. 실제 프레임 수신 전에 재생 조작은 비활성화한다.

상단 ? 및 F1은 비모달 HelpDialog를 연다. 연결/라이브, 녹화/재생, 상태/문제 해결을 한국어로 설명한다. 같은 도움말 창을 다시 사용하며 모의 기능과 실제 미지원 API를 구분한다.

## 대안과 선택 이유

- Qt Multimedia 로컬 파일 재생: 현재 모듈과 영상 위젯을 재사용하고 동일 PC의 저장 파일에 접근할 수 있어 선택했다.
- VMS HTTP/RTSP 녹화 재생 API: 다른 PC에서도 재생할 수 있지만 현재 서버 계약/구현이 없어 이번 변경에서는 가정하지 않는다. 향후 원격 지원 시 검토한다.
- 직접 FFmpeg 디코더 작성: 시간 이동과 자원 관리에 새 구현이 필요하다. 현재 파일 재생 요구에는 기존 Qt API를 사용한다.
- 운영체제 외부 플레이어 실행: 통합 타임라인과 앱 내부 영상 표시 요구를 충족하지 못한다.
- 계속 FILE INFO만 제공: 사용자가 실제 녹화 영상을 볼 수 없으므로 이번 요청에 따라 변경했다.

## 검증 및 제한

Linux Qt 6.4.2에서 H.264 MKV 테스트 파일을 생성하고, 한글/공백 파일명으로 선택 재생·프레임 160×120·일시정지·정지·재열기·clear를 검사했다. missing/remote 파일은 오류와 비활성 조작을 확인했다. 도움말 열기/닫기/재열기 및 기존 UI도 검사했다.

이 Linux FFmpeg 백엔드는 테스트 MKV의 duration을 0으로 보고했다. 서버 시작/종료 시각으로 표시 범위는 구성하되 실제 백엔드가 duration/seek를 제공하지 않으면 시간 이동을 비활성화한다. 따라서 이 환경에서 실제 seek 성공은 검증하지 않았으며 Windows Qt 6.11.2에서 확인해야 한다. 화면 없는 테스트는 Vulkan 장치를 제외해 소프트웨어 디코딩을 사용한다. 앱의 Windows 디코더 설정은 변경하지 않는다.

실제 Pi 녹화 목록/Windows 파일 경로/코덱 배포 및 WSL 경로 변환은 Windows 실행 검증이 필요하다. 원격 파일 전송이나 미완료 녹화 재생은 지원하지 않는다. 시간/지연 성능 수치를 주장하지 않는다.

검증 결과: Linux 실행 파일/테스트 빌드와 MiniVmsUiTests 통과. H.264 MKV의 실제 프레임, pause/stop, 파일 재열기, 오류 처리, 도움말 재열기를 확인했다. seek는 백엔드 duration이 0인 경우 비활성화되는 경로를 확인했고, 정상 duration의 seek 성공은 Windows 검증 대상이다.

## 근거

- [Qt QMediaPlayer: source device, position, seekable](https://doc.qt.io/qt-6/qmediaplayer.html)
- [Qt QVideoSink: 영상 프레임 수신](https://doc.qt.io/qt-6/qvideosink.html)
- VMS src/core/Config.cpp의 절대 녹화 경로 구성 및 src/recording/MuxRecorder.cpp의 완료 파일 기록
