#include "OnvifPanelWidget.h"

#include "network/onvif/OnvifClient.h"

#include <QComboBox>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalBlocker>

OnvifPanelWidget::OnvifPanelWidget(QWidget *parent) : QWidget(parent)
{
    m_client = new OnvifClient(this);
    auto *layout = new QGridLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setHorizontalSpacing(8);
    layout->setVerticalSpacing(6);

    m_devices = new QComboBox(this);
    m_devices->addItem(QStringLiteral("No devices found"));
    auto *search = new QPushButton(QStringLiteral("Search LAN"), this);
    layout->addWidget(new QLabel(QStringLiteral("ONVIF devices"), this), 0, 0);
    layout->addWidget(m_devices, 0, 1, 1, 3);
    layout->addWidget(search, 0, 4);

    m_serviceUrl = new QLineEdit(QStringLiteral("http://192.168.0.92:8080/onvif/device_service"), this);
    layout->addWidget(new QLabel(QStringLiteral("Service URL"), this), 1, 0);
    layout->addWidget(m_serviceUrl, 1, 1, 1, 4);

    m_username = new QLineEdit(this);
    m_username->setPlaceholderText(QStringLiteral("optional"));
    m_password = new QLineEdit(this);
    m_password->setEchoMode(QLineEdit::Password);
    m_password->setPlaceholderText(QStringLiteral("optional"));
    auto *fetch = new QPushButton(QStringLiteral("Get RTSP URL"), this);
    fetch->setObjectName(QStringLiteral("primaryButton"));
    layout->addWidget(new QLabel(QStringLiteral("User"), this), 2, 0);
    layout->addWidget(m_username, 2, 1);
    layout->addWidget(new QLabel(QStringLiteral("Password"), this), 2, 2);
    layout->addWidget(m_password, 2, 3);
    layout->addWidget(fetch, 2, 4);

    m_streamUrl = new QLineEdit(this);
    m_streamUrl->setReadOnly(true);
    m_streamUrl->setPlaceholderText(QStringLiteral("RTSP URL will appear here"));
    layout->addWidget(new QLabel(QStringLiteral("RTSP URL"), this), 3, 0);
    layout->addWidget(m_streamUrl, 3, 1, 1, 4);
    m_status = new QLabel(QStringLiteral("Search LAN or enter a service URL, then get the RTSP URL"), this);
    m_status->setWordWrap(true);
    layout->addWidget(m_status, 4, 0, 1, 5);
    layout->setColumnStretch(1, 1);
    layout->setColumnStretch(3, 1);

    connect(search, &QPushButton::clicked, m_client, &OnvifClient::discover);
    connect(m_devices, &QComboBox::currentIndexChanged, this, [this](int index) {
        const QString url = m_devices->itemData(index).toString();
        if (!url.isEmpty())
            m_serviceUrl->setText(url);
    });
    connect(m_client, &OnvifClient::discoveryStarted, this, [this] {
        m_devices->clear();
        m_devices->addItem(QStringLiteral("Searching..."));
    });
    connect(m_client, &OnvifClient::deviceFound, this, [this](const QUrl &url) {
        QSignalBlocker blocker(m_devices);
        if (m_devices->count() == 1 && m_devices->itemData(0).toString().isEmpty())
            m_devices->clear();
        const bool first = m_devices->count() == 0;
        m_devices->addItem(QStringLiteral("%1  —  %2").arg(url.host(), url.toString()), url.toString());
        if (first)
            m_serviceUrl->setText(url.toString());
    });
    connect(m_client, &OnvifClient::discoveryFinished, this, [this] {
        if (m_devices->count() == 1 && m_devices->itemData(0).toString().isEmpty()) {
            m_devices->setItemText(0, QStringLiteral("No devices found"));
            m_status->setText(QStringLiteral("No discovery reply; enter the service URL manually"));
        } else {
            m_status->setText(QStringLiteral("Select a device, then get its RTSP URL"));
        }
    });
    connect(m_client, &OnvifClient::statusChanged, m_status, &QLabel::setText);
    connect(m_client, &OnvifClient::errorOccurred, this, [this](const QString &error) {
        m_status->setText(error);
        emit message(QStringLiteral("ONVIF: %1").arg(error));
    });
    connect(m_client, &OnvifClient::streamUriReady, this,
            [this](const QUrl &uri, const QString &profileName) {
                m_streamUrl->setText(uri.toDisplayString(QUrl::RemoveUserInfo));
                QUrl playbackUri = uri;
                if (playbackUri.userName().isEmpty() && !m_username->text().isEmpty()) {
                    playbackUri.setUserName(m_username->text());
                    playbackUri.setPassword(m_password->text());
                }
                emit streamUriReady(playbackUri);
                emit message(QStringLiteral("ONVIF RTSP URL received (profile: %1)")
                                 .arg(profileName.isEmpty() ? QStringLiteral("default") : profileName));
            });
    connect(fetch, &QPushButton::clicked, this, &OnvifPanelWidget::fetchRequested);
}

void OnvifPanelWidget::fetchStreamUri(bool useUdp)
{
    m_streamUrl->clear();
    m_client->fetchStreamUri(QUrl::fromUserInput(m_serviceUrl->text().trimmed()),
                             m_username->text(), m_password->text(), useUdp);
}
