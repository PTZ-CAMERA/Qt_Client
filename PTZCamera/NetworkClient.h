#pragma once

#include "TrackingInfo.h"

#include <QByteArray>
#include <QObject>
#include <QTcpSocket>

class NetworkClient : public QObject
{
    Q_OBJECT
public:
    explicit NetworkClient(QObject *parent = nullptr);
    void connectToServer(const QString &host, quint16 port);
    void disconnectFromServer();
    bool isConnected() const;
    void sendPtzCommand(const QString &direction);
    void sendTrackingCommand(bool enabled);

signals:
    void connected();
    void disconnected();
    void connectionError(const QString &reason);
    void trackingInfoReceived(const TrackingInfo &info);
    void noDetectionReceived();
    void cameraConnectionChanged(bool connected);
    void protocolWarning(const QString &message);

private:
    void readAvailable();
    void parseMessage(const QString &message);
    void sendLine(const QByteArray &line);

    QTcpSocket m_socket;
    QByteArray m_buffer;
};
