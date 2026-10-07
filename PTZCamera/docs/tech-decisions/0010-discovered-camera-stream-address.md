# 0010: 검색된 카메라 옆에 VMS RTSP 주소 표시

- 날짜: 2026-10-08
- 상태: Qt/VMS 구현 및 로컬 테스트 완료. Windows/Pi 확인 대기.

## 문제와 제약

사용자는 GET URL/COPY 대신 DISCOVERED CAMERAS 옆에 RTSP 주소를 표시하고 ADD CAMERA로 연결하기를 원한다. 기존 REST 응답은 등록/수신 후에만 ready URI를 제공했다. Qt가 Pi에 직접 연결하거나 수신 전에 Ready/Live를 표시하면 안 된다.

## 선택과 사용

기존 DISCOVER_CAMERAS 응답에 cameraId, rtspUri, registered, ready를 추가한다. 서버가 ONVIF 서비스 URL별로 카메라 ID/릴레이 주소를 할당하며 검색에는 ingest를 시작하지 않는다. REGISTER_CAMERA는 같은 ID를 사용하고 기존 ONVIF 검증/프로파일 조회/수신을 시작한다. 원본 카메라 RTSP URL과 자격 정보는 검색 응답에 노출하지 않는다.

Qt는 검색 항목 선택 시 주소와 Not added/Waiting/Ready를 표시한다. ADD 후 Connecting으로 바꾸고 기존 REST 준비 확인과 영상 시작을 유지한다. 주소 할당은 연결 성공과 다르며 Live는 실제 Qt 프레임으로 판정한다. GET URL/COPY와 수동 주소 조회 분기를 제거한다. 검색 대상은 왼쪽 Live 카메라와 별도로 관리한다.

서버 예약은 control thread에서 최대 64개를 보관하고 등록 한도는 기존 16대를 유지한다. 기존/configured ID를 우선 사용하며 예약은 프로세스 수명 동안 유지한다. v1 응답의 추가 필드라서 기존 클라이언트와 호환된다.

## 대안과 이유

- 검색 중 자동 등록: ADD 전에 스트림을 시작하고 계정/프로파일 선택을 우회하므로 제외한다.
- 원본 RTSP URI 표시: 기존 GET URL의 VMS 주소와 다르며 연결 경계를 혼동시킨다.
- Qt에서 URI 추측: ID/포트/public host와 충돌할 수 있어 서버가 할당한다.
- 등록 후 주소 표시: 요청한 검색→주소 확인→ADD 순서를 충족하지 못한다.

## 검증과 제한

Qt UI 테스트로 버튼 삭제, URI 선택 전환, 등록 대상/상태, 검색 초기화, 다른 카메라/자격 정보 URI 거부를 확인했다. 로컬 VMS SOAP/RTSP fixture로 검색 단계 upstream 연결 0개, ADD 후 같은 ID/URI의 ready REST 응답, 재검색 registered/ready 상태를 확인했다. Windows Qt+Pi 검증은 별도 필요하다. 이전 VMS는 추가 필드가 없어 주소 미제공 안내가 나오므로 서버도 재빌드해야 한다. 실제 표시 지연 수치는 주장하지 않는다.
