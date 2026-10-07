#pragma once

#include <QNetworkAccessManager>
#include <QObject>
#include <QPointer>
#include <QSet>
#include <QString>
#include <QTimer>
#include <QUdpSocket>
#include <QUrl>

class QAuthenticator;
class QNetworkReply;

class OnvifClient : public QObject
{
    Q_OBJECT
public:
    explicit OnvifClient(QObject *parent = nullptr);
    void discover();
    void fetchStreamUri(const QUrl &deviceService, const QString &username,
                        const QString &password, bool useUdp);

signals:
    void discoveryStarted();
    void deviceFound(const QUrl &serviceUrl);
    void discoveryFinished();
    void streamUriReady(const QUrl &uri, const QString &profileName);
    void statusChanged(const QString &message);
    void errorOccurred(const QString &message);

private:
    enum class Step { None, GetServices, GetCapabilities, GetProfiles, GetStreamUri };

    void readDiscoveryReplies();
    void sendSoap(const QUrl &url, const QString &action, const QString &body, Step step);
    void handleSoapReply(QNetworkReply *reply);
    void requestCapabilities();
    void requestProfiles();
    void requestStreamUri(const QString &profileToken);
    void fail(const QString &message);

    QUdpSocket m_discoverySocket;
    QTimer m_discoveryTimer;
    QSet<QString> m_seenAddresses;
    QString m_probeId;
    QNetworkAccessManager m_http;
    QPointer<QNetworkReply> m_reply;
    Step m_step = Step::None;
    QUrl m_deviceService;
    QUrl m_mediaService;
    QString m_username;
    QString m_password;
    QString m_profileName;
    bool m_media2 = false;
    bool m_useUdp = false;
};
