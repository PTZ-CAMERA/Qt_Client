// 데모, RTSP와 WebRTC 화면을 관리하는 상위 카메라 위젯이다.
// 재생 방식 전환, ONVIF 설정 표시와 공통 상태 판단을 담당하고 디코딩은 각 플레이어에 맡긴다.
#include "CameraPlaybackWidget.h"

#include "CameraWidget.h"
#include "OnvifPanelWidget.h"
#include "rtsp/RtspPlayerWidget.h"
#ifdef PTZ_WITH_WEBENGINE
#include "webrtc/WebRtcPlayerWidget.h"
#endif

#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSizePolicy>
#include <QStackedWidget>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

namespace {
// QStackedWidget에 추가하는 순서와 일치해야 하는 화면 번호다.
// WebEngine을 제외한 빌드에서도 WebRTC 자리에는 사용 불가 안내 화면을 넣는다.
constexpr int demoPage = 0;
constexpr int rtspPage = 1;
constexpr int webRtcPage = 2;
const QUrl webRtcUrl(QStringLiteral("http://192.168.0.92:8889/cam/"));
}

// 조작 행 → 접을 수 있는 ONVIF 설정 → 상태 문구 → 영상 화면 순으로 구성한다.
// 플레이어와 패널을 부모 위젯에 연결해 Qt가 수명을 관리하도록 한다.
CameraPlaybackWidget::CameraPlaybackWidget(QWidget *parent) : QWidget(parent)
{
    // ONVIF 조회 전에는 알려진 Pi 주소를 사용하고, 조회 성공 시 반환 주소로 교체한다.
    m_rtspUrl = QUrl(QStringLiteral("rtsp://192.168.0.92:8554/cam"));
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    auto *controls = new QHBoxLayout;
    controls->addWidget(new QLabel(QStringLiteral("Play Method"), this));
    m_modeSelector = new QComboBox(this);
    // 항목 순서는 Mode 열거형과 맞춰야 한다. 이후 선택 인덱스를 Mode로 변환한다.
    m_modeSelector->addItem(QStringLiteral("WebRTC"));
    m_modeSelector->addItem(QStringLiteral("RTSP TCP"));
    m_modeSelector->addItem(QStringLiteral("RTSP UDP"));
    m_modeSelector->setCurrentIndex(0);
    controls->addWidget(m_modeSelector);
    m_onvifToggle = new QPushButton(this);
    m_onvifToggle->setObjectName(QStringLiteral("secondaryButton"));
    m_onvifToggle->setCheckable(true);
    controls->addWidget(m_onvifToggle);
    auto *startButton = new QPushButton(QStringLiteral("Start"), this);
    startButton->setObjectName(QStringLiteral("primaryButton"));
    auto *stopButton = new QPushButton(QStringLiteral("Stop"), this);
    controls->addWidget(startButton);
    controls->addWidget(stopButton);
    controls->addStretch();
    layout->addLayout(controls);

    m_onvifPanel = new OnvifPanelWidget(this);
    layout->addWidget(m_onvifPanel);
    connect(m_onvifToggle, &QPushButton::toggled, this, &CameraPlaybackWidget::updateOnvifControls);
    // 기본 모드인 WebRTC에서는 ONVIF 토글과 설정 입력 영역을 모두 숨긴다.
    updateOnvifControls();
    connect(m_onvifPanel, &OnvifPanelWidget::fetchRequested, this, [this] {
        // ONVIF에 요청하는 영상 전송 방식도 현재 선택한 RTSP TCP/UDP에 맞춘다.
        m_onvifPanel->fetchStreamUri(m_modeSelector->currentIndex() == static_cast<int>(Mode::RtspUdp));
    });
    connect(m_onvifPanel, &OnvifPanelWidget::streamUriReady, this, [this](const QUrl &uri) {
        // 주소를 적용한 뒤 설정을 접어 영상 공간을 확보한다. 조회 결과는 재생 성공과 별개다.
        m_rtspUrl = uri;
        m_onvifUrlReady = true;
        m_onvifToggle->setChecked(false);
        updateOnvifControls();
        // RTSP를 재생하던 중에 주소가 바뀌면 새 주소로 다시 연결한다.
        if (m_running && m_activeMode != Mode::WebRTC)
            start();
    });
    connect(m_onvifPanel, &OnvifPanelWidget::message, this, &CameraPlaybackWidget::onvifMessage);

    m_status = new QLabel(QStringLiteral("IDLE"), this);
    m_status->setObjectName(QStringLiteral("playbackStatus"));
    m_status->setWordWrap(true);
    layout->addWidget(m_status);

    m_stack = new QStackedWidget(this);
    m_demoCamera = new CameraWidget(m_stack);
    m_rtspView = new RtspPlayerWidget(m_stack);
    m_stack->addWidget(m_demoCamera);
    m_stack->addWidget(m_rtspView);
#ifdef PTZ_WITH_WEBENGINE
    m_webView = new WebRtcPlayerWidget(m_stack);
    m_stack->addWidget(m_webView);
    connect(m_webView, &WebRtcPlayerWidget::frameReceived, this, [this](const QSize &size) {
        // 모드 전환 뒤 늦게 들어온 다른 플레이어의 신호는 현재 상태에 반영하지 않는다.
        if (!m_running || m_activeMode != Mode::WebRTC)
            return;
        m_progressClock.restart();
        // WebRTC 플레이어가 영상 프레임 수 증가를 확인한 뒤에만 정상 재생으로 표시한다.
        setState(State::Playing, QStringLiteral("WebRTC video is advancing"));
        emit frameSizeChanged(size);
    });
    connect(m_webView, &WebRtcPlayerWidget::playbackError, this,
            [this](const QString &message) {
                if (m_running && m_activeMode == Mode::WebRTC)
                    setState(State::Failed, message);
            });
#else
    auto *unavailable = new QLabel(QStringLiteral("WebRTC requires a Qt WebEngine build"), m_stack);
    unavailable->setAlignment(Qt::AlignCenter);
    m_stack->addWidget(unavailable);
#endif
    m_stack->setCurrentIndex(demoPage);
    // 설정 패널을 접으면 남는 높이를 영상 위젯이 사용할 수 있게 한다.
    m_stack->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    layout->addWidget(m_stack, 1);

    connect(m_rtspView, &RtspPlayerWidget::frameReceived, this, [this](const QSize &size) {
        // RTSP의 유효한 프레임이 도착할 때마다 마지막 진행 시각과 해상도를 갱신한다.
        if (!m_running || m_activeMode == Mode::WebRTC)
            return;
        m_progressClock.restart();
        setState(State::Playing, m_activeMode == Mode::RtspTcp
                                     ? QStringLiteral("RTSP TCP video frames received")
                                     : QStringLiteral("RTSP UDP video frames received"));
        emit frameSizeChanged(size);
    });
    connect(m_rtspView, &RtspPlayerWidget::playbackError, this,
            [this](const QString &message) {
                if (m_running && m_activeMode != Mode::WebRTC)
                    setState(State::Failed, message);
            });

    m_watchdog = new QTimer(this);
    // 500ms마다 프레임 정지 여부를 검사한다. 이 타이머는 영상 수신 버퍼를 설정하지 않는다.
    m_watchdog->setInterval(500);
    connect(m_watchdog, &QTimer::timeout, this, &CameraPlaybackWidget::checkPlayback);
    connect(startButton, &QPushButton::clicked, this, &CameraPlaybackWidget::start);
    connect(stopButton, &QPushButton::clicked, this, &CameraPlaybackWidget::stop);
    connect(m_modeSelector, &QComboBox::currentIndexChanged, this, [this] {
        // 모드 변경 시 설정은 접는다. 실행 중이면 이전 재생을 중지하고 새 모드로 전환한다.
        m_onvifToggle->setChecked(false);
        updateOnvifControls();
        if (m_running)
            start();
    });
}

