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
constexpr int demoPage = 0;
constexpr int rtspPage = 1;
constexpr int webRtcPage = 2;
const QUrl webRtcUrl(QStringLiteral("http://192.168.0.92:8889/cam/"));
}

CameraPlaybackWidget::CameraPlaybackWidget(QWidget *parent) : QWidget(parent)
{
    m_rtspUrl = QUrl(QStringLiteral("rtsp://192.168.0.92:8554/cam"));
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    auto *controls = new QHBoxLayout;
    controls->addWidget(new QLabel(QStringLiteral("Play Method"), this));
    m_modeSelector = new QComboBox(this);
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
    updateOnvifControls();
    connect(m_onvifPanel, &OnvifPanelWidget::fetchRequested, this, [this] {
        m_onvifPanel->fetchStreamUri(m_modeSelector->currentIndex() == static_cast<int>(Mode::RtspUdp));
    });
    connect(m_onvifPanel, &OnvifPanelWidget::streamUriReady, this, [this](const QUrl &uri) {
        m_rtspUrl = uri;
        m_onvifUrlReady = true;
        m_onvifToggle->setChecked(false);
        updateOnvifControls();
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
        if (!m_running || m_activeMode != Mode::WebRTC)
            return;
        m_progressClock.restart();
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
    m_stack->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    layout->addWidget(m_stack, 1);

    connect(m_rtspView, &RtspPlayerWidget::frameReceived, this, [this](const QSize &size) {
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
    m_watchdog->setInterval(500);
    connect(m_watchdog, &QTimer::timeout, this, &CameraPlaybackWidget::checkPlayback);
    connect(startButton, &QPushButton::clicked, this, &CameraPlaybackWidget::start);
    connect(stopButton, &QPushButton::clicked, this, &CameraPlaybackWidget::stop);
    connect(m_modeSelector, &QComboBox::currentIndexChanged, this, [this] {
        m_onvifToggle->setChecked(false);
        updateOnvifControls();
        if (m_running)
            start();
    });
}

void CameraPlaybackWidget::setDemoFrame(const QImage &frame)
{
    m_demoCamera->setFrame(frame);
}

void CameraPlaybackWidget::setDetection(const QRect &box, const QString &label,
                                        float confidence, const QPoint &center)
{
    m_demoCamera->setDetection({box, label, confidence, center});
}

void CameraPlaybackWidget::clearDetection()
{
    m_demoCamera->clearDetection();
}

bool CameraPlaybackWidget::isRunning() const
{
    return m_running;
}

void CameraPlaybackWidget::start()
{
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

void CameraPlaybackWidget::stop()
{
    m_running = false;
    stopPlayers();
    m_stack->setCurrentIndex(demoPage);
    setState(State::Idle);
}

void CameraPlaybackWidget::stopPlayers()
{
    m_watchdog->stop();
    m_rtspView->stop();
#ifdef PTZ_WITH_WEBENGINE
    m_webView->stop();
#endif
}

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

void CameraPlaybackWidget::checkPlayback()
{
    if (m_running && m_progressClock.isValid() && m_progressClock.elapsed() > 10000
        && m_state != State::Failed)
        setState(State::Failed, QStringLiteral("No video frames received for 10 seconds"));
}
