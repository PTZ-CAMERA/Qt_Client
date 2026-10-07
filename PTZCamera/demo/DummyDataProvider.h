#pragma once
#include "model/VmsTypes.h"
#include "model/TrackingInfo.h"
#include <QHash>
#include <QImage>
#include <QObject>
class DummyDataProvider : public QObject {
    Q_OBJECT
public:
    explicit DummyDataProvider(QObject *parent = nullptr);
    bool isEnabled() const { return m_enabled; }
    void setEnabled(bool enabled);
    void requestCameraList();
    void requestCameraStatus(const QString &cameraId);
    void requestEvents(const QString &cameraId, const QDateTime &start, const QDateTime &end, const QString &type);
    void requestRecordings(const QString &cameraId, const QDateTime &start, const QDateTime &end);
    void requestRecordingAt(const QString &cameraId, const QDateTime &time);
    void setTrackingEnabled(const QString &cameraId, bool enabled);
    void movePtz(const QString &cameraId, float pan, float tilt);
    void stopPtz(const QString &cameraId);
    void centerPtz(const QString &cameraId);
signals:
    void serverConnectionChanged(bool connected);
    void cameraListReceived(const QList<CameraInfo> &cameras);
    void cameraStatusChanged(const CameraInfo &camera);
    void frameReceived(const QString &cameraId, const QImage &frame);
    void detectionReceived(const QString &cameraId, const DetectionInfo &info);
    void trackingInfoReceived(const QString &cameraId, const TrackingInfo &info);
    void eventListReceived(const QList<EventInfo> &events);
    void recordingListReceived(const QList<RecordingInfo> &recordings);
    void recordingResolved(const RecordingInfo &recording, const QDateTime &targetTime);
    void message(const QString &source, const QString &message);
private:
    QList<CameraInfo> m_cameras;
    QHash<QString, TrackingInfo> m_tracking;
    QList<EventInfo> m_events;
    QList<RecordingInfo> m_recordings;
    QImage m_frame;
    bool m_enabled = false;
};
