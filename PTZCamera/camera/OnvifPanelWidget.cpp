// ONVIF 장치 검색, 서비스 주소/계정 입력과 조회한 RTSP 주소 표시를 담당한다.
// 네트워크 처리 자체는 OnvifClient에 맡기고 결과를 카메라 재생 위젯에 전달한다.
#include "OnvifPanelWidget.h"

#include "network/onvif/OnvifClient.h"

#include <QComboBox>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalBlocker>

// 검색 결과 목록과 직접 입력 경로를 함께 제공한다.
// 이 위젯을 언제 접거나 표시할지는 상위 CameraPlaybackWidget에서 결정한다.
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

    // 멀티캐스트 검색이 실패해도 알려진 Pi 서비스 주소로 SOAP 조회를 시작할 수 있다.
    m_serviceUrl = new QLineEdit(QStringLiteral("http://192.168.0.92:8080/onvif/device_service"), this);
    layout->addWidget(new QLabel(QStringLiteral("Service URL"), this), 1, 0);
    layout->addWidget(m_serviceUrl, 1, 1, 1, 4);

    m_username = new QLineEdit(this);
    m_username->setPlaceholderText(QStringLiteral("optional"));
    m_password = new QLineEdit(this);
    // 비밀번호는 입력 화면에서 가리고 파일에 저장하지 않는다.
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
    // 조회 결과를 복사할 수 있는 읽기 전용 칸이다. 영상 재생의 성공 여부를 뜻하지 않는다.
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
        // 이전 검색 목록을 비워 새 검색에서 받은 결과만 표시한다.
        m_devices->clear();
        m_devices->addItem(QStringLiteral("Searching..."));
    });
    connect(m_client, &OnvifClient::deviceFound, this, [this](const QUrl &url) {
        // 항목을 추가하면서 발생하는 선택 변경 신호가 사용자의 선택을 덮어쓰지 않게 한다.
        QSignalBlocker blocker(m_devices);
        if (m_devices->count() == 1 && m_devices->itemData(0).toString().isEmpty())
            m_devices->clear();
        const bool first = m_devices->count() == 0;
        m_devices->addItem(QStringLiteral("%1  —  %2").arg(url.host(), url.toString()), url.toString());
        if (first)
            // 첫 결과는 자동 선택하고, 이후 발견된 장치는 목록에만 추가한다.
            m_serviceUrl->setText(url.toString());
    });
    connect(m_client, &OnvifClient::discoveryFinished, this, [this] {
        // 결과가 없더라도 기본/수동 서비스 URL은 유지해 직접 조회할 수 있게 한다.
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
                // 화면 주소에서는 사용자 정보를 숨긴다. 실제 재생에 전달하는 주소는 별도로 만든다.
                m_streamUrl->setText(uri.toDisplayString(QUrl::RemoveUserInfo));
                QUrl playbackUri = uri;
                // 반환 주소에 계정이 없으면 입력한 ONVIF 계정을 RTSP에도 사용한다.
                // ONVIF와 RTSP 계정이 같은 장치를 전제로 하므로 서로 다르면 별도 처리가 필요하다.
                if (playbackUri.userName().isEmpty() && !m_username->text().isEmpty()) {
                    playbackUri.setUserName(m_username->text());
                    playbackUri.setPassword(m_password->text());
                }
                emit streamUriReady(playbackUri);
                emit message(QStringLiteral("ONVIF RTSP URL received (profile: %1)")
                                 .arg(profileName.isEmpty() ? QStringLiteral("default") : profileName));
            });
    // 조회 버튼은 상위 위젯에 요청한다. 상위 위젯이 현재 TCP/UDP 모드를 선택해 호출한다.
    connect(fetch, &QPushButton::clicked, this, &OnvifPanelWidget::fetchRequested);
}

// 이전 표시를 지우고 비동기 주소 조회를 시작한다.
// 서비스 URL, 계정과 원하는 전송 방식을 전달하며 완료/오류는 클라이언트 신호로 받는다.
void OnvifPanelWidget::fetchStreamUri(bool useUdp)
{
    m_streamUrl->clear();
    m_client->fetchStreamUri(QUrl::fromUserInput(m_serviceUrl->text().trimmed()),
                             m_username->text(), m_password->text(), useUdp);
}
