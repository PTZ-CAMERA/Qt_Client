#include "CameraPlaybackWidget.h"

#include "CameraWidget.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMediaPlayer>
#include <QPointer>
#include <QPushButton>
#include <QStackedWidget>
#include <QTimer>
#include <QUrl>
#include <QVariantMap>
#include <QVideoFrame>
#include <QVideoSink>
#include <QVideoWidget>
#include <QVBoxLayout>
#ifdef PTZ_WITH_WEBENGINE
#include <QWebEnginePage>
#include <QWebEngineView>
#endif

namespace {
constexpr int demoPage = 0;
constexpr int rtspPage = 1;
constexpr int webRtcPage = 2;
const QUrl webRtcUrl(QStringLiteral("http://192.168.0.92:8889/cam/"));
const QUrl rtspUrl(QStringLiteral("rtsp://192.168.0.92:8554/cam"));
}

CameraPlaybackWidget::CameraPlaybackWidget(QWidget *parent) : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    auto *controls = new QHBoxLayout;
    controls->addWidget(new QLabel(QStringLiteral("Playback"), this));
    m_modeSelector = new QComboBox(this);
    m_modeSelector->addItem(QStringLiteral("WebRTC"));
    m_modeSelector->addItem(QStringLiteral("RTSP"));
    m_modeSelector->setCurrentIndex(0);
    controls->addWidget(m_modeSelector);
    auto *startButton = new QPushButton(QStringLiteral("Start"), this);
    startButton->setObjectName(QStringLiteral("primaryButton"));
    auto *stopButton = new QPushButton(QStringLiteral("Stop"), this);
    controls->addWidget(startButton);
    controls->addWidget(stopButton);
    controls->addStretch();
    layout->addLayout(controls);

    m_status = new QLabel(QStringLiteral("IDLE"), this);
    m_status->setObjectName(QStringLiteral("playbackStatus"));
    m_status->setWordWrap(true);
    layout->addWidget(m_status);

    m_stack = new QStackedWidget(this);
    m_demoCamera = new CameraWidget(m_stack);
    m_rtspView = new QVideoWidget(m_stack);
    m_rtspView->setAspectRatioMode(Qt::KeepAspectRatio);
    m_stack->addWidget(m_demoCamera);
    m_stack->addWidget(m_rtspView);
#ifdef PTZ_WITH_WEBENGINE
    m_webView = new QWebEngineView(m_stack);
    m_stack->addWidget(m_webView);
    connect(m_webView, &QWebEngineView::loadFinished, this, [this](bool ok) {
        if (m_running && m_activeMode == Mode::WebRTC && !ok
            && m_webView->url().host() == webRtcUrl.host())
            setState(State::Failed, QStringLiteral("WebRTC page could not load"));
    });
#else
    auto *unavailable = new QLabel(QStringLiteral("WebRTC requires a Qt WebEngine build"), m_stack);
    unavailable->setAlignment(Qt::AlignCenter);
    m_stack->addWidget(unavailable);
#endif
    m_stack->setCurrentIndex(demoPage);
    layout->addWidget(m_stack, 1);

    m_player = new QMediaPlayer(this);
    m_player->setVideoOutput(m_rtspView);
    connect(m_rtspView->videoSink(), &QVideoSink::videoFrameChanged, this,
            [this](const QVideoFrame &frame) {
                if (!m_running || m_activeMode != Mode::RTSP || !frame.isValid())
                    return;
                m_progressClock.restart();
                setState(State::Playing, QStringLiteral("RTSP video frames received"));
                emit frameSizeChanged(frame.size());
            });
    connect(m_player, &QMediaPlayer::errorOccurred, this,
            [this](QMediaPlayer::Error, const QString &message) {
                if (m_running && m_activeMode == Mode::RTSP)
                    setState(State::Failed, message.isEmpty()
                                                ? QStringLiteral("RTSP playback error") : message);
            });

    m_watchdog = new QTimer(this);
    m_watchdog->setInterval(500);
    connect(m_watchdog, &QTimer::timeout, this, &CameraPlaybackWidget::checkPlayback);
    connect(startButton, &QPushButton::clicked, this, &CameraPlaybackWidget::start);
    connect(stopButton, &QPushButton::clicked, this, &CameraPlaybackWidget::stop);
    connect(m_modeSelector, &QComboBox::currentIndexChanged, this, [this] {
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
    m_activeMode = m_modeSelector->currentIndex() == 0 ? Mode::WebRTC : Mode::RTSP;
    m_lastWebRtcFrames = -1;
    m_progressClock.start();
    setState(State::Connecting, m_activeMode == Mode::WebRTC
                                    ? QStringLiteral("Waiting for WebRTC video frames")
                                    : QStringLiteral("Waiting for RTSP video frames"));
    m_stack->setCurrentIndex(m_activeMode == Mode::WebRTC ? webRtcPage : rtspPage);
    m_watchdog->start();

    if (m_activeMode == Mode::RTSP) {
        m_player->setSource(rtspUrl);
        m_player->play();
        return;
    }
#ifdef PTZ_WITH_WEBENGINE
    m_webView->load(webRtcUrl);
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
    ++m_generation;
    m_watchdog->stop();
    m_player->stop();
    m_player->setSource(QUrl());
#ifdef PTZ_WITH_WEBENGINE
    if (!m_webView->url().isEmpty())
        m_webView->setUrl(QUrl(QStringLiteral("about:blank")));
#endif
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
    if (!m_running)
        return;
    if (m_activeMode == Mode::WebRTC)
        pollWebRtc();
    if (m_progressClock.isValid() && m_progressClock.elapsed() > 10000
        && m_state != State::Failed)
        setState(State::Failed, QStringLiteral("No video frames received for 10 seconds"));
}

void CameraPlaybackWidget::pollWebRtc()
{
#ifdef PTZ_WITH_WEBENGINE
    static const QString script = QString::fromLatin1(R"JS(
        (() => {
            const video = document.querySelector('video');
            if (!video) return null;
            const quality = video.getVideoPlaybackQuality?.();
            return { ready: video.readyState,
                     frames: quality ? quality.totalVideoFrames : (video.webkitDecodedFrameCount || 0),
                     width: video.videoWidth, height: video.videoHeight,
                     paused: video.paused, ended: video.ended };
        })()
    )JS");
    const int generation = m_generation;
    QPointer<CameraPlaybackWidget> self(this);
    m_webView->page()->runJavaScript(script, [self, generation](const QVariant &result) {
        if (!self || !self->m_running || self->m_generation != generation
            || self->m_activeMode != Mode::WebRTC)
            return;
        const QVariantMap video = result.toMap();
        if (video.isEmpty())
            return;
        const qint64 frames = video.value(QStringLiteral("frames")).toLongLong();
        const int width = video.value(QStringLiteral("width")).toInt();
        const int height = video.value(QStringLiteral("height")).toInt();
        if (video.value(QStringLiteral("ready")).toInt() >= 2 && width > 0 && height > 0
            && !video.value(QStringLiteral("paused")).toBool()
            && !video.value(QStringLiteral("ended")).toBool()
            && self->m_lastWebRtcFrames >= 0 && frames > self->m_lastWebRtcFrames) {
            self->m_progressClock.restart();
            self->setState(State::Playing, QStringLiteral("WebRTC video is advancing"));
            emit self->frameSizeChanged(QSize(width, height));
        }
        self->m_lastWebRtcFrames = frames;
    });
#endif
}
