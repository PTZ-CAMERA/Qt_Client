// UI 전용 샘플 공급자다. 네트워크, ONVIF, FFmpeg, 파일 녹화와 SQL을 사용하지 않는다.
// 조회/명령은 메모리의 모의 자료만 다루며 모든 로그에 DUMMY 출처를 표시한다.
#include "DummyDataProvider.h"
#include <QPainter>
DummyDataProvider::DummyDataProvider(QObject *parent) : QObject(parent) {
    CameraInfo camera;
    camera.id = QStringLiteral("CAM01"); camera.name = QStringLiteral("Raspberry Pi PTZ");
    camera.ipAddress = QStringLiteral("192.168.0.x"); camera.online = true; camera.recording = true; camera.tracking = true;
    camera.onvifStatus = QStringLiteral("CONNECTED (DUMMY)"); camera.rtspStatus = QStringLiteral("ACTIVE (DUMMY)");
    camera.webRtcStatus = QStringLiteral("ACTIVE (DUMMY)"); camera.codec = QStringLiteral("H264");
    camera.resolution = QSize(1280, 720); camera.fps = 30; m_cameras.append(camera);
    CameraInfo offline; offline.id = QStringLiteral("CAM02"); offline.name = QStringLiteral("Offline demo camera"); m_cameras.append(offline);
    TrackingInfo info; info.enabled = true; info.detected = true; info.target = info.label = QStringLiteral("Person");
    info.status = QStringLiteral("TRACKING"); info.confidence = 0.92F; info.errorX = 32; info.errorY = -18;
    info.panAngle = 103; info.tiltAngle = 86; info.objectX = 672; info.objectY = 342; info.frameWidth = 1280; info.frameHeight = 720;
    m_tracking.insert(camera.id, info);
    const QDate day = QDate::currentDate();
    m_events = {{QDateTime(day, QTime(15, 31, 22)), camera.id, QStringLiteral("PERSON_DETECTED"), 0.92F, 103, 86},
                {QDateTime(day, QTime(15, 32, 10)), camera.id, QStringLiteral("PERSON_LOST"), 0, 103, 86},
                {QDateTime(day, QTime(15, 33, 44)), camera.id, QStringLiteral("PERSON_DETECTED"), 0.88F, 101, 87}};
    m_recordings = {{camera.id, QDateTime(day, QTime(15, 0)), QDateTime(day, QTime(16, 0)), QString(), true}};
    m_frame = QImage(1280, 720, QImage::Format_RGB32); m_frame.fill(QColor("#101c27"));
    QPainter painter(&m_frame); painter.setPen(QPen(QColor("#243c4c"), 1));
    for (int x = 0; x < 1280; x += 80) painter.drawLine(x, 0, x, 720);
    for (int y = 0; y < 720; y += 80) painter.drawLine(0, y, 1280, y);
    painter.setPen(QColor("#9eb0be")); QFont font = painter.font(); font.setPixelSize(24); painter.setFont(font);
    painter.drawText(QRect(24, 20, 900, 40), QStringLiteral("DUMMY FRAME / NO VMS STREAM / 1280 x 720"));
}
void DummyDataProvider::setEnabled(bool enabled) {
    m_enabled = enabled; emit serverConnectionChanged(enabled); requestCameraList();
    emit eventListReceived(enabled ? m_events : QList<EventInfo>{});
    emit recordingListReceived(enabled ? m_recordings : QList<RecordingInfo>{});
    emit message(QStringLiteral("DUMMY"), enabled ? QStringLiteral("Simulated VMS data enabled; no network connection") : QStringLiteral("Dummy data disabled"));
}
void DummyDataProvider::requestCameraList() { emit cameraListReceived(m_enabled ? m_cameras : QList<CameraInfo>{}); }
void DummyDataProvider::requestCameraStatus(const QString &id) {
    if (!m_enabled) return;
    for (const auto &camera : m_cameras) if (camera.id == id) {
        emit cameraStatusChanged(camera);
        emit frameReceived(id, camera.online ? m_frame : QImage());
        DetectionInfo detection;
        if (camera.online) {
            detection.detected = true; detection.label = QStringLiteral("Person"); detection.confidence = 0.92F;
            detection.boundingBox = QRect(582, 202, 180, 280); detection.objectCenter = QPoint(672, 342);
            detection.errorX = 32; detection.errorY = -18;
        }
        emit detectionReceived(id, detection);
        emit trackingInfoReceived(id, m_tracking.value(id));
        return;
    }
}
// 검색은 서버 API가 정해지기 전의 UI 시험용 메모리 필터다.
void DummyDataProvider::requestEvents(const QString &id, const QDateTime &start, const QDateTime &end, const QString &type) {
    QList<EventInfo> results;
    if (m_enabled) for (const auto &event : m_events)
        if (event.cameraId == id && event.timestamp >= start && event.timestamp <= end
            && (type == QStringLiteral("ALL") || type == event.type)) results.append(event);
    emit eventListReceived(results);
    emit message(QStringLiteral("DUMMY"), QStringLiteral("%1 event results for %2").arg(results.size()).arg(id));
}
void DummyDataProvider::requestRecordings(const QString &id, const QDateTime &start, const QDateTime &end) {
    QList<RecordingInfo> results;
    if (m_enabled) for (const auto &recording : m_recordings)
        if (recording.cameraId == id && recording.endTime >= start && recording.startTime <= end) results.append(recording);
    emit recordingListReceived(results);
}
void DummyDataProvider::requestRecordingAt(const QString &id, const QDateTime &time) {
    if (m_enabled) for (const auto &recording : m_recordings)
        if (recording.cameraId == id && time >= recording.startTime && time <= recording.endTime) {
            emit recordingResolved(recording, time); return;
        }
    emit message(QStringLiteral("DUMMY"), QStringLiteral("No recording covers the selected event"));
}
void DummyDataProvider::setTrackingEnabled(const QString &id, bool enabled) {
    if (!m_enabled || !m_tracking.contains(id)) return;
    auto &info = m_tracking[id]; info.enabled = enabled;
    info.status = enabled ? QStringLiteral("TRACKING") : QStringLiteral("IDLE");
    for (auto &camera : m_cameras) if (camera.id == id) { camera.tracking = enabled; emit cameraStatusChanged(camera); }
    emit trackingInfoReceived(id, info);
    emit message(QStringLiteral("DUMMY"), QStringLiteral("%1 tracking %2").arg(id, enabled ? QStringLiteral("ON") : QStringLiteral("OFF")));
}
void DummyDataProvider::movePtz(const QString &id, float pan, float tilt) {
    if (m_enabled) emit message(QStringLiteral("DUMMY"), QStringLiteral("%1 PTZ MOVE pan=%2 tilt=%3 (not transmitted)").arg(id).arg(pan).arg(tilt));
}
void DummyDataProvider::stopPtz(const QString &id) {
    if (m_enabled && !id.isEmpty()) emit message(QStringLiteral("DUMMY"), QStringLiteral("%1 PTZ STOP (not transmitted)").arg(id));
}
void DummyDataProvider::centerPtz(const QString &id) {
    if (!m_enabled || !m_tracking.contains(id)) return;
    auto &info = m_tracking[id]; info.panAngle = 90; info.tiltAngle = 90;
    emit trackingInfoReceived(id, info);
    emit message(QStringLiteral("DUMMY"), QStringLiteral("%1 PTZ CENTER (not transmitted)").arg(id));
}