// 데모용 직접 그리기 위젯에 QImage를 전달한다. RTSP/WebRTC 실영상 경로와는 독립적이다.
void CameraPlaybackWidget::setDemoFrame(const QImage &frame)
{
    m_demoCamera->setFrame(frame);
}

// 현재 탐지 오버레이는 데모 화면에만 전달한다.
// 실영상 위에 상자를 표시하려면 플레이어별 오버레이와 좌표 동기화 구현이 추가로 필요하다.
void CameraPlaybackWidget::setDetection(const QRect &box, const QString &label,
                                        float confidence, const QPoint &center)
{
    m_demoCamera->setDetection({box, label, confidence, center});
}

// 데모 탐지 표시를 제거한다. 재생 중인 네트워크 영상은 중지하지 않는다.
void CameraPlaybackWidget::clearDetection()
{
    m_demoCamera->clearDetection();
}

// 재생 시도를 시작했는지 반환한다. 연결 실패 상태에서도 참일 수 있으므로
// 실제 영상 진행 여부는 statusChanged의 Playing 상태로 구분한다.
bool CameraPlaybackWidget::isRunning() const
{
    return m_running;
}

// 항상 기존 플레이어를 정리한 후 선택한 한 가지 방식만 시작한다.
// 소스를 여는 동안은 Connecting으로 두고 실제 프레임 수신으로 Playing을 결정한다.
void CameraPlaybackWidget::start()
{
    // 정리 중 발생하는 이전 플레이어 신호가 현재 재생 상태를 바꾸지 않도록 잠시 끈다.
    m_running = false;
    stopPlayers();
    m_running = true;
    m_activeMode = static_cast<Mode>(m_modeSelector->currentIndex());
    m_progressClock.start();
    QString waitMessage;
    switch (m_activeMode) {
    case Mode::WebRTC: waitMessage = QStringLiteral("Waiting for WebRTC video frames"); break;
    case Mode::RtspTcp: waitMessage = QStringLiteral("Waiting for RTSP TCP video frames"); break;
    case Mode::RtspUdp: waitMessage = QStringLiteral("Waiting for RTSP UDP video frames"); break;
    }
    setState(State::Connecting, waitMessage);
    m_stack->setCurrentIndex(m_activeMode == Mode::WebRTC ? webRtcPage : rtspPage);
    m_watchdog->start();

    if (m_activeMode != Mode::WebRTC) {
        m_rtspView->start(m_rtspUrl, m_activeMode == Mode::RtspTcp
                                      ? RtspPlayerWidget::Transport::Tcp
                                      : RtspPlayerWidget::Transport::Udp);
        return;
    }
#ifdef PTZ_WITH_WEBENGINE
    m_webView->start(webRtcUrl);
#else
    setState(State::Failed, QStringLiteral("Qt WebEngine is unavailable in this build"));
    m_watchdog->stop();
#endif
}

