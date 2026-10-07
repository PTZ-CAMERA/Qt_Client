// ONVIF 장치 검색과 RTSP 영상 주소 조회를 담당하는 비동기 클라이언트이다.
// 검색은 WS-Discovery UDP, 서비스/프로파일/주소 조회는 HTTP SOAP로 수행한다.
// 현재 구현 범위는 Media1/Media2 주소 조회이며 ONVIF PTZ 이동 명령은 포함하지 않는다.
#include "OnvifClient.h"

#include <QAuthenticator>
#include <QByteArray>
#include <QCryptographicHash>
#include <QDateTime>
#include <QHostAddress>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QStringList>
#include <QUuid>
#include <QXmlStreamReader>

namespace {
// 서비스 종류를 구분하는 표준 네임스페이스다. 장치가 알려주는 실제 접속 주소(XAddr)와는 다르다.
const QString deviceNamespace = QStringLiteral("http://www.onvif.org/ver10/device/wsdl");
const QString media1Namespace = QStringLiteral("http://www.onvif.org/ver10/media/wsdl");
const QString media2Namespace = QStringLiteral("http://www.onvif.org/ver20/media/wsdl");

// SOAP 서비스를 호출할 수 있는 HTTP(S) 주소인지 검사한다.
// RTSP 영상 주소는 이 함수가 아니라 최종 GetStreamUri 처리 단계에서 따로 검사한다.
bool isHttpUrl(const QUrl &url)
{
    return url.isValid() && !url.host().isEmpty()
           && (url.scheme() == QLatin1String("http") || url.scheme() == QLatin1String("https"));
}

// HTTP 응답이 성공이어도 SOAP 본문에 Fault가 있으면 명령은 실패한 것이다.
// SOAP 1.2의 Reason/Text와 일부 서버의 faultstring을 읽어 사용자에게 보여줄 설명을 만든다.
QString soapFault(const QByteArray &data)
{
    QXmlStreamReader xml(data);
    bool inFault = false;
    while (!xml.atEnd()) {
        xml.readNext();
        if (xml.isStartElement() && xml.name() == QLatin1String("Fault"))
            inFault = true;
        else if (inFault && xml.isStartElement()
                 && (xml.name() == QLatin1String("Text") || xml.name() == QLatin1String("faultstring")))
            return xml.readElementText().left(200);
    }
    return inFault ? QStringLiteral("ONVIF SOAP fault") : QString();
}

// 장치가 접속 불가능한 로컬/와일드카드 호스트를 반환하는 경우 원래 장치 호스트로 보정한다.
// 포트와 경로는 서버가 반환한 값을 유지하며 다른 종류의 잘못된 주소까지 추정해서 고치지 않는다.
QUrl normalizedUrl(const QString &text, const QUrl &deviceService)
{
    QUrl url(text.trimmed());
    if (url.host() == QLatin1String("0.0.0.0") || url.host() == QLatin1String("localhost")
        || url.host() == QLatin1String("127.0.0.1"))
        url.setHost(deviceService.host());
    return url;
}
}

// 소켓 수신과 검색 종료 타이머, HTTP 인증 요청을 연결한다.
// 네트워크 객체는 멤버로 보유하므로 이 클라이언트와 함께 정리된다.
OnvifClient::OnvifClient(QObject *parent) : QObject(parent)
{
    m_discoveryTimer.setSingleShot(true);
    connect(&m_discoverySocket, &QUdpSocket::readyRead, this, &OnvifClient::readDiscoveryReplies);
    connect(&m_discoveryTimer, &QTimer::timeout, this, [this] {
        m_discoverySocket.close();
        emit discoveryFinished();
    });
    connect(&m_http, &QNetworkAccessManager::authenticationRequired, this,
            [this](QNetworkReply *reply, QAuthenticator *authenticator) {
                // HTTP 인증은 Qt가 서버의 인증 요청에 맞춰 처리한다.
                // 현재 조회의 응답에만 입력 계정을 제공하며 별도의 인증 팝업을 만들지 않는다.
                if (reply == m_reply && !m_username.isEmpty()) {
                    authenticator->setUser(m_username);
                    authenticator->setPassword(m_password);
                }
            });
}

