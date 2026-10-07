#include "VmsWebSocketTests.h"
#include "network/VmsClient.h"
#include "app/MainWindow.h"
#include "camera/CameraListWidget.h"
#include "camera/CameraViewWidget.h"
#include "device/DeviceInfoWidget.h"
#include "ui/ConnectionStatusWidget.h"
#include "ui/PTZControlWidget.h"
#include <QCheckBox>
#include <QComboBox>
#include <QPushButton>
#include <QSpinBox>
#include <QFile>
#include <QFileInfo>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QPointer>
#include <QProcess>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTemporaryDir>
#include <QTabWidget>
#include <QWebSocket>
#include <QWebSocketServer>
#include <QtTest>
namespace {
QJsonObject camera(const QString &status = QStringLiteral("ONLINE")) {
    return {{"id", "CAM01"}, {"name", "Pi PTZ"}, {"status", status}, {"recording", false},
        {"onvifStatus", "NOT_IMPLEMENTED"}, {"webRtcStatus", "NOT_IMPLEMENTED"},
        {"codec", "H264"}, {"width", 1280}, {"height", 720}, {"fps", 30}, {"packets", 42}, {"bytes", 1024},
        {"timeBase", QJsonObject{{"num", 1}, {"den", 90000}}}};
}
QString text(const QJsonObject &object) { return QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Compact)); }
class MockVms : public QObject {
public:
    QWebSocketServer server{QStringLiteral("VMS fixture"), QWebSocketServer::NonSecureMode};
    QList<QPointer<QWebSocket>> sockets;
    bool silent = false;
    bool ptz = false;
    QList<QJsonObject> ptzRequests;
    MockVms() {
        if (!server.listen(QHostAddress::LocalHost, 0)) qFatal("Cannot start test WebSocket server");
        connect(&server, &QWebSocketServer::newConnection, this, [this] {
            auto *socket = server.nextPendingConnection(); socket->setParent(this); sockets.append(socket);
            connect(socket, &QWebSocket::textMessageReceived, this, [this, socket](const QString &message) {
                if (silent) return;
                const auto request = QJsonDocument::fromJson(message.toUtf8()).object();
                const auto command = request.value("command").toString();
                QJsonObject reply{{"version", 1}, {"type", "response"}, {"requestId", request.value("requestId")}, {"ok", true}};
                if (command == QStringLiteral("GET_CAMERA_LIST")) reply.insert("data", QJsonObject{{"cameras", QJsonArray{camera()}}});
                else if (command == QStringLiteral("GET_CAMERA_STATUS")) reply.insert("data", QJsonObject{{"camera", camera()}});
                else if (command == QStringLiteral("DISCOVER_CAMERAS")) {
                    reply.insert("data", QJsonObject{{"devices", QJsonArray{QJsonObject{{"name", "Test camera"}, {"address", "127.0.0.1"},
                        {"deviceServiceUrl", "http://127.0.0.1/onvif/device_service"}, {"source", "ws-discovery"}}}}});
                    QTimer::singleShot(150, socket, [socket, reply] { socket->sendTextMessage(text(reply)); }); return;
                }
                else if (ptz && command.startsWith(QStringLiteral("PTZ_"))) {
                    ptzRequests.append(request);
                    reply.insert("data", QJsonObject{{"cameraId", request.value("cameraId")}, {"command", command}, {"phase", "ACCEPTED"}, {"motorArrivalConfirmed", false}});
                }
                else { reply.insert("ok", false); reply.insert("error", QJsonObject{{"code", "NOT_SUPPORTED"}, {"message", "Not implemented"}}); }
                socket->sendTextMessage(text(reply));
            });
        });
    }
    void status(const QString &value) {
        const auto notification = text({{"version", 1}, {"type", "notification"}, {"event", "CAMERA_STATUS"},
            {"cameraId", "CAM01"}, {"data", QJsonObject{{"camera", camera(value)}}}});
        for (auto socket : sockets) if (socket) socket->sendTextMessage(notification);
    }
};
}
void VmsWebSocketTests::dummyConnectAndDiscoveryButtons() {
    MockVms server; MainWindow window; window.show();
    auto *connection = window.findChild<ConnectionStatusWidget *>();
    auto *client = window.findChild<VmsClient *>();
    auto *discover = window.findChild<QPushButton *>(QStringLiteral("discoverOnvifCameras"));
    auto *devices = window.findChild<QComboBox *>(QStringLiteral("onvifDevices"));
    auto *dummy = window.findChild<QCheckBox *>();
    QPushButton *connectButton = nullptr;
    for (auto *button : connection->findChildren<QPushButton *>()) if (button->text() == QStringLiteral("Connect")) connectButton = button;
    QVERIFY(connectButton); QVERIFY(discover); QVERIFY(devices); QVERIFY(dummy->isChecked());
    QVERIFY(connectButton->isEnabled());
    QTest::mouseClick(discover, Qt::LeftButton);
    QCOMPARE(devices->placeholderText(), QStringLiteral("Connect to VMS first")); QVERIFY(!client->isConnected());
    connection->findChild<QSpinBox *>()->setValue(server.server.serverPort());
    QTest::mouseClick(connectButton, Qt::LeftButton);
    QTRY_VERIFY(client->isConnected()); QVERIFY(!dummy->isChecked());
    QSignalSpy found(client, &VmsClient::discoveryReceived);
    QTest::mouseClick(discover, Qt::LeftButton);
    QVERIFY(!discover->isEnabled()); QCOMPARE(discover->text(), QStringLiteral("SEARCHING..."));
    QTRY_COMPARE(found.count(), 1);
    QVERIFY(discover->isEnabled()); QCOMPARE(devices->count(), 1);
    QCOMPARE(devices->currentData().toString(), QStringLiteral("http://127.0.0.1/onvif/device_service"));
}
void VmsWebSocketTests::ptzAcceptanceAndPiConfirmationAreSeparate() {
    MockVms server; server.ptz = true; VmsClient client;
    QStringList sources; connect(&client, &VmsClient::message, this, [&](const QString &source, const QString &) { sources.append(source); });
    client.connectToServer(QStringLiteral("127.0.0.1"), server.server.serverPort()); QTRY_VERIFY(client.isConnected());
    const auto moveId = client.sendPtzMove(QStringLiteral("CAM01"), 0.5F, 0.0F);
    QVERIFY(!moveId.isEmpty()); QTRY_VERIFY(sources.contains(QStringLiteral("PTZ VMS")));
    QVERIFY(sources.contains(QStringLiteral("PTZ TX"))); QVERIFY(!sources.contains(QStringLiteral("PTZ PI")));
    QCOMPARE(server.ptzRequests.first().value("cameraId").toString(), QStringLiteral("CAM01"));
    QCOMPARE(server.ptzRequests.first().value("panVelocity").toDouble(), 0.5);
    const auto acknowledge = [&](const QString &id, const QString &command) {
        server.sockets.first()->sendTextMessage(text({{"version", 1}, {"type", "notification"}, {"event", "PTZ_RESULT"}, {"requestId", id}, {"ok", true},
            {"data", QJsonObject{{"cameraId", "CAM01"}, {"command", command}, {"phase", "PI_ACKNOWLEDGED"}, {"motorArrivalConfirmed", false}}}}));
    };
    acknowledge(moveId, QStringLiteral("PTZ_MOVE")); QTRY_VERIFY(sources.contains(QStringLiteral("PTZ PI")));
    const auto stopId = client.sendPtzStop(QStringLiteral("CAM01")); QTRY_COMPARE(server.ptzRequests.size(), 2);
    client.sendPtzCenter(QStringLiteral("CAM01")); QCOMPARE(server.ptzRequests.size(), 2);
    acknowledge(stopId, QStringLiteral("PTZ_STOP")); QTRY_COMPARE(server.ptzRequests.size(), 3);
    QCOMPARE(server.ptzRequests.last().value("command").toString(), QStringLiteral("PTZ_CENTER"));
    client.disconnectFromServer();
}
void VmsWebSocketTests::connectionQueriesNotificationsAndErrors() {
    MockVms server; VmsClient client;
    QList<CameraInfo> cameras; CameraInfo status;
    connect(&client, &VmsClient::cameraListReceived, this, [&](const QList<CameraInfo> &items) { cameras = items; });
    connect(&client, &VmsClient::cameraStatusChanged, this, [&](const CameraInfo &item) { status = item; });
    QSignalSpy failures(&client, &VmsClient::requestFailed);
    client.connectToServer(QStringLiteral("127.0.0.1"), server.server.serverPort());
    QTRY_VERIFY(client.isConnected()); QTRY_COMPARE(cameras.size(), 1);
    QCOMPARE(cameras[0].id, QStringLiteral("CAM01")); QCOMPARE(cameras[0].resolution, QSize(1280, 720));
    QCOMPARE(cameras[0].packets, quint64(42)); QCOMPARE(cameras[0].timeBase, QStringLiteral("1/90000"));
    client.requestCameraStatus(QStringLiteral("CAM01")); QTRY_VERIFY(status.online);
    server.status(QStringLiteral("ERROR")); QTRY_COMPARE(status.status, QStringLiteral("ERROR")); QVERIFY(!status.online);
    client.request(QStringLiteral("START_LIVE"), QStringLiteral("CAM01")); QTRY_COMPARE(failures.count(), 1);
    QCOMPARE(failures[0][1].toString(), QStringLiteral("NOT_SUPPORTED"));
    server.sockets.first()->sendTextMessage(QStringLiteral("not-json"));
    client.requestCameraList(); QTRY_VERIFY(client.isConnected());
    client.disconnectFromServer(); QVERIFY(!client.isConnected());
}
void VmsWebSocketTests::reconnectAndRequestTimeout() {
    MockVms server; VmsClient client;
    QSignalSpy failures(&client, &VmsClient::requestFailed);
    client.connectToServer(QStringLiteral("127.0.0.1"), server.server.serverPort()); QTRY_VERIFY(client.isConnected());
    server.silent = true;
    client.requestCameraStatus(QStringLiteral("CAM01"));
    QTRY_VERIFY_WITH_TIMEOUT(failures.count() >= 1, 7000);
    QCOMPARE(failures.last()[1].toString(), QStringLiteral("TIMEOUT"));
    server.silent = false;
    client.disconnectFromServer(); QVERIFY(!client.isConnected());
    client.connectToServer(QStringLiteral("127.0.0.1"), server.server.serverPort()); QTRY_VERIFY(client.isConnected());
    for (auto socket : server.sockets) if (socket) socket->close();
    QTRY_VERIFY(!client.isConnected());
}
void VmsWebSocketTests::uiClearsRealStateWhenDisconnectedOrDummyEnabled() {
    MockVms server; MainWindow window(nullptr, false);
    auto *connection = window.findChild<ConnectionStatusWidget *>();
    auto *client = window.findChild<VmsClient *>();
    auto *list = window.findChild<CameraListWidget *>();
    auto *dummy = window.findChild<QCheckBox *>();
    auto *ptz = window.findChild<PTZControlWidget *>();
    emit connection->connectRequested(QStringLiteral("127.0.0.1"), server.server.serverPort());
    QTRY_VERIFY(client->isConnected()); QTRY_COMPARE(list->selectedCameraId(), QStringLiteral("CAM01"));
    QVERIFY(!ptz->isEnabled()); QVERIFY(!dummy->isChecked());
    auto *device = window.findChild<DeviceInfoWidget *>();
    auto hasText = [&](const QString &value) {
        for (auto *label : device->findChildren<QLabel *>()) if (label->text() == value) return true;
        return false;
    };
    QTRY_VERIFY(hasText(QStringLiteral("H264"))); QVERIFY(hasText(QStringLiteral("42")));
    emit connection->disconnectRequested(); QTRY_VERIFY(list->selectedCameraId().isEmpty()); QVERIFY(!ptz->isEnabled());
    emit connection->connectRequested(QStringLiteral("127.0.0.1"), server.server.serverPort());
    QTRY_VERIFY(client->isConnected()); QTRY_COMPARE(list->selectedCameraId(), QStringLiteral("CAM01"));
    dummy->setChecked(true); QVERIFY(!client->isConnected()); QVERIFY(ptz->isEnabled());
    server.status(QStringLiteral("ERROR")); QCoreApplication::processEvents();
    QVERIFY(ptz->isEnabled()); // Late real notifications cannot replace dummy state.
}
void VmsWebSocketTests::realVmsServer() {
    const auto executable = qEnvironmentVariable("VMS_SERVER_TEST_EXECUTABLE");
    if (executable.isEmpty()) QSKIP("Set VMS_SERVER_TEST_EXECUTABLE for real C++ server integration");
    QTemporaryDir directory; QVERIFY(directory.isValid());
    QTcpServer reserve; QVERIFY(reserve.listen(QHostAddress::LocalHost, 0));
    const auto port = reserve.serverPort();
    QTcpServer relayReserve; QVERIFY(relayReserve.listen(QHostAddress::LocalHost, 0));
    const auto relayPort = relayReserve.serverPort(); reserve.close(); relayReserve.close();
    QByteArray config("camera_id=CAM01\nrtsp_url=rtsp://127.0.0.1:9/cam\nconnect_timeout_ms=500\nread_timeout_ms=500\nreconnect_delay_ms=500\n");
    const auto liveConfig = qEnvironmentVariable("VMS_TEST_CAMERA_CONFIG");
    if (!liveConfig.isEmpty()) {
        QFile input(liveConfig); QVERIFY(input.open(QIODevice::ReadOnly)); config = input.readAll();
        // Use a unique API port even if the user's camera config has explicit API options.
        const auto lines = config.split('\n'); config.clear();
        for (const auto &line : lines) if (!line.startsWith("client_port=") && !line.startsWith("client_bind=") && !line.startsWith("rtsp_relay_port=") && !line.startsWith("recording_root=") && !line.startsWith("recording_database=")) config += line + '\n';
    }
    config += "client_bind=127.0.0.1\nclient_port=" + QByteArray::number(port) + '\n';
    config += "rtsp_relay_port=" + QByteArray::number(relayPort) + '\n';
    config += "recording_root=" + directory.filePath(QStringLiteral("recordings")).toUtf8() + '\n';
    config += "recording_database=" + directory.filePath(QStringLiteral("vms.db")).toUtf8() + '\n';
    const auto configPath = directory.filePath(QStringLiteral("test.conf"));
    QFile output(configPath); QVERIFY(output.open(QIODevice::WriteOnly)); output.write(config); output.close();
    QProcess process;
    struct Guard { QProcess &p; ~Guard() { if (p.state() != QProcess::NotRunning) { p.terminate(); if (!p.waitForFinished(4000)) { p.kill(); p.waitForFinished(); } } } } guard{process};
    process.setProcessChannelMode(QProcess::MergedChannels); process.start(executable, {QStringLiteral("--config"), configPath});
    QVERIFY(process.waitForStarted());
    QByteArray logs;
    QElapsedTimer deadline; deadline.start();
    while (deadline.elapsed() < 5000 && !logs.contains("Listening ws://")) { process.waitForReadyRead(200); logs += process.readAll(); }
    QVERIFY2(logs.contains("Listening ws://"), "VMS did not start its WebSocket listener");
    CameraInfo status;
    MainWindow window(nullptr, false);
    auto *client = window.findChild<VmsClient *>();
    auto *connection = window.findChild<ConnectionStatusWidget *>();
    auto *list = window.findChild<CameraListWidget *>();
    emit connection->connectRequested(QStringLiteral("127.0.0.1"), port);
    QTRY_VERIFY(client->isConnected()); QTRY_COMPARE(list->selectedCameraId(), QStringLiteral("CAM01"));
    if (!liveConfig.isEmpty()) {
        window.show(); window.findChild<QTabWidget *>()->setCurrentIndex(2);
        auto *device = window.findChild<DeviceInfoWidget *>();
        auto *view = window.findChild<CameraViewWidget *>();
        QSignalSpy discovery(client, &VmsClient::discoveryReceived);
        QSignalSpy registration(client, &VmsClient::cameraRegistered);
        QSignalSpy frames(view, &CameraViewWidget::videoFrameReceived);
        QSignalSpy streamUri(client, &VmsClient::streamUriReady);
        emit device->discoverRequested();
        QTRY_VERIFY_WITH_TIMEOUT(discovery.count() > 0, 12000);
        emit device->registerRequested(QStringLiteral("http://192.168.0.92:8080/onvif/device_service"), {}, {}, QStringLiteral("main"));
        QTRY_VERIFY_WITH_TIMEOUT(registration.count() > 0, 25000);
        QTRY_VERIFY_WITH_TIMEOUT(streamUri.count() > 0, 25000);
        const auto uri = streamUri.last()[1].toUrl();
        QCOMPARE(uri.host(), QStringLiteral("127.0.0.1")); QVERIFY(uri.userInfo().isEmpty()); QVERIFY(uri.port() != 8554);
        QTRY_VERIFY_WITH_TIMEOUT(frames.count() >= 3, 25000);
        QVERIFY(view->isLive());
        const int uriCount = streamUri.count();
        view->findChild<QComboBox *>(QStringLiteral("rtspTransport"))->setCurrentIndex(1);
        emit view->startRequested(QStringLiteral("RTSP UDP"));
        QTRY_VERIFY_WITH_TIMEOUT(streamUri.count() > uriCount, 15000);
        QVERIFY(streamUri.last()[1].toUrl().query().contains(QStringLiteral("transport=udp")));
        frames.clear(); QTRY_VERIFY_WITH_TIMEOUT(frames.count() >= 3, 15000);
        QVERIFY(view->isLive());
        qInfo() << "Qt UDP live frames:" << frames.count() << frames.last()[0].toSize();
        const auto preview = qEnvironmentVariable("VMS_PREVIEW_PATH");
        if (!preview.isEmpty()) QVERIFY(window.grab().save(preview));
        qInfo() << "ONVIF registration -> managed RTSP -> Qt decoded frames:" << frames.count() << frames.last()[0].toSize();
        connect(client, &VmsClient::cameraStatusChanged, this, [&](const CameraInfo &value) { status = value; });
        client->requestCameraStatus(QStringLiteral("CAM01"));
        QTRY_VERIFY_WITH_TIMEOUT(status.online && status.packets > 0, 15000);
        QCOMPARE(status.codec, QStringLiteral("H264")); QCOMPARE(status.resolution, QSize(1280, 720));
        const auto firstPackets = status.packets;
        QTRY_VERIFY_WITH_TIMEOUT(status.packets > firstPackets, 5000);
        emit view->startRecordingRequested();
        QTRY_VERIFY_WITH_TIMEOUT(status.recording, 10000);
        QTest::qWait(1500);
        emit view->stopRecordingRequested();
        QTRY_VERIFY_WITH_TIMEOUT(!status.recording && !status.recordingRequested, 10000);
        QSignalSpy recordingList(client, &VmsClient::recordingListReceived);
        client->requestRecordings(QStringLiteral("CAM01"), QDateTime::currentDateTime().addSecs(-60), QDateTime::currentDateTime().addSecs(60));
        QTRY_VERIFY(recordingList.count() > 0);
        const auto entries = qvariant_cast<QList<RecordingInfo>>(recordingList.last()[1]);
        QVERIFY(!entries.isEmpty()); QVERIFY(QFile::exists(entries[0].filePath));
        qInfo() << "Qt REC START/STOP saved file:" << QFileInfo(entries[0].filePath).fileName();
        qInfo() << "Real Pi through VMS/QWebSocket:" << status.fps << "FPS, packets" << firstPackets << "->" << status.packets;
    }
    process.terminate(); QVERIFY(process.waitForFinished(4000)); QCOMPARE(process.exitCode(), 0);
    QTRY_VERIFY(!client->isConnected()); QTRY_VERIFY(list->selectedCameraId().isEmpty());
}
QTEST_MAIN(VmsWebSocketTests)
