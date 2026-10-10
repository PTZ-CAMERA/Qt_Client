#pragma once
#include "model/VmsTypes.h"
#include <QTimer>
#include <QWidget>
#include <QJsonObject>
class QComboBox;
class QCheckBox;
class QDateEdit;
class QTimeEdit;
class QLabel;
class QSlider;
class QPushButton;
class CameraWidget;
class QStackedWidget;
class QMediaPlayer;
class QVideoSink;
class PlaybackWidget : public QWidget {
    Q_OBJECT
public:
    explicit PlaybackWidget(QWidget *parent = nullptr);
    ~PlaybackWidget() override;
    void setCameras(const QList<CameraInfo> &cameras);
    QString selectedCameraId() const;
    void setSelectedCamera(const QString &cameraId);
    void setRecordings(const QList<RecordingInfo> &recordings);
    void openRecording(const RecordingInfo &recording, const QDateTime &targetTime);
    void openLocalFile(const QString &path);
    bool openPlaybackResult(const QJsonObject &result);
    void setLocalVms(bool local) { m_localVms = local; }
    void clear();
    void setSearchAvailable(bool available);
    void setSearchError(const QString &message);
signals:
    void playbackFrameReceived(const QSize &size);
    void playbackFailed(const QString &message);
    void queryInvalidated();
    void searchRequested(const QString &cameraId, const QDateTime &start, const QDateTime &end);
private:
    void updateTime(int seconds);
    void openSelectedRecording();
    void startFile(const QString &path, qint64 offsetMs);
    void stopPlayer();
    void showPlaybackError(const QString &message);
    QComboBox *m_camera = nullptr;
    QComboBox *m_recordings = nullptr;
    QDateEdit *m_date = nullptr;
    QTimeEdit *m_fromTime = nullptr, *m_toTime = nullptr;
    QLabel *m_video = nullptr;
    QLabel *m_resultCount = nullptr;
    QLabel *m_playbackState = nullptr;
    QStackedWidget *m_videoStack = nullptr;
    CameraWidget *m_cameraView = nullptr;
    QMediaPlayer *m_player = nullptr;
    QVideoSink *m_sink = nullptr;
    QPushButton *m_back = nullptr, *m_forward = nullptr;
    QTimer m_loadTimeout;
    quint64 m_generation = 0;
    qint64 m_durationMs = 0, m_pendingSeekMs = 0;
    bool m_localVms = false, m_receivedFrame = false;
    QPushButton *m_open = nullptr, *m_search = nullptr;
    bool m_searchAvailable = false;
    QLabel *m_startTime = nullptr;
    QLabel *m_currentTime = nullptr;
    QLabel *m_endTime = nullptr;
    QSlider *m_timeline = nullptr;
    QWidget *m_controls = nullptr;
    QTimer m_timer;
    RecordingInfo m_current;
    QList<RecordingInfo> m_results;
    QCheckBox *m_preRoll = nullptr;
};
