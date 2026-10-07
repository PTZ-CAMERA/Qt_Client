#include "MainWindow.h"

#include "camera/CameraPlaybackWidget.h"
#include "network/NetworkClient.h"
#include "ui/ConnectionStatusWidget.h"
#include "ui/PTZControlWidget.h"
#include "ui/TrackingPanel.h"

#include <QDateTime>
#include <QApplication>
#include <QComboBox>
#include <QEvent>
#include <QFile>
#include <QFont>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QImage>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPlainTextEdit>
#include <QScrollArea>
#include <QScrollBar>
#include <QSpinBox>
#include <QSplitter>
#include <QTimer>
#include <QVBoxLayout>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("PTZ Object Tracking Camera"));
    setMinimumSize(1200, 750);
    resize(1400, 850);

    QFile styleFile(QStringLiteral(":/resources/style.qss"));
    if (styleFile.open(QIODevice::ReadOnly))
        setStyleSheet(QString::fromUtf8(styleFile.readAll()));

    m_network = new NetworkClient(this);
    qApp->installEventFilter(this);
    auto *central = new QWidget(this);
    setCentralWidget(central);
    auto *page = new QVBoxLayout(central);
    page->setContentsMargins(16, 14, 16, 14);
    page->setSpacing(12);

    auto *header = new QHBoxLayout;
    auto *heading = new QLabel(QStringLiteral("PTZ  /  OBJECT TRACKING CAMERA"), this);
    heading->setObjectName(QStringLiteral("appHeading"));
    header->addWidget(heading);
    header->addStretch();
    m_mode = new QLabel(QStringLiteral("DEMO MODE"), this);
    m_mode->setObjectName(QStringLiteral("modeBadge"));
    header->addWidget(m_mode);
    page->addLayout(header);

    auto *horizontal = new QSplitter(Qt::Horizontal, this);
    horizontal->setChildrenCollapsible(false);
    m_camera = new CameraPlaybackWidget(this);
    horizontal->addWidget(makePanel(QStringLiteral("CAMERA VIEW  /  CH 01"), m_camera));

    auto *rightScroll = new QScrollArea(this);
    rightScroll->setWidgetResizable(true);
    rightScroll->setFrameShape(QFrame::NoFrame);
    rightScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    rightScroll->setMinimumWidth(330);
    auto *right = new QWidget(rightScroll);
    auto *rightLayout = new QVBoxLayout(right);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(10);
    m_connection = new ConnectionStatusWidget(this);
    m_tracking = new TrackingPanel(this);
    m_ptz = new PTZControlWidget(this);
    rightLayout->addWidget(makePanel(QStringLiteral("CONNECTION"), m_connection));
    rightLayout->addWidget(makePanel(QStringLiteral("TRACKING"), m_tracking));
    rightLayout->addWidget(makePanel(QStringLiteral("PTZ CONTROL"), m_ptz));
    rightLayout->addWidget(makePanel(QStringLiteral("OBJECT INFORMATION"), makeInformationPanel()));
    rightLayout->addStretch();
    rightScroll->setWidget(right);
    horizontal->addWidget(rightScroll);
    horizontal->setStretchFactor(0, 7);
    horizontal->setStretchFactor(1, 3);
    page->addWidget(horizontal, 1);

    m_log = new QPlainTextEdit(this);
    m_log->setReadOnly(true);
    m_log->setMaximumBlockCount(1000);
    m_log->setMinimumHeight(95);
    auto *logPanel = makePanel(QStringLiteral("SYSTEM LOG"), m_log);
    logPanel->setMinimumHeight(145);
    page->addWidget(logPanel);

    connect(m_ptz, &PTZControlWidget::panLeftRequested, this, [this] { sendPtz(QStringLiteral("LEFT")); });
    connect(m_ptz, &PTZControlWidget::panRightRequested, this, [this] { sendPtz(QStringLiteral("RIGHT")); });
    connect(m_ptz, &PTZControlWidget::tiltUpRequested, this, [this] { sendPtz(QStringLiteral("UP")); });
    connect(m_ptz, &PTZControlWidget::tiltDownRequested, this, [this] { sendPtz(QStringLiteral("DOWN")); });
    connect(m_ptz, &PTZControlWidget::centerRequested, this, [this] { sendPtz(QStringLiteral("CENTER")); });
    connect(m_tracking, &TrackingPanel::trackingChanged, this, [this](bool enabled) {
        addLog(enabled ? QStringLiteral("Auto Tracking ENABLED") : QStringLiteral("Auto Tracking DISABLED"));
        m_network->sendTrackingCommand(enabled);
    });
    connect(m_connection, &ConnectionStatusWidget::connectRequested, this,
            [this](const QString &host, quint16 port) {
                if (host.isEmpty()) {
                    addLog(QStringLiteral("Connection failed: enter a host or IP"));
                    m_connection->setRaspberryPiStatus(ConnectionStatusWidget::Status::Error);
                    return;
                }
                m_demoEnabled = false;
                setDemoStatus(false);
                m_connection->setRaspberryPiStatus(ConnectionStatusWidget::Status::Disconnected);
                addLog(QStringLiteral("Connecting to %1:%2...").arg(host).arg(port));
                m_network->connectToServer(host, port);
            });
    connect(m_connection, &ConnectionStatusWidget::disconnectRequested, this, [this] {
        m_network->disconnectFromServer();
        m_connection->setRaspberryPiConnected(false);
        addLog(QStringLiteral("Disconnect requested"));
    });
    connect(m_connection, &ConnectionStatusWidget::demoStatusRequested, this, [this] {
        if (m_network->isConnected() || m_camera->isRunning()) {
            addLog(QStringLiteral("Demo status unavailable during live connection"));
            return;
        }
        setDemoStatus(!m_demoStatus);
    });

    connect(m_network, &NetworkClient::connected, this, [this] {
        setDemoStatus(false);
        m_connection->setRaspberryPiConnected(true);
        addLog(QStringLiteral("Raspberry Pi connected"));
    });
    connect(m_network, &NetworkClient::disconnected, this, [this] {
        m_connection->setRaspberryPiConnected(false);
        addLog(QStringLiteral("Raspberry Pi disconnected"));
    });
    connect(m_network, &NetworkClient::connectionError, this, [this](const QString &reason) {
        m_connection->setRaspberryPiStatus(ConnectionStatusWidget::Status::Error);
        addLog(QStringLiteral("Connection error: %1").arg(reason));
    });
    connect(m_network, &NetworkClient::trackingInfoReceived, this, [this](const TrackingInfo &info) {
        updateTrackingInfo(info);
        if (m_tracking->trackingEnabled())
            m_tracking->setState(TrackingPanel::State::Tracking);
        addLog(QStringLiteral("%1 detected (%2, %3)").arg(info.label).arg(info.objectX).arg(info.objectY));
    });
    connect(m_network, &NetworkClient::noDetectionReceived, this, [this] {
        clearTrackingInfo();
        if (m_tracking->trackingEnabled())
            m_tracking->setState(TrackingPanel::State::Lost);
        addLog(QStringLiteral("Object lost"));
    });
    connect(m_network, &NetworkClient::cameraConnectionChanged, this, [this](bool connected) {
        addLog(connected ? QStringLiteral("Pi camera source connected")
                         : QStringLiteral("Pi camera source disconnected"));
    });
    connect(m_camera, &CameraPlaybackWidget::statusChanged, this,
            [this](CameraPlaybackWidget::State state, const QString &detail) {
                switch (state) {
                case CameraPlaybackWidget::State::Connecting:
                    m_demoEnabled = false;
                    m_videoActive = false;
                    setDemoStatus(false);
                    m_connection->setCameraConnected(false);
                    m_mode->setText(QStringLiteral("CONNECTING VIDEO"));
                    addLog(QStringLiteral("Video connecting: %1").arg(detail));
                    break;
                case CameraPlaybackWidget::State::Playing:
                    m_videoActive = true;
                    m_connection->setCameraConnected(true);
                    m_mode->setText(QStringLiteral("LIVE VIDEO"));
                    addLog(QStringLiteral("Video playing: %1").arg(detail));
                    break;
                case CameraPlaybackWidget::State::Failed:
                    m_videoActive = false;
                    m_connection->setCameraStatus(ConnectionStatusWidget::Status::Error);
                    m_mode->setText(QStringLiteral("VIDEO ERROR"));
                    addLog(QStringLiteral("Video connection failed: %1").arg(detail));
                    break;
                case CameraPlaybackWidget::State::Idle:
                    m_videoActive = false;
                    m_connection->setCameraConnected(false);
                    m_mode->setText(QStringLiteral("NO VIDEO STREAM"));
                    addLog(QStringLiteral("Video stopped"));
                    break;
                }
            });
    connect(m_camera, &CameraPlaybackWidget::frameSizeChanged, this, [this](const QSize &size) {
        m_frameSize->setText(QStringLiteral("%1 x %2").arg(size.width()).arg(size.height()));
    });
    connect(m_camera, &CameraPlaybackWidget::onvifMessage, this, [this](const QString &message) {
        addLog(message);
    });
    connect(m_network, &NetworkClient::protocolWarning, this, [this](const QString &message) {
        addLog(QStringLiteral("Protocol: %1").arg(message));
    });

    clearTrackingInfo();
    addLog(QStringLiteral("Application started"));
    addLog(QStringLiteral("Waiting for Raspberry Pi..."));
    addLog(QStringLiteral("Demo detection scheduled in 5 seconds"));
    QTimer::singleShot(5000, this, [this] {
        if (m_demoEnabled && !m_network->isConnected())
            showDemoDetection();
    });
}

