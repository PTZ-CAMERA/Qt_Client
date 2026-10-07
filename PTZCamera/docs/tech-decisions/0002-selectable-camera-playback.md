# 0002: Qt 카메라 영역의 WebRTC/RTSP 재생 선택

- 날짜: 2026-10-01
- 상태: 0003 결정으로 RTSP 전송 방식 선택 기능이 추가됨. 이 문서는 이전 선택의 기록이다.
- 후속 결정: [0003 RTSP 전송 방식 선택](0003-selectable-rtsp-transport.md)
- 이전 결정: [0001 저지연 영상 수신](0001-low-latency-video.md)

## 문제와 제약

Windows PC의 Qt 6 Widgets 관제 화면에서 Pi가 이미 제공하는 WebRTC와 RTSP를 선택해 재생해야 한다. WebRTC 주소는 `http://192.168.0.92:8889/cam/`, RTSP 주소는 `rtsp://192.168.0.92:8554/cam`이다. 기본 선택은 WebRTC다. Pi의 RTSP 전송은 TCP다. PTZ 버튼과 방향키/R 명령은 유지해야 한다.

최초 구현 당시에는 Qt 6.11.0 MinGW 64비트 Kit에 `Qt6WebEngineWidgets`가 없었다. 이후 Windows PC에 Qt 6.11.2 MSVC 2022 x64 Kit과 WebEngine/Multimedia 모듈을 설치했다. Qt 공식 문서에 따르면 Windows용 Qt WebEngine은 MinGW로 빌드할 수 없다. 완성 빌드에는 MSVC Kit을 사용하고, 기존 MinGW Kit에서는 `PTZ_WITH_WEBENGINE=OFF`로 RTSP 경로만 진단 빌드할 수 있다.

## 선택과 사용 방식

카메라 영역에 `WebRTC / RTSP` 선택기, Start/Stop 버튼, 재생 상태를 둔다. WebRTC는 `QWebEngineView`로 MediaMTX 페이지를 표시한다. 페이지 로드 성공만으로 재생 성공을 표시하지 않고, 페이지의 `<video>` 요소에서 영상 크기와 재생 프레임 수 증가를 확인한 뒤 `PLAYING`으로 전환한다. RTSP는 `QMediaPlayer`와 `QVideoWidget`을 사용하고, `QVideoWidget::videoSink()`의 유효한 프레임 수신을 재생 성공 기준으로 삼는다. 연결 중과 실패도 별도 상태로 표시한다.

방식을 바꿀 때 `QMediaPlayer`를 중지하고 WebEngine 페이지를 `about:blank`로 옮긴 다음 선택한 방식만 시작한다. 비동기 JavaScript 결과는 세대 번호와 `QPointer`로 이전 재생 결과를 무시한다. 프레임이 10초 동안 오지 않거나 진행하지 않으면 연결 실패로 표시한다. RTSP 하위 전송은 앱 시작 시 `QT_FFMPEG_RTSP_TRANSPORT=tcp`로 설정한다.

영상 재생과 TCP 5000 제어 연결은 계속 독립적이다. Pi 송출 코드는 수정하지 않는다.

구현 위치는 `camera/CameraPlaybackWidget`(재생 방식과 공통 상태), `camera/rtsp/RtspPlayerWidget`(RTSP 프레임), `camera/webrtc/WebRtcPlayerWidget`(WebRTC 페이지와 프레임 진행), `network/NetworkClient`(TCP 제어·탐지 프로토콜)로 분리한다.

## 검토한 후보

| 후보 | 판단 |
| --- | --- |
| RTSP만 유지 | 현재 MinGW Kit으로 빌드하기 쉽지만 사용자가 요구한 WebRTC 기본 선택을 제공하지 못한다. |
| WebRTC 페이지를 외부 브라우저에서 열기 | 기존 Kit에서 가능하지만 관제 화면 안의 모드 전환과 재생 상태 표시를 제공하지 못한다. |
| WebRTC를 `QWebEngineView`로 내장하고 RTSP를 `QVideoWidget`으로 재생 | 요구한 두 방식의 화면 내 전환과 각 방식의 재생 상태 감지가 가능해 선택했다. WebEngine이 있는 MSVC Kit이 필요하다. |
| RTSP 프레임을 `QImage`로 변환해 기존 `CameraWidget`에 표시 | 기존 데모 오버레이를 재사용할 수 있지만, 이번 요구는 `QMediaPlayer`와 영상 출력 위젯 사용을 지정한다. 매 프레임 변환 비용도 피한다. |

## 제한과 검증 계획

- Windows PC에 WebEngine과 MSVC Qt Kit은 설치했으나, 현재 관리형 WSL 환경에서 Windows 실행 연동이 실패해 이 변경 후의 완성 빌드 및 WebRTC 런타임은 확인하지 못했다.
- WebRTC 페이지의 `<video>` 요소와 `getVideoPlaybackQuality().totalVideoFrames`에 상태 감지가 의존한다. Pi 페이지 구조가 달라지면 감지 코드를 조정해야 한다.
- Pi 스트림이 꺼져 있거나 프레임이 진행하지 않으면 실패 상태를 보이고 앱은 유지되어야 한다. 실제 Pi ON/OFF 실험이 필요하다.
- 데모 탐지 상자 오버레이는 기존 `CameraWidget`의 데모 화면에 표시된다. 실영상 탐지 상자에는 Pi 프로토콜의 바운딩 박스와 영상 좌표 동기화가 별도로 필요하다.
- 어느 재생 방식의 지연이 더 낮은지는 실측하지 않았다. 같은 장면을 촬영해 비교한다.

## 참고 문서

- [Qt WebEngine 플랫폼 조건](https://doc.qt.io/qt-6/qtwebengine-platform-notes.html)
- [QWebEnginePage JavaScript와 로드 상태](https://doc.qt.io/qt-6/qwebenginepage.html)
- [QVideoWidget](https://doc.qt.io/qt-6/qvideowidget.html)
- [Qt FFmpeg RTSP 전송 설정](https://doc.qt.io/qt-6/advanced-ffmpeg-configuration.html)
