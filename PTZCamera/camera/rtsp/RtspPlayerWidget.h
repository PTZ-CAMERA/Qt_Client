#pragma once

#include <QSize>
#include <QString>
#include <QUrl>
#include <QVideoWidget>

class QMediaPlayer;

class RtspPlayerWidget : public QVideoWidget
{
    Q_OBJECT
public:
    enum class Transport { Tcp, Udp };

    explicit RtspPlayerWidget(QWidget *parent = nullptr);
    void start(const QUrl &url, Transport transport);
    void stop();

signals:
    void frameReceived(const QSize &size);
    void playbackError(const QString &message);

private:
    QMediaPlayer *m_player = nullptr;
};
