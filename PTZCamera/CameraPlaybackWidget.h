#pragma once

#include <QElapsedTimer>
#include <QImage>
#include <QPoint>
#include <QRect>
#include <QSize>
#include <QString>
#include <QWidget>

class CameraWidget;
class QComboBox;
class QLabel;
class QMediaPlayer;
class QStackedWidget;
class QTimer;
class QVideoWidget;
#ifdef PTZ_WITH_WEBENGINE
class QWebEngineView;
#endif

class CameraPlaybackWidget : public QWidget
{
    Q_OBJECT
public:
    enum class Mode { WebRTC, RTSP };
    enum class State { Idle, Connecting, Playing, Failed };

    explicit CameraPlaybackWidget(QWidget *parent = nullptr);
    void setDemoFrame(const QImage &frame);
    void setDetection(const QRect &box, const QString &label, float confidence, const QPoint &center);
    void clearDetection();
    void start();
    void stop();
    bool isRunning() const;

signals:
    void statusChanged(CameraPlaybackWidget::State state, const QString &detail);
    void frameSizeChanged(const QSize &size);

private:
    void stopPlayers();
    void setState(State state, const QString &detail = QString());
    void checkPlayback();
    void pollWebRtc();

    QComboBox *m_modeSelector = nullptr;
    QLabel *m_status = nullptr;
    QStackedWidget *m_stack = nullptr;
    CameraWidget *m_demoCamera = nullptr;
    QVideoWidget *m_rtspView = nullptr;
    QMediaPlayer *m_player = nullptr;
#ifdef PTZ_WITH_WEBENGINE
    QWebEngineView *m_webView = nullptr;
#endif
    QTimer *m_watchdog = nullptr;
    QElapsedTimer m_progressClock;
    Mode m_activeMode = Mode::WebRTC;
    State m_state = State::Idle;
    QString m_stateDetail;
    bool m_running = false;
    qint64 m_lastWebRtcFrames = -1;
    int m_generation = 0;
};
