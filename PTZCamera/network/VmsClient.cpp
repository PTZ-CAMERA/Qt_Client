#include "VmsClient.h"
#include <QAbstractSocket>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QUrl>
#include <QUrlQuery>
#include <QWebSocket>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <cmath>
namespace {
quint64 counter(const QJsonValue &value) {
    const auto number = value.toDouble(-1);
    return std::isfinite(number) && number >= 0 && number < std::ldexp(1.0, 64)
        ? static_cast<quint64>(number) : 0;
}
bool parseCamera(const QJsonObject &object, CameraInfo &camera) {
    if (!object.value(QStringLiteral("id")).isString() || object.value(QStringLiteral("id")).toString().isEmpty()
        || !object.value(QStringLiteral("status")).isString()) return false;
    camera.id = object.value(QStringLiteral("id")).toString();
    camera.ipAddress = object.value(QStringLiteral("ipAddress")).toString();
    camera.name = object.value(QStringLiteral("name")).toString();
    camera.status = object.value(QStringLiteral("status")).toString();
    if (camera.status != QStringLiteral("OFFLINE") && camera.status != QStringLiteral("CONNECTING")
        && camera.status != QStringLiteral("ONLINE") && camera.status != QStringLiteral("ERROR")) return false;
    camera.online = camera.status == QStringLiteral("ONLINE");
    camera.recording = object.value(QStringLiteral("recording")).toBool();
    camera.supportsRecordings = object.value(QStringLiteral("capabilities")).toObject().value(QStringLiteral("recordings")).toBool();
    camera.supportsPtz = object.value(QStringLiteral("capabilities")).toObject().value(QStringLiteral("ptz")).toBool();
    camera.supportsPtzCenter = object.value(QStringLiteral("capabilities")).toObject().value(QStringLiteral("ptzCenter")).toBool();
    camera.recordingRequested = object.value(QStringLiteral("recordingRequested")).toBool();
    camera.recordingState = object.value(QStringLiteral("recordingState")).toString(QStringLiteral("STOPPED"));
    camera.recordingError = object.value(QStringLiteral("recordingError")).toString();
    camera.rtspStatus = camera.status;
    camera.onvifStatus = object.value(QStringLiteral("onvifStatus")).toString(QStringLiteral("UNKNOWN"));
    camera.webRtcStatus = object.value(QStringLiteral("webRtcStatus")).toString(QStringLiteral("UNKNOWN"));
    camera.codec = object.value(QStringLiteral("codec")).toString();
    const int width = object.value(QStringLiteral("width")).toInt();
    const int height = object.value(QStringLiteral("height")).toInt();
    if (width > 0 && height > 0) camera.resolution = QSize(width, height);
    const double fps = object.value(QStringLiteral("fps")).toDouble();
    camera.fps = std::isfinite(fps) && fps > 0 && fps < 1000 ? qRound(fps) : 0;
    camera.packets = counter(object.value(QStringLiteral("packets")));
    camera.bytes = counter(object.value(QStringLiteral("bytes")));
    const auto timeBase = object.value(QStringLiteral("timeBase")).toObject();
    const int denominator = timeBase.value(QStringLiteral("den")).toInt();
    if (denominator > 0) camera.timeBase = QStringLiteral("%1/%2").arg(timeBase.value(QStringLiteral("num")).toInt()).arg(denominator);
    return true;
}
}
VmsClient::VmsClient(QObject *parent) : QObject(parent) {
    m_clock.start();
    m_streamRetry.setSingleShot(true); m_streamRetry.setInterval(1000);
    connect(&m_streamRetry, &QTimer::timeout, this, &VmsClient::fetchStream);
    m_connectTimer.setSingleShot(true); m_connectTimer.setInterval(5000);
    connect(&m_connectTimer, &QTimer::timeout, this, [this] {
        disconnectFromServer(); emit connectionError(QStringLiteral("VMS connection timed out"));
    });
    m_requestTimer.setInterval(250);
    connect(&m_requestTimer, &QTimer::timeout, this, [this] {
        const auto ids = m_pending.keys();
        for (const auto &id : ids) if (m_pending.value(id).deadline <= m_clock.elapsed()) {
            const auto pending = m_pending.value(id);
            m_pending.remove(id); emit requestFailed(id, QStringLiteral("TIMEOUT"), QStringLiteral("VMS request timed out"));
            failPtz(id, QStringLiteral("TIMEOUT"), QStringLiteral("VMS acceptance timed out"));
            if (id == m_latestRecordingQuery) {
                m_latestRecordingQuery.clear(); emit recordingSearchFailed(pending.cameraId, QStringLiteral("VMS request timed out"));
            }
        }
        for (const auto &id : m_ptzResults.keys()) if (m_ptzResults.value(id).deadline <= m_clock.elapsed())
            failPtz(id, QStringLiteral("TIMEOUT"), QStringLiteral("Pi response was not confirmed"));
    });
    m_requestTimer.start();
    m_heartbeat.setInterval(5000);
    connect(&m_heartbeat, &QTimer::timeout, this, [this] {
        if (!isConnected()) return;
        if (m_clock.elapsed() - m_lastActivity > 15000) {
            disconnectFromServer(); emit connectionError(QStringLiteral("VMS heartbeat timed out")); return;
        }
        m_socket->ping();
    });
}
VmsClient::~VmsClient() {
    cancelStreamRequest();
    if (m_socket) { m_socket->disconnect(this); m_socket->abort(); }
}
bool VmsClient::isConnected() const { return m_socket && m_socket->state() == QAbstractSocket::ConnectedState; }
void VmsClient::connectToServer(const QString &host, quint16 port) {
    disconnectFromServer();
    QUrl url; url.setScheme(QStringLiteral("ws")); url.setHost(host); url.setPort(port); url.setPath(QStringLiteral("/ws"));
    if (host.isEmpty() || !port || !url.isValid() || url.host().isEmpty()) {
        emit connectionError(QStringLiteral("Invalid VMS host or port")); return;
    }
    m_httpBase = url; m_httpBase.setScheme(QStringLiteral("http")); m_httpBase.setPath(QString());
    auto *socket = new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this);
    m_socket = socket;
    socket->setMaxAllowedIncomingFrameSize(65536); socket->setMaxAllowedIncomingMessageSize(65536);
    connect(socket, &QWebSocket::connected, this, [this, socket] {
        if (socket != m_socket) return;
        m_connectTimer.stop(); m_lastActivity = m_clock.elapsed(); m_heartbeat.start();
        emit message(QStringLiteral("VMS"), QStringLiteral("WebSocket connected"));
        emit serverConnectionChanged(true); requestCameraList();
    });
    connect(socket, &QWebSocket::disconnected, this, [this, socket] {
        if (socket != m_socket) return;
        cancelStreamRequest();
        m_connectTimer.stop(); m_heartbeat.stop(); clearPending(QStringLiteral("DISCONNECTED"), QStringLiteral("VMS disconnected"));
        emit serverConnectionChanged(false); emit message(QStringLiteral("VMS"), QStringLiteral("WebSocket disconnected"));
    });
    connect(socket, &QWebSocket::textMessageReceived, this, [this, socket](const QString &text) {
        if (socket == m_socket) { m_lastActivity = m_clock.elapsed(); receive(text); }
    });
    connect(socket, &QWebSocket::pong, this, [this, socket](quint64, const QByteArray &) {
        if (socket == m_socket) m_lastActivity = m_clock.elapsed();
    });
    auto onError = [this, socket](QAbstractSocket::SocketError) {
        if (socket != m_socket) return;
        const auto reason = socket->errorString();
        disconnectFromServer(); emit connectionError(reason);
    };
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    connect(socket, &QWebSocket::errorOccurred, this, onError);
#else
    connect(socket, QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::error), this, onError);
