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
const QString deviceNamespace = QStringLiteral("http://www.onvif.org/ver10/device/wsdl");
const QString media1Namespace = QStringLiteral("http://www.onvif.org/ver10/media/wsdl");
const QString media2Namespace = QStringLiteral("http://www.onvif.org/ver20/media/wsdl");

bool isHttpUrl(const QUrl &url)
{
    return url.isValid() && !url.host().isEmpty()
           && (url.scheme() == QLatin1String("http") || url.scheme() == QLatin1String("https"));
}

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

QUrl normalizedUrl(const QString &text, const QUrl &deviceService)
{
    QUrl url(text.trimmed());
    if (url.host() == QLatin1String("0.0.0.0") || url.host() == QLatin1String("localhost")
        || url.host() == QLatin1String("127.0.0.1"))
        url.setHost(deviceService.host());
    return url;
}
}

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
                if (reply == m_reply && !m_username.isEmpty()) {
                    authenticator->setUser(m_username);
                    authenticator->setPassword(m_password);
                }
            });
}

void OnvifClient::discover()
{
    m_discoveryTimer.stop();
    m_discoverySocket.close();
    m_seenAddresses.clear();
    emit discoveryStarted();
    if (!m_discoverySocket.bind(QHostAddress::AnyIPv4, 0)) {
        emit errorOccurred(QStringLiteral("ONVIF discovery socket: %1").arg(m_discoverySocket.errorString()));
        emit discoveryFinished();
        return;
    }

    m_probeId = QStringLiteral("urn:uuid:%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
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
    if (m_discoverySocket.writeDatagram(probe, QHostAddress(QStringLiteral("239.255.255.250")), 3702) < 0) {
        emit errorOccurred(QStringLiteral("ONVIF discovery send: %1").arg(m_discoverySocket.errorString()));
        m_discoverySocket.close();
        emit discoveryFinished();
        return;
    }
    emit statusChanged(QStringLiteral("Searching for ONVIF devices (3 seconds)..."));
    m_discoveryTimer.start(3000);
}

void OnvifClient::readDiscoveryReplies()
{
    while (m_discoverySocket.hasPendingDatagrams()) {
        QByteArray data;
        data.resize(static_cast<int>(m_discoverySocket.pendingDatagramSize()));
        QHostAddress sender;
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
            m_seenAddresses.insert(url.toString());
            emit deviceFound(url);
        }
    }
}

void OnvifClient::fetchStreamUri(const QUrl &deviceService, const QString &username,
                                 const QString &password, bool useUdp)
{
    if (!isHttpUrl(deviceService)) {
        fail(QStringLiteral("Enter a valid ONVIF device service URL (http:// or https://)"));
        return;
    }
    if (m_reply) {
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

void OnvifClient::sendSoap(const QUrl &url, const QString &action, const QString &body, Step step)
{
    if (!isHttpUrl(url)) {
        fail(QStringLiteral("ONVIF service returned an invalid HTTP address"));
        return;
    }
    QString securityHeader;
    if (!m_username.isEmpty()) {
        QByteArray nonce(16, '\0');
        for (char &byte : nonce)
            byte = static_cast<char>(QRandomGenerator::system()->generate() & 0xff);
        const QString created = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
        const QByteArray digest = QCryptographicHash::hash(nonce + created.toUtf8() + m_password.toUtf8(),
                                                            QCryptographicHash::Sha1).toBase64();
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
    request.setTransferTimeout(7000);
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/soap+xml; charset=utf-8; action=\"%1\"").arg(action));
    request.setRawHeader("SOAPAction", QStringLiteral("\"%1\"").arg(action).toUtf8());
    m_step = step;
    QNetworkReply *reply = m_http.post(request, envelope);
    m_reply = reply;
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        if (reply == m_reply) {
            m_reply = nullptr;
            handleSoapReply(reply);
        }
        reply->deleteLater();
    });
}

void OnvifClient::handleSoapReply(QNetworkReply *reply)
{
    const QByteArray data = reply->readAll();
    const QString fault = soapFault(data);
    if (reply->error() != QNetworkReply::NoError || !fault.isEmpty()) {
        if (m_step == Step::GetServices && reply->error() != QNetworkReply::AuthenticationRequiredError) {
            requestCapabilities();
            return;
        }
        fail(!fault.isEmpty() ? fault : QStringLiteral("ONVIF request failed: %1").arg(reply->errorString()));
        return;
    }

    if (m_step == Step::GetServices) {
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

void OnvifClient::requestCapabilities()
{
    emit statusChanged(QStringLiteral("Reading ONVIF media capabilities..."));
    sendSoap(m_deviceService, deviceNamespace + QStringLiteral("/GetCapabilities"),
             QStringLiteral("<tds:GetCapabilities xmlns:tds=\"%1\">"
                            "<tds:Category>Media</tds:Category></tds:GetCapabilities>")
                 .arg(deviceNamespace), Step::GetCapabilities);
}

void OnvifClient::requestProfiles()
{
    emit statusChanged(QStringLiteral("Reading ONVIF media profiles..."));
    const QString ns = m_media2 ? media2Namespace : media1Namespace;
    const QString prefix = m_media2 ? QStringLiteral("tr2") : QStringLiteral("trt");
    sendSoap(m_mediaService, ns + QStringLiteral("/GetProfiles"),
             QStringLiteral("<%1:GetProfiles xmlns:%1=\"%2\"/>").arg(prefix, ns), Step::GetProfiles);
}

void OnvifClient::requestStreamUri(const QString &profileToken)
{
    const QString token = profileToken.toHtmlEscaped();
    if (m_media2) {
        const QString body = QStringLiteral(
            "<tr2:GetStreamUri xmlns:tr2=\"%1\">"
            "<tr2:Protocol>%2</tr2:Protocol><tr2:ProfileToken>%3</tr2:ProfileToken>"
            "</tr2:GetStreamUri>")
            .arg(media2Namespace, m_useUdp ? QStringLiteral("RtspUnicast") : QStringLiteral("RTSP"), token);
        sendSoap(m_mediaService, media2Namespace + QStringLiteral("/GetStreamUri"), body,
                 Step::GetStreamUri);
    } else {
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

void OnvifClient::fail(const QString &message)
{
    m_step = Step::None;
    emit errorOccurred(message);
}
