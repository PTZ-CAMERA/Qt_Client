// 앱의 화면 구성과 각 위젯/통신 클래스 사이의 신호 연결을 담당한다.
// 영상은 CameraPlaybackWidget, TCP 제어/탐지 메시지는 NetworkClient가 처리한다.
// 현재 ONVIF 기능은 검색과 RTSP 주소 조회까지이며 PTZ 입력은 기존 TCP로 송신한다.
#include "LegacyMainWindow.h"

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

// 화면을 만든 뒤 버튼 입력, 네트워크 응답, 영상 재생 상태를 서로 연결한다.
// 자식 위젯과 NetworkClient의 수명은 Qt의 부모-자식 소유권으로 관리한다.
LegacyMainWindow::LegacyMainWindow(QWidget *parent) : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("PTZ Object Tracking Camera"));
    setMinimumSize(1200, 750);
    resize(1400, 850);

    // CMake 리소스에 포함된 스타일을 읽으므로 실행 폴더에 QSS 파일을 따로 둘 필요가 없다.
    QFile styleFile(QStringLiteral(":/resources/style.qss"));
    if (styleFile.open(QIODevice::ReadOnly))
        setStyleSheet(QString::fromUtf8(styleFile.readAll()));

    m_network = new NetworkClient(this);
    // 앱 전체의 키 입력을 확인하되, eventFilter()에서 현재 창과 입력칸 포커스를 검사한다.
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

    // 영상과 오른쪽 조작 패널 사이의 경계를 사용자가 드래그할 수 있게 한다.
    // 패널이 완전히 접히지 않도록 하고 남는 공간의 배분 비중을 7:3으로 설정한다.
    auto *horizontal = new QSplitter(Qt::Horizontal, this);
    horizontal->setChildrenCollapsible(false);
    m_camera = new CameraPlaybackWidget(this);
    horizontal->addWidget(makePanel(QStringLiteral("CAMERA VIEW  /  CH 01"), m_camera));

    // 창 높이가 부족하면 조작 패널만 스크롤해 영상 영역은 계속 보이도록 한다.
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

    // 로그는 읽기 전용이며 최대 1000개 문단을 유지해 장시간 실행 시 누적량을 제한한다.
    m_log = new QPlainTextEdit(this);
    m_log->setReadOnly(true);
    m_log->setMaximumBlockCount(1000);
    m_log->setMinimumHeight(95);
    auto *logPanel = makePanel(QStringLiteral("SYSTEM LOG"), m_log);
    logPanel->setMinimumHeight(145);
    page->addWidget(logPanel);

    // 화면 버튼과 키보드가 모두 sendPtz()를 사용해 연결 확인과 로그 규칙을 공유한다.
    connect(m_ptz, &PTZControlWidget::panLeftRequested, this, [this] { sendPtz(QStringLiteral("LEFT")); });
    connect(m_ptz, &PTZControlWidget::panRightRequested, this, [this] { sendPtz(QStringLiteral("RIGHT")); });
    connect(m_ptz, &PTZControlWidget::tiltUpRequested, this, [this] { sendPtz(QStringLiteral("UP")); });
    connect(m_ptz, &PTZControlWidget::tiltDownRequested, this, [this] { sendPtz(QStringLiteral("DOWN")); });
    connect(m_ptz, &PTZControlWidget::centerRequested, this, [this] { sendPtz(QStringLiteral("CENTER")); });
    connect(m_tracking, &TrackingPanel::trackingChanged, this, [this](bool enabled) {
        // UI 요청 상태는 바로 표시한다. 서버가 요청을 적용했다는 확인 응답은 현재 없다.
        addLog(enabled ? QStringLiteral("Auto Tracking ENABLED") : QStringLiteral("Auto Tracking DISABLED"));
        m_network->sendTrackingCommand(enabled);
    });
    connect(m_connection, &ConnectionStatusWidget::connectRequested, this,
            [this](const QString &host, quint16 port) {
                // 실제 장치 접속을 시도하면 예약된 데모 데이터가 나중에 끼어들지 않게 한다.
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
        // 실제 연결/재생과 데모 LED를 섞어 연결 상태를 오해하지 않게 한다.
        if (m_network->isConnected() || m_camera->isRunning()) {
            addLog(QStringLiteral("Demo status unavailable during live connection"));
            return;
        }
        setDemoStatus(!m_demoStatus);
    });

    // TCP 연결 상태는 Raspberry Pi LED로 표시한다. 영상 수신 상태와는 따로 관리한다.
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
        // 숫자 정보는 중앙 갱신 함수에 전달하고, 추적 모드가 켜져 있을 때만 TRACKING으로 바꾼다.
        updateTrackingInfo(info);
        if (m_tracking->trackingEnabled())
            m_tracking->setState(TrackingPanel::State::Tracking);
        addLog(QStringLiteral("%1 detected (%2, %3)").arg(info.label).arg(info.objectX).arg(info.objectY));
    });
    connect(m_network, &NetworkClient::noDetectionReceived, this, [this] {
        // 이전 객체 정보와 데모 상자를 제거한다. 추적 모드가 켜져 있다면 LOST로 표시한다.
        clearTrackingInfo();
        if (m_tracking->trackingEnabled())
            m_tracking->setState(TrackingPanel::State::Lost);
        addLog(QStringLiteral("Object lost"));
    });
    connect(m_network, &NetworkClient::cameraConnectionChanged, this, [this](bool connected) {
        // Pi가 보내는 카메라 소스 상태는 로그에 기록한다. Camera LED는 PC의 실제 영상
        // 프레임 재생 상태를 따르므로 이 메시지만으로 녹색으로 바꾸지 않는다.
        addLog(connected ? QStringLiteral("Pi camera source connected")
                         : QStringLiteral("Pi camera source disconnected"));
    });
    connect(m_camera, &CameraPlaybackWidget::statusChanged, this,
            [this](CameraPlaybackWidget::State state, const QString &detail) {
                // 영상의 연결 중/재생/오류/정지 상태를 LED, 상단 문구와 로그에 함께 반영한다.
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
    // 장치 없이 화면을 점검할 수 있도록 5초 뒤 데모 탐지를 한 번 표시한다.
    // 그 전에 실제 연결 또는 영상 재생을 시작했다면 데모가 나타나지 않는다.
    QTimer::singleShot(5000, this, [this] {
        if (m_demoEnabled && !m_network->isConnected())
            showDemoDetection();
    });
}

// 공통 패널 테두리와 제목을 만들고 전달받은 내용 위젯을 배치한다.
// 내용 위젯의 부모 관계는 레이아웃에 들어가면서 Qt가 관리한다.
QWidget *LegacyMainWindow::makePanel(const QString &title, QWidget *content)
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

// 객체 정보의 이름/값 행을 생성하고, 이후 갱신할 값 QLabel의 주소를 멤버에 보관한다.
QWidget *LegacyMainWindow::makeInformationPanel()
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

// 탐지 정보의 표시 형식을 한곳에서 관리한다.
// 신뢰도는 백분율, 오차는 부호 포함 픽셀 값, 각도는 정수처럼 보이도록 표시한다.
// 여기서 오차나 서보 각도를 계산하지 않고 서버/데모가 전달한 값을 그대로 사용한다.
void LegacyMainWindow::updateTrackingInfo(const TrackingInfo &info)
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

// 객체가 없을 때 지난 탐지값이 현재값처럼 보이지 않도록 빈 표시로 되돌린다.
// 프레임 크기는 기본값 640×480으로 초기화하며, 실영상 프레임 신호가 오면 다시 갱신된다.
void LegacyMainWindow::clearTrackingInfo()
{
    m_camera->clearDetection();
    for (QLabel *value : {m_object, m_confidence, m_x, m_y, m_errorX, m_errorY, m_pan, m_tilt})
        value->setText(QStringLiteral("—"));
    m_frameSize->setText(QStringLiteral("640 x 480"));
}

// 모든 로그에 PC의 현재 시각을 붙이고 마지막 줄로 스크롤한다.
void LegacyMainWindow::addLog(const QString &message)
{
    m_log->appendPlainText(QStringLiteral("[%1] %2")
                               .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss")), message));
    m_log->verticalScrollBar()->setValue(m_log->verticalScrollBar()->maximum());
}

// 현재 PTZ 송신 경로는 기존 TCP 텍스트 프로토콜이다.
// 미연결이면 송신하지 않고 이유를 기록한다. 연결 중에 남기는 명령 로그도 Pi의 처리 완료
// 확인은 아니며, 서버 확인 응답을 검증하는 기능은 아직 구현되어 있지 않다.
void LegacyMainWindow::sendPtz(const QString &direction)
{
    if (!m_network->isConnected()) {
        addLog(QStringLiteral("PTZ %1 not sent: connect to Raspberry Pi first").arg(direction));
        return;
    }
    addLog(QStringLiteral("PTZ %1 command").arg(direction));
    m_network->sendPtzCommand(direction);
}

// 전역으로 받은 키 중 이 창의 PTZ 단축키만 처리한다.
// 처리한 키는 true로 반환해 다른 위젯이 같은 입력을 다시 처리하지 않게 한다.
bool LegacyMainWindow::eventFilter(QObject *watched, QEvent *event)
{
    Q_UNUSED(watched)
    if (event->type() != QEvent::KeyPress || QApplication::activeWindow() != this)
        return QMainWindow::eventFilter(watched, event);

    QWidget *focus = QApplication::focusWidget();
    // 주소/계정/포트 편집과 재생 방식 선택 중에는 방향키와 R을 원래 위젯에 전달한다.
    if (qobject_cast<QLineEdit *>(focus) || qobject_cast<QSpinBox *>(focus)
        || qobject_cast<QComboBox *>(focus))
        return QMainWindow::eventFilter(watched, event);

    auto *key = static_cast<QKeyEvent *>(event);
    // 키를 누르고 있을 때의 자동 반복 송신을 막고, 일반 입력과 Shift 입력만 허용한다.
    // Qt::Key_R로 비교하므로 소문자 r과 대문자 R은 같은 중앙 복귀 명령을 사용한다.
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

// 실제 소켓이나 스트림을 연결하지 않고 상태 LED만 시험하는 기능이다.
// 실제 TCP 연결과 영상 재생 여부도 함께 고려해 표시 상태를 만든다.
void LegacyMainWindow::setDemoStatus(bool enabled)
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

// 테스트용 격자 프레임과 Person 탐지값을 만들어 UI의 좌표/상자 표시를 점검한다.
// 실제 카메라 수신, AI 탐지 또는 서보 이동을 실행하는 함수가 아니다.
void LegacyMainWindow::showDemoDetection()
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
    // 이미지에 그리기를 마친 뒤 위젯에 전달해 화면 그리기와 겹치지 않도록 한다.
    painter.end();
    m_camera->setDemoFrame(frame);
    m_camera->setDetection(QRect(250, 120, 140, 260), QStringLiteral("Person"), 0.92F,
                           QPoint(320, 250));

    // 데모의 프레임 중심은 (320, 240), 객체 중심은 (320, 250)이므로 오차는 (0, +10)이다.
    // Pan/Tilt도 표시 시험용 값이며 실제 장치에서 읽어온 각도가 아니다.
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