QWidget *MainWindow::makePanel(const QString &title, QWidget *content)
{
    auto *panel = new QWidget(this);
    panel->setObjectName(QStringLiteral("panel"));
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(14, 12, 14, 14);
    layout->setSpacing(10);
    auto *heading = new QLabel(title, panel);
    heading->setObjectName(QStringLiteral("panelHeading"));
    layout->addWidget(heading);
    layout->addWidget(content, 1);
    return panel;
}

QWidget *MainWindow::makeInformationPanel()
{
    auto *content = new QWidget(this);
    auto *grid = new QGridLayout(content);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(14);
    grid->setVerticalSpacing(6);
    const auto add = [&](int row, const QString &caption, QLabel *&value) {
        auto *name = new QLabel(caption, content);
        name->setObjectName(QStringLiteral("infoName"));
        value = new QLabel(QStringLiteral("—"), content);
        value->setObjectName(QStringLiteral("infoValue"));
        value->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        grid->addWidget(name, row, 0);
        grid->addWidget(value, row, 1);
    };
    add(0, QStringLiteral("Object"), m_object);
    add(1, QStringLiteral("Confidence"), m_confidence);
    add(2, QStringLiteral("Object X"), m_x);
    add(3, QStringLiteral("Object Y"), m_y);
    add(4, QStringLiteral("Error X"), m_errorX);
    add(5, QStringLiteral("Error Y"), m_errorY);
    add(6, QStringLiteral("Pan"), m_pan);
    add(7, QStringLiteral("Tilt"), m_tilt);
    add(8, QStringLiteral("Frame size"), m_frameSize);
    grid->setColumnStretch(0, 1);
    return content;
}

