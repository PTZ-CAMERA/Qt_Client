#include "HelpDialog.h"
#include <QDialogButtonBox>
#include <QTabWidget>
#include <QTextBrowser>
#include <QVBoxLayout>

HelpDialog::HelpDialog(QWidget *parent) : QDialog(parent) {
    setWindowTitle(QStringLiteral("Mini VMS 사용 도움말"));
    setObjectName(QStringLiteral("vmsHelpDialog"));
    // 한국어 설명은 Windows 기본 한글 글꼴로 읽기 쉽게 표시한다.
    setStyleSheet(QStringLiteral("QWidget { font-family: 'Malgun Gothic', 'Noto Sans CJK KR', 'Segoe UI'; font-size: 13px; }"));
    resize(720, 560); setMinimumSize(560, 420);
    auto *layout = new QVBoxLayout(this);
    auto *tabs = new QTabWidget(this); layout->addWidget(tabs, 1);
    const auto page = [this, tabs](const QString &title, const QString &html) {
        auto *text = new QTextBrowser(this); text->setOpenExternalLinks(false);
        text->setHtml(html); tabs->addTab(text, title);
    };
    page(QStringLiteral("연결 · 라이브"), QStringLiteral(
        "<h2>카메라 영상을 보는 순서</h2>"
        "<ol><li>Raspberry Pi 카메라와 PC의 VMS 서버를 실행합니다.</li>"
        "<li>실제 장치를 사용할 때는 상단 <b>Dummy Mode</b>를 해제합니다.</li>"
        "<li><b>DEVICE</b> 탭의 VMS SERVER에서 Host / Port를 입력하고 <b>Connect</b>를 누릅니다. 같은 PC의 기본값은 <b>127.0.0.1 / 5000</b>입니다.</li>"
        "<li>왼쪽 CAMERAS에서 등록된 카메라를 선택합니다. 새 장치는 <b>DISCOVER CAMERAS</b>로 찾고 DISCOVERED CAMERAS에서 선택한 뒤, 필요한 계정 정보를 입력하고 <b>ADD CAMERA</b>로 등록합니다.</li>"
        "<li>중앙 LIVE VIEW의 <b>Start</b>를 누릅니다. VMS에서 Pi 원본 URI를 받아 직접 RTSP로 연결합니다. 실제 프레임 수신 후 <b>Live (Camera)</b>가 표시됩니다.</li></ol>"
        "<h3>주소 확인과 연결</h3><p><b>DISCOVER CAMERAS</b>를 누르고 카메라를 선택하면 <b>DISCOVERED CAMERAS</b> 옆에 VMS RTSP 주소가 자동으로 표시됩니다. <b>ADD CAMERA</b>를 누르면 등록·스트림 연결을 시작합니다.</p>"
        "<p><b>Not added</b>는 미등록, <b>Connecting / Waiting</b>은 준비 중입니다. 검색 때 URI는 VMS relay 미리보기이고 직접 URI 조회 후 카메라 주소로 바뀝니다. 인증은 숨깁니다.</p>"
        "<p>PTZ·메타데이터·녹화·검색은 VMS를 사용합니다. 녹화 영상 재생은 Qt에서 제공합니다.</p>"
        "<p>카메라 직접 RTSP TCP / UDP를 같은 URI에서 선택합니다.</p>"));
    page(QStringLiteral("녹화 · 재생"), QStringLiteral(
        "<h2>녹화하기</h2><ol><li>연결된 카메라를 선택하고 <b>REC START</b>를 누릅니다.</li>"
        "<li>서버가 실제로 저장을 시작하면 REC가 Active로 표시됩니다.</li>"
        "<li><b>REC STOP</b>으로 녹화를 종료합니다. 파일 마감 후 검색할 수 있습니다.</li></ol>"
        "<p>녹화 중인 파일은 아직 검색 결과에 없을 수 있습니다. 10분 파일 분할 또는 REC STOP 후 완료된 파일을 검색하세요.</p>"
        "<h2>저장된 영상 보기</h2><ol><li><b>PLAYBACK</b> 탭에서 카메라, 날짜, 시작/종료 시간을 고릅니다.</li>"
        "<li><b>Search recordings</b>를 누릅니다. 한 번에 최대 100건을 조회합니다.</li>"
        "<li>결과 목록에서 파일을 선택하면 재생됩니다. 선택된 파일은 <b>OPEN</b>으로 다시 열 수 있습니다.</li>"
        "<li><b>Play / Pause / Stop</b>, <b>±10s</b>, 타임라인 슬라이더로 조작합니다. 시간 이동은 파일이 seek를 지원할 때 활성화됩니다.</li></ol>"
        "<h3>직접 파일 선택</h3><p><b>OPEN FILE</b>로 로컬 MKV/MP4 등을 고르면 서버 연결 없이도 재생할 수 있습니다.</p>"
        "<p>목록을 통한 직접 재생은 Qt와 VMS가 같은 PC에서 실행되고 반환된 파일 경로에 접근할 수 있을 때 지원합니다. 다른 PC의 VMS 파일은 복사한 뒤 OPEN FILE로 여세요. 원격 파일 전송 API는 아직 없습니다.</p>"
        "<p>Dummy Mode의 타임라인은 모의 동작이며 실제 녹화 영상이 아닙니다.</p>"));
    page(QStringLiteral("상태 · 문제 해결"), QStringLiteral(
        "<h2>상태는 서로 다릅니다</h2><ul><li><b>VMS Connected</b>: PC 서버에 접속했습니다.</li>"
        "<li><b>CAM01 Online</b>: 서버가 카메라 스트림을 수신하고 있습니다.</li>"
        "<li><b>Stream Live (VMS)</b>: Qt에서 실제 영상 프레임을 표시하고 있습니다.</li>"
        "<li><b>REC Active</b>: 서버가 녹화 파일을 저장하고 있습니다.</li></ul>"
        "<h3>녹화 목록이 비어 있을 때</h3><p>날짜·시간·카메라를 확인하고 REC STOP 후 다시 검색하세요. 목록에는 완료된 파일만 나타납니다. 서버 녹화 오류는 SYSTEM LOG에서 확인합니다.</p>"
        "<h3>파일은 있는데 재생되지 않을 때</h3><p>다른 PC의 경로, 이동/삭제된 파일, 권한 문제, 아직 마감되지 않은 .part 파일인지 확인하세요. OPEN FILE로 실제 저장된 파일을 선택할 수 있습니다. 코덱 오류가 나면 SYSTEM LOG의 재생 오류를 확인하세요.</p>"
        "<p>Windows 클라이언트는 같은 PC의 WSL 서버가 반환한 /mnt/c/... 경로를 C:/...로 변환합니다. WSL 내부 /home/...에 저장한 파일은 Windows에서 접근할 수 있는 위치로 복사한 후 OPEN FILE로 여세요.</p>"
        "<h3>영상 영역이 작을 때</h3><p>중앙 영상과 하단 탭 사이 경계선을 아래로 끌면 LIVE VIEW가 커지고, 위로 끌면 녹화 영상 영역이 커집니다.</p>"
        "<h3>PTZ 조작</h3><p>서버가 PTZ를 지원하는 온라인 카메라를 선택하면 방향 버튼이나 W/A/S/D·방향키를 누르는 동안 이동하고 놓으면 정지합니다. C/R 또는 중앙 버튼으로 복귀합니다. 포커스·창·카메라·연결 변경 시 정지합니다.</p>"
        "<p>PTZ TX는 명령 송신, ACCEPTED는 VMS 접수, PI_ACKNOWLEDGED는 Pi ONVIF 응답이며 모터 도착 확인이 아닙니다.</p>"
        "<h3>자동 추적</h3><p>카메라가 지원하면 TRACKING MODE를 눌러 시작·종료합니다. 요청 중에는 버튼을 잠그며 실제 ON/OFF는 Pi 메타데이터로 확인합니다. 수동 이동·중앙 복귀는 추적을 해제합니다. 상태 알림이 없으면 20초 뒤 미확인을 표시합니다.</p>"
        "<h3>탐지 기록 검색</h3><p><b>EVENTS</b>에서 탐지 샘플 또는 상태 이력, 카메라·로컬 날짜/시간·최소 신뢰도를 선택하고 SEARCH를 누릅니다. 다음 페이지로 추가 결과를 보고 더블클릭하면 완료 녹화 위치를 조회합니다. 없는 값은 —로 표시합니다.</p>"
        "<h3>자연어 검색</h3><p><b>CHAT SEARCH</b>에 오늘 사람이 나온 영상 찾아줘처럼 입력합니다. 시간 기준은 Asia/Seoul입니다. Gemini는 조건만 해석하고 VMS가 DB와 녹화를 확인합니다. 결과 선택 후 재생하거나 다음 페이지·두 번째 결과 재생을 입력할 수 있습니다.</p>"
        "<p>검색 재생은 서버의 offsetMs를 사용하며 추정 위치입니다. 녹화 없음·원격 파일·시간 이동 불가이면 재생 오류를 표시합니다. Gemini 키는 VMS 설정에서 관리하고 Qt에는 넣지 않습니다.</p>"
        "<p>상단 <b>?</b> 또는 <b>F1</b>을 누르면 이 도움말을 다시 볼 수 있습니다.</p>"));
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject); layout->addWidget(buttons);
}
