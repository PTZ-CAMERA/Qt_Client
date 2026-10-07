#pragma once

#include <QSize>
#include <QString>
#include <QTimer>
#include <QUrl>
#include <QWebEngineView>

class WebRtcPlayerWidget : public QWebEngineView
{
    Q_OBJECT
public:
    explicit WebRtcPlayerWidget(QWidget *parent = nullptr);
    void start(const QUrl &url);
    void stop();

signals:
    void frameReceived(const QSize &size);
    void playbackError(const QString &message);

private:
    void pollFrames();

    QTimer m_pollTimer;
    QString m_streamHost;
    qint64 m_lastFrames = -1;
    int m_generation = 0;
    bool m_running = false;
};
