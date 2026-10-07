// Pi와 줄바꿈으로 구분된 텍스트 명령/상태를 주고받는 기존 TCP 클라이언트이다.
// 현재 PTZ와 추적 ON/OFF를 송신하고 탐지 정보를 수신한다. ONVIF PTZ 송신은 아직 없다.
#include "NetworkClient.h"

#include <QStringList>

// 소켓 이벤트를 UI가 사용할 신호로 연결한다.
// connectToHost()/write()는 비동기 방식이므로 연결 성공과 오류는 이후 신호로 확인한다.
NetworkClient::NetworkClient(QObject *parent) : QObject(parent)
{
    connect(&m_socket, &QTcpSocket::connected, this, &NetworkClient::connected);
    connect(&m_socket, &QTcpSocket::disconnected, this, [this] {
        // 끊어진 연결의 미완성 메시지가 다음 연결에 섞이지 않도록 버린다.
        m_buffer.clear();
        emit disconnected();
    });
    connect(&m_socket, &QTcpSocket::readyRead, this, &NetworkClient::readAvailable);
    connect(&m_socket, &QTcpSocket::errorOccurred, this, [this](QAbstractSocket::SocketError) {
        emit connectionError(m_socket.errorString());
    });
}

// 기존 연결과 수신 누적값을 정리한 후 새 접속을 요청한다.
// 함수 반환 시점에는 연결이 완료되지 않을 수 있다.
void NetworkClient::connectToServer(const QString &host, quint16 port)
{
    m_buffer.clear();
    m_socket.abort();
    m_socket.connectToHost(host, port);
}

// 정상 연결 종료를 요청한다. 종료 완료는 disconnected 신호로 알려진다.
void NetworkClient::disconnectFromServer()
{
    m_socket.disconnectFromHost();
}

// 소켓이 TCP 연결 완료 상태인지 확인한다. Pi의 명령 처리 성공 여부까지 뜻하지는 않는다.
bool NetworkClient::isConnected() const
{
    return m_socket.state() == QAbstractSocket::ConnectedState;
}

// UI 방향값을 서버가 받는 PTZ:방향 형식으로 변환한다.
// 정의한 다섯 가지 값만 허용하고, 각도 제한과 실제 서보 이동은 Pi 서버에서 처리한다.
void NetworkClient::sendPtzCommand(const QString &direction)
{
    static const QStringList allowed = {QStringLiteral("LEFT"), QStringLiteral("RIGHT"),
                                        QStringLiteral("UP"), QStringLiteral("DOWN"),
                                        QStringLiteral("CENTER")};
    if (allowed.contains(direction))
        sendLine(QStringLiteral("PTZ:%1").arg(direction).toUtf8());
}

// 자동 추적 요청도 같은 TCP 텍스트 프로토콜을 사용한다.
void NetworkClient::sendTrackingCommand(bool enabled)
{
    sendLine(enabled ? QByteArray("TRACK:ON") : QByteArray("TRACK:OFF"));
}

// 모든 송신 메시지에 줄바꿈을 붙인다. write()는 송신 버퍼에 데이터를 맡기는 호출이며
// 서버 응답이나 실제 서보 동작의 확인 응답을 기다리는 함수는 아니다.
void NetworkClient::sendLine(const QByteArray &line)
{
    if (isConnected()) {
        m_socket.write(line);
        m_socket.write("\n", 1);
    }
}

// TCP는 메시지 단위가 아닌 바이트 스트림이다. 한 메시지가 나뉘어 들어오거나 여러 메시지가
// 한 번에 올 수 있으므로 누적 버퍼에서 줄바꿈이 있는 완성된 줄만 꺼내 파싱한다.
void NetworkClient::readAvailable()
{
    m_buffer += m_socket.readAll();
    qsizetype end = m_buffer.indexOf('\n');
    while (end >= 0) {
        // 처리한 줄은 제거하고, 줄바꿈이 아직 없는 마지막 조각은 다음 수신까지 남긴다.
        const QByteArray line = m_buffer.left(end).trimmed();
        m_buffer.remove(0, end + 1);
        if (!line.isEmpty())
            parseMessage(QString::fromUtf8(line));
        end = m_buffer.indexOf('\n');
    }
    if (m_buffer.size() > 8192) {
        // 줄바꿈 없이 계속 들어오는 잘못된 데이터가 메모리를 무제한 차지하지 않게 한다.
        m_buffer.clear();
        emit protocolWarning(QStringLiteral("Incoming protocol line exceeded 8 KiB"));
    }
}

// 완성된 한 줄을 상태 신호 또는 TrackingInfo로 변환한다.
// UI 위젯을 직접 접근하지 않으므로 통신 파싱과 화면 표시를 따로 유지할 수 있다.
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
        // 필드 순서: 이름, 신뢰도, 객체 X/Y, 오차 X/Y, Pan/Tilt 각도.
        // 현재 프로토콜에는 바운딩 박스와 프레임 크기가 포함되지 않는다.
        const QStringList fields = message.mid(7).split(QLatin1Char(','));
        if (fields.size() != 8) {
            emit protocolWarning(QStringLiteral("Malformed DETECT message"));
            return;
        }
        TrackingInfo info;
        info.label = fields.at(0).trimmed();
        bool ok = !info.label.isEmpty();
        bool valid = false;
        // 각 숫자의 변환 성공 여부와 신뢰도 범위를 검사하고 모든 검사를 누적한다.
        // 변환 실패를 0으로 받아들여 정상 탐지처럼 표시하는 일을 막는다.
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
    // 알 수 없는 메시지는 경고로 전달한다. 긴 문자열은 로그에 앞부분만 표시한다.
    emit protocolWarning(QStringLiteral("Unknown message: %1").arg(message.left(100)));
}