#endif
    emit message(QStringLiteral("VMS"), QStringLiteral("Connecting to %1").arg(url.toString()));
    m_connectTimer.start(); socket->open(url);
}
void VmsClient::disconnectFromServer() {
    cancelStreamRequest();
    m_connectTimer.stop(); m_heartbeat.stop();
    auto *socket = m_socket; m_socket = nullptr;
    if (socket) { socket->abort(); socket->deleteLater(); }
    clearPending(QStringLiteral("DISCONNECTED"), QStringLiteral("VMS disconnected"));
    emit serverConnectionChanged(false);
}
void VmsClient::clearPending(const QString &code, const QString &reason) {
    for (const auto &pending : m_ptzResults)
        emit message(QStringLiteral("PTZ ERROR"), QStringLiteral("%1 %2: %3; Pi response unconfirmed").arg(pending.cameraId, pending.command, reason));
    m_ptzResults.clear(); m_queuedMoves.clear(); m_deferredCenter.clear();
    cancelRecordingSearch();
    const auto ids = m_pending.keys(); m_pending.clear();
    for (const auto &id : ids) emit requestFailed(id, code, reason);
}
QString VmsClient::request(const QString &command, const QString &cameraId) {
    return sendRequest(command, cameraId, {});
}
QString VmsClient::sendRequest(const QString &command, const QString &cameraId, QJsonObject object) {
    if (!isConnected()) { emit message(QStringLiteral("VMS"), QStringLiteral("Connect to VMS first")); return {}; }
    if (m_pending.size() >= 64) { emit message(QStringLiteral("VMS"), QStringLiteral("Too many pending VMS requests")); return {}; }
    const auto id = QString::number(++m_nextRequest);
    object.insert(QStringLiteral("version"), 1); object.insert(QStringLiteral("requestId"), id); object.insert(QStringLiteral("command"), command);
    if (!cameraId.isEmpty()) object.insert(QStringLiteral("cameraId"), cameraId);
    const int timeout = command == QStringLiteral("REGISTER_CAMERA") ? 25000 : command == QStringLiteral("DISCOVER_CAMERAS") ? 10000 : (command == QStringLiteral("START_RECORDING") || command == QStringLiteral("STOP_RECORDING") || command == QStringLiteral("GET_RECORDINGS")) ? 15000 : 5000;
    m_pending.insert(id, {command, cameraId, m_clock.elapsed() + timeout});
    m_socket->sendTextMessage(QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Compact)));
    return id;
}
void VmsClient::requestCameraList() { request(QStringLiteral("GET_CAMERA_LIST")); }
void VmsClient::requestRecordings(const QString &id, const QDateTime &from, const QDateTime &to) {
    if (!isConnected() || id.isEmpty() || !from.isValid() || !to.isValid() || from > to) {
        cancelRecordingSearch(); emit recordingSearchFailed(id, QStringLiteral("Connect to VMS and select a valid camera/time range")); return;
    }
    m_latestRecordingQuery = sendRequest(QStringLiteral("GET_RECORDINGS"), id, {{QStringLiteral("fromMs"), from.toMSecsSinceEpoch()}, {QStringLiteral("toMs"), to.toMSecsSinceEpoch() + 1}, {QStringLiteral("limit"), 100}});
    if (m_latestRecordingQuery.isEmpty()) emit recordingSearchFailed(id, QStringLiteral("VMS request could not be submitted"));
}
void VmsClient::requestCameraStatus(const QString &id) { if (!id.isEmpty()) request(QStringLiteral("GET_CAMERA_STATUS"), id); }
QString VmsClient::discoverCameras() { return request(QStringLiteral("DISCOVER_CAMERAS")); }
void VmsClient::registerCamera(const QString &url, const QString &username, const QString &password, const QString &profile) {
    sendRequest(QStringLiteral("REGISTER_CAMERA"), {}, {{QStringLiteral("deviceServiceUrl"), url},
        {QStringLiteral("username"), username}, {QStringLiteral("password"), password}, {QStringLiteral("profileToken"), profile}});
}
void VmsClient::cancelStreamRequest() {
    m_streamRetry.stop(); m_streamCameraId.clear();
    auto reply = m_streamReply; m_streamReply.clear();
    if (reply) reply->abort();
}
void VmsClient::requestStream(const QString &id, const QString &transport) {
    if (transport != QStringLiteral("tcp") && transport != QStringLiteral("udp")) { emit streamError(QStringLiteral("Invalid RTSP transport")); return; }
    if (!isConnected() || id.isEmpty()) { emit streamError(QStringLiteral("Select a camera and connect to VMS first")); return; }
    cancelStreamRequest(); m_streamTransport = transport; m_streamCameraId = id; m_streamDeadline = m_clock.elapsed() + 20000; fetchStream();
}
void VmsClient::fetchStream() {
    if (!isConnected() || m_streamCameraId.isEmpty()) return;
    if (m_clock.elapsed() >= m_streamDeadline) {
        cancelStreamRequest(); emit streamError(QStringLiteral("VMS stream is not ready; retry after checking the camera")); return;
    }
    auto url = m_httpBase; url.setPath(QStringLiteral("/api/v1/cameras/%1/stream").arg(m_streamCameraId));
    QUrlQuery query; query.addQueryItem(QStringLiteral("transport"), m_streamTransport); url.setQuery(query);
    QNetworkRequest request(url); request.setTransferTimeout(5000);
    auto *reply = m_http.get(request); m_streamReply = reply;
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater(); if (reply != m_streamReply) return;
        m_streamReply.clear();
        const auto status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const auto body = reply->readAll();
        if (body.size() > 65536) { cancelStreamRequest(); emit streamError(QStringLiteral("Invalid VMS stream response")); return; }
        const auto object = QJsonDocument::fromJson(body).object();
        if (status == 503 && object.value(QStringLiteral("error")).toObject().value(QStringLiteral("code")).toString() == QStringLiteral("STREAM_NOT_READY")) {
            m_streamRetry.start(); return;
        }
        const auto data = object.value(QStringLiteral("data")).toObject();
        const QUrl uri(data.value(QStringLiteral("uri")).toString());
        if (status != 200 || !object.value(QStringLiteral("ok")).toBool() || !data.value(QStringLiteral("ready")).toBool()
            || data.value(QStringLiteral("cameraId")).toString() != m_streamCameraId || !uri.isValid()
            || uri.scheme() != QStringLiteral("rtsp") || uri.host().isEmpty() || !uri.userInfo().isEmpty()
            || data.value(QStringLiteral("transport")).toString() != m_streamTransport) {
            cancelStreamRequest(); emit streamError(QStringLiteral("VMS did not return a ready managed RTSP stream")); return;
        }
        const auto id = m_streamCameraId; m_streamCameraId.clear(); emit streamUriReady(id, uri);
    });
}
void VmsClient::receive(const QString &text) {
    QJsonParseError error;
    const auto doc = QJsonDocument::fromJson(text.toUtf8(), &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject()) {
        emit message(QStringLiteral("VMS"), QStringLiteral("Invalid JSON response")); return;
    }
    const auto object = doc.object();
    if (object.value(QStringLiteral("version")).toInt() != 1) {
        emit message(QStringLiteral("VMS"), QStringLiteral("Unsupported VMS protocol version")); return;
    }
    const auto type = object.value(QStringLiteral("type")).toString();
    if (type == QStringLiteral("notification") && object.value(QStringLiteral("event")).toString() == QStringLiteral("PTZ_RESULT")) {
        handlePtzResult(object); return;
    }
    if (type == QStringLiteral("notification") && object.value(QStringLiteral("event")).toString() == QStringLiteral("CAMERA_STATUS")) {
        CameraInfo camera;
        if (parseCamera(object.value(QStringLiteral("data")).toObject().value(QStringLiteral("camera")).toObject(), camera)
            && camera.id == object.value(QStringLiteral("cameraId")).toString()) emit cameraStatusChanged(camera);
        return;
    }
    if (type != QStringLiteral("response")) return;
    const auto id = object.value(QStringLiteral("requestId")).toString();
    if (!m_pending.contains(id)) return;
    const auto pending = m_pending.take(id);
    if (!object.value(QStringLiteral("ok")).isBool()) {
        failPtz(id, QStringLiteral("INVALID_RESPONSE"), QStringLiteral("Missing VMS acceptance status"));
        if (id == m_latestRecordingQuery) {
            cancelRecordingSearch(); emit recordingSearchFailed(pending.cameraId, QStringLiteral("Invalid VMS response"));
        }
        emit requestFailed(id, QStringLiteral("INVALID_RESPONSE"), QStringLiteral("Missing response status")); return;
    }
    if (!object.value(QStringLiteral("ok")).toBool()) {
        const auto failure = object.value(QStringLiteral("error")).toObject();
        failPtz(id, failure.value(QStringLiteral("code")).toString(), failure.value(QStringLiteral("message")).toString());
        if (id == m_latestRecordingQuery) {
            cancelRecordingSearch(); emit recordingSearchFailed(pending.cameraId, failure.value(QStringLiteral("message")).toString());
        }
        emit requestFailed(id, failure.value(QStringLiteral("code")).toString(), failure.value(QStringLiteral("message")).toString()); return;
    }
    const auto data = object.value(QStringLiteral("data")).toObject();
    if (pending.command.startsWith(QStringLiteral("PTZ_"))) {
        if (data.value(QStringLiteral("phase")).toString() != QStringLiteral("ACCEPTED")
            || data.value(QStringLiteral("cameraId")).toString() != pending.cameraId
            || data.value(QStringLiteral("command")).toString() != pending.command) {
            failPtz(id, QStringLiteral("INVALID_RESPONSE"), QStringLiteral("Invalid PTZ acceptance")); return;
        }
        if (m_ptzResults.contains(id)) m_ptzResults[id].deadline = m_clock.elapsed() + 5000;
        emit message(QStringLiteral("PTZ VMS"), QStringLiteral("%1 %2 request=%3 ACCEPTED; Pi response pending").arg(pending.cameraId, pending.command, id));
        return;
    }
    if (pending.command == QStringLiteral("GET_CAMERA_LIST")) {
        if (!data.value(QStringLiteral("cameras")).isArray()) {
            emit requestFailed(id, QStringLiteral("INVALID_RESPONSE"), QStringLiteral("Missing camera list")); return;
        }
        QList<CameraInfo> cameras;
        for (const auto &value : data.value(QStringLiteral("cameras")).toArray()) {
            CameraInfo camera;
            if (!value.isObject() || !parseCamera(value.toObject(), camera)) {
                emit requestFailed(id, QStringLiteral("INVALID_RESPONSE"), QStringLiteral("Invalid camera data")); return;
            }
            cameras.append(camera);
        }
        emit cameraListReceived(cameras);
    } else if (pending.command == QStringLiteral("START_RECORDING") || pending.command == QStringLiteral("STOP_RECORDING")) {
        emit message(QStringLiteral("REC"), QStringLiteral("%1: %2").arg(pending.cameraId, data.value(QStringLiteral("state")).toString()));
        requestCameraStatus(pending.cameraId);
    } else if (pending.command == QStringLiteral("GET_RECORDINGS")) {
        if (id != m_latestRecordingQuery) return;
        cancelRecordingSearch();
        if (!data.value(QStringLiteral("recordings")).isArray()) {
            emit recordingSearchFailed(pending.cameraId, QStringLiteral("Missing recording results")); return;
        }
        QList<RecordingInfo> recordings;
        for (const auto &value : data.value(QStringLiteral("recordings")).toArray()) {
            const auto item = value.toObject(); RecordingInfo recording;
            const auto start = item.value(QStringLiteral("startTimeMs")).toDouble(NAN);
            const auto end = item.value(QStringLiteral("endTimeMs")).toDouble(NAN);
            if (!value.isObject() || item.value(QStringLiteral("cameraId")).toString() != pending.cameraId
                || item.value(QStringLiteral("filePath")).toString().isEmpty() || !std::isfinite(start) || !std::isfinite(end)
                || std::abs(start) >= std::ldexp(1.0, 53) || std::abs(end) >= std::ldexp(1.0, 53) || end <= start) {
                emit recordingSearchFailed(pending.cameraId, QStringLiteral("Invalid recording result")); return;
            }
            recording.cameraId = item.value(QStringLiteral("cameraId")).toString();
            recording.startTime = QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(item.value(QStringLiteral("startTimeMs")).toDouble()));
            recording.endTime = QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(item.value(QStringLiteral("endTimeMs")).toDouble()));
            recording.filePath = item.value(QStringLiteral("filePath")).toString(); recordings.append(recording);
        }
        emit recordingListReceived(pending.cameraId, recordings);
    } else if (pending.command == QStringLiteral("DISCOVER_CAMERAS")) {
        QList<DiscoveredCamera> devices;
        for (const auto &value : data.value(QStringLiteral("devices")).toArray()) {
            const auto item = value.toObject();
            devices.append({item.value(QStringLiteral("deviceServiceUrl")).toString(), item.value(QStringLiteral("name")).toString(),
                item.value(QStringLiteral("address")).toString(), item.value(QStringLiteral("source")).toString(),
                item.value(QStringLiteral("cameraId")).toString(), item.value(QStringLiteral("rtspUri")).toString(),
                item.value(QStringLiteral("registered")).toBool(), item.value(QStringLiteral("ready")).toBool()});
        }
        emit discoveryReceived(devices);
    } else if (pending.command == QStringLiteral("REGISTER_CAMERA")) {
        const auto cameraId = data.value(QStringLiteral("cameraId")).toString();
        if (!cameraId.isEmpty()) { emit cameraRegistered(cameraId); requestCameraList(); }
    } else if (pending.command == QStringLiteral("GET_CAMERA_STATUS")) {
        CameraInfo camera;
        if (parseCamera(data.value(QStringLiteral("camera")).toObject(), camera) && camera.id == pending.cameraId) emit cameraStatusChanged(camera);
        else emit requestFailed(id, QStringLiteral("INVALID_RESPONSE"), QStringLiteral("Invalid camera data"));
    }
}

