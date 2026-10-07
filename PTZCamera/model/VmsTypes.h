#pragma once
#include <QDateTime>
#include <QList>
#include <QMetaType>
#include <QPoint>
#include <QRect>
#include <QSize>
#include <QString>

// 화면에서 사용하는 자료형이다. 네트워크 패킷 형식이나 서버 API를 정의하지 않는다.
struct CameraInfo {
    QString id;
    QString name;
    QString status = QStringLiteral("OFFLINE");
    QString ipAddress;
    bool online = false;
    bool supportsRecordings = false;
    bool supportsPtz = false, supportsPtzCenter = false;
    bool recording = false, recordingRequested = false;
    QString recordingState = QStringLiteral("STOPPED"), recordingError;
    bool tracking = false;
    QString onvifStatus = QStringLiteral("UNKNOWN");
    QString rtspStatus = QStringLiteral("UNKNOWN");
    QString webRtcStatus = QStringLiteral("UNKNOWN");
    QString codec;
    QSize resolution;
    int fps = 0;
    QString timeBase;
    quint64 packets = 0, bytes = 0;
};
struct DetectionInfo {
    bool detected = false;
    QString label;
    float confidence = 0;
    QRect boundingBox;
    QPoint objectCenter;
    int errorX = 0;
    int errorY = 0;
};
struct EventInfo {
    QDateTime timestamp;
    QString cameraId;
    QString type;
    float confidence = 0;
    float panAngle = 0;
    float tiltAngle = 0;
};
struct RecordingInfo {
    QString cameraId;
    QDateTime startTime;
    QDateTime endTime;
    QString playbackUri;
    bool simulated = false;
    QString filePath;
};
// 향후 queued signal과 Qt 메타 객체에서도 화면 자료형을 사용할 수 있게 등록한다.
Q_DECLARE_METATYPE(CameraInfo)
Q_DECLARE_METATYPE(DetectionInfo)
Q_DECLARE_METATYPE(EventInfo)
Q_DECLARE_METATYPE(RecordingInfo)

struct DiscoveredCamera {
    QString deviceServiceUrl, name, address, source;
    QString cameraId, rtspUri;
    bool registered = false, ready = false;
};
Q_DECLARE_METATYPE(DiscoveredCamera)