// 기존 검색을 끝내고 새 Probe를 보낸 뒤 3초 동안 응답을 모은다.
// 한 번의 IPv4 멀티캐스트 송신을 사용하므로 다중 어댑터/VPN 환경에서는 선택되는
// 네트워크 경로에 영향을 받을 수 있다. 검색 실패 시 서비스 URL 직접 조회도 가능하다.
void OnvifClient::discover()
{
    m_discoveryTimer.stop();
    m_discoverySocket.close();
    m_seenAddresses.clear();
    emit discoveryStarted();
    // 포트 0으로 바인딩하면 OS가 수신 포트를 지정한다. 장치의 검색 응답은 이 포트로 받는다.
    if (!m_discoverySocket.bind(QHostAddress::AnyIPv4, 0)) {
        emit errorOccurred(QStringLiteral("ONVIF discovery socket: %1").arg(m_discoverySocket.errorString()));
        emit discoveryFinished();
        return;
    }

    // 검색마다 고유 MessageID를 만들어 이전 검색에 대한 늦은 응답을 구분한다.
    m_probeId = QStringLiteral("urn:uuid:%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    // WS-Discovery Probe의 타입을 영상 송신 장치로 지정한다.
    // 이 XML은 RTSP URL을 직접 요청하는 메시지가 아니라 장치 서비스 주소를 찾는 메시지다.
    const QByteArray probe = QStringLiteral(
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
        "<s:Envelope xmlns:s=\"http://www.w3.org/2003/05/soap-envelope\" "
        "xmlns:a=\"http://schemas.xmlsoap.org/ws/2004/08/addressing\" "
        "xmlns:d=\"http://schemas.xmlsoap.org/ws/2005/04/discovery\" "
        "xmlns:dn=\"http://www.onvif.org/ver10/network/wsdl\">"
        "<s:Header><a:Action>http://schemas.xmlsoap.org/ws/2005/04/discovery/Probe</a:Action>"
        "<a:MessageID>%1</a:MessageID>"
        "<a:To>urn:schemas-xmlsoap-org:ws:2005:04:discovery</a:To>"
        "<a:ReplyTo><a:Address>http://schemas.xmlsoap.org/ws/2004/08/addressing/role/anonymous</a:Address>"
        "</a:ReplyTo></s:Header><s:Body><d:Probe>"
        "<d:Types>dn:NetworkVideoTransmitter</d:Types>"
        "</d:Probe></s:Body></s:Envelope>")
        .arg(m_probeId).toUtf8();
    // WS-Discovery가 사용하는 IPv4 멀티캐스트 주소/포트로 보낸다.
    // 송신 성공은 요청을 보냈다는 뜻이며, 검색된 장치가 있다는 뜻은 아니다.
    if (m_discoverySocket.writeDatagram(probe, QHostAddress(QStringLiteral("239.255.255.250")), 3702) < 0) {
        emit errorOccurred(QStringLiteral("ONVIF discovery send: %1").arg(m_discoverySocket.errorString()));
        m_discoverySocket.close();
        emit discoveryFinished();
        return;
    }
    emit statusChanged(QStringLiteral("Searching for ONVIF devices (3 seconds)..."));
    m_discoveryTimer.start(3000);
}

// 도착한 UDP 데이터그램을 각각 독립된 XML 문서로 읽는다.
// RelatesTo로 현재 Probe에 대한 응답인지 확인하고 XAddrs의 장치 서비스 주소를 추출한다.
void OnvifClient::readDiscoveryReplies()
{
    while (m_discoverySocket.hasPendingDatagrams()) {
        QByteArray data;
        data.resize(static_cast<int>(m_discoverySocket.pendingDatagramSize()));
        QHostAddress sender;
        // 송신자의 IP는 잘못된 로컬 호스트 주소를 보정할 때 사용한다.
        m_discoverySocket.readDatagram(data.data(), data.size(), &sender);
        QXmlStreamReader xml(data);
        QString relatesTo;
        QStringList addresses;
        while (!xml.atEnd()) {
            xml.readNext();
            if (!xml.isStartElement())
                continue;
            if (xml.name() == QLatin1String("RelatesTo"))
                relatesTo = xml.readElementText().trimmed();
            else if (xml.name() == QLatin1String("XAddrs"))
                // XAddrs에는 하나 이상의 URL이 공백/줄바꿈으로 나열될 수 있다.
                addresses += xml.readElementText().trimmed().split(QRegularExpression(QStringLiteral("\\s+")),
                                                                  Qt::SkipEmptyParts);
        }
        if (xml.hasError() || relatesTo != m_probeId)
            continue;
        for (const QString &address : addresses) {
            QUrl url(address.trimmed());
            if (url.host() == QLatin1String("0.0.0.0") || url.host() == QLatin1String("localhost")
                || url.host() == QLatin1String("127.0.0.1"))
                url.setHost(sender.toString());
            if (!isHttpUrl(url) || m_seenAddresses.contains(url.toString()))
                continue;
            // 같은 장치가 반복 응답하거나 같은 주소를 여러 번 보내도 목록 항목은 한 번만 만든다.
            m_seenAddresses.insert(url.toString());
            emit deviceFound(url);
        }
    }
}

// 선택한 장치에 대한 주소 조회를 새로 시작한다.
// 조회 흐름: GetServices(필요 시 GetCapabilities) → GetProfiles → GetStreamUri.
// 계정은 메모리에 보관해 후속 SOAP 요청과 HTTP 인증에 사용하며 파일에 저장하지 않는다.
void OnvifClient::fetchStreamUri(const QUrl &deviceService, const QString &username,
                                 const QString &password, bool useUdp)
{
    if (!isHttpUrl(deviceService)) {
        fail(QStringLiteral("Enter a valid ONVIF device service URL (http:// or https://)"));
        return;
    }
    if (m_reply) {
        // abort()가 finished 신호를 발생시켜도 이전 응답을 새 조회 결과로 처리하지 않도록
        // 현재 응답 포인터부터 분리한다. 응답 객체는 연결된 finished 처리에서 정리된다.
        QNetworkReply *oldReply = m_reply;
        m_reply = nullptr;
        oldReply->abort();
    }
    m_deviceService = deviceService;
    m_mediaService = QUrl();
    m_username = username;
    m_password = password;
    m_useUdp = useUdp;
    m_media2 = false;
    m_profileName.clear();
    emit statusChanged(QStringLiteral("Reading ONVIF services..."));
    sendSoap(m_deviceService, deviceNamespace + QStringLiteral("/GetServices"),
             QStringLiteral("<tds:GetServices xmlns:tds=\"%1\"><tds:IncludeCapability>false"
                            "</tds:IncludeCapability></tds:GetServices>").arg(deviceNamespace),
             Step::GetServices);
}

// 서비스별 요청 본문을 SOAP 1.2 Envelope로 감싸 HTTP POST한다.
// step은 같은 완료 콜백에서 현재 응답을 어떤 종류로 해석할지 구분하는 값이다.
void OnvifClient::sendSoap(const QUrl &url, const QString &action, const QString &body, Step step)
{
    if (!isHttpUrl(url)) {
        fail(QStringLiteral("ONVIF service returned an invalid HTTP address"));
        return;
    }
    QString securityHeader;
    if (!m_username.isEmpty()) {
        // WS-Security UsernameToken의 PasswordDigest 형식:
        // Base64(SHA-1(원본 Nonce 바이트 + Created의 UTF-8 + Password의 UTF-8)).
        // 이는 HTTPS 암호화와 별개이며, Created 검증을 하는 장치에는 PC/장치 시간 동기화가 필요하다.
        QByteArray nonce(16, '\0');
        for (char &byte : nonce)
            byte = static_cast<char>(QRandomGenerator::system()->generate() & 0xff);
        const QString created = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
        const QByteArray digest = QCryptographicHash::hash(nonce + created.toUtf8() + m_password.toUtf8(),
                                                            QCryptographicHash::Sha1).toBase64();
        // XML에 넣는 사용자명은 특수 문자를 이스케이프하고 Nonce/Digest는 Base64로 표현한다.
        securityHeader = QStringLiteral(
            "<wsse:Security s:mustUnderstand=\"1\" "
            "xmlns:wsse=\"http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-wssecurity-secext-1.0.xsd\" "
            "xmlns:wsu=\"http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-wssecurity-utility-1.0.xsd\">"
            "<wsse:UsernameToken><wsse:Username>%1</wsse:Username>"
            "<wsse:Password Type=\"http://docs.oasis-open.org/wss/2004/01/"
            "oasis-200401-wss-username-token-profile-1.0#PasswordDigest\">%2</wsse:Password>"
            "<wsse:Nonce EncodingType=\"http://docs.oasis-open.org/wss/2004/01/"
            "oasis-200401-wss-soap-message-security-1.0#Base64Binary\">%3</wsse:Nonce>"
            "<wsu:Created>%4</wsu:Created></wsse:UsernameToken></wsse:Security>")
            .arg(m_username.toHtmlEscaped(), QString::fromLatin1(digest),
                 QString::fromLatin1(nonce.toBase64()), created);
    }
    const QByteArray envelope = QStringLiteral(
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
        "<s:Envelope xmlns:s=\"http://www.w3.org/2003/05/soap-envelope\">"
        "<s:Header>%1</s:Header><s:Body>%2</s:Body></s:Envelope>")
        .arg(securityHeader, body).toUtf8();
    QNetworkRequest request(url);
    // 전송 중 데이터 교환이 일정 시간 진행되지 않으면 요청을 종료하도록 한다.
    // 영상 지터 버퍼나 RTSP 재생 지연을 지정하는 설정이 아니다.
    request.setTransferTimeout(7000);
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/soap+xml; charset=utf-8; action=\"%1\"").arg(action));
    request.setRawHeader("SOAPAction", QStringLiteral("\"%1\"").arg(action).toUtf8());
    m_step = step;
    QNetworkReply *reply = m_http.post(request, envelope);
    m_reply = reply;
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        // 취소된 이전 조회의 완료 신호는 파싱하지 않는다. 모든 응답 객체는 나중에 삭제한다.
        if (reply == m_reply) {
            m_reply = nullptr;
            handleSoapReply(reply);
        }
        reply->deleteLater();
    });
}

