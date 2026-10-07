#include "RtspPlayerWidget.h"

#include <QMediaPlayer>
#include <QVideoFrame>
#include <QVideoSink>
#include <QtGlobal>

RtspPlayerWidget::RtspPlayerWidget(QWidget *parent) : QVideoWidget(parent)
{
    setAspectRatioMode(Qt::KeepAspectRatio);
    m_player = new QMediaPlayer(this);
    m_player->setVideoOutput(this);

    connect(videoSink(), &QVideoSink::videoFrameChanged, this,
            [this](const QVideoFrame &frame) {
                if (frame.isValid())
                    emit frameReceived(frame.size());
            });
    connect(m_player, &QMediaPlayer::errorOccurred, this,
            [this](QMediaPlayer::Error, const QString &message) {
                emit playbackError(message.isEmpty() ? QStringLiteral("RTSP playback error") : message);
            });
}

void RtspPlayerWidget::start(const QUrl &url, Transport transport)
{
    // Qt FFmpeg reads this setting when it opens an RTSP source.
    qputenv("QT_FFMPEG_RTSP_TRANSPORT", transport == Transport::Tcp ? "tcp" : "udp");
    m_player->setSource(url);
    m_player->play();
}

void RtspPlayerWidget::stop()
{
    m_player->stop();
    m_player->setSource(QUrl());
}
