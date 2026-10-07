#pragma once

#include <QElapsedTimer>
#include <QImage>
#include <QPoint>
#include <QRect>
#include <QSize>
#include <QString>
#include <QUrl>
#include <QWidget>

class CameraWidget;
class OnvifPanelWidget;
class QComboBox;
class QLabel;
class QPushButton;
class QStackedWidget;
class QTimer;
class RtspPlayerWidget;
#ifdef PTZ_WITH_WEBENGINE
class WebRtcPlayerWidget;
#endif

class CameraPlaybackWidget : public QWidget
{
    Q_OBJECT
public:
    enum class Mode { WebRTC, RtspTcp, RtspUdp };
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
    void onvifMessage(const QString &message);

private:
    void stopPlayers();
    void updateOnvifControls();
    void setState(State state, const QString &detail = QString());
    void checkPlayback();

    QComboBox *m_modeSelector = nullptr;
    QPushButton *m_onvifToggle = nullptr;
    QLabel *m_status = nullptr;
    QStackedWidget *m_stack = nullptr;
    CameraWidget *m_demoCamera = nullptr;
    OnvifPanelWidget *m_onvifPanel = nullptr;
    RtspPlayerWidget *m_rtspView = nullptr;
#ifdef PTZ_WITH_WEBENGINE
    WebRtcPlayerWidget *m_webView = nullptr;
#endif
    QTimer *m_watchdog = nullptr;
    QElapsedTimer m_progressClock;
    QUrl m_rtspUrl;
    Mode m_activeMode = Mode::WebRTC;
    State m_state = State::Idle;
    QString m_stateDetail;
    bool m_running = false;
    bool m_onvifUrlReady = false;
};