// 모든 재생을 멈추고 데모/무신호 화면으로 돌아간다.
void CameraPlaybackWidget::stop()
{
    m_running = false;
    stopPlayers();
    m_stack->setCurrentIndex(demoPage);
    setState(State::Idle);
}

// 상태 표시를 바꾸지 않고 타이머와 각 플레이어만 정리하는 내부 공통 함수다.
void CameraPlaybackWidget::stopPlayers()
{
    m_watchdog->stop();
    m_rtspView->stop();
#ifdef PTZ_WITH_WEBENGINE
    m_webView->stop();
#endif
}

// ONVIF 설정은 RTSP 모드에서 토글을 눌렀을 때만 공간을 차지한다.
// 이 함수는 표시 상태만 갱신하므로 설정을 접고 펼쳐도 재생 연결은 다시 시작하지 않는다.
void CameraPlaybackWidget::updateOnvifControls()
{
    const bool rtspSelected = m_modeSelector->currentIndex() != static_cast<int>(Mode::WebRTC);
    const bool expanded = rtspSelected && m_onvifToggle->isChecked();
    m_onvifToggle->setVisible(rtspSelected);
    m_onvifPanel->setVisible(expanded);
    m_onvifToggle->setText(QStringLiteral("ONVIF Settings%1  %2")
                               .arg(m_onvifUrlReady ? QStringLiteral(" / URL ready") : QString(),
                                    expanded ? QStringLiteral("▾") : QStringLiteral("▸")));
}

// 상태별 문구/색상을 갱신하고 MainWindow에 상태 변화를 전달한다.
// 동일한 상태와 설명이 반복되면 신호를 생략해 매 프레임마다 로그가 쌓이지 않게 한다.
void CameraPlaybackWidget::setState(State state, const QString &detail)
{
    if (m_state == state && m_stateDetail == detail)
        return;
    m_state = state;
    m_stateDetail = detail;
    QString name;
    QString color;
    switch (state) {
    case State::Idle: name = QStringLiteral("IDLE"); color = QStringLiteral("#8795a3"); break;
    case State::Connecting: name = QStringLiteral("CONNECTING"); color = QStringLiteral("#efb35d"); break;
    case State::Playing: name = QStringLiteral("PLAYING"); color = QStringLiteral("#52d7a5"); break;
    case State::Failed: name = QStringLiteral("CONNECTION FAILED"); color = QStringLiteral("#ef7272"); break;
    }
    m_status->setText(detail.isEmpty() ? name : QStringLiteral("%1  /  %2").arg(name, detail));
    m_status->setStyleSheet(QStringLiteral("color: %1; font-weight: 700;").arg(color));
    emit statusChanged(state, detail);
}

// 시작 후 또는 마지막 정상 프레임 이후 10초가 지나면 연결 실패로 표시한다.
// 자동 재접속은 하지 않으며, 이후 프레임이 다시 도착하면 Playing으로 회복할 수 있다.
void CameraPlaybackWidget::checkPlayback()
{
    if (m_running && m_progressClock.isValid() && m_progressClock.elapsed() > 10000
        && m_state != State::Failed)
        setState(State::Failed, QStringLiteral("No video frames received for 10 seconds"));
}
