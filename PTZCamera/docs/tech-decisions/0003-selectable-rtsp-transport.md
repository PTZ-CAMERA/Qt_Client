# 0003: 카메라 재생 화면에서 RTSP TCP/UDP 선택

- 날짜: 2026-10-02
- 상태: 구현, Windows 빌드 및 Pi 실재생 검증 대기
- 이전 결정: [0002 WebRTC/RTSP 재생 선택](0002-selectable-camera-playback.md)

## 문제와 제약

사용자는 기존 WebRTC/RTSP 선택기에 RTSP UDP를 추가해 WebRTC, RTSP TCP, RTSP UDP 세 가지를 화면에서 선택하려 한다. Pi 주소와 RTSP 경로는 동일하다. 기존 WebRTC 기본값, 한 번에 한 재생만 실행하는 동작, PTZ 제어는 유지한다. Pi가 UDP 전송을 허용하는지는 아직 확인하지 못했다.

## 선택과 사용 방식

`CameraPlaybackWidget`에 세 가지 모드를 둔다. RTSP 두 모드는 동일한 `QMediaPlayer`와 `QVideoWidget`을 사용한다. 모드를 바꾸면 이전 플레이어를 정지한 뒤, `RtspPlayerWidget::start()`에서 `QT_FFMPEG_RTSP_TRANSPORT`를 `tcp` 또는 `udp`로 지정하고 RTSP 소스를 연다. 앱 시작 시 Qt Multimedia 백엔드를 FFmpeg로 지정한다. RTSP TCP/UDP 모두 유효한 영상 프레임이 들어와야 `PLAYING`으로 표시한다.

Qt 문서에 따르면 이 환경 변수의 값은 FFmpeg의 RTSP `rtsp_transport` 옵션에 전달된다. RTSP 제어 연결 주소 `rtsp://192.168.0.92:8554/cam`은 두 모드에서 같다. UDP 모드는 RTSP 전체를 UDP로 바꾸는 것이 아니라, 영상 전송에 UDP를 요청하는 방식이다.

## 검토한 후보

| 후보 | 판단 |
| --- | --- |
| Qt FFmpeg 백엔드 설정값을 재생 시작 시 변경 | 기존 Qt Multimedia 재생 코드를 재사용하면서 사용자가 요청한 세 모드를 제공할 수 있어 선택했다. |
| RTSP TCP만 유지 | 기존 구현이 단순하지만 UDP 비교가 불가능하다. |
| FFmpeg 라이브러리를 직접 연동 | 플레이어별 옵션을 더 세밀하게 제어할 수 있지만 디코딩, 영상 출력, 배포 의존성이 늘어난다. 현재 세 모드 UI에는 과하다. |
| UDP용 별도 외부 플레이어 프로세스 실행 | 프로세스별 환경 변수를 격리할 수 있지만 창 내 표시와 상태 관리가 복잡해진다. |

## 제한, 가정 및 검증

- `QT_FFMPEG_RTSP_TRANSPORT`는 Qt FFmpeg 백엔드의 환경 변수 기반 설정이다. Qt 문서는 이를 변경될 수 있는 비공개 API로 분류한다. 버전 갱신 시 다시 확인해야 한다.
- 설정값이 프로세스 전체에 적용되므로 RTSP 플레이어를 동시에 여러 개 실행하는 구조에는 적합하지 않다. 현재 앱에는 RTSP 플레이어가 하나다. 빠른 모드 전환 중 이전 비동기 열기 작업이 새 설정값을 읽는지는 Windows 실기 검증이 필요하다.
- Pi 서버가 UDP 전송을 거절하거나 네트워크에서 RTP UDP 패킷이 막히면 UDP 모드는 실패할 수 있다. 실패 시 TCP 또는 WebRTC를 선택한다.
- 현재 관리형 WSL에서는 Windows Qt 빌드와 Pi 네트워크 접속이 허용되지 않아 실제 빌드, 재생, 전환, 지연을 확인하지 못했다. TCP와 UDP의 지연 차이는 같은 장면에서 측정해야 한다.

## 참고 문서

- [Qt FFmpeg RTSP 전송 설정](https://doc.qt.io/qt-6/advanced-ffmpeg-configuration.html)
- [Qt Multimedia 백엔드 선택](https://doc.qt.io/qt-6/qtmultimedia-index.html)
