// UI controller. WebSocket/JSON parsing belongs to VmsClient.
#include "MainWindow.h"
#include "camera/CameraListWidget.h"
#include "camera/CameraViewWidget.h"
#include "demo/DummyDataProvider.h"
#include "network/VmsClient.h"
#include "device/DeviceInfoWidget.h"
#include "events/EventSearchWidget.h"
#include "chat/ChatSearchWidget.h"
#include "log/SystemLogWidget.h"
#include "playback/PlaybackWidget.h"
#include "ui/ConnectionStatusWidget.h"
#include "ui/PTZControlWidget.h"
#include "ui/PtzKeyboardController.h"
#include "ui/PtzCommandController.h"
#include "ui/StatusIndicatorWidget.h"
#include "ui/TrackingPanel.h"
#include "ui/HelpDialog.h"
#include <QCheckBox>
#include <QSignalBlocker>
#include <QFile>
#include <QHBoxLayout>
#include <QLabel>
#include <QScrollArea>
#include <QSplitter>
#include <QStatusBar>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QPushButton>
#include <QShortcut>
#include <QHostAddress>
#include <QSignalBlocker>
#include <algorithm>
#include <cmath>

MainWindow::MainWindow(QWidget *parent, bool dummyMode) : QMainWindow(parent) {
    setWindowTitle(QStringLiteral("Edge AI PTZ Mini VMS"));
    setMinimumSize(1200, 750); resize(1400, 850);
    QFile style(QStringLiteral(":/resources/style.qss"));
    if (style.open(QIODevice::ReadOnly)) setStyleSheet(QString::fromUtf8(style.readAll()));
    m_dummy = new DummyDataProvider(this); m_client = new VmsClient(this); m_keyboard = new PtzKeyboardController(this);
    m_ptzCommands = new PtzCommandController(this);
    auto *central = new QWidget(this); setCentralWidget(central);
    auto *layout = new QVBoxLayout(central); layout->setContentsMargins(12, 10, 12, 10);
    auto *header = new QHBoxLayout;
    auto *title = new QLabel(QStringLiteral("EDGE AI PTZ  /  MINI VMS"), this); title->setObjectName(QStringLiteral("appHeading"));
    header->addWidget(title); header->addStretch();
    m_dummyToggle = new QCheckBox(QStringLiteral("Dummy Mode / NO VMS NETWORK"), this); header->addWidget(m_dummyToggle);
    m_headerVms = new StatusIndicatorWidget(QStringLiteral("VMS"), this); header->addWidget(m_headerVms);
    auto *helpButton = new QPushButton(QStringLiteral("?"), this);
    helpButton->setObjectName(QStringLiteral("helpButton")); helpButton->setFixedSize(32, 32);
    helpButton->setToolTip(QStringLiteral("사용 도움말 (F1)")); header->addWidget(helpButton); layout->addLayout(header);
    connect(helpButton, &QPushButton::clicked, this, [this] {
        if (!m_help) m_help = new HelpDialog(this);
        m_help->show(); m_help->raise(); m_help->activateWindow();
    });
    auto *helpShortcut = new QShortcut(QKeySequence(Qt::Key_F1), this);
    connect(helpShortcut, &QShortcut::activated, helpButton, &QPushButton::click);
    // 기존 중앙 영상과 오른쪽 패널에 목록을 추가하고 하단 탭은 세로 splitter로 조절한다.
    auto *vertical = new QSplitter(Qt::Vertical, this); vertical->setChildrenCollapsible(false);
    auto *top = new QSplitter(Qt::Horizontal, this); top->setChildrenCollapsible(false);
    m_list = new CameraListWidget(this); top->addWidget(makePanel(QStringLiteral("CAMERAS"), m_list));
    m_view = new CameraViewWidget(this); top->addWidget(makePanel(QStringLiteral("LIVE VIEW"), m_view));
    auto *rightScroll = new QScrollArea(this); rightScroll->setWidgetResizable(true); rightScroll->setMinimumWidth(250);
    rightScroll->setFrameShape(QFrame::NoFrame); rightScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto *right = new QWidget; auto *rightLayout = new QVBoxLayout(right); rightLayout->setContentsMargins(0, 0, 0, 0);
    m_ptz = new PTZControlWidget(this); m_tracking = new TrackingPanel(this);
    auto *unsupported = new QLabel(QStringLiteral("PTZ / Tracking: unavailable on current VMS"), this);
    m_ptzNotice = unsupported;
    unsupported->setWordWrap(true); unsupported->setObjectName(QStringLiteral("ptzHint"));
    rightLayout->addWidget(unsupported);
    m_ptz->setEnabled(false); m_tracking->setEnabled(false);
    m_ptz->setToolTip(QStringLiteral("VMS PTZ API is not supported"));
    m_tracking->setToolTip(QStringLiteral("VMS tracking/metadata API is not supported"));
    connect(m_dummyToggle, &QCheckBox::toggled, unsupported, [unsupported](bool enabled) { unsupported->setVisible(!enabled); });
    rightLayout->addWidget(makePanel(QStringLiteral("PTZ CONTROL"), m_ptz));
    rightLayout->addWidget(makePanel(QStringLiteral("TRACKING / OBJECT INFORMATION"), m_tracking));
    m_diagnostics = new QLabel(QStringLiteral("VMS 연결 후 탐지·저장 상태를 확인합니다."),this);
    m_diagnostics->setObjectName(QStringLiteral("metadataDiagnostics")); m_diagnostics->setWordWrap(true);
    m_autoRecording=new QCheckBox(QStringLiteral("자동 녹화 · Tracking ON에서 사람 탐지 시"),this);
    m_autoRecording->setObjectName(QStringLiteral("autoRecordingToggle")); m_autoRecording->setEnabled(false);
    rightLayout->addWidget(m_autoRecording);
    connect(m_autoRecording,&QCheckBox::toggled,this,[this](bool enabled){
        if (m_dummy->isEnabled() || !m_client->isConnected() || m_current.id.isEmpty()) return;
        m_autoRecording->setEnabled(false);
        m_autoRecordingRequestId=m_client->setAutoRecording(m_current.id,enabled);
    });
    connect(m_client,&VmsClient::autoRecordingConfigured,this,[this](const QString &id,const QString &camera,bool enabled){
        if (id!=m_autoRecordingRequestId || camera!=m_current.id) return;
        m_autoRecordingRequestId.clear(); const QSignalBlocker blocker(m_autoRecording);
        m_autoRecording->setChecked(enabled); m_autoRecording->setEnabled(m_client->isConnected() && m_current.supportsEvents);
        addLog(QStringLiteral("REC"),enabled ? QStringLiteral("탐지 자동 녹화 ON · Tracking ON 필요") : QStringLiteral("탐지 자동 녹화 OFF"));
    });
    rightLayout->addWidget(makePanel(QStringLiteral("DETECTION / RECORDING STATUS"),m_diagnostics)); rightLayout->addStretch();
    // DB 조회는 비동기로 요청하며 이전 요청이 끝나기 전에는 중복 조회하지 않는다.
    m_diagnosticsTimer.setInterval(2000);
    connect(&m_diagnosticsTimer,&QTimer::timeout,this,[this]{
        if (m_dummy->isEnabled() || !m_client->isConnected() || m_current.id.isEmpty()) {
            m_autoRecording->setEnabled(false);
            m_diagnosticsRequestId.clear(); m_diagnostics->setText(QStringLiteral("실제 VMS와 카메라를 연결하세요.")); return;
        }
        if (m_diagnosticsRequestId.isEmpty()) m_diagnosticsRequestId=m_client->request(QStringLiteral("GET_METADATA_STATUS"),m_current.id);
    });
    m_diagnosticsTimer.start();
    connect(m_client,&VmsClient::metadataStatusReceived,this,[this](const QString &requestId,const QJsonObject &data){
        if (requestId!=m_diagnosticsRequestId) return;
        m_diagnosticsRequestId.clear();
        if (m_dummy->isEnabled() || data.value(QStringLiteral("cameraId")).toString()!=m_current.id) return;
        const auto timeText=[](const QJsonValue &v){return v.isDouble() ? QDateTime::fromMSecsSinceEpoch(v.toInteger()).toLocalTime().toString(QStringLiteral("MM-dd HH:mm:ss")) : QStringLiteral("없음");};
        const auto age=QDateTime::currentMSecsSinceEpoch()-data.value(QStringLiteral("detectedAtMs")).toInteger(0);
        const auto detected=data.value(QStringLiteral("detected"));
        const auto detection=detected.isBool() && age>=0 && age<3000 && m_current.eventsStatus==QStringLiteral("SUBSCRIBED")
            ? detected.toBool() ? QStringLiteral("사람 있음") : QStringLiteral("사람 없음") : QStringLiteral("현재 상태 미확인");
        const auto rec=data.value(QStringLiteral("recording")).toObject();
        if (m_autoRecordingRequestId.isEmpty()) {
            const QSignalBlocker blocker(m_autoRecording); m_autoRecording->setChecked(rec.value(QStringLiteral("autoRecordingEnabled")).toBool());
            m_autoRecording->setEnabled(m_current.supportsEvents);
        }
        m_diagnostics->setText(QStringLiteral("이벤트 연결: %1\n마지막 수신: %2\n마지막 샘플 저장: %3\n탐지: %4\n저장된 탐지 샘플: %5건\n샘플 저장: %6\n녹화: %7")
            .arg(m_current.eventsStatus,timeText(data.value(QStringLiteral("lastReceivedTimeMs"))),timeText(data.value(QStringLiteral("lastStoredTimeMs"))),detection)
            .arg(data.value(QStringLiteral("storedDetections")).toInteger())
            .arg(data.value(QStringLiteral("samplingEnabled")).toBool() ? QStringLiteral("ON") : QStringLiteral("OFF"),rec.value(QStringLiteral("state")).toString(QStringLiteral("미확인"))));
    });
    rightScroll->setWidget(right); top->addWidget(rightScroll);
    top->setStretchFactor(0, 0); top->setStretchFactor(1, 1); top->setStretchFactor(2, 0); top->setSizes({200, 850, 280});
    vertical->addWidget(top); m_tabs = new QTabWidget(this);
    m_events = new EventSearchWidget(this); m_playback = new PlaybackWidget(this); m_device = new DeviceInfoWidget(this);
    m_connection = new ConnectionStatusWidget(this, ConnectionStatusWidget::Purpose::VmsServer);
    auto *devicePage = new QWidget(this); auto *deviceRow = new QHBoxLayout(devicePage);
    deviceRow->addWidget(makePanel(QStringLiteral("VMS SERVER"), m_connection), 1); deviceRow->addWidget(m_device, 3);
    m_log = new SystemLogWidget(this);
    m_tabs->addTab(m_events, QStringLiteral("EVENTS")); m_tabs->addTab(m_playback, QStringLiteral("PLAYBACK"));
    m_tabs->addTab(devicePage, QStringLiteral("DEVICE")); m_tabs->addTab(m_log, QStringLiteral("SYSTEM LOG"));
    m_chat=new ChatSearchWidget(this); m_tabs->addTab(m_chat,QStringLiteral("CHAT SEARCH"));
    m_tabs->setTabEnabled(0, false); m_events->setEnabled(false);
    m_tabs->setTabToolTip(0, QStringLiteral("탐지 샘플·상태 이력 검색 / 결과 더블클릭 녹화 조회"));
    m_metadataExpiry.setSingleShot(true); m_metadataExpiry.setInterval(2000);
    connect(&m_metadataExpiry,&QTimer::timeout,this,[this]{
        m_view->clearDetection(); m_metadata[QStringLiteral("detected")]=QJsonValue::Null;
        m_metadata[QStringLiteral("errorX")]=QJsonValue::Null; m_metadata[QStringLiteral("errorY")]=QJsonValue::Null;
        if (!m_dummy->isEnabled()) m_tracking->updateMetadata(m_metadata);
    });
    vertical->addWidget(m_tabs); vertical->setStretchFactor(0, 3); vertical->setStretchFactor(1, 1); vertical->setSizes({520, 250});
    layout->addWidget(vertical, 1);
    m_vmsStatus = new StatusIndicatorWidget(QStringLiteral("VMS"), this); m_cameraStatus = new StatusIndicatorWidget(QStringLiteral("Camera"), this);
    m_streamStatus = new StatusIndicatorWidget(QStringLiteral("Stream"), this); m_recStatus = new StatusIndicatorWidget(QStringLiteral("REC"), this);
    for (auto *indicator : {m_vmsStatus, m_cameraStatus, m_streamStatus, m_recStatus}) statusBar()->addWidget(indicator, 1);

    connect(m_client, &VmsClient::message, this, &MainWindow::addLog);
    connect(m_client, &VmsClient::serverConnectionChanged, this, [this](bool connected) {
        if (m_dummy->isEnabled()) return;
        const auto state = connected ? StatusIndicatorWidget::State::Active : StatusIndicatorWidget::State::Inactive;
        const auto text = connected ? QStringLiteral("Connected") : QStringLiteral("Disconnected");
        m_headerVms->setState(text, state); m_vmsStatus->setState(text, state);
        m_connection->setServerStatus(connected ? ConnectionStatusWidget::Status::Connected : ConnectionStatusWidget::Status::Disconnected);
        if (!connected) {
            m_autoRecordingRequestId.clear(); m_autoRecording->setEnabled(false);
            m_diagnosticsRequestId.clear(); m_diagnostics->setText(QStringLiteral("VMS 연결 끊김 · 상태 미확인"));
            m_metadata={}; m_metadataSourceTimes.clear(); m_metadataExpiry.stop(); m_playbackRequestId.clear();
            m_client->cancelInteractiveRequests(); m_events->resetResults(); m_events->setSearchAvailable(false);
            m_chat->setContext({},false); m_tabs->setTabEnabled(0,false);
            requestStop(); m_ptzCommands->setTarget(QString(), false);
            m_discoveryRequestId.clear(); m_device->setDiscoveryState(false, QStringLiteral("Connect to VMS first"));
            m_playback->setSearchAvailable(false);
            m_view->setRecordingControls(false, false);
            m_liveWanted = false; m_streamRequestPending = false; m_view->stopStream();
            m_cameras.clear(); m_events->setCameras({}); m_events->setEvents({});
            m_playback->setCameras({}); m_playback->clear(); m_playback->setRecordings({}); m_list->setCameras({});
        }
        else { m_events->setRealMode(true); m_events->setSearchAvailable(true); m_tabs->setTabEnabled(0,true); m_events->setEnabled(true); }
    });
    connect(m_client, &VmsClient::connectionError, this, [this](const QString &reason) {
        if (m_dummy->isEnabled()) return;
        addLog(QStringLiteral("VMS"), reason);
        m_connection->setServerStatus(ConnectionStatusWidget::Status::Error);
    });
    connect(m_client, &VmsClient::requestFailed, this, [this](const QString &id, const QString &code, const QString &reason) {
        if (id==m_autoRecordingRequestId) { m_autoRecordingRequestId.clear(); m_autoRecording->setEnabled(false); }
        if (id==m_diagnosticsRequestId) { m_diagnosticsRequestId.clear(); m_diagnostics->setText(QStringLiteral("탐지·저장 상태 조회 실패: %1").arg(code)); }
        addLog(QStringLiteral("VMS"), QStringLiteral("Request %1: %2 — %3").arg(id, code, reason));
        if (!m_discoveryRequestId.isEmpty() && id == m_discoveryRequestId) {
            m_discoveryRequestId.clear(); m_device->setDiscoveryState(false, reason);
        }
        m_events->requestError(id,code+QStringLiteral(": ")+reason); m_chat->requestError(id,code+QStringLiteral(": ")+reason);
        if (!m_playbackRequestId.isEmpty() && id==m_playbackRequestId) { m_playbackRequestId.clear(); m_playback->setSearchError(reason); }
    });
    connect(m_client,&VmsClient::metadataReceived,this,&MainWindow::applyMetadata);
    connect(m_client,&VmsClient::eventReceived,this,[this](const QString &id,const QJsonObject &data){
        if (!m_dummy->isEnabled()) addLog(QStringLiteral("EVENT"),id+QStringLiteral(" ")+data.value(QStringLiteral("type")).toString());
    });
    connect(m_client,&VmsClient::eventReceiverStatus,this,[this](const QString &id,const QJsonObject &data){
        if (m_dummy->isEnabled() || id!=m_current.id) return;
        const auto state=data.value(QStringLiteral("state")).toString();
        if (state==QStringLiteral("RECONNECTING") || state==QStringLiteral("STOPPED") || state==QStringLiteral("DATABASE_ERROR")) {
            m_metadataExpiry.stop(); m_metadata={}; m_metadataSourceTimes.clear(); m_view->clearDetection(); m_tracking->updateMetadata({});
        }
        addLog(QStringLiteral("METADATA"),id+QStringLiteral(" ")+state);
    });
    connect(m_events,&EventSearchWidget::recordSearchRequested,this,[this](const QString &id,const QDateTime &from,const QDateTime &to,bool detections,const QString &type,double confidence,const QJsonValue &cursor){
        if (!m_dummy->isEnabled()) m_events->beginRequest(m_client->searchMetadata(id,from,to,detections,type,confidence,cursor));
    });
    connect(m_events,&EventSearchWidget::queryInvalidated,m_client,&VmsClient::cancelMetadataSearch);
    connect(m_client,&VmsClient::metadataSearchReceived,this,[this](const QString &requestId,const QString &,const QJsonArray &records,const QJsonValue &cursor){
        if (!m_dummy->isEnabled()) m_events->acceptResults(requestId,records,cursor);
    });
    connect(m_events,&EventSearchWidget::recordPlaybackRequested,this,&MainWindow::requestMetadataPlayback);
    connect(m_client,&VmsClient::eventPlaybackReceived,this,[this](const QString &requestId,const QJsonObject &data){
        if (requestId!=m_playbackRequestId || m_dummy->isEnabled()) return;
        m_playbackRequestId.clear(); openMetadataPlayback(data);
    });
    connect(m_chat,&ChatSearchWidget::searchRequested,this,[this](const QString &text){
        if (!m_dummy->isEnabled()) m_chat->beginRequest(m_client->chatSearch(m_current.id,text));
    });
    connect(m_client,&VmsClient::chatSearchReceived,this,[this](const QString &id,const QJsonObject &data){
        if (!m_dummy->isEnabled()) m_chat->acceptResponse(id,data);
    });
    connect(m_chat,&ChatSearchWidget::recordPlaybackRequested,this,&MainWindow::requestMetadataPlayback);
    connect(m_chat,&ChatSearchWidget::selectedPlaybackReceived,this,&MainWindow::openMetadataPlayback);
    connect(m_client, &VmsClient::ptzFailed, this, [this](const QString &id, const QString &command, const QString &, const QString &) {
        if (id == m_current.id && command != QStringLiteral("PTZ_STOP")) requestStop();
    });
    connect(m_client,&VmsClient::controlPhase,this,[this](const QString &id,const QString &command,const QString &phase){
        if (!m_dummy->isEnabled() && id==m_current.id && command.startsWith(QStringLiteral("TRACKING_"))) m_tracking->commandPhase(phase);
    });
    connect(m_client, &VmsClient::cameraListReceived, this, [this](const QList<CameraInfo> &cameras) {
        if (m_dummy->isEnabled()) return;
        m_cameras = cameras; m_events->setCameras(cameras); m_playback->setCameras(cameras); m_list->setCameras(cameras);
        m_playback->setSearchAvailable(std::any_of(cameras.cbegin(), cameras.cend(), [](const CameraInfo &camera) { return camera.supportsRecordings; }));
        if (!m_registerSelection.isEmpty()) m_list->selectCamera(m_registerSelection);
        addLog(QStringLiteral("VMS"), QStringLiteral("Received %1 cameras").arg(cameras.size()));
    });
    connect(m_client, &VmsClient::discoveryReceived, this, [this](const QList<DiscoveredCamera> &devices) {
        if (m_dummy->isEnabled()) return;
        m_discoveryRequestId.clear(); m_device->setDiscoveredCameras(devices);
        addLog(QStringLiteral("ONVIF"), QStringLiteral("Found %1 cameras through VMS; select one or enter its ONVIF address").arg(devices.size()));
    });
    connect(m_client, &VmsClient::cameraRegistered, this, [this](const QString &id) {
        m_registerSelection = id; m_device->cameraRegistered(id);
        addLog(QStringLiteral("ONVIF"), QStringLiteral("%1 registered; opening VMS stream").arg(id));
    });
    connect(m_client, &VmsClient::streamUriReady, this, [this](const QString &id, const QUrl &uri) {
        m_streamRequestPending = false;
        if (!m_dummy->isEnabled()) m_device->setStreamUri(id, uri);
        if (m_dummy->isEnabled() || id != m_current.id || !m_liveWanted) return;
        m_view->startStream(uri);
        addLog(QStringLiteral("STREAM"), QStringLiteral("Opening camera-direct RTSP for %1").arg(id));
    });
    connect(m_client, &VmsClient::streamError, this, [this](const QString &reason) {
        m_device->setStreamQueryError(m_current.id, reason);
        m_streamRequestPending = false; m_liveWanted = false; m_view->stopStream();
        m_streamStatus->setState(QStringLiteral("Stream error"), StatusIndicatorWidget::State::Error);
        addLog(QStringLiteral("STREAM"), reason);
    });
    connect(m_view, &CameraViewWidget::videoFrameReceived, this, [this](const QSize &) {
        m_streamStatus->setState(QStringLiteral("Live (VMS)"), StatusIndicatorWidget::State::Active);
    });
    connect(m_view, &CameraViewWidget::playbackError, this, [this](const QString &reason) {
        m_liveWanted = false; m_streamRequestPending = false;
        m_streamStatus->setState(QStringLiteral("Playback error"), StatusIndicatorWidget::State::Error); addLog(QStringLiteral("STREAM"), reason);
    });
    connect(m_view, &CameraViewWidget::startRecordingRequested, this, [this] {
        if (!m_dummy->isEnabled() && m_client->isConnected() && m_current.supportsRecordings && m_current.online && !m_current.recordingRequested)
            m_client->request(QStringLiteral("START_RECORDING"), m_current.id);
    });
    connect(m_view, &CameraViewWidget::stopRecordingRequested, this, [this] {
        if (!m_dummy->isEnabled() && m_client->isConnected() && m_current.supportsRecordings && m_current.recordingRequested)
            m_client->request(QStringLiteral("STOP_RECORDING"), m_current.id);
    });
    connect(m_client, &VmsClient::recordingListReceived, this, [this](const QString &id, const QList<RecordingInfo> &recordings) {
        if (!m_dummy->isEnabled() && id == m_playback->selectedCameraId()) {
            m_playback->setRecordings(recordings);
            addLog(QStringLiteral("REC"), QStringLiteral("%1: %2 recording results").arg(id).arg(recordings.size()));
        }
    });
    connect(m_client, &VmsClient::recordingSearchFailed, this, [this](const QString &id, const QString &reason) {
        if (!m_dummy->isEnabled() && id == m_playback->selectedCameraId()) m_playback->setSearchError(reason);
    });
    connect(m_playback, &PlaybackWidget::playbackFailed, this, [this](const QString &reason) { addLog(QStringLiteral("PLAYBACK"), reason); });
    connect(m_playback, &PlaybackWidget::queryInvalidated, m_client, &VmsClient::cancelRecordingSearch);
    connect(m_client, &VmsClient::cameraStatusChanged, this, [this](const CameraInfo &camera) {
        if (!m_dummy->isEnabled()) applyCameraStatus(camera);
    });
    connect(m_dummy, &DummyDataProvider::message, this, &MainWindow::addLog);
    connect(m_dummy, &DummyDataProvider::serverConnectionChanged, this, [this](bool connected) {
        const auto state = connected ? StatusIndicatorWidget::State::Active : StatusIndicatorWidget::State::Inactive;
        const QString text = connected ? QStringLiteral("Connected (DUMMY)") : QStringLiteral("Disconnected");
        m_headerVms->setState(text, state); m_vmsStatus->setState(text, state);
        // Dummy state does not represent a real socket; keep Connect available.
        m_connection->setServerStatus(ConnectionStatusWidget::Status::Disconnected);
    });
    connect(m_dummy, &DummyDataProvider::cameraListReceived, this, [this](const QList<CameraInfo> &cameras) {
        m_cameras = cameras; m_events->setCameras(cameras); m_playback->setCameras(cameras); m_list->setCameras(cameras);
    });
    connect(m_list, &CameraListWidget::cameraSelected, this, &MainWindow::selectCamera);
    connect(m_dummy, &DummyDataProvider::cameraStatusChanged, this, &MainWindow::applyCameraStatus);
    connect(m_dummy, &DummyDataProvider::frameReceived, this, [this](const QString &id, const QImage &frame) {
        if (id != m_current.id) return;
        m_view->setFrame(frame); m_view->setLive(!frame.isNull(), true);
        m_streamStatus->setState(frame.isNull() ? QStringLiteral("Idle") : QStringLiteral("Live (DUMMY)"),
            frame.isNull() ? StatusIndicatorWidget::State::Inactive : StatusIndicatorWidget::State::Active);
    });
    connect(m_dummy, &DummyDataProvider::detectionReceived, this, [this](const QString &id, const DetectionInfo &info) {
        if (id == m_current.id) m_view->setDetection(info);
    });
    connect(m_dummy, &DummyDataProvider::trackingInfoReceived, this, [this](const QString &id, const TrackingInfo &info) {
        if (id == m_current.id) m_tracking->updateTrackingInfo(info);
    });
    connect(m_dummy, &DummyDataProvider::eventListReceived, m_events, &EventSearchWidget::setEvents);
    connect(m_dummy, &DummyDataProvider::recordingListReceived, m_playback, &PlaybackWidget::setRecordings);
    connect(m_events, &EventSearchWidget::searchRequested, this, [this](const QString &id, const QDateTime &start, const QDateTime &end, const QString &type) {
        if (m_dummy->isEnabled()) m_dummy->requestEvents(id, start, end, type);
        else addLog(QStringLiteral("EVENT"), QStringLiteral("Event search is not supported by VMS"));
    });
    connect(m_playback, &PlaybackWidget::searchRequested, this, [this](const QString &id, const QDateTime &start, const QDateTime &end) {
        if (m_dummy->isEnabled()) m_dummy->requestRecordings(id, start, end);
        else {
            const auto camera = std::find_if(m_cameras.cbegin(), m_cameras.cend(), [&id](const CameraInfo &item) { return item.id == id; });
            if (camera == m_cameras.cend() || !camera->supportsRecordings) {
                m_playback->setSearchError(QStringLiteral("Recording search is not supported for this camera")); return;
            }
            m_client->requestRecordings(id, start, end);
        }
    });
    connect(m_events, &EventSearchWidget::eventPlaybackRequested, this, [this](const QString &id, const QDateTime &time) {
        if (m_dummy->isEnabled()) m_dummy->requestRecordingAt(id, time);
        else addLog(QStringLiteral("REC"), QStringLiteral("Recording lookup requires the VMS API"));
    });
    connect(m_dummy, &DummyDataProvider::recordingResolved, this, [this](const RecordingInfo &recording, const QDateTime &time) {
        m_playback->openRecording(recording, time); m_tabs->setCurrentWidget(m_playback);
        addLog(QStringLiteral("REC"), QStringLiteral("DUMMY timeline opened at %1").arg(time.toString(QStringLiteral("HH:mm:ss"))));
    });
    connect(m_ptz, &PTZControlWidget::moveRequested, m_ptzCommands, &PtzCommandController::setButtonMovement);
    connect(m_ptz, &PTZControlWidget::stopRequested, this, [this] { m_ptzCommands->setButtonMovement(0, 0); });
    connect(m_ptz, &PTZControlWidget::centerRequested, this, &MainWindow::requestCenter);
    connect(m_keyboard, &PtzKeyboardController::moveRequested, m_ptzCommands, &PtzCommandController::setKeyboardMovement);
    connect(m_keyboard, &PtzKeyboardController::stopRequested, this, [this] { m_ptzCommands->setKeyboardMovement(0, 0); });
    connect(m_keyboard, &PtzKeyboardController::centerRequested, this, &MainWindow::requestCenter);
    connect(m_keyboard, &PtzKeyboardController::inputCancelled, this, &MainWindow::requestStop);
    connect(m_ptzCommands, &PtzCommandController::moveRequested, this, [this](const QString &id, float pan, float tilt) {
        if (m_dummy->isEnabled()) m_dummy->movePtz(id, pan, tilt);
        else { m_tracking->cancelCommand(QStringLiteral("수동 PTZ 우선 · Pi 추적 상태 알림 확인")); m_client->sendPtzMove(id, pan, tilt); }
    });
    connect(m_ptzCommands, &PtzCommandController::stopRequested, this, [this](const QString &id) {
        if (m_dummy->isEnabled()) m_dummy->stopPtz(id); else m_client->sendPtzStop(id);
    });
    connect(m_ptzCommands, &PtzCommandController::centerRequested, this, [this](const QString &id) {
        if (m_dummy->isEnabled()) m_dummy->centerPtz(id);
        else { m_tracking->cancelCommand(QStringLiteral("중앙 복귀 · Pi 추적 상태 알림 확인")); m_client->sendPtzCenter(id); }
    });
    connect(m_tracking, &TrackingPanel::trackingChanged, this, [this](bool enabled) {
        if (m_dummy->isEnabled()) m_dummy->setTrackingEnabled(m_current.id, enabled);
        else if (m_client->isConnected() && m_current.supportsTracking && (!enabled || m_current.online)) {
            requestStop(); m_tracking->updateMetadata(m_metadata); m_tracking->beginCommand(enabled);
            m_client->sendTracking(m_current.id,enabled);
        }
    });
    connect(m_view, &CameraViewWidget::startRequested, this, [this](const QString &method) {
        if (m_dummy->isEnabled() && m_current.online) {
            m_dummy->requestCameraStatus(m_current.id); addLog(QStringLiteral("STREAM"), method + QStringLiteral(" dummy preview; no real stream"));
        } else if (!m_dummy->isEnabled() && (method == QStringLiteral("RTSP TCP") || method == QStringLiteral("RTSP UDP"))) {
            m_streamStatus->setState(QStringLiteral("Connecting (VMS)"), StatusIndicatorWidget::State::Inactive);
            m_liveWanted = true; m_streamRequestPending = true; m_client->requestStream(m_current.id, m_view->selectedTransport());
        } else addLog(QStringLiteral("STREAM"), QStringLiteral("Select RTSP TCP or RTSP UDP"));
    });
    connect(m_view, &CameraViewWidget::stopRequested, this, [this] {
        m_liveWanted = false; m_streamRequestPending = false; m_client->cancelStreamRequest(); m_view->stopStream();
        m_view->setLive(false); m_view->setFrame(QImage()); m_view->clearDetection();
        m_streamStatus->setState(QStringLiteral("Stopped"), StatusIndicatorWidget::State::Inactive);
        addLog(QStringLiteral("STREAM"), QStringLiteral("Preview stopped"));
    });
    connect(m_view, &CameraViewWidget::streamMethodChanged, this, [this](const QString &method) {
        addLog(QStringLiteral("STREAM"), QStringLiteral("Selected %1").arg(method));
    });
    connect(m_device, &DeviceInfoWidget::refreshRequested, this, [this](const QString &id) {
        if (m_dummy->isEnabled()) m_dummy->requestCameraStatus(id); else m_client->requestCameraStatus(id);
    });
    connect(m_device, &DeviceInfoWidget::discoverRequested, this, [this] {
        if (m_dummy->isEnabled() || !m_client->isConnected()) {
            m_device->setDiscoveryState(false, QStringLiteral("Connect to VMS first"));
            addLog(QStringLiteral("ONVIF"), QStringLiteral("Connect to VMS first; no search request sent")); return;
        }
        m_device->setDiscoveryState(true, QStringLiteral("Searching cameras through VMS..."));
        addLog(QStringLiteral("ONVIF"), QStringLiteral("Searching cameras through VMS..."));
        m_discoveryRequestId = m_client->discoverCameras();
        if (m_discoveryRequestId.isEmpty()) m_device->setDiscoveryState(false, QStringLiteral("Search request could not be sent"));
    });
    connect(m_device, &DeviceInfoWidget::listRequested, this, [this] {
        if (m_dummy->isEnabled()) m_dummy->requestCameraList(); else m_client->requestCameraList();
    });
    connect(m_device, &DeviceInfoWidget::registerRequested, this, [this](const QString &url, const QString &username, const QString &password, const QString &profile) {
        if (m_dummy->isEnabled() || !m_client->isConnected()) { addLog(QStringLiteral("ONVIF"), QStringLiteral("Connect to VMS first")); return; }
        addLog(QStringLiteral("ONVIF"), QStringLiteral("Registering camera through VMS"));
        m_client->registerCamera(url, username, password, profile);
    });
    connect(m_connection, &ConnectionStatusWidget::connectRequested, this, [this](const QString &host, quint16 port) {
        requestStop();
        m_dummyToggle->setChecked(false);
        m_playback->setLocalVms(host.compare(QStringLiteral("localhost"), Qt::CaseInsensitive) == 0 || QHostAddress(host).isLoopback());
        m_client->connectToServer(host, port);
    });
    connect(m_connection, &ConnectionStatusWidget::disconnectRequested, this, [this] {
        requestStop();
        m_dummyToggle->setChecked(false); m_client->disconnectFromServer();
    });
    connect(m_dummyToggle, &QCheckBox::toggled, this, [this](bool enabled) {
        m_device->setDiscoveryState(false, QStringLiteral("Connect to VMS first"));
        m_events->setRealMode(!enabled); m_events->setSearchAvailable(enabled); m_events->setEnabled(enabled); m_tabs->setTabEnabled(0, enabled);
        m_chat->setContext({},false); m_client->cancelInteractiveRequests(); m_playbackRequestId.clear(); m_metadata={}; m_metadataSourceTimes.clear(); m_metadataExpiry.stop();
        requestStop(); m_keyboard->setEnabled(false); m_playback->clear(); m_liveWanted = false; m_streamRequestPending = false; m_view->stopStream(); m_client->disconnectFromServer(); m_dummy->setEnabled(enabled);
        m_playback->setSearchAvailable(enabled);
    });
    addLog(QStringLiteral("SYSTEM"), QStringLiteral("Mini VMS UI — WebSocket camera status API"));
    m_dummyToggle->setChecked(dummyMode); if (!dummyMode) { m_dummy->setEnabled(false); applyCameraStatus(CameraInfo{}); }
}
MainWindow::~MainWindow() {
    // QWidget teardown can emit focus/window-deactivate signals after this derived
    // destructor. Stop input and detach child-to-controller connections first.
    requestStop(); m_ptzCommands->setTarget(QString(), false); m_keyboard->setEnabled(false);
    m_view->stopStream(); m_client->disconnectFromServer();
    for (auto *child : findChildren<QObject *>()) QObject::disconnect(child, nullptr, this, nullptr);
}
QWidget *MainWindow::makePanel(const QString &title, QWidget *content) {
    auto *panel = new QWidget(this); panel->setObjectName(QStringLiteral("panel"));
    auto *layout = new QVBoxLayout(panel); layout->setContentsMargins(10, 8, 10, 8);
    auto *heading = new QLabel(title, panel); heading->setObjectName(QStringLiteral("panelHeading"));
    layout->addWidget(heading); layout->addWidget(content, 1); return panel;
}
// 선택 변경 시 이전 장치의 표시와 입력을 정리해 cameraId 간 정보가 섞이지 않게 한다.
void MainWindow::selectCamera(const QString &id) {
    m_autoRecordingRequestId.clear(); m_autoRecording->setEnabled(false);
    m_diagnosticsRequestId.clear(); m_diagnostics->setText(QStringLiteral("선택한 카메라의 탐지·저장 상태 조회 대기"));
    if (!m_dummy->isEnabled() && id!=m_current.id && !m_current.id.isEmpty() && m_client->ownsTracking(m_current.id)) m_client->sendPtzStop(m_current.id);
    m_tracking->cancelCommand(QString());
    m_metadata={}; m_metadataSourceTimes.clear(); m_metadataExpiry.stop(); m_client->cancelInteractiveRequests(); m_playbackRequestId.clear();
    m_events->resetResults(); m_chat->setContext(id,false);
    const bool registered = !id.isEmpty() && id == m_registerSelection;
    if (registered) m_registerSelection.clear();
    m_liveWanted = registered; m_streamRequestPending = false;
    m_client->cancelStreamRequest(); m_view->stopStream();
    requestStop(); m_keyboard->stop(); m_current = CameraInfo{}; m_current.id = id;
    m_view->setFrame(QImage()); m_view->clearDetection(); m_view->setLive(false);
    m_streamStatus->setState(QStringLiteral("Idle"), StatusIndicatorWidget::State::Inactive);
    m_tracking->updateTrackingInfo(TrackingInfo{}); m_events->setEvents({}); m_playback->clear(); m_playback->setRecordings({});
    m_events->setSelectedCamera(id); m_playback->setSelectedCamera(id);
    if (id.isEmpty()) applyCameraStatus(CameraInfo{});
    else for (const auto &camera : m_cameras) if (camera.id == id) { applyCameraStatus(camera); break; }
    if (!id.isEmpty()) {
        if (m_dummy->isEnabled()) m_dummy->requestCameraStatus(id);
        else {
            m_client->requestCameraStatus(id);
            if (registered && !m_streamRequestPending) { m_streamRequestPending = true; m_client->requestStream(id, m_view->selectedTransport()); }
        }
    }
}
void MainWindow::applyCameraStatus(const CameraInfo &camera) {
    for (auto &stored : m_cameras) if (stored.id == camera.id) stored = camera;
    m_list->updateCamera(camera); if (camera.id != m_current.id) return;
    if (!m_dummy->isEnabled() && !camera.recordingError.isEmpty() && camera.recordingError != m_current.recordingError)
        addLog(QStringLiteral("REC"), camera.recordingError);
    m_current = camera; m_device->setCamera(camera);
    m_view->setRecordingControls(!m_dummy->isEnabled() && m_client->isConnected() && camera.supportsRecordings && camera.online && !camera.recordingRequested,
        !m_dummy->isEnabled() && m_client->isConnected() && camera.supportsRecordings && camera.recordingRequested);
    m_view->setCameraName(camera.id.isEmpty() ? QStringLiteral("Select a camera") : camera.id + QStringLiteral(" / ") + camera.name);
    m_view->setOnline(camera.online); m_view->setRecording(camera.recording);
    m_cameraStatus->setName(camera.id.isEmpty() ? QStringLiteral("Camera") : camera.id);
    m_cameraStatus->setState(camera.online ? QStringLiteral("Online") : camera.status,
        camera.online ? StatusIndicatorWidget::State::Active : StatusIndicatorWidget::State::Inactive);
    const QString recText = camera.recording ? (m_dummy->isEnabled() ? QStringLiteral("Active (DUMMY)") : QStringLiteral("Active"))
        : camera.recordingState == QStringLiteral("STOPPED") || m_dummy->isEnabled() ? QStringLiteral("Inactive") : camera.recordingState;
    m_recStatus->setState(recText, camera.recordingState == QStringLiteral("ERROR") && !m_dummy->isEnabled() ? StatusIndicatorWidget::State::Error
        : camera.recording ? StatusIndicatorWidget::State::Active : StatusIndicatorWidget::State::Inactive);
    if (!m_dummy->isEnabled()) {
        const bool live = camera.online && m_view->isLive();
        const QString streamText = live ? QStringLiteral("Live (Camera)") : camera.online
            ? m_liveWanted ? QStringLiteral("Connecting (Camera)") : QStringLiteral("Idle") : camera.rtspStatus;
        m_streamStatus->setState(streamText, live ? StatusIndicatorWidget::State::Active
            : camera.status == QStringLiteral("ERROR") ? StatusIndicatorWidget::State::Error : StatusIndicatorWidget::State::Inactive);
        if (!camera.online) m_view->stopStream();
        else if (m_liveWanted && !m_streamRequestPending && !m_view->isPlaying()) {
            m_streamRequestPending = true; m_client->requestStream(camera.id, m_view->selectedTransport());
        }
    }
    const bool enabled = camera.online && (m_dummy->isEnabled() || (m_client->isConnected() && camera.supportsPtz));
    if (!m_dummy->isEnabled()) {
        m_tracking->updateMetadata(m_metadata);
    }
    if (!enabled) requestStop();
    m_ptzCommands->setTarget(camera.id, enabled);
    m_ptz->setEnabled(enabled); m_ptz->setCenterEnabled(m_dummy->isEnabled() || camera.supportsPtzCenter);
    m_ptz->setToolTip(enabled ? QStringLiteral("Hold to move via VMS; release to stop") : QStringLiteral("Register an online camera with VMS PTZ capability"));
    m_ptzNotice->setText(camera.supportsPtz ? QStringLiteral("PTZ via VMS · Manual move cancels tracking") : QStringLiteral("PTZ requires VMS capability"));
    m_tracking->setEnabled((m_dummy->isEnabled() && camera.online) || (!m_dummy->isEnabled() && (camera.supportsTracking || camera.supportsEvents || !m_metadata.isEmpty())));
    m_tracking->setControlAvailable((camera.online || m_metadata.value(QStringLiteral("tracking")).toBool()) && (m_dummy->isEnabled() || (m_client->isConnected() && camera.supportsTracking)));
    m_keyboard->setEnabled(enabled);
    m_chat->setContext(camera.id,!m_dummy->isEnabled() && m_client->isConnected() && camera.supportsChatSearch);
    if (!m_dummy->isEnabled() && m_client->isConnected()) { m_events->setRealMode(true); m_events->setSearchAvailable(true); m_events->setEnabled(true); m_tabs->setTabEnabled(0,true); }
}
void MainWindow::requestMetadataPlayback(const QJsonObject &record) {
    if (m_dummy->isEnabled()) return;
    m_playbackRequestId=m_client->requestEventPlayback(record);
    if (m_playbackRequestId.isEmpty()) addLog(QStringLiteral("PLAYBACK"),QStringLiteral("유효한 검색 기록을 선택하고 VMS에 연결하세요."));
}
void MainWindow::openMetadataPlayback(const QJsonObject &data) {
    if (m_dummy->isEnabled()) return;
    m_playback->openPlaybackResult(data); m_tabs->setCurrentWidget(m_playback);
}
void MainWindow::applyMetadata(const QString &id,const QJsonObject &data) {
    if (m_dummy->isEnabled() || id!=m_current.id) return;
    const auto topic=data.value(QStringLiteral("topic")).toString();
    const auto source=data.value(QStringLiteral("sourceTimeMs"));
    if (source.isDouble()) {
        if (m_metadataSourceTimes.contains(topic) && source.toInteger()<m_metadataSourceTimes[topic]) return;
        m_metadataSourceTimes[topic]=source.toInteger();
    }
    // 다른 topic의 null은 이미 받은 각도/추적 상태를 덮지 않는다. 새 detection의 누락 bbox는 이전 상자를 재사용하지 않는다.
    if (topic==QStringLiteral("Analytics/PersonDetection")) {
        for (const char *key:{"detected","confidence","bboxX","bboxY","bboxWidth","bboxHeight","imageWidth","imageHeight","errorX","errorY","frameId","streamEpoch","framePtsNs","captureTimeMs","analysisTimeMs"})
            m_metadata.insert(QString::fromLatin1(key),data.value(QString::fromLatin1(key)));
    }
    for (const char *key:{"available","tracking","autoAllowed","targetState","panCommandAngle","tiltCommandAngle","ptzMode","moving","fault"}) {
        const auto value=data.value(QString::fromLatin1(key)); if (!value.isNull() && !value.isUndefined()) m_metadata.insert(QString::fromLatin1(key),value);
    }
    if (m_metadata.value(QStringLiteral("available")).isBool() && !m_metadata.value(QStringLiteral("available")).toBool()) {
        m_metadata[QStringLiteral("detected")]=false; m_metadataExpiry.stop(); m_view->clearDetection();
    } else if (topic==QStringLiteral("Analytics/PersonDetection")) {
        const auto number=[&](const char *key){return data.value(QString::fromLatin1(key)).isDouble() && std::isfinite(data.value(QString::fromLatin1(key)).toDouble());};
        const int x=data.value(QStringLiteral("bboxX")).toInt(-1), y=data.value(QStringLiteral("bboxY")).toInt(-1), w=data.value(QStringLiteral("bboxWidth")).toInt(), h=data.value(QStringLiteral("bboxHeight")).toInt();
        const int iw=data.value(QStringLiteral("imageWidth")).toInt(), ih=data.value(QStringLiteral("imageHeight")).toInt();
        if (data.value(QStringLiteral("detected")).isBool() && data.value(QStringLiteral("detected")).toBool()
            && number("bboxX") && number("bboxY") && number("bboxWidth") && number("bboxHeight") && number("imageWidth") && number("imageHeight")
            && iw>0 && iw<=8192 && ih>0 && ih<=8192 && x>=0 && y>=0 && w>0 && h>0 && x<=iw && w<=iw-x && y<=ih && h<=ih-y) {
            DetectionInfo detection; detection.detected=true; detection.label=QStringLiteral("Person"); detection.boundingBox=QRect(x,y,w,h);
            detection.imageSize=QSize(iw,ih); detection.objectCenter=QPoint(x+w/2,y+h/2);
            const auto confidence=data.value(QStringLiteral("confidence")); detection.hasConfidence=confidence.isDouble() && std::isfinite(confidence.toDouble()) && confidence.toDouble()>=0 && confidence.toDouble()<=1;
            if (detection.hasConfidence) detection.confidence=static_cast<float>(confidence.toDouble());
            m_view->setDetection(detection); m_view->setToolTip(QStringLiteral("실시간 metadata overlay · 영상 프레임과 정확한 동기화는 미검증")); m_metadataExpiry.start();
        } else { m_metadataExpiry.stop(); m_view->clearDetection(); }
    }
    m_tracking->setEnabled(true); m_tracking->setControlAvailable(m_client->isConnected() && m_current.supportsTracking && (m_current.online || m_metadata.value(QStringLiteral("tracking")).toBool())); m_tracking->updateMetadata(m_metadata);
}
void MainWindow::requestStop() {
    const QSignalBlocker blocker(m_keyboard); m_keyboard->stop(); m_ptz->resetPressedState(); m_ptzCommands->cancel();
}
void MainWindow::requestCenter() {
    if (!m_dummy->isEnabled() && !m_current.supportsPtzCenter) {
        addLog(QStringLiteral("PTZ ERROR"), QStringLiteral("CENTER is unavailable for this camera")); return;
    }
    const QSignalBlocker blocker(m_keyboard); m_keyboard->stop(); m_ptz->resetPressedState(); m_ptzCommands->center();
}
void MainWindow::addLog(const QString &source, const QString &message) { m_log->addLog(source, message); }
