# 0004: Qt UI에서 ONVIF 검색과 RTSP 주소 조회

- 날짜: 2026-10-07
- 상태: Qt 코드 구현, Windows 실기 검증 대기

## 문제와 제약

카메라 RTSP 주소를 코드에만 고정하지 않고, 사용자가 Qt UI에서 같은 LAN의 ONVIF 카메라를 검색해 주소를 받아와야 한다. Pi 측은 WS-Discovery 응답과 ONVIF 장치 서비스를 구현했고, 알려진 장치 서비스 주소는 `http://192.168.0.92:8080/onvif/device_service`다. 실제 Windows PC의 Qt에서 발견·조회·재생되는지는 아직 확인하지 못했다. 네트워크 검색이 실패해도 주소 직접 입력 경로가 필요하다.

## 선택한 기술과 사용 방식

Qt Network의 `QUdpSocket`으로 `239.255.255.250:3702`에 WS-Discovery Probe를 보내고, 일치하는 ProbeMatches의 XAddrs를 장치 목록에 표시한다. 선택한 장치 서비스에 `QNetworkAccessManager`로 ONVIF SOAP 요청을 보내 Media 서비스 주소, 첫 번째 프로파일 토큰, RTSP Stream URI를 순서대로 조회한다. GetServices 결과가 없으면 GetCapabilities의 Media XAddr를 사용한다. Media1을 우선하고 Media2만 있으면 Media2를 사용한다.

화면에는 `Search LAN`, 장치 목록, 직접 입력할 수 있는 서비스 URL, 계정 입력, `Get RTSP URL`, 결과 주소를 둔다. 가져온 RTSP 주소는 기존 `QMediaPlayer`에 적용한다. TCP/UDP 선택에 맞는 ONVIF 전송 옵션을 요청한다. 인증이 필요한 장치에는 WS-Security UsernameToken PasswordDigest와 Qt의 HTTP 인증 처리를 사용한다. 표시·로그에서 비밀번호를 숨긴다.

## 검토한 후보

| 후보 | 선택 이유 또는 제외 이유 |
| --- | --- |
| WS-Discovery + ONVIF Media 조회 | 사용자가 장치를 목록에서 고르고 표준 GetStreamUri 응답을 사용할 수 있어 선택했다. |
| ONVIF 장치 서비스 주소만 수동 입력 | 멀티캐스트가 막힌 환경에는 필요하지만 자동 검색 요구를 충족하지 못한다. 따라서 대체 입력 경로로 남겼다. |
| RTSP 주소를 계속 고정 | Pi 한 대에서는 단순하지만 주소·프로파일이 바뀌면 사용자가 UI에서 갱신할 수 없다. |
| 외부 ONVIF SDK 추가 | 더 많은 장치 특수 동작을 지원할 수 있지만 현재 검색·첫 프로파일 조회에는 새 의존성과 배포 작업이 과하다. |

## 제한, 가정 및 검증 계획

- Pi가 WS-Discovery와 ONVIF Device/Media SOAP를 제공한다는 사용자 확인에 의존한다. Pi 서버의 실제 GetServices/GetCapabilities/GetProfiles/GetStreamUri 응답 형식과 인증 방식은 아직 받지 못했다.
- 현재 첫 번째 Media 프로파일을 자동 선택한다. 여러 프로파일 중 선택하는 UI는 없다.
- Windows 방화벽, VPN, 다중 네트워크 어댑터, 공유기 멀티캐스트 제한으로 검색이 실패할 수 있다. 장치 서비스 URL 직접 입력으로 같은 SOAP 조회를 시험한다.
- 장치가 반환한 RTSP URL의 호스트가 `0.0.0.0` 또는 로컬 루프백이면 장치 서비스의 호스트로 바꾼다. 다른 잘못된 호스트 주소는 실기 확인 후 처리한다.
- 이 관리형 WSL 환경에는 Qt CMake 빌드 도구가 없고 Pi 네트워크 접속도 차단돼 있다. Windows Qt Creator에서 빌드한 뒤 검색 목록, 수동 조회, 인증, 실제 RTSP 프레임 수신을 검증해야 한다.
- ONVIF 주소 조회는 PTZ 제어용 TCP `5000` 및 WebRTC 송출을 변경하지 않는다.

## 근거

- [ONVIF 개발자 가이드: Probe, ProbeMatches, XAddrs](https://www.onvif.org/wp-content/uploads/2016/12/ONVIF_WG-APG-Application_Programmers_Guide-1.pdf)
- [ONVIF Media 서비스: GetStreamUri](https://www.onvif.org/specs/srv/media/ONVIF-Media-Service-Spec-v221.pdf)
- [ONVIF Media2 서비스: GetStreamUri](https://www.onvif.org/specs/srv/media/ONVIF-Media2-Service-Spec-v1912.pdf)
- [Qt QNetworkAccessManager 인증](https://doc.qt.io/qt-6/qnetworkaccessmanager.html)
