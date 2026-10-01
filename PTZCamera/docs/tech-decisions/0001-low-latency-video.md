# 0001: Qt 영상 수신과 저지연 표시 방식

- 날짜: 2026-10-01
- 상태: 0002 결정으로 재생 UI와 RTSP 전송 설정이 변경됨. 이 문서는 이전 선택의 기록이다.

## 문제와 제약

Raspberry Pi의 카메라 영상을 Windows용 Qt 6 Widgets 관제 화면에 표시한다. 사람이 조작하는 PTZ 화면이므로 영상 지연을 낮추는 것이 우선이다. 기존 `CameraWidget`의 프레임 중심 표시와 탐지 오버레이를 유지해야 한다. 현재 PC의 빌드 환경은 Qt 6.11.0 MinGW 64비트이며 Qt Multimedia가 설치되어 있다.

현재 Pi의 영상 경로는 `rtsp://192.168.0.92:8554/cam`과 WebRTC 페이지 `http://192.168.0.92:8889/cam/`이다. 영상 경로와 PTZ/탐지 텍스트용 TCP 포트 5000은 별개다. 2026-10-01 확인 당시 WebRTC 페이지는 HTTP 200으로 응답했고, TCP 5000은 연결을 거부했다. 이 주소와 상태는 환경에 따라 바뀔 수 있다.

구현 직후 재확인에서는 Pi의 8554와 8889 포트가 연결을 거부했다. 따라서 이번 검증은 Qt 빌드와 창 실행까지 완료했고, 실제 Pi 영상을 받은 결과나 종단 간 지연 수치는 아직 없다.

## 선택

1. `QMediaPlayer`의 Qt Multimedia FFmpeg 백엔드에서 Pi의 RTSP 경로를 재생한다.
2. 같은 LAN에서 먼저 RTSP의 UDP 전송을 시도한다. `QT_FFMPEG_RTSP_TRANSPORT` 환경 변수를 사용자가 지정하면 그 값을 우선한다.
3. `QVideoSink`에서 받은 `QVideoFrame`을 `CameraWidget`의 `QPainter`에 직접 그린다. 같은 위젯에서 중심점과 탐지 표시를 이어서 그린다. 매 프레임 `QImage`로 변환하지 않는다.
4. RTSP 시작/중지 버튼을 TCP 제어 연결 버튼과 분리한다. TCP 서버가 준비되지 않아도 영상 확인이 가능하다.
5. Pi의 RTSP 주소나 포트가 바뀔 수 있으므로 Qt 화면에서 RTSP URL을 수정할 수 있게 한다. Pi가 제공하는 WebRTC 페이지 주소도 별도 입력란에 두고 시스템 브라우저에서 열어 비교할 수 있게 한다. WebRTC URL을 RTSP 재생기에 전달하지 않는다.

이 방식은 현재 설치된 Qt MinGW Kit으로 빌드할 수 있고, 기존 Widgets 화면과 오버레이 구조를 유지한다. MediaMTX 문서는 RTSP의 UDP를 성능상 유리한 전송 방식으로 설명한다. 다만 이것이 이 환경에서 가장 낮은 실제 표시 지연을 보장하지는 않는다.

Pi 측은 스트림 재시작 후 `http://192.168.0.92:8889/cam/`의 WebRTC 페이지와 실제 프레임 수신을 확인했다고 전달했다. 이는 Pi 측 검증 결과이며, Qt 내 WebRTC 재생 성공이나 RTSP 포트 변경을 뜻하지 않는다. 현재 Qt MinGW 설치에는 WebEngine이 없으므로 WebRTC는 외부 브라우저에서 열고, Qt 카메라 영역은 RTSP 주소가 확인되는 대로 연결한다.

## 후보와 판단

| 후보 | 장점 | 이번 선택에서 밀린 이유 |
| --- | --- | --- |
| WebRTC + `QWebEngineView` | MediaMTX의 저지연 브라우저 재생 경로를 이용할 수 있다. | 현재 MinGW Kit에 Qt WebEngine이 없고, Qt 문서상 Windows MinGW에서 WebEngine 빌드가 지원되지 않는다. MSVC Kit으로 옮기고 기존 오버레이를 웹뷰 위에 다시 구성해야 한다. |
| RTSP + Qt Multimedia + `QVideoFrame::toImage()` | 기존 `CameraWidget::setFrame(QImage)` API에 바로 맞는다. | 매 프레임 변환과 복사 비용이 생길 수 있어 직접 페인팅을 우선한다. 실제 성능 차이는 측정이 필요하다. |
| RTSP + `QVideoWidget` | 영상 표시 구현이 단순하다. | 현재 `CameraWidget`의 바운딩 박스 및 중심점 페인팅을 영상 위에 얹으려면 별도 오버레이 위젯과 좌표 동기화가 필요하다. |
| RTSP + 직접 FFmpeg/GStreamer 파이프라인 | 디코더 큐와 버퍼를 세밀하게 조정할 수 있다. | 의존성과 유지보수 비용이 크다. 현재 방식의 실측 지연이 목표를 만족하지 못할 때 검토한다. |
| HLS | 브라우저 접근과 배포가 쉽다. | MediaMTX 문서상 WebRTC보다 지연이 크다. PTZ 조작 화면의 우선순위에 맞지 않는다. |
| RTSP over TCP | 방화벽과 패킷 손실이 있는 망에서 연결하기 쉽다. | 같은 LAN에서 우선 UDP를 시험한다. UDP가 불안정하면 TCP로 전환해 측정한다. |

## 남은 확인

- 카메라 앞에서 시간 표시 또는 움직임을 촬영해 Pi 입력부터 Qt 표시까지의 지연을 실제 측정한다. UDP와 TCP를 같은 조건에서 비교한다.
- Pi 서버가 UDP RTP 포트를 허용하는지, 장시간 재생에서 프레임 정지나 누적 지연이 없는지 확인한다.
- Pi에서 변경한 RTSP URL과 포트를 확인한다. WebRTC의 HTTP 8889 포트를 RTSP URL에 넣어서는 재생되지 않는다.
- Qt Multimedia의 내부 버퍼링으로 지연 목표를 달성하지 못하면 직접 FFmpeg/GStreamer 파이프라인 또는 WebRTC를 다시 평가한다.
- 탐지 좌표가 영상 해상도와 일치하는지 확인한다. 현재 텍스트 프로토콜은 바운딩 박스 크기를 전달하지 않는다.

## 참고 자료

- [Qt Multimedia 개요](https://doc.qt.io/qt-6/qtmultimedia-index.html)
- [QVideoSink](https://doc.qt.io/qt-6/qvideosink.html), [QVideoFrame](https://doc.qt.io/qt-6/qvideoframe.html)
- [Qt FFmpeg RTSP 전송 설정](https://doc.qt.io/qt-6/advanced-ffmpeg-configuration.html)
- [Qt WebEngine 플랫폼 조건](https://doc.qt.io/qt-6/qtwebengine-platform-notes.html)
- [MediaMTX RTSP 전송 방식](https://mediamtx.org/docs/features/rtsp-specific-features)
- [MediaMTX 브라우저 재생](https://mediamtx.org/docs/read/web-browsers)
