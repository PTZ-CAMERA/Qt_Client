# PTZ Object Tracking Camera

Windows PC용 Qt 6 Widgets 관제 클라이언트다. 카메라 영역에서 WebRTC와 RTSP를 선택한다. 기본 선택은 WebRTC이며 Start를 누르면 연결을 시작한다.

| 기능 | 주소/설정 |
| --- | --- |
| WebRTC | `http://192.168.0.92:8889/cam/` |
| RTSP | `rtsp://192.168.0.92:8554/cam` (`tcp` 전송) |
| Pi 제어/탐지 TCP | 기본 `192.168.0.92:5000`, 오른쪽 Connection 패널에서 변경 |

재생 중이라는 표시는 웹 페이지 로드 완료가 아닌 실제 영상 프레임 진행 또는 수신을 기준으로 한다. 재생 방식 선택을 바꾸면 이전 재생을 정지하고 새 방식을 시작한다. 스트림이 꺼져 있으면 카메라 영역에 `CONNECTION FAILED`가 표시되고 앱은 계속 열린다. 영상 재생과 TCP 제어 연결은 별개다.

## Qt Creator에서 완성 빌드

1. Windows에 **Qt 6.11.2 MSVC 2022 x64 Kit**과 `WebEngineWidgets`, `Multimedia`, `MultimediaWidgets`, `WebChannel`, `Positioning` 모듈을 설치한다.
2. Qt Creator에서 저장소 최상위의 `CMakeLists.txt`를 연다.
3. MSVC Kit을 선택하고 CMake 옵션 `PTZ_WITH_WEBENGINE=ON`(기본값)으로 구성한다. 빌드 디렉터리는 이전 산출물과 분리된 `build-msvc`를 사용한다.
4. 빌드 후 실행해 카메라 영역에서 WebRTC 또는 RTSP를 고르고 Start를 누른다. `Connect` 버튼은 Pi의 별도 TCP 5000 서버에 연결한다.

명령줄에서는 프로젝트 최상위 폴더에서 구성한다.

```powershell
cd C:\path\to\Qt_Client
cmake -S . -B build-msvc -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH="C:/Qt/6.11.2/msvc2022_64" -DPTZ_WITH_WEBENGINE=ON
cmake --build build-msvc --config Debug
```

Qt Creator 밖으로 실행 파일을 복사할 때는 같은 MSVC Kit의 `windeployqt`로 Qt WebEngine과 Multimedia 실행 파일 및 플러그인을 배치한다.

현재 MinGW Kit에서 RTSP 경로만 컴파일 확인하려면 `-DPTZ_WITH_WEBENGINE=OFF`로 별도 빌드할 수 있다. 이 빌드에서는 WebRTC를 선택하면 사용할 수 없다는 오류가 표시된다.

Pi 영상 서비스가 꺼져 있으면 Pi에서 `sudo systemctl start ptz-camera.service`로 켠 뒤 다시 Start를 누른다. 이 명령은 PC가 아닌 Pi에서 실행한다.

## 조작

- 카메라 영역: WebRTC/RTSP 선택, Start/Stop, 연결 상태 확인
- PTZ: 화면 버튼 또는 방향키, `R`로 원위치 명령
- TCP 제어: 오른쪽 Connection 패널의 IP·Port와 Connect/Disconnect

기술 선택 기록은 [docs/tech-decisions/0002-selectable-camera-playback.md](docs/tech-decisions/0002-selectable-camera-playback.md)에 있다.
