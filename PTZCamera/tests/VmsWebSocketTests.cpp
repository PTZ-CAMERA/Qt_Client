#include "VmsWebSocketTests.h"
#include "network/VmsClient.h"
#include "app/MainWindow.h"
#include "camera/CameraListWidget.h"
#include "camera/CameraViewWidget.h"
#include "device/DeviceInfoWidget.h"
#include "ui/ConnectionStatusWidget.h"
#include "ui/PTZControlWidget.h"
#include "ui/TrackingPanel.h"
#include "camera/CameraWidget.h"
#include "events/EventSearchWidget.h"
#include "chat/ChatSearchWidget.h"
#include "playback/PlaybackWidget.h"
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
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QTableView>
#include <QMediaPlayer>
#include <QStandardPaths>
#include <algorithm>
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
    bool features = false;
    bool tracking = false;
    QList<QJsonObject> featureRequests;
    QJsonObject featureCamera() const {
        auto c=camera();
        if (features) c["capabilities"]=QJsonObject{{"events",true},{"chatSearch",true},{"recordings",true},{"ptz",false},{"tracking",false}};
        if (tracking) c["capabilities"]=QJsonObject{{"events",true},{"tracking",true},{"ptz",true},{"ptzCenter",true}};
        return c;
    }
    QJsonObject row(bool event=false) const {
        auto result=QJsonObject{{"id",1},{"sampleId",1},{"recordKind","detection_sample"},{"cameraId","CAM01"},{"type","DETECTION_SAMPLE"},
            {"searchTimeMs",QDateTime::currentMSecsSinceEpoch()},{"sourceTimeMs",QDateTime::currentMSecsSinceEpoch()},
            {"confidence",.92},{"panCommandAngle",QJsonValue::Null},{"tiltCommandAngle",QJsonValue::Null}};
        if (event) { result.remove("sampleId"); result["recordKind"]="state_event"; result["type"]="PERSON_DETECTED"; result["confidence"]=QJsonValue::Null; }
        return result;
    }
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
                if (command == QStringLiteral("GET_CAMERA_LIST")) reply.insert("data", QJsonObject{{"cameras", QJsonArray{featureCamera()}}});
                else if (command == QStringLiteral("GET_CAMERA_STATUS")) reply.insert("data", QJsonObject{{"camera", featureCamera()}});
                else if (features && (command==QStringLiteral("GET_EVENTS") || command==QStringLiteral("GET_DETECTIONS"))) {
                    featureRequests.append(request);
                    reply["data"]=QJsonObject{{command==QStringLiteral("GET_EVENTS") ? "events" : "detections",QJsonArray{row(command==QStringLiteral("GET_EVENTS"))}},{"nextCursor",QJsonObject{{"id",1},{"timeMs",QDateTime::currentMSecsSinceEpoch()}}}};
                }
                else if (features && command==QStringLiteral("GET_EVENT_PLAYBACK")) {
                    featureRequests.append(request); reply["data"]=QJsonObject{{"cameraId","CAM01"},{"playable",false},{"reason","NO_RECORDING_AT_TIME"}};
                }
                else if (features && command==QStringLiteral("CHAT_SEARCH")) {
                    featureRequests.append(request); auto record=row(); record["playback"]=QJsonObject{{"playable",false},{"reason","NO_RECORDING_AT_TIME"}};
                    reply["data"]=QJsonObject{{"action","search"},{"answer","조건에 맞는 기록 1개. 녹화는 없습니다."},{"results",QJsonArray{record}},{"nextCursor",QJsonValue::Null}};
                    QTimer::singleShot(150,socket,[socket,reply]{socket->sendTextMessage(text(reply));}); return;
                }
                else if (command == QStringLiteral("DISCOVER_CAMERAS")) {
                    reply.insert("data", QJsonObject{{"devices", QJsonArray{QJsonObject{{"name", "Test camera"}, {"address", "127.0.0.1"},
                        {"deviceServiceUrl", "http://127.0.0.1/onvif/device_service"}, {"source", "ws-discovery"}}}}});
                    QTimer::singleShot(150, socket, [socket, reply] { socket->sendTextMessage(text(reply)); }); return;
                }
                else if ((ptz && command.startsWith(QStringLiteral("PTZ_"))) || (tracking && command.startsWith(QStringLiteral("TRACKING_")))) {
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
void VmsWebSocketTests::trackingUsesConfirmedMetadataAndSurvivesIdleFocusChanges() {
    MockVms server; server.tracking=true; server.ptz=true;
    MainWindow window(nullptr,false); window.show();
    auto *client=window.findChild<VmsClient*>();
    emit window.findChild<ConnectionStatusWidget*>()->connectRequested(QStringLiteral("127.0.0.1"),server.server.serverPort());
    QTRY_VERIFY(client->isConnected()); QTRY_COMPARE(window.findChild<CameraListWidget*>()->selectedCameraId(),QStringLiteral("CAM01"));
    auto *toggle=window.findChild<QPushButton*>(QStringLiteral("trackingToggle"));
    auto *state=window.findChild<QLabel*>(QStringLiteral("trackingCommandStatus"));
    const auto metadata=[&](bool enabled){server.sockets.first()->sendTextMessage(text({{"version",1},{"type","notification"},{"event","CAMERA_METADATA"},{"cameraId","CAM01"},{"data",QJsonObject{{"cameraId","CAM01"},{"topic","Tracking/State"},{"tracking",enabled},{"receivedTimeMs",QDateTime::currentMSecsSinceEpoch()}}}}));};
    const auto result=[&](const QJsonObject &request,bool ok){
        server.sockets.first()->sendTextMessage(text({{"version",1},{"type","notification"},{"event","PTZ_RESULT"},{"requestId",request.value("requestId")},{"ok",ok},
            {"data",QJsonObject{{"cameraId","CAM01"},{"command",request.value("command")},{"phase",ok ? "PI_ACKNOWLEDGED" : "FAILED"}}},
            {"error",QJsonObject{{"code","ONVIF_FAULT"},{"message","fixture fault"}}}}));
    };
    metadata(false); QTRY_COMPARE(toggle->text(),QStringLiteral("OFF")); QTRY_VERIFY(toggle->isEnabled());
    QTest::mouseClick(toggle,Qt::LeftButton); QTRY_COMPARE(server.ptzRequests.size(),1);
    QCOMPARE(server.ptzRequests.last().value("command").toString(),QStringLiteral("TRACKING_ON"));
    QVERIFY(!toggle->isChecked()); QVERIFY(!toggle->isEnabled());
    result(server.ptzRequests.last(),true); QTRY_VERIFY(state->text().contains(QStringLiteral("상태 알림 대기"))); QVERIFY(!toggle->isChecked());
    metadata(true); QTRY_VERIFY(toggle->isChecked()); QTRY_VERIFY(toggle->isEnabled());
    const int confirmed=server.ptzRequests.size();
    auto *query=window.findChild<QLineEdit*>(QStringLiteral("onvifService")); query->setFocus();
    QTest::qWait(250); QCOMPARE(server.ptzRequests.size(),confirmed);
    QTest::mouseClick(toggle,Qt::LeftButton); QTRY_COMPARE(server.ptzRequests.size(),confirmed+1);
    QCOMPARE(server.ptzRequests.last().value("command").toString(),QStringLiteral("TRACKING_OFF"));
    QVERIFY(toggle->isChecked()); result(server.ptzRequests.last(),false);
    QTRY_VERIFY(state->text().contains(QStringLiteral("실패"))); QVERIFY(toggle->isChecked()); QTRY_VERIFY(toggle->isEnabled());
    metadata(false); QTRY_VERIFY(!toggle->isChecked()); QCOMPARE(server.ptzRequests.size(),confirmed+1);
    server.sockets.first()->close(); QTRY_VERIFY(!client->isConnected()); QTRY_VERIFY(!toggle->isEnabled());
}
void VmsWebSocketTests::metadataSearchChatAndScopeIsolation() {
    MockVms server; server.features=true; MainWindow window(nullptr,false); window.show();
    auto *client=window.findChild<VmsClient*>(); auto *connection=window.findChild<ConnectionStatusWidget*>();
    emit connection->connectRequested(QStringLiteral("127.0.0.1"),server.server.serverPort());
    QTRY_VERIFY(client->isConnected()); QTRY_COMPARE(window.findChild<CameraListWidget*>()->selectedCameraId(),QStringLiteral("CAM01"));
    auto *canvas=window.findChild<CameraViewWidget*>()->findChild<CameraWidget*>();
    QImage frame(1280,720,QImage::Format_RGB32); frame.fill(Qt::black); window.findChild<CameraViewWidget*>()->setFrame(frame);
    QSignalSpy metadata(client,&VmsClient::metadataReceived);
    const auto notify=[&](const QString &id,const QJsonObject &data){server.sockets.first()->sendTextMessage(text({{"version",1},{"type","notification"},{"event","CAMERA_METADATA"},{"cameraId",id},{"data",data}}));};
    QJsonObject detection{{"cameraId","CAM01"},{"topic","Analytics/PersonDetection"},{"sourceTimeMs",1000},{"detected",true},{"available",true},
        {"confidence",.92},{"bboxX",250},{"bboxY",120},{"bboxWidth",140},{"bboxHeight",260},{"imageWidth",1280},{"imageHeight",720},{"frameId","9007199254740993"}};
    notify(QStringLiteral("CAM01"),detection); QTRY_VERIFY(canvas->hasDetection());
    QCOMPARE(canvas->detection().boundingBox,QRect(250,120,140,260)); QCOMPARE(canvas->detection().imageSize,QSize(1280,720));
    QCOMPARE(metadata.last()[1].toJsonObject().value("frameId").toString(),QStringLiteral("9007199254740993"));
    notify(QStringLiteral("CAM01"),{{"cameraId","CAM01"},{"topic","Tracking/State"},{"sourceTimeMs",1100},{"tracking",true},{"targetState","TRACKING"},{"confidence",QJsonValue::Null},{"bboxX",QJsonValue::Null}});
    QTRY_COMPARE(metadata.count(),2); QVERIFY(canvas->hasDetection());
    QCOMPARE(window.findChild<TrackingPanel*>()->findChild<QPushButton*>(QStringLiteral("trackingToggle"))->isEnabled(),false);
    auto other=detection; other["cameraId"]="CAM99"; other["detected"]=false;
    notify(QStringLiteral("CAM99"),other); QTRY_COMPARE(metadata.count(),3); QVERIFY(canvas->hasDetection());
    detection["sourceTimeMs"]=1200; detection["confidence"]=QJsonValue::Null;
    notify(QStringLiteral("CAM01"),detection); QTRY_COMPARE(metadata.count(),4); QVERIFY(!canvas->detection().hasConfidence);
    QTRY_VERIFY_WITH_TIMEOUT(!canvas->hasDetection(),2500);
    detection["sourceTimeMs"]=1300; detection["detected"]=false;
    notify(QStringLiteral("CAM01"),detection); QTRY_VERIFY(!canvas->hasDetection());

    auto *events=window.findChild<EventSearchWidget*>(); auto *table=events->findChild<QTableView*>();
    events->findChild<QSpinBox*>(QStringLiteral("eventMinConfidence"))->setValue(90);
    events->findChild<QPushButton*>(QStringLiteral("eventSearchButton"))->click(); QTRY_COMPARE(table->model()->rowCount(),1);
    QCOMPARE(server.featureRequests.last().value("command").toString(),QStringLiteral("GET_DETECTIONS"));
    QCOMPARE(server.featureRequests.last().value("minConfidence").toDouble(),.9);
    QCOMPARE(table->model()->index(0,3).data().toString(),QStringLiteral("92%"));
    events->findChild<QPushButton*>(QStringLiteral("eventNextPage"))->click();
    QTRY_VERIFY(server.featureRequests.last().value("cursor").isObject());
    events->findChild<QComboBox*>(QStringLiteral("eventRecordKind"))->setCurrentIndex(1);
    events->findChild<QSpinBox*>(QStringLiteral("eventMinConfidence"))->setValue(0);
    events->findChild<QPushButton*>(QStringLiteral("eventSearchButton"))->click(); QTRY_COMPARE(table->model()->rowCount(),1);
    QCOMPARE(server.featureRequests.last().value("command").toString(),QStringLiteral("GET_EVENTS"));
    QCOMPARE(table->model()->index(0,3).data().toString(),QStringLiteral("—"));
    emit events->recordPlaybackRequested(server.row());
    QTRY_COMPARE(server.featureRequests.last().value("command").toString(),QStringLiteral("GET_EVENT_PLAYBACK"));
    QCOMPARE(server.featureRequests.last().value("sampleId").toInt(),1);
    QTRY_COMPARE(window.findChild<QTabWidget*>()->currentWidget(),static_cast<QWidget*>(window.findChild<PlaybackWidget*>()));
    QVERIFY(window.findChild<PlaybackWidget*>()->findChild<QMediaPlayer*>()==nullptr);

    auto *chat=window.findChild<ChatSearchWidget*>(); auto *input=chat->findChild<QLineEdit*>(QStringLiteral("chatInput"));
    auto *send=chat->findChild<QPushButton*>(QStringLiteral("chatSend"));
    QVERIFY(send->isEnabled()); input->setText(QStringLiteral("오늘 사람이 나온 영상 찾아줘")); send->click(); QVERIFY(!send->isEnabled());
    QTRY_COMPARE(chat->findChild<QTableView*>(QStringLiteral("chatResults"))->model()->rowCount(),1); QVERIFY(send->isEnabled());
    QCOMPARE(server.featureRequests.last().value("command").toString(),QStringLiteral("CHAT_SEARCH"));
    QCOMPARE(server.featureRequests.last().value("timezone").toString(),QStringLiteral("Asia/Seoul"));
    auto *chatTable=chat->findChild<QTableView*>(QStringLiteral("chatResults")); chatTable->selectRow(0);
    QVERIFY(!chat->findChild<QPushButton*>(QStringLiteral("chatPlay"))->isEnabled());
    QVERIFY(chat->findChild<QPlainTextEdit*>(QStringLiteral("chatHistory"))->toPlainText().contains(QStringLiteral("녹화는 없습니다")));
    client->disconnectFromServer(); QTRY_VERIFY(!send->isEnabled()); QCOMPARE(chatTable->model()->rowCount(),0);
    QVERIFY(!canvas->hasDetection());
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
    if (liveConfig.isEmpty()) {
        QSignalSpy direct(client, &VmsClient::streamUriReady);
        client->requestStream(QStringLiteral("CAM01"), QStringLiteral("tcp"));
        QTRY_COMPARE(direct.count(), 1);
        QCOMPARE(direct.last()[1].toUrl().port(), 9);
        client->requestStream(QStringLiteral("CAM01"), QStringLiteral("udp"));
        QTRY_COMPARE(direct.count(), 2);
        QCOMPARE(direct.last()[1].toUrl(), direct.first()[1].toUrl());
        QSignalSpy metadata(client,&VmsClient::metadataSearchReceived), playback(client,&VmsClient::eventPlaybackReceived), failures(client,&VmsClient::requestFailed);
        const auto from=QDateTime::currentDateTime().addSecs(-60), to=QDateTime::currentDateTime();
        QVERIFY(!client->searchMetadata(QStringLiteral("CAM01"),from,to,true,QStringLiteral("ALL"),0.9).isEmpty());
        QTRY_COMPARE(metadata.count(),1); QVERIFY(metadata.last()[2].toJsonArray().isEmpty());
        QVERIFY(!client->searchMetadata(QStringLiteral("CAM01"),from,to,false,QStringLiteral("PERSON_DETECTED"),0).isEmpty());
        QTRY_COMPARE(metadata.count(),2); QVERIFY(metadata.last()[2].toJsonArray().isEmpty());
        const auto python=QStandardPaths::findExecutable(QStringLiteral("python3")), ffmpeg=QStandardPaths::findExecutable(QStringLiteral("ffmpeg"));
        if (!python.isEmpty() && !ffmpeg.isEmpty()) {
            // 실제 VMS의 DB/검색/녹화 조회 계약을 합성 파일로 검사한다. Pi/LLM을 호출하지 않는다.
            const auto file=directory.filePath(QStringLiteral("search-fixture.mkv"));
            QProcess generator; generator.start(ffmpeg,{"-v","error","-f","lavfi","-i","testsrc2=size=160x120:rate=10","-t","4","-c:v","libx264","-pix_fmt","yuv420p","-y",file});
            QVERIFY(generator.waitForFinished(10000)); QCOMPARE(generator.exitCode(),0);
            const auto start=QDateTime::currentMSecsSinceEpoch()-4000;
            const auto seed=QStringLiteral(R"PY(
import json,sqlite3,sys
db=sqlite3.connect(sys.argv[1]); start=int(sys.argv[3]); stamp=start+1234
m={'cameraId':'CAM01','sourceTimeMs':stamp,'receivedTimeMs':stamp,'confidence':0.95,'frameId':'9007199254740993'}
db.execute('INSERT INTO recordings(camera_id,start_time,end_time,file_path,duration,codec,width,height) VALUES(?,?,?,?,?,?,?,?)',('CAM01',start,start+4000,sys.argv[2],4,'H264',160,120))
s=db.execute('INSERT INTO metadata_samples(camera_id,source_time_ms,received_time_ms,payload_json) VALUES(?,?,?,?)',('CAM01',stamp,stamp,json.dumps(m)))
db.execute('INSERT INTO detection_index VALUES(?,?,?,?)',(s.lastrowid,'CAM01',stamp,0.95)); db.commit()
)PY");
            QProcess seeder; seeder.start(python,{QStringLiteral("-c"),seed,directory.filePath(QStringLiteral("vms.db")),file,QString::number(start)});
            QVERIFY(seeder.waitForFinished(5000)); QCOMPARE(seeder.exitCode(),0);
            QVERIFY(!client->searchMetadata(QStringLiteral("CAM01"),QDateTime::currentDateTime().addSecs(-60),QDateTime::currentDateTime(),true,QStringLiteral("ALL"),0.9).isEmpty());
            QTRY_COMPARE(metadata.count(),3); const auto records=metadata.last()[2].toJsonArray(); QCOMPARE(records.size(),1);
            const auto row=records[0].toObject(); QCOMPARE(row.value("frameId").toString(),QStringLiteral("9007199254740993"));
            QVERIFY(!client->requestEventPlayback(row).isEmpty()); QTRY_COMPARE(playback.count(),1);
            const auto location=playback.last()[1].toJsonObject(); QVERIFY(location.value("playable").toBool()); QCOMPARE(location.value("offsetMs").toInteger(),qint64(1234));
            QCOMPARE(location.value("recording").toObject().value("filePath").toString(),file);
            qInfo()<<"Real VMS detection sample -> recording lookup -> offsetMs=1234 verified";
        }
        QVERIFY(!client->requestEventPlayback({{"id",123},{"sampleId",123},{"recordKind","detection_sample"},{"cameraId","CAM01"}}).isEmpty());
        QTRY_VERIFY(!playback.isEmpty() && !playback.last()[1].toJsonObject().value(QStringLiteral("playable")).toBool());
        const auto chatId=client->chatSearch(QStringLiteral("CAM01"),QStringLiteral("오늘 사람 탐지 기록 찾아줘")); QVERIFY(!chatId.isEmpty());
        QTRY_VERIFY(std::any_of(failures.begin(),failures.end(),[&](const QList<QVariant>& row){return row[0].toString()==chatId && row[1].toString()==QStringLiteral("CHAT_DISABLED");}));
    }
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
        QCOMPARE(uri.host(), QStringLiteral("192.168.0.92")); QCOMPARE(uri.port(), 8554);
        QTRY_VERIFY_WITH_TIMEOUT(frames.count() >= 3, 25000);
        QVERIFY(view->isLive());
        const int uriCount = streamUri.count();
        view->findChild<QComboBox *>(QStringLiteral("rtspTransport"))->setCurrentIndex(1);
        emit view->startRequested(QStringLiteral("RTSP UDP"));
        QTRY_VERIFY_WITH_TIMEOUT(streamUri.count() > uriCount, 15000);
        QCOMPARE(streamUri.last()[1].toUrl().host(), uri.host());
        frames.clear(); QTRY_VERIFY_WITH_TIMEOUT(frames.count() >= 3, 15000);
        QVERIFY(view->isLive());
        qInfo() << "Qt UDP live frames:" << frames.count() << frames.last()[0].toSize();
        const auto preview = qEnvironmentVariable("VMS_PREVIEW_PATH");
        if (!preview.isEmpty()) QVERIFY(window.grab().save(preview));
        qInfo() << "ONVIF registration -> camera-direct RTSP -> Qt decoded frames:" << frames.count() << frames.last()[0].toSize();
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
