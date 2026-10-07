# PTZ Object Tracking Camera

Windows PC용 Qt 6 Widgets 관제 클라이언트다. 카메라 영역에서 WebRTC, RTSP TCP, RTSP UDP를 선택한다. 기본 선택은 WebRTC이며 Start를 누르면 연결을 시작한다.

| 기능 | 주소/설정 |
| --- | --- |
| WebRTC | `http://192.168.0.92:8889/cam/` |
| RTSP TCP / UDP | `rtsp://192.168.0.92:8554/cam` (선택한 전송 방식으로 재생) |
| ONVIF 장치 서비스 | 기본 `http://192.168.0.92:8080/onvif/device_service` |
| Pi 제어/탐지 TCP | 기본 `192.168.0.92:5000`, 오른쪽 Connection 패널에서 변경 |

재생 중이라는 표시는 웹 페이지 로드 완료가 아닌 실제 영상 프레임 진행 또는 수신을 기준으로 한다. 재생 방식 선택을 바꾸면 이전 재생을 정지하고 새 방식을 시작한다. 스트림이 꺼져 있으면 카메라 영역에 `CONNECTION FAILED`가 표시되고 앱은 계속 열린다. 영상 재생과 TCP 제어 연결은 별개다.

## ONVIF에서 RTSP 주소 가져오기

1. 카메라 영역의 `Play Method`에서 `RTSP TCP` 또는 `RTSP UDP`를 선택하고 `ONVIF Settings`를 펼친다. WebRTC를 선택하면 ONVIF 설정은 숨겨진다.
2. `Search LAN`을 눌러 같은 LAN의 ONVIF 카메라를 검색하고 목록에서 장치를 선택한다. 검색은 UDP 멀티캐스트 `239.255.255.250:3702`로 3초 동안 진행된다.
3. 검색 결과가 없으면 `Service URL`에 `http://192.168.0.92:8080/onvif/device_service`를 직접 입력한다. Pi와 PC의 네트워크 또는 Windows 방화벽이 멀티캐스트를 막아도 이 방법으로 조회할 수 있다.
4. 장치가 인증을 요구하면 User/Password를 입력하고 `Get RTSP URL`을 누른다. 첫 번째 Media 프로파일의 주소가 표시되며 RTSP 플레이어에 적용된다. 조회에 성공하면 ONVIF 설정이 접혀 영상 공간이 넓어진다. 영상이 이미 재생 중이면 새 주소로 다시 시작한다.
5. `Start`를 눌러 실제 영상을 확인한다. `Get RTSP URL` 성공은 영상 재생 성공과 다르며, `PLAYING`은 영상 프레임이 도착했을 때 표시된다.

ONVIF 설정 토글은 영상 재생을 다시 시작하지 않는다. ONVIF 검색·주소 조회와 오른쪽 Connection 패널의 PTZ 제어용 TCP `5000` 연결은 별개다. RTSP TCP/UDP 모드를 바꾼 뒤 다른 전송 방식의 주소가 필요하면 `Get RTSP URL`을 다시 누른다. 로그인 정보는 저장 파일에 기록하지 않으며, 화면의 RTSP URL 칸에는 사용자 정보를 숨긴다.

## Qt Creator에서 완성 빌드

1. Windows에 **Qt 6.11.2 MSVC 2022 x64 Kit**과 `WebEngineWidgets`, `Multimedia`, `MultimediaWidgets`, `WebChannel`, `Positioning` 모듈을 설치한다.
2. Qt Creator에서 저장소 최상위의 `CMakeLists.txt`를 연다.
3. MSVC Kit을 선택하고 CMake 옵션 `PTZ_WITH_WEBENGINE=ON`(기본값)으로 구성한다. 빌드 디렉터리는 이전 산출물과 분리된 `build-msvc`를 사용한다.
4. 빌드 후 실행해 카메라 영역에서 WebRTC, RTSP TCP, RTSP UDP 중 하나를 고르고 Start를 누른다. `Connect` 버튼은 Pi의 별도 TCP 5000 서버에 연결한다.

명령줄에서는 프로젝트 최상위 폴더에서 구성한다.

```powershell
cd C:\path\to\Qt_Client
cmake -S . -B build-msvc -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH="C:/Qt/6.11.2/msvc2022_64" -DPTZ_WITH_WEBENGINE=ON
cmake --build build-msvc --config Debug
```

Qt Creator 밖으로 실행 파일을 복사할 때는 같은 MSVC Kit의 `windeployqt`로 Qt WebEngine과 Multimedia 실행 파일 및 플러그인을 배치한다.

## 소스 디렉터리

| 경로 | 역할 |
| --- | --- |
| `app/` | 앱 시작점과 메인 화면 |
| `camera/` | 카메라 화면, 재생 방식 선택, 공통 상태 표시 |
| `camera/rtsp/` | RTSP 재생 (`QMediaPlayer`, `QVideoWidget`) |
| `camera/webrtc/` | WebRTC 페이지 재생 (`QWebEngineView`) |
| `network/` | Pi 제어·탐지용 TCP 소켓과 텍스트 프로토콜 |
| `network/onvif/` | WS-Discovery 검색, ONVIF Media 주소 조회 |
| `ui/` | PTZ, 추적, 연결 상태 패널 |
| `model/` | 추적 정보 자료형 |
| `resources/` | 스타일시트 |

현재 MinGW Kit에서 RTSP 경로만 컴파일 확인하려면 `-DPTZ_WITH_WEBENGINE=OFF`로 별도 빌드할 수 있다. 이 빌드에서는 WebRTC를 선택하면 사용할 수 없다는 오류가 표시된다.

Pi 영상 서비스가 꺼져 있으면 Pi에서 `sudo systemctl start ptz-camera.service`로 켠 뒤 다시 Start를 누른다. 이 명령은 PC가 아닌 Pi에서 실행한다.

## 조작

- 카메라 영역: WebRTC/RTSP TCP/RTSP UDP 선택, Start/Stop, 연결 상태 확인
- PTZ: 화면 버튼 또는 방향키, `R`로 원위치 명령
- TCP 제어: 오른쪽 Connection 패널의 IP·Port와 Connect/Disconnect

UDP 모드는 Pi RTSP 서버가 UDP 전송을 허용해야 재생된다. 지원 여부와 지연 차이는 실제 스트림으로 확인해야 한다.

기술 선택 기록은 [docs/tech-decisions/0003-selectable-rtsp-transport.md](docs/tech-decisions/0003-selectable-rtsp-transport.md)와 [docs/tech-decisions/0004-onvif-stream-uri.md](docs/tech-decisions/0004-onvif-stream-uri.md)에 있다.
