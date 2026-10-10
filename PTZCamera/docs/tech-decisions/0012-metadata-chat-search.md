# VMS metadata·탐지·채팅 검색 연결

## 문제와 선택

VMS의 ONVIF metadata/탐지 검색/Gemini CHAT_SEARCH 계약을 기존 Qt 6 Widgets 화면에 연결한다.
QWebSocket/QJson DTO와 signal/slot을 재사용하고 EVENTS에 탐지 샘플/상태 이력·confidence·페이지를 추가한다.
CHAT SEARCH는 별도 Widgets 탭이며 모델/API 키/영상 분석은 VMS 책임으로 유지한다.

## 대안

- Qt에서 Gemini 직접 호출: 키가 클라이언트로 복제되고 VMS DB와 상태가 분리되므로 선택하지 않는다.
- WebEngine/QML로 화면 재작성: 기존 Qt Widgets/Multimedia 재생 경로가 있어 새 dependency와 UI 교체가 필요하지 않다.
- null을 기본 0으로 변환: 모르는 confidence/각도를 실제 값으로 표시하므로 선택하지 않는다.

## 계약과 제한

CAMERA_METADATA는 topic별로 병합한다. 새 detection의 누락 bbox는 이전 상자를 재사용하지 않는다.
reference imageWidth/imageHeight로 bbox를 표시하고 false/Unavailable/2초 갱신 중단 시 지운다.
영상 프레임과 metadata의 정확한 동기화는 검증되지 않았으며 tooltip에 표시한다.
64bit frame 필드는 QJson 문자열로 보존한다. 추적 ON/OFF 제어는 VMS 미지원이므로 실제 모드에서 비활성화한다.

GET_EVENTS/GET_DETECTIONS/GET_EVENT_PLAYBACK/CHAT_SEARCH는 비동기 요청이며 requestId로 오래된 결과를 제거한다.
연결 종료·Dummy 전환·카메라 선택 변경 시 검색 상태를 정리한다. CHAT_SEARCH timeout은 45초다.
재생은 playable 검사, cameraId/파일/시간 검증 후 기존 로컬 player에 offsetMs로 이동한다.
receive_estimated는 추정 위치로 표시하며 원격 파일 다운로드는 구현하지 않는다.
검색 offset이 양수면 local file URL로 열어 Qt backend의 seek 정보를 받는다.
기존 0초 파일 재생은 QFile/sourceDevice를 유지한다. seek 불가/실제 파일 범위 밖이면
검색 시각 재생 오류로 표시하며 처음부터 재생한 것을 검색 위치라고 표시하지 않는다.

## 검증

Qt mock WS 및 실제 VMS fixture로 nullable metadata/topic 격리, 탐지/채팅 검색,
페이지·오류·연결 해제·재생 가능 여부와 ms offset을 검사한다. Pi 실물/장시간 시험은 사용자가 수행한다.
