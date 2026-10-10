#pragma once
#include "model/VmsTypes.h"
#include <QElapsedTimer>
#include <QHash>
#include <QJsonObject>
#include <QSet>
#include <QJsonArray>
#include <QObject>
#include <QNetworkAccessManager>
#include <QPointer>
#include <QUrl>
#include <QTimer>
class QWebSocket;
class QNetworkReply;
// UI-thread async WebSocket adapter. Never connects directly to a camera.
class VmsClient : public QObject {
    Q_OBJECT
public:
    explicit VmsClient(QObject *parent = nullptr);
    ~VmsClient() override;
    void connectToServer(const QString &host, quint16 port);
    void disconnectFromServer();
    bool isConnected() const;
    QString request(const QString &command, const QString &cameraId = {});
    void requestCameraList();
    void requestRecordings(const QString &cameraId, const QDateTime &from, const QDateTime &to);
    void cancelRecordingSearch() { m_latestRecordingQuery.clear(); }
    void requestCameraStatus(const QString &cameraId);
    QString discoverCameras();
    void registerCamera(const QString &serviceUrl, const QString &username, const QString &password, const QString &profileToken);
    void requestStream(const QString &cameraId, const QString &transport = QStringLiteral("tcp"));
    void cancelStreamRequest();
    QString sendPtzMove(const QString &cameraId, float panVelocity, float tiltVelocity);
    QString sendPtzStop(const QString &cameraId);
    QString sendPtzCenter(const QString &cameraId);
    QString sendTracking(const QString &cameraId, bool enabled);
    bool ownsTracking(const QString &cameraId) const;
    QString searchMetadata(const QString &cameraId,const QDateTime &from,const QDateTime &to,
        bool detections,const QString &type,double minConfidence,const QJsonValue &cursor = {});
    QString requestEventPlayback(const QJsonObject &record);
    QString chatSearch(const QString &cameraId,const QString &text);
    QString setAutoRecording(const QString &cameraId,bool enabled);
    void cancelInteractiveRequests();
    void cancelMetadataSearch() { m_latestMetadataQuery.clear(); }
signals:
    void metadataStatusReceived(const QString &requestId,const QJsonObject &data);
    void autoRecordingConfigured(const QString &requestId,const QString &cameraId,bool enabled);
    void metadataReceived(const QString &cameraId,const QJsonObject &data);
    void eventReceived(const QString &cameraId,const QJsonObject &data);
    void eventReceiverStatus(const QString &cameraId,const QJsonObject &data);
    void metadataSearchReceived(const QString &requestId,const QString &cameraId,const QJsonArray &records,const QJsonValue &cursor);
    void eventPlaybackReceived(const QString &requestId,const QJsonObject &data);
    void chatSearchReceived(const QString &requestId,const QJsonObject &data);
    void ptzFailed(const QString &cameraId, const QString &command, const QString &code, const QString &message);
    void controlPhase(const QString &cameraId, const QString &command, const QString &phase);
    void serverConnectionChanged(bool connected);
    void cameraListReceived(const QList<CameraInfo> &cameras);
    void cameraStatusChanged(const CameraInfo &camera);
    void recordingListReceived(const QString &cameraId, const QList<RecordingInfo> &recordings);
    void recordingSearchFailed(const QString &cameraId, const QString &message);
    void discoveryReceived(const QList<DiscoveredCamera> &devices);
    void cameraRegistered(const QString &cameraId);
    void streamUriReady(const QString &cameraId, const QUrl &uri);
    void streamError(const QString &message);
    void message(const QString &source, const QString &text);
    void connectionError(const QString &text);
    void requestFailed(const QString &requestId, const QString &code, const QString &message);
private:
    struct Pending { QString command, cameraId; qint64 deadline; };
    QString sendRequest(const QString &command, const QString &cameraId, QJsonObject fields);
    void fetchStream();
    void receive(const QString &text);
    void clearPending(const QString &code, const QString &reason);
    QString sendPtz(const QString &command, const QString &cameraId, QJsonObject fields = {});
    bool ptzStopPending(const QString &cameraId) const;
    void failPtz(const QString &requestId, const QString &code, const QString &reason);
    void handlePtzResult(const QJsonObject &object);
    QHash<QString, Pending> m_ptzResults;
    struct PtzMove { float pan, tilt; qint64 time; };
    QHash<QString, PtzMove> m_queuedMoves;
    QString m_deferredCenter;
    QHash<QString,bool> m_deferredTracking;
    QSet<QString> m_trackingOwners;
    QWebSocket *m_socket = nullptr;
    QHash<QString, Pending> m_pending;
    QTimer m_connectTimer, m_requestTimer, m_heartbeat;
    QElapsedTimer m_clock;
    qint64 m_lastActivity = 0;
    quint64 m_nextRequest = 0;
    QString m_latestRecordingQuery;
    QString m_latestMetadataQuery,m_latestPlaybackQuery,m_latestChatQuery;
    QNetworkAccessManager m_http;
    QPointer<QNetworkReply> m_streamReply;
    QTimer m_streamRetry;
    QUrl m_httpBase;
    QString m_streamCameraId;
    QString m_streamTransport = QStringLiteral("tcp");
    qint64 m_streamDeadline = 0;
};
