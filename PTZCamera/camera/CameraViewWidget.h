#pragma once
#include "model/VmsTypes.h"
#include <QImage>
#include <QWidget>
class CameraWidget;
class QComboBox;
class QLabel;
class QPushButton;
class QMediaPlayer;
class QVideoSink;
class QTimer;
class QUrl;
class CameraViewWidget : public QWidget {
    Q_OBJECT
public:
    explicit CameraViewWidget(QWidget *parent = nullptr);
    QString selectedTransport() const;
    void startStream(const QUrl &uri);
    void stopStream();
    bool isLive() const { return m_hasLiveFrame; }
    bool isPlaying() const { return m_playing; }
    void setDetection(const DetectionInfo &info);
    void clearDetection();
    void setCameraName(const QString &name);
    void setRecordingControls(bool canStart, bool canStop);
    void setRecording(bool recording);
    void setOnline(bool online);
    void setFrame(const QImage &frame);
    void setLive(bool live, bool simulated = false);
signals:
    void startRequested(const QString &method);
    void stopRequested();
    void startRecordingRequested();
    void stopRecordingRequested();
    void videoFrameReceived(const QSize &size);
    void playbackError(const QString &message);
    void streamMethodChanged(const QString &method);
private:
    CameraWidget *m_view = nullptr;
    QComboBox *m_method = nullptr;
    QLabel *m_name = nullptr;
    QLabel *m_live = nullptr;
    QLabel *m_rec = nullptr;
    QLabel *m_online = nullptr;
    QPushButton *m_startRec = nullptr, *m_stopRec = nullptr;
    QMediaPlayer *m_player = nullptr;
    QVideoSink *m_sink = nullptr;
    QTimer *m_frameTimeout = nullptr;
    bool m_playing = false, m_hasLiveFrame = false;
    quint64 m_generation = 0;
};
