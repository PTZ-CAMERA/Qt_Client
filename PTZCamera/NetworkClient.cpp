#include "NetworkClient.h"

#include <QStringList>

NetworkClient::NetworkClient(QObject *parent) : QObject(parent)
{
    connect(&m_socket, &QTcpSocket::connected, this, &NetworkClient::connected);
    connect(&m_socket, &QTcpSocket::disconnected, this, [this] {
        m_buffer.clear();
        emit disconnected();
    });
    connect(&m_socket, &QTcpSocket::readyRead, this, &NetworkClient::readAvailable);
    connect(&m_socket, &QTcpSocket::errorOccurred, this, [this](QAbstractSocket::SocketError) {
        emit connectionError(m_socket.errorString());
    });
}

void NetworkClient::connectToServer(const QString &host, quint16 port)
{
    m_buffer.clear();
    m_socket.abort();
    m_socket.connectToHost(host, port);
}

void NetworkClient::disconnectFromServer()
{
    m_socket.disconnectFromHost();
}

bool NetworkClient::isConnected() const
{
    return m_socket.state() == QAbstractSocket::ConnectedState;
}

void NetworkClient::sendPtzCommand(const QString &direction)
{
    static const QStringList allowed = {QStringLiteral("LEFT"), QStringLiteral("RIGHT"),
                                        QStringLiteral("UP"), QStringLiteral("DOWN"),
                                        QStringLiteral("CENTER")};
    if (allowed.contains(direction))
        sendLine(QStringLiteral("PTZ:%1").arg(direction).toUtf8());
}

void NetworkClient::sendTrackingCommand(bool enabled)
{
    sendLine(enabled ? QByteArray("TRACK:ON") : QByteArray("TRACK:OFF"));
}

void NetworkClient::sendLine(const QByteArray &line)
{
    if (isConnected()) {
        m_socket.write(line);
        m_socket.write("\n", 1);
    }
}

void NetworkClient::readAvailable()
{
    m_buffer += m_socket.readAll();
    qsizetype end = m_buffer.indexOf('\n');
    while (end >= 0) {
        const QByteArray line = m_buffer.left(end).trimmed();
        m_buffer.remove(0, end + 1);
        if (!line.isEmpty())
            parseMessage(QString::fromUtf8(line));
        end = m_buffer.indexOf('\n');
    }
    if (m_buffer.size() > 8192) {
        m_buffer.clear();
        emit protocolWarning(QStringLiteral("Incoming protocol line exceeded 8 KiB"));
    }
}

void NetworkClient::parseMessage(const QString &message)
{
    if (message == QLatin1String("NO_DETECTION")) {
        emit noDetectionReceived();
        return;
    }
    if (message == QLatin1String("CAMERA:CONNECTED") || message == QLatin1String("CAMERA:DISCONNECTED")) {
        emit cameraConnectionChanged(message == QLatin1String("CAMERA:CONNECTED"));
        return;
    }
    if (message.startsWith(QLatin1String("DETECT:"))) {
        const QStringList fields = message.mid(7).split(QLatin1Char(','));
        if (fields.size() != 8) {
            emit protocolWarning(QStringLiteral("Malformed DETECT message"));
            return;
        }
        TrackingInfo info;
        info.label = fields.at(0).trimmed();
        bool ok = !info.label.isEmpty();
        bool valid = false;
        info.confidence = fields.at(1).toFloat(&valid); ok &= valid && info.confidence >= 0 && info.confidence <= 1;
        info.objectX = fields.at(2).toInt(&valid); ok &= valid;
        info.objectY = fields.at(3).toInt(&valid); ok &= valid;
        info.errorX = fields.at(4).toInt(&valid); ok &= valid;
        info.errorY = fields.at(5).toInt(&valid); ok &= valid;
        info.panAngle = fields.at(6).toFloat(&valid); ok &= valid;
        info.tiltAngle = fields.at(7).toFloat(&valid); ok &= valid;
        if (!ok) {
            emit protocolWarning(QStringLiteral("Invalid DETECT values"));
            return;
        }
        info.detected = true;
        emit trackingInfoReceived(info);
        return;
    }
    emit protocolWarning(QStringLiteral("Unknown message: %1").arg(message.left(100)));
}
