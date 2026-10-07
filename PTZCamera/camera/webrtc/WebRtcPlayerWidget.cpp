#include "WebRtcPlayerWidget.h"

#include <QPointer>
#include <QVariantMap>
#include <QWebEnginePage>

WebRtcPlayerWidget::WebRtcPlayerWidget(QWidget *parent) : QWebEngineView(parent)
{
    m_pollTimer.setInterval(500);
    connect(&m_pollTimer, &QTimer::timeout, this, &WebRtcPlayerWidget::pollFrames);
    connect(this, &QWebEngineView::loadFinished, this, [this](bool ok) {
        if (m_running && !ok && url().host() == m_streamHost)
            emit playbackError(QStringLiteral("WebRTC page could not load"));
    });
}

void WebRtcPlayerWidget::start(const QUrl &url)
{
    ++m_generation;
    m_running = true;
    m_lastFrames = -1;
    m_streamHost = url.host();
    load(url);
    m_pollTimer.start();
}

void WebRtcPlayerWidget::stop()
{
    ++m_generation;
    m_running = false;
    m_pollTimer.stop();
    if (!url().isEmpty())
        setUrl(QUrl(QStringLiteral("about:blank")));
}

void WebRtcPlayerWidget::pollFrames()
{
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
    QPointer<WebRtcPlayerWidget> self(this);
    page()->runJavaScript(script, [self, generation](const QVariant &result) {
        if (!self || !self->m_running || self->m_generation != generation)
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
            && self->m_lastFrames >= 0 && frames > self->m_lastFrames) {
            emit self->frameReceived(QSize(width, height));
        }
        self->m_lastFrames = frames;
    });
}
