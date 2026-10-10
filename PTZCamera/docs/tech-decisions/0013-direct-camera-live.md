# Qt 카메라 직접 RTSP 라이브 (2026-10-10)

사용자 요청으로 라이브 경로를 Pi → Qt로 변경한다. PTZ·메타데이터·녹화·검색은 VMS에 유지한다.

기존 QMediaPlayer/FFmpeg backend를 재사용하고 VMS의 `/api/v1/cameras/{id}/direct-stream?transport=tcp|udp`에서 ONVIF로 조회한 실제 카메라 URI를 받는다. source=camera 계약을 검사하고 VMS 중계 주소로 자동 fallback하지 않는다. transport는 Qt UI에서 설정하며 원본 URI의 query는 변경하지 않는다.

대안: 기존 VMS RTSP relay는 Web 게이트웨이 입력에 계속 사용하지만 Qt 경로에서는 사용자 요구와 다르다. Qt에서 ONVIF를 다시 구현하면 인증·등록 코드가 중복되므로 VMS의 조회 기능을 재사용한다. 새 직접 FFmpeg decode worker는 필요하지 않다.

카메라 RTSP 인증이 필요하면 직접 URI에 서버 설정의 URL-encoded 인증이 포함될 수 있다. camera list/status/WS 방송에는 원본 URI를 넣지 않는다. Qt는 URI를 파일에 저장하지 않고 장치 표시·복사·tooltip에서는 userInfo를 제거한다. 로그도 계정을 숨긴다. 현재 API는 localhost 개발용이며 외부 운용의 인증/TLS는 별도 과제다.

제약: Qt PC가 Pi에 직접 도달해야 한다. RTSP TCP/UDP는 동일 주소에서 선택한다. VMS도 Web 중계·녹화용으로 별도의 Pi 연결을 유지한다. 지연 개선이나 처리량 수치는 측정 없이 주장하지 않는다. 모의 direct-stream 계약과 기존 로컬 녹화/UI 시험을 실행하고 실물 영상·서보·장시간 시험은 사용자가 수행한다.