// 네트워크 응답과 SOAP Fault를 검사하고 현재 단계의 XML 결과를 처리한다.
// QXmlStreamReader의 지역 이름을 비교해 서버별 XML 접두사 차이에 의존하지 않게 한다.
void OnvifClient::handleSoapReply(QNetworkReply *reply)
{
    const QByteArray data = reply->readAll();
    const QString fault = soapFault(data);
    if (reply->error() != QNetworkReply::NoError || !fault.isEmpty()) {
        // 일부 장치가 GetServices를 제공하지 않으면 기존 GetCapabilities 조회로 재시도한다.
        // HTTP 인증 실패에서는 이 대체 조회를 하지 않고 오류를 보고한다.
        if (m_step == Step::GetServices && reply->error() != QNetworkReply::AuthenticationRequiredError) {
            requestCapabilities();
            return;
        }
        fail(!fault.isEmpty() ? fault : QStringLiteral("ONVIF request failed: %1").arg(reply->errorString()));
        return;
    }

    if (m_step == Step::GetServices) {
        // Service별 Namespace와 XAddr를 묶어 읽어 Media1/Media2의 실제 주소를 찾는다.
        // 서비스 목록 전체를 읽고 Media1이 있으면 우선 사용하며, 없으면 Media2를 선택한다.
        QUrl media1;
        QUrl media2;
        QXmlStreamReader xml(data);
        QString serviceNamespace;
        QString xaddr;
        bool inService = false;
        while (!xml.atEnd()) {
            xml.readNext();
            if (xml.isStartElement() && xml.name() == QLatin1String("Service")) {
                inService = true;
                serviceNamespace.clear();
                xaddr.clear();
            } else if (inService && xml.isStartElement()
                       && xml.name() == QLatin1String("Namespace")) {
                serviceNamespace = xml.readElementText();
            } else if (inService && xml.isStartElement()
                       && xml.name() == QLatin1String("XAddr")) {
                xaddr = xml.readElementText();
            } else if (inService && xml.isEndElement()
                       && xml.name() == QLatin1String("Service")) {
                if (serviceNamespace == media1Namespace)
                    media1 = normalizedUrl(xaddr, m_deviceService);
                else if (serviceNamespace == media2Namespace)
                    media2 = normalizedUrl(xaddr, m_deviceService);
                inService = false;
            }
        }
        if (xml.hasError()) {
            requestCapabilities();
            return;
        }
        m_media2 = !isHttpUrl(media1) && isHttpUrl(media2);
        m_mediaService = m_media2 ? media2 : media1;
        if (!isHttpUrl(m_mediaService)) {
            requestCapabilities();
            return;
        }
        requestProfiles();
        return;
    }

    if (m_step == Step::GetCapabilities) {
        // 대체 경로는 기존 Media1의 Media/XAddr를 읽는다. Media2 주소는 GetServices로 찾는다.
        QXmlStreamReader xml(data);
        while (!xml.atEnd()) {
            xml.readNext();
            if (xml.isStartElement() && xml.name() == QLatin1String("Media")) {
                while (xml.readNextStartElement()) {
                    if (xml.name() == QLatin1String("XAddr")) {
                        m_mediaService = normalizedUrl(xml.readElementText(), m_deviceService);
                        break;
                    }
                    xml.skipCurrentElement();
                }
                break;
            }
        }
        if (xml.hasError() || !isHttpUrl(m_mediaService)) {
            fail(QStringLiteral("ONVIF media service was not found"));
            return;
        }
        requestProfiles();
        return;
    }

    if (m_step == Step::GetProfiles) {
        // 첫 프로파일의 token은 뒤의 GetStreamUri 요청에 필수다.
        // Name은 로그 표시용이며 현재 여러 프로파일 중 선택하는 UI는 구현하지 않았다.
        QXmlStreamReader xml(data);
        QString token;
        while (!xml.atEnd()) {
            xml.readNext();
            if (xml.isStartElement() && xml.name() == QLatin1String("Profiles")) {
                token = xml.attributes().value(QLatin1String("token")).toString();
                while (xml.readNextStartElement()) {
                    if (xml.name() == QLatin1String("Name"))
                        m_profileName = xml.readElementText();
                    else
                        xml.skipCurrentElement();
                }
                break;
            }
        }
        if (xml.hasError() || token.isEmpty()) {
            fail(QStringLiteral("ONVIF media profile was not found"));
            return;
        }
        emit statusChanged(QStringLiteral("Reading ONVIF RTSP URL..."));
        requestStreamUri(token);
        return;
    }

    if (m_step == Step::GetStreamUri) {
        // 최종 반환 주소의 형식을 검사한다. 주소 조회 성공만으로 영상 프레임이 도착한 것은
        // 아니므로 재생 여부는 별도 플레이어가 실제 프레임 수신으로 판단한다.
        QXmlStreamReader xml(data);
        QUrl uri;
        while (!xml.atEnd()) {
            xml.readNext();
            if (xml.isStartElement() && xml.name() == QLatin1String("Uri")) {
                uri = normalizedUrl(xml.readElementText(), m_deviceService);
                break;
            }
        }
        if (xml.hasError() || !uri.isValid() || uri.host().isEmpty()
            || (uri.scheme() != QLatin1String("rtsp") && uri.scheme() != QLatin1String("rtsps"))) {
            fail(QStringLiteral("ONVIF did not return a valid RTSP URL"));
            return;
        }
        m_step = Step::None;
        emit statusChanged(QStringLiteral("ONVIF RTSP URL received"));
        emit streamUriReady(uri, m_profileName);
    }
}

