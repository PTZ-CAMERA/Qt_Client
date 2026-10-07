// RTSP 스트림을 QMediaPlayer로 재생하고 QVideoWidget에 출력하는 위젯이다.
// 연결 상태 판단에 필요한 프레임 수신과 오류만 상위 카메라 위젯에 전달한다.
#include "RtspPlayerWidget.h"

#include <QMediaPlayer>
#include <QVideoFrame>
#include <QVideoSink>
#include <QtGlobal>

// 플레이어는 이 위젯의 자식이므로 위젯이 삭제되면 함께 정리된다.
RtspPlayerWidget::RtspPlayerWidget(QWidget *parent) : QVideoWidget(parent)
{
    setAspectRatioMode(Qt::KeepAspectRatio);
    m_player = new QMediaPlayer(this);
    m_player->setVideoOutput(this);

    // play() 호출 성공이 아니라 유효한 영상 프레임 도착을 재생 확인 근거로 사용한다.
    connect(videoSink(), &QVideoSink::videoFrameChanged, this,
            [this](const QVideoFrame &frame) {
                if (frame.isValid())
                    emit frameReceived(frame.size());
            });
    // 오류는 신호로 보고한다. 실패 상태 표시와 로그 기록은 상위 UI에서 처리한다.
    connect(m_player, &QMediaPlayer::errorOccurred, this,
            [this](QMediaPlayer::Error, const QString &message) {
                emit playbackError(message.isEmpty() ? QStringLiteral("RTSP playback error") : message);
            });
}

// 선택한 TCP/UDP 설정을 적용하고 스트림을 연다.
// 현재 수신 버퍼의 시간이나 크기는 직접 지정하지 않고 Qt/FFmpeg 기본값을 사용한다.
void RtspPlayerWidget::start(const QUrl &url, Transport transport)
{
    // FFmpeg가 RTSP 소스를 열 때 참조하는 설정이다. 프로세스 전체에 적용되므로
    // 현재 앱처럼 RTSP 플레이어를 하나만 사용하는 구조를 전제로 한다.
    qputenv("QT_FFMPEG_RTSP_TRANSPORT", transport == Transport::Tcp ? "tcp" : "udp");
    m_player->setSource(url);
    m_player->play();
}

// 재생을 멈추고 소스도 비워 다음 시작 시 스트림을 새로 열도록 한다.
void RtspPlayerWidget::stop()
{
    m_player->stop();
    m_player->setSource(QUrl());
}