bool VmsClient::ptzStopPending(const QString &cameraId) const {
    for (const auto &pending : m_ptzResults)
        if (pending.cameraId == cameraId && pending.command == QStringLiteral("PTZ_STOP")) return true;
    return false;
}
QString VmsClient::sendPtz(const QString &command, const QString &cameraId, QJsonObject fields) {
    if (!isConnected() || cameraId.isEmpty()) {
        emit message(QStringLiteral("PTZ ERROR"), QStringLiteral("%1 %2 not sent: VMS disconnected or camera missing").arg(cameraId, command)); return {};
    }
    if (m_ptzResults.size() >= 64) {
        emit ptzFailed(cameraId, command, QStringLiteral("BUSY"), QStringLiteral("Too many unconfirmed PTZ commands")); return {};
    }
    const auto id = sendRequest(command, cameraId, fields);
    if (id.isEmpty()) { emit ptzFailed(cameraId, command, QStringLiteral("SEND_FAILED"), QStringLiteral("VMS request not submitted")); return {}; }
    m_ptzResults.insert(id, {command, cameraId, m_clock.elapsed() + 5000});
    const QString velocity = command == QStringLiteral("PTZ_MOVE")
        ? QStringLiteral(" pan=%1 tilt=%2").arg(fields.value(QStringLiteral("panVelocity")).toDouble()).arg(fields.value(QStringLiteral("tiltVelocity")).toDouble()) : QString();
    emit message(QStringLiteral("PTZ TX"), QStringLiteral("%1 %2 request=%3%4 → VMS").arg(cameraId, command, id, velocity));
    return id;
}
QString VmsClient::sendPtzMove(const QString &cameraId, float pan, float tilt) {
    if (!std::isfinite(pan) || !std::isfinite(tilt) || std::abs(pan) > 1 || std::abs(tilt) > 1) {
        emit ptzFailed(cameraId, QStringLiteral("PTZ_MOVE"), QStringLiteral("INVALID_VELOCITY"), QStringLiteral("PTZ velocity must be finite and within -1..1")); return {};
    }
    if (!isConnected()) return sendPtz(QStringLiteral("PTZ_MOVE"), cameraId);
    if (ptzStopPending(cameraId)) {
        m_queuedMoves.insert(cameraId, {pan, tilt, m_clock.elapsed()});
        if (m_deferredCenter == cameraId) m_deferredCenter.clear();
        return {};
    }
    return sendPtz(QStringLiteral("PTZ_MOVE"), cameraId, {{QStringLiteral("panVelocity"), pan}, {QStringLiteral("tiltVelocity"), tilt}});
}
QString VmsClient::sendPtzStop(const QString &cameraId) {
    m_queuedMoves.remove(cameraId); if (m_deferredCenter == cameraId) m_deferredCenter.clear();
    for (auto it = m_ptzResults.cbegin(); it != m_ptzResults.cend(); ++it)
        if (it->cameraId == cameraId && it->command == QStringLiteral("PTZ_STOP")) return it.key();
    return sendPtz(QStringLiteral("PTZ_STOP"), cameraId);
}
QString VmsClient::sendPtzCenter(const QString &cameraId) {
    m_queuedMoves.remove(cameraId);
    if (ptzStopPending(cameraId)) {
        m_deferredCenter = cameraId;
        emit message(QStringLiteral("PTZ VMS"), QStringLiteral("%1 CENTER waits for Pi STOP acknowledgement").arg(cameraId)); return {};
    }
    return sendPtz(QStringLiteral("PTZ_CENTER"), cameraId);
}
void VmsClient::failPtz(const QString &requestId, const QString &code, const QString &reason) {
    if (!m_ptzResults.contains(requestId)) return;
    const auto pending = m_ptzResults.take(requestId);
    if (pending.command == QStringLiteral("PTZ_STOP")) {
        m_queuedMoves.remove(pending.cameraId); if (m_deferredCenter == pending.cameraId) m_deferredCenter.clear();
    }
    emit message(QStringLiteral("PTZ ERROR"), QStringLiteral("%1 %2 request=%3 %4: %5; Pi confirmation absent").arg(pending.cameraId, pending.command, requestId, code, reason));
    emit ptzFailed(pending.cameraId, pending.command, code, reason);
}
void VmsClient::handlePtzResult(const QJsonObject &object) {
    const auto id = object.value(QStringLiteral("requestId")).toString();
    if (!m_ptzResults.contains(id)) return;
    const auto pending = m_ptzResults.value(id); const auto data = object.value(QStringLiteral("data")).toObject();
    const auto phase = data.value(QStringLiteral("phase")).toString();
    if (data.value(QStringLiteral("cameraId")).toString() != pending.cameraId || data.value(QStringLiteral("command")).toString() != pending.command) {
        failPtz(id, QStringLiteral("INVALID_RESPONSE"), QStringLiteral("PTZ result camera/command mismatch")); return;
    }
    if (phase == QStringLiteral("FAILED")) {
        const auto error = object.value(QStringLiteral("error")).toObject();
        failPtz(id, error.value(QStringLiteral("code")).toString(), error.value(QStringLiteral("message")).toString()); return;
    }
    if (phase == QStringLiteral("SUPERSEDED") && !object.value(QStringLiteral("ok")).toBool()) {
        m_ptzResults.remove(id);
        emit message(QStringLiteral("PTZ VMS"), QStringLiteral("%1 %2 request=%3 SUPERSEDED; no Pi confirmation for this request").arg(pending.cameraId, pending.command, id)); return;
    }
    if (phase != QStringLiteral("PI_ACKNOWLEDGED") || !object.value(QStringLiteral("ok")).toBool()) {
        failPtz(id, QStringLiteral("INVALID_RESPONSE"), QStringLiteral("Invalid Pi acknowledgement")); return;
    }
    m_ptzResults.remove(id);
    emit message(QStringLiteral("PTZ PI"), QStringLiteral("%1 %2 request=%3 PI_ACKNOWLEDGED (ONVIF response; motor arrival not confirmed)").arg(pending.cameraId, pending.command, id));
    if (pending.command == QStringLiteral("PTZ_STOP") && !ptzStopPending(pending.cameraId)) {
        if (m_deferredCenter == pending.cameraId) { m_deferredCenter.clear(); sendPtzCenter(pending.cameraId); }
        else if (m_queuedMoves.contains(pending.cameraId)) {
            const auto movement = m_queuedMoves.take(pending.cameraId);
            if (m_clock.elapsed() - movement.time <= 400) sendPtzMove(pending.cameraId, movement.pan, movement.tilt);
        }
    }
}