// GetServices로 Media 주소를 얻지 못했을 때 장치 서비스에 기존 Media 능력 조회를 보낸다.
void OnvifClient::requestCapabilities()
{
    emit statusChanged(QStringLiteral("Reading ONVIF media capabilities..."));
    sendSoap(m_deviceService, deviceNamespace + QStringLiteral("/GetCapabilities"),
             QStringLiteral("<tds:GetCapabilities xmlns:tds=\"%1\">"
                            "<tds:Category>Media</tds:Category></tds:GetCapabilities>")
                 .arg(deviceNamespace), Step::GetCapabilities);
}

// 찾은 Media 서비스 버전에 맞는 GetProfiles 요청을 작성한다.
void OnvifClient::requestProfiles()
{
    emit statusChanged(QStringLiteral("Reading ONVIF media profiles..."));
    const QString ns = m_media2 ? media2Namespace : media1Namespace;
    const QString prefix = m_media2 ? QStringLiteral("tr2") : QStringLiteral("trt");
    sendSoap(m_mediaService, ns + QStringLiteral("/GetProfiles"),
             QStringLiteral("<%1:GetProfiles xmlns:%1=\"%2\"/>").arg(prefix, ns), Step::GetProfiles);
}

// 프로파일 토큰과 선택한 TCP/UDP 전송 조건으로 영상 URI를 요청한다.
// 토큰을 XML에 삽입하기 전에 이스케이프한다. Media1과 Media2는 요청 구조/프로토콜 이름이 다르다.
void OnvifClient::requestStreamUri(const QString &profileToken)
{
    const QString token = profileToken.toHtmlEscaped();
    if (m_media2) {
        // Media2: TCP 영상은 RTSP, UDP 유니캐스트 영상은 RtspUnicast로 요청한다.
        const QString body = QStringLiteral(
            "<tr2:GetStreamUri xmlns:tr2=\"%1\">"
            "<tr2:Protocol>%2</tr2:Protocol><tr2:ProfileToken>%3</tr2:ProfileToken>"
            "</tr2:GetStreamUri>")
            .arg(media2Namespace, m_useUdp ? QStringLiteral("RtspUnicast") : QStringLiteral("RTSP"), token);
        sendSoap(m_mediaService, media2Namespace + QStringLiteral("/GetStreamUri"), body,
                 Step::GetStreamUri);
    } else {
        // Media1: StreamSetup의 RTP-Unicast와 Transport/Protocol 조합을 사용한다.
        // TCP 인터리브 영상은 Protocol=RTSP, UDP 영상은 Protocol=UDP이다.
        const QString body = QStringLiteral(
            "<trt:GetStreamUri xmlns:trt=\"%1\" xmlns:tt=\"http://www.onvif.org/ver10/schema\">"
            "<trt:StreamSetup><tt:Stream>RTP-Unicast</tt:Stream><tt:Transport>"
            "<tt:Protocol>%2</tt:Protocol></tt:Transport></trt:StreamSetup>"
            "<trt:ProfileToken>%3</trt:ProfileToken></trt:GetStreamUri>")
            .arg(media1Namespace, m_useUdp ? QStringLiteral("UDP") : QStringLiteral("RTSP"), token);
        sendSoap(m_mediaService, media1Namespace + QStringLiteral("/GetStreamUri"), body,
                 Step::GetStreamUri);
    }
}

// 조회 단계 진행을 종료하고 UI가 표시/기록할 오류 신호를 보낸다.
// 위젯을 직접 조작하지 않아 같은 클라이언트를 다른 UI에도 연결할 수 있다.
void OnvifClient::fail(const QString &message)
{
    m_step = Step::None;
    emit errorOccurred(message);
}
