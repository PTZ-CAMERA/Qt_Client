# VMS ONVIF 추적 제어 연동

Pi의 기존 MoveAndStartTracking/Stop을 재사용한다. Qt에서 GPIO·탐지·ONVIF adapter를 중복 구현하지 않고 기존 VmsClient/PTZ_RESULT 계약으로 TRACKING_ON/OFF를 요청한다. capabilities.tracking은 서버가 실제 PTZVector 지원을 조회한 결과이다.

수동 Stop 응답 전 ON을 보내면 PTZ_STOPPING이 될 수 있어 기존 deferred PTZ 경로로 순서를 보장한다. tracking 명령은 클릭형이며 200ms 반복 송신하지 않는다. 요청·Pi 응답은 별도 문구로 표시하고 TrackingPanel의 ON/OFF는 확인된 metadata만 사용한다.

기존 focus cancel은 idle 상태에도 Stop을 보냈다. 수동 timer가 활성일 때만 cancel Stop을 보내도록 변경해 자동 추적이 UI focus 이동으로 꺼지지 않게 한다. 단절은 서버의 세션 정리로 Stop을 시도한다. 다른 카메라로 전환 시 자신의 추적 요청은 정리한다.

대안: 버튼만 켜거나 Pi 내부 TCP를 직접 사용하면 VMS routing/권한/오류 처리를 우회한다. 별도 worker를 추가하면 수동 PTZ와 경쟁하므로 기존 카메라별 PTZ worker를 재사용한다. SOAP 성공을 즉시 ON으로 표시하면 실제 모드와 혼동하므로 이벤트 확인을 기다린다.

검증 범위는 모의 서버와 자동 UI 시험이다. 실제 AI 대상 추적·GPIO 동작 및 장시간 시험은 사용자가 수행한다. 성능 수치는 측정 없이 주장하지 않는다.
