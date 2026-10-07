// 기존 연결 입력 UI를 재사용한다. 용도에 따라 VMS 서버 또는 legacy Pi 접속값을 표시한다.
// 이 위젯은 신호만 내보내며 소켓 생성이나 연결 성공 판정은 하지 않는다.
#include "ConnectionStatusWidget.h"

#include <QFormLayout>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

// 장치 LED, IP/포트 입력, 연결 버튼과 데모 상태 버튼을 구성한다.
ConnectionStatusWidget::ConnectionStatusWidget(QWidget *parent, Purpose purpose)
    : QWidget(parent), m_vmsServer(purpose == Purpose::VmsServer)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);

    auto *statuses = new QGridLayout;
    statuses->setHorizontalSpacing(8);
    statuses->setVerticalSpacing(6);
    auto addStatus = [&](int row, const QString &name, QLabel *&indicator, QLabel *&stateText) {
        // 상태마다 LED와 설명 문구를 함께 사용해 색상 없이도 상태를 구분할 수 있게 한다.
        indicator = new QLabel(QStringLiteral("●"), this);
        indicator->setFixedWidth(18);
        stateText = new QLabel(this);
        statuses->addWidget(indicator, row, 0);
        statuses->addWidget(new QLabel(name, this), row, 1);
        statuses->addWidget(stateText, row, 2, Qt::AlignRight);
    };
    const bool vms = purpose == Purpose::VmsServer;
    addStatus(0, vms ? QStringLiteral("VMS Server") : QStringLiteral("Raspberry Pi"), m_piIndicator, m_piText);
    if (!vms) addStatus(1, QStringLiteral("Camera"), m_cameraIndicator, m_cameraText);
    statuses->setColumnStretch(1, 1);
    layout->addLayout(statuses);
    setRaspberryPiStatus(Status::Disconnected);
    setCameraStatus(Status::Disconnected);

    // VMS 화면에서는 서버 API 포트, legacy에서는 Pi 텍스트 제어 포트를 입력한다.
    auto *form = new QFormLayout;
    form->setLabelAlignment(Qt::AlignLeft);
    m_ip = new QLineEdit(vms ? QStringLiteral("127.0.0.1") : QStringLiteral("192.168.0.92"), this);
    m_ip->setPlaceholderText(vms ? QStringLiteral("VMS host or IP") : QStringLiteral("Raspberry Pi IP or hostname"));
    m_port = new QSpinBox(this);
    m_port->setRange(1, 65535);
    m_port->setValue(5000);
    form->addRow(vms ? QStringLiteral("Host") : QStringLiteral("IP"), m_ip);
    form->addRow(QStringLiteral("Port"), m_port);
    layout->addLayout(form);

    auto *buttons = new QHBoxLayout;
    auto *connectButton = new QPushButton(QStringLiteral("Connect"), this);
    connectButton->setObjectName(QStringLiteral("primaryButton"));
    auto *disconnectButton = new QPushButton(QStringLiteral("Disconnect"), this);
    m_connectButton = connectButton;
    m_disconnectButton = disconnectButton;
    buttons->addWidget(connectButton);
    buttons->addWidget(disconnectButton);
    layout->addLayout(buttons);
    setRaspberryPiStatus(Status::Disconnected);

    // 장치 없이 LED를 시험하는 버튼이다. 실제 연결 성공 여부를 바꾸지는 않는다.
    auto *demoButton = new QPushButton(QStringLiteral("Toggle demo status"), this);
    demoButton->setObjectName(QStringLiteral("secondaryButton"));
    demoButton->setToolTip(QStringLiteral("Simulate device LEDs without a network connection"));
    layout->addWidget(demoButton);
    if (vms) demoButton->hide();

    connect(connectButton, &QPushButton::clicked, this, [this] {
        // 소켓을 직접 열지 않고 입력값을 MainWindow로 전달한다.
        emit connectRequested(m_ip->text().trimmed(), static_cast<quint16>(m_port->value()));
    });
    connect(disconnectButton, &QPushButton::clicked, this, &ConnectionStatusWidget::disconnectRequested);
    connect(demoButton, &QPushButton::clicked, this, &ConnectionStatusWidget::demoStatusRequested);
}

// 공통 상태 표시 함수다. 개별 장치의 LED와 문구를 같은 규칙으로 갱신한다.
void ConnectionStatusWidget::setStatus(QLabel *indicator, QLabel *text, Status status)
{
    QString color;
    QString caption;
    switch (status) {
    case Status::Connected: color = QStringLiteral("#52d7a5"); caption = QStringLiteral("CONNECTED"); break;
    case Status::Error: color = QStringLiteral("#ef7272"); caption = QStringLiteral("ERROR"); break;
    case Status::Disconnected: color = QStringLiteral("#778491"); caption = QStringLiteral("DISCONNECTED"); break;
    }
    indicator->setStyleSheet(QStringLiteral("color: %1; font-size: 16px;").arg(color));
    text->setText(caption);
    text->setStyleSheet(QStringLiteral("color: %1; font-size: 10px; font-weight: 700;").arg(color));
}

// 단순 연결 여부를 세 가지 상태 중 Connected/Disconnected로 변환하는 편의 함수다.
void ConnectionStatusWidget::setRaspberryPiConnected(bool connected)
{
    setRaspberryPiStatus(connected ? Status::Connected : Status::Disconnected);
}

// 카메라 프레임 재생 여부를 표시할 때 사용한다. 실제 상태의 판단은 상위 UI에서 한다.
void ConnectionStatusWidget::setCameraConnected(bool connected)
{
    setCameraStatus(connected ? Status::Connected : Status::Disconnected);
}

// TCP 연결 오류처럼 Error 상태까지 구분해야 할 때 사용한다.
void ConnectionStatusWidget::setRaspberryPiStatus(Status status)
{
    setStatus(m_piIndicator, m_piText, status);
    // 상위 UI가 전달한 VMS 접속 상태에 맞춰 버튼만 활성화한다.
    if (m_vmsServer && m_connectButton && m_disconnectButton) {
        m_connectButton->setEnabled(status != Status::Connected);
        m_disconnectButton->setEnabled(status == Status::Connected);
    }
}

// 영상 연결 실패를 포함해 카메라 상태를 지정한다.
void ConnectionStatusWidget::setCameraStatus(Status status)
{
    if (!m_cameraIndicator || !m_cameraText) return;
    setStatus(m_cameraIndicator, m_cameraText, status);
}
