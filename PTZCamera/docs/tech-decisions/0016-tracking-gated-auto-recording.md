# Tracking ON 기반 탐지 자동 녹화

사용자 요구에 따라 자동 녹화는 Pi 메타데이터로 확인된 Tracking ON과 사람 탐지가 함께 있을 때 시작한다. Qt는 기존 QWebSocket으로 SET_AUTO_RECORDING을 요청하며 VMS가 녹화 판단을 소유한다. 클라이언트에서 타이머와 recorder를 제어하면 연결 종료·다른 클라이언트·수동 녹화와 충돌하므로 서버의 기존 녹화 worker를 재사용했다.

진단 패널 위 체크박스로 모드를 선택한다. 응답 전에는 비활성화하며 requestId/cameraId를 확인한다. GET_METADATA_STATUS로 늦게 연결한 UI도 실제 모드 상태를 조회한다. 수동 녹화는 자동 종료 규칙에 영향을 받지 않는다. Tracking OFF는 자동 녹화만 종료한다.

사전 녹화 버퍼는 없고 다음 keyframe부터 저장한다. 이벤트 오류는 사람 없음으로 해석하지 않는다. 실물 검증은 Pi 온도가 안정된 후 사용자가 수행한다.
