#include "ConnectionStatusWidget.h"

#include <QFormLayout>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

ConnectionStatusWidget::ConnectionStatusWidget(QWidget *parent) : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);

    auto *statuses = new QGridLayout;
    statuses->setHorizontalSpacing(8);
    statuses->setVerticalSpacing(6);
    auto addStatus = [&](int row, const QString &name, QLabel *&indicator, QLabel *&stateText) {
        indicator = new QLabel(QStringLiteral("●"), this);
        indicator->setFixedWidth(18);
        stateText = new QLabel(this);
        statuses->addWidget(indicator, row, 0);
        statuses->addWidget(new QLabel(name, this), row, 1);
        statuses->addWidget(stateText, row, 2, Qt::AlignRight);
    };
    addStatus(0, QStringLiteral("Raspberry Pi"), m_piIndicator, m_piText);
    addStatus(1, QStringLiteral("Camera"), m_cameraIndicator, m_cameraText);
    statuses->setColumnStretch(1, 1);
    layout->addLayout(statuses);
    setRaspberryPiStatus(Status::Disconnected);
    setCameraStatus(Status::Disconnected);

    auto *form = new QFormLayout;
    form->setLabelAlignment(Qt::AlignLeft);
    m_ip = new QLineEdit(QStringLiteral("192.168.0.92"), this);
    m_ip->setPlaceholderText(QStringLiteral("Raspberry Pi IP or hostname"));
    m_port = new QSpinBox(this);
    m_port->setRange(1, 65535);
    m_port->setValue(5000);
    form->addRow(QStringLiteral("IP"), m_ip);
    form->addRow(QStringLiteral("Port"), m_port);
    layout->addLayout(form);

    auto *buttons = new QHBoxLayout;
    auto *connectButton = new QPushButton(QStringLiteral("Connect"), this);
    connectButton->setObjectName(QStringLiteral("primaryButton"));
    auto *disconnectButton = new QPushButton(QStringLiteral("Disconnect"), this);
    buttons->addWidget(connectButton);
    buttons->addWidget(disconnectButton);
    layout->addLayout(buttons);

    auto *demoButton = new QPushButton(QStringLiteral("Toggle demo status"), this);
    demoButton->setObjectName(QStringLiteral("secondaryButton"));
    demoButton->setToolTip(QStringLiteral("Simulate device LEDs without a network connection"));
    layout->addWidget(demoButton);

    connect(connectButton, &QPushButton::clicked, this, [this] {
        emit connectRequested(m_ip->text().trimmed(), static_cast<quint16>(m_port->value()));
    });
    connect(disconnectButton, &QPushButton::clicked, this, &ConnectionStatusWidget::disconnectRequested);
    connect(demoButton, &QPushButton::clicked, this, &ConnectionStatusWidget::demoStatusRequested);
}

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

void ConnectionStatusWidget::setRaspberryPiConnected(bool connected)
{
    setRaspberryPiStatus(connected ? Status::Connected : Status::Disconnected);
}

void ConnectionStatusWidget::setCameraConnected(bool connected)
{
    setCameraStatus(connected ? Status::Connected : Status::Disconnected);
}

void ConnectionStatusWidget::setRaspberryPiStatus(Status status)
{
    setStatus(m_piIndicator, m_piText, status);
}

void ConnectionStatusWidget::setCameraStatus(Status status)
{
    setStatus(m_cameraIndicator, m_cameraText, status);
}
