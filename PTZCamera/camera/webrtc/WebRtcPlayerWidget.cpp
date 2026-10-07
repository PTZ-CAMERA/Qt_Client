// Pi의 WebRTC 재생 페이지를 Qt WebEngine으로 표시한다.
// HTTP 페이지 로드와 실제 영상 재생을 구분하기 위해 HTML video 요소를 확인한다.
#include "WebRtcPlayerWidget.h"

#include <QPointer>
#include <QVariantMap>
#include <QWebEnginePage>

// 500ms 간격은 영상 프레임의 진행 여부를 검사하는 주기이며 수신 버퍼 시간이 아니다.
WebRtcPlayerWidget::WebRtcPlayerWidget(QWidget *parent) : QWebEngineView(parent)
{
    m_pollTimer.setInterval(500);
    connect(&m_pollTimer, &QTimer::timeout, this, &WebRtcPlayerWidget::pollFrames);
    connect(this, &QWebEngineView::loadFinished, this, [this](bool ok) {
        // 현재 스트림 호스트의 로드 실패만 보고한다. 로드 성공만으로 PLAYING을 내보내지 않는다.
        if (m_running && !ok && url().host() == m_streamHost)
            emit playbackError(QStringLiteral("WebRTC page could not load"));
    });
}

// 재생 세대 번호를 바꿔 이전 조회 결과를 무효화하고 새 페이지를 연다.
// 첫 검사에서는 프레임 수 기준값을 잡고, 다음 검사부터 증가 여부를 비교한다.
void WebRtcPlayerWidget::start(const QUrl &url)
{
    ++m_generation;
    m_running = true;
    m_lastFrames = -1;
    m_streamHost = url.host();
    load(url);
    m_pollTimer.start();
}

// 검사 타이머와 이전 콜백을 무효화한 뒤 빈 페이지로 이동해 스트림 페이지를 해제한다.
void WebRtcPlayerWidget::stop()
{
    ++m_generation;
    m_running = false;
    m_pollTimer.stop();
    if (!url().isEmpty())
        setUrl(QUrl(QStringLiteral("about:blank")));
}

// JavaScript로 video 요소의 준비 상태, 프레임 수, 해상도 등을 비동기로 읽는다.
// 페이지에 video 요소가 없거나 프레임 수가 늘지 않으면 재생 확인 신호를 보내지 않는다.
void WebRtcPlayerWidget::pollFrames()
{
    // 일반 프레임 통계 API를 우선 사용하고, 없으면 WebKit 계열 통계값을 사용한다.
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
    // JavaScript 결과가 돌아오기 전에 위젯이 삭제되거나 다른 재생이 시작될 수 있다.
    // QPointer와 세대 번호로 해당 결과가 아직 현재 위젯/재생에 속하는지 확인한다.
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
        // 영상 데이터와 해상도가 있고, 정지/종료 상태가 아니며 프레임 수도 증가해야 한다.
        // 단순히 마지막 프레임이 화면에 남아 있는 상태를 정상 재생으로 판단하지 않게 한다.
        if (video.value(QStringLiteral("ready")).toInt() >= 2 && width > 0 && height > 0
            && !video.value(QStringLiteral("paused")).toBool()
            && !video.value(QStringLiteral("ended")).toBool()
            && self->m_lastFrames >= 0 && frames > self->m_lastFrames) {
            emit self->frameReceived(QSize(width, height));
        }
        self->m_lastFrames = frames;
    });
}