void MainWindow::updateTrackingInfo(const TrackingInfo &info)
{
    if (!info.detected) {
        clearTrackingInfo();
        return;
    }
    m_object->setText(info.label);
    m_confidence->setText(QStringLiteral("%1 %").arg(qRound(info.confidence * 100.0F)));
    m_x->setText(QString::number(info.objectX));
    m_y->setText(QString::number(info.objectY));
    m_errorX->setText(QStringLiteral("%1%2").arg(info.errorX >= 0 ? QStringLiteral("+") : QString()).arg(info.errorX));
    m_errorY->setText(QStringLiteral("%1%2").arg(info.errorY >= 0 ? QStringLiteral("+") : QString()).arg(info.errorY));
    m_pan->setText(QStringLiteral("%1 deg").arg(info.panAngle, 0, 'f', 0));
    m_tilt->setText(QStringLiteral("%1 deg").arg(info.tiltAngle, 0, 'f', 0));
    m_frameSize->setText(QStringLiteral("%1 x %2").arg(info.frameWidth).arg(info.frameHeight));
}

void MainWindow::clearTrackingInfo()
{
    m_camera->clearDetection();
    for (QLabel *value : {m_object, m_confidence, m_x, m_y, m_errorX, m_errorY, m_pan, m_tilt})
        value->setText(QStringLiteral("—"));
    m_frameSize->setText(QStringLiteral("640 x 480"));
}

void MainWindow::addLog(const QString &message)
{
    m_log->appendPlainText(QStringLiteral("[%1] %2")
                               .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss")), message));
    m_log->verticalScrollBar()->setValue(m_log->verticalScrollBar()->maximum());
}

void MainWindow::sendPtz(const QString &direction)
{
    if (!m_network->isConnected()) {
        addLog(QStringLiteral("PTZ %1 not sent: connect to Raspberry Pi first").arg(direction));
        return;
    }
    addLog(QStringLiteral("PTZ %1 command").arg(direction));
    m_network->sendPtzCommand(direction);
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    Q_UNUSED(watched)
    if (event->type() != QEvent::KeyPress || QApplication::activeWindow() != this)
        return QMainWindow::eventFilter(watched, event);

    QWidget *focus = QApplication::focusWidget();
    if (qobject_cast<QLineEdit *>(focus) || qobject_cast<QSpinBox *>(focus)
        || qobject_cast<QComboBox *>(focus))
        return QMainWindow::eventFilter(watched, event);

    auto *key = static_cast<QKeyEvent *>(event);
    if (key->isAutoRepeat() || (key->modifiers() != Qt::NoModifier
                                && key->modifiers() != Qt::ShiftModifier))
        return QMainWindow::eventFilter(watched, event);

    switch (key->key()) {
    case Qt::Key_Left: sendPtz(QStringLiteral("LEFT")); return true;
    case Qt::Key_Right: sendPtz(QStringLiteral("RIGHT")); return true;
    case Qt::Key_Up: sendPtz(QStringLiteral("UP")); return true;
    case Qt::Key_Down: sendPtz(QStringLiteral("DOWN")); return true;
    case Qt::Key_R: sendPtz(QStringLiteral("CENTER")); return true;
    default: return QMainWindow::eventFilter(watched, event);
    }
}

void MainWindow::setDemoStatus(bool enabled)
{
    if (m_demoStatus == enabled)
        return;
    m_demoStatus = enabled;
    m_connection->setRaspberryPiConnected(enabled || m_network->isConnected());
    m_connection->setCameraConnected(enabled || m_videoActive);
    m_mode->setText(enabled ? QStringLiteral("DEMO STATUS / NO TCP")
                            : (m_demoEnabled ? QStringLiteral("DEMO MODE")
                                             : QStringLiteral("LIVE / NO VIDEO STREAM")));
    addLog(enabled ? QStringLiteral("Demo device status CONNECTED (simulated)")
                   : QStringLiteral("Demo device status DISCONNECTED"));
}

void MainWindow::showDemoDetection()
{
    QImage frame(640, 480, QImage::Format_RGB32);
    frame.fill(QColor("#17232b"));
    QPainter painter(&frame);
    painter.setPen(QPen(QColor("#273b43"), 1));
    for (int x = 0; x < frame.width(); x += 40)
        painter.drawLine(x, 0, x, frame.height());
    for (int y = 0; y < frame.height(); y += 40)
        painter.drawLine(0, y, frame.width(), y);
    painter.setPen(QColor("#78909b"));
    QFont font = painter.font();
    font.setPixelSize(16);
    painter.setFont(font);
    painter.drawText(QRect(18, 16, 220, 32), QStringLiteral("DEMO FRAME  /  640 x 480"));
    painter.end();
    m_camera->setDemoFrame(frame);
    m_camera->setDetection(QRect(250, 120, 140, 260), QStringLiteral("Person"), 0.92F,
                           QPoint(320, 250));

    TrackingInfo info;
    info.detected = true;
    info.label = QStringLiteral("Person");
    info.confidence = 0.92F;
    info.objectX = 320;
    info.objectY = 250;
    info.errorX = 0;
    info.errorY = 10;
    info.panAngle = 90;
    info.tiltAngle = 92;
    updateTrackingInfo(info);
    if (m_tracking->trackingEnabled())
        m_tracking->setState(TrackingPanel::State::Tracking);
    addLog(QStringLiteral("Demo: Person detected (320, 250)"));
}
