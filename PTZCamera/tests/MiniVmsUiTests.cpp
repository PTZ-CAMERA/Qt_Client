// 실서버 없이 UI 상태/신호를 시험한다. 네트워크가 만들어지지 않는 것도 확인한다.
#include "MiniVmsUiTests.h"
#include "app/MainWindow.h"
#include "camera/CameraListWidget.h"
#include "demo/DummyDataProvider.h"
#include "events/EventSearchWidget.h"
#include "playback/PlaybackWidget.h"
#include "ui/PTZControlWidget.h"
#include "ui/PtzKeyboardController.h"
#include "ui/PtzCommandController.h"
#include <QKeyEvent>
#include "ui/TrackingPanel.h"
#include "camera/CameraViewWidget.h"
#include "device/DeviceInfoWidget.h"
#include <QClipboard>
#include <QComboBox>
#include <QDateEdit>
#include <QTemporaryDir>
#include <QProcess>
#include <QStandardPaths>
#include <QMediaPlayer>
#include "ui/HelpDialog.h"
#include <QApplication>
#include <QCheckBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSignalSpy>
#include <QSlider>
#include <QTabWidget>
#include <QTcpSocket>
#include <QTableView>
#include <QtTest>

void MiniVmsUiTests::cameraSelectionAndDummyIsolation() {
    MainWindow window;
    window.show(); window.resize(1200, 750); QCoreApplication::processEvents();
    QCOMPARE(window.size(), QSize(1200, 750));
    QCOMPARE(window.findChild<QTabWidget *>()->count(), 5);
    QVERIFY(window.findChildren<QTcpSocket *>().isEmpty());
    auto *list = window.findChild<CameraListWidget *>();
    QCOMPARE(list->selectedCameraId(), QStringLiteral("CAM01"));
    auto *ptz = window.findChild<PTZControlWidget *>(); QVERIFY(ptz->isEnabled());
    list->findChild<QListWidget *>()->setCurrentRow(1);
    QCOMPARE(list->selectedCameraId(), QStringLiteral("CAM02")); QVERIFY(!ptz->isEnabled());
    auto *dummy = window.findChild<QCheckBox *>(); dummy->setChecked(false);
    QVERIFY(list->selectedCameraId().isEmpty()); QVERIFY(!ptz->isEnabled());
    dummy->setChecked(true); QCOMPARE(list->selectedCameraId(), QStringLiteral("CAM01"));
}
void MiniVmsUiTests::ptzPressRelease() {
    PTZControlWidget widget; widget.show();
    QSignalSpy move(&widget, &PTZControlWidget::moveRequested), stop(&widget, &PTZControlWidget::stopRequested);
    QPushButton *right = nullptr;
    for (auto *button : widget.findChildren<QPushButton *>()) if (button->text() == QStringLiteral("▶")) right = button;
    QVERIFY(right);
    QTest::mousePress(right, Qt::LeftButton); QCOMPARE(move.count(), 1);
    QCOMPARE(move.first()[0].toFloat(), 0.5F); QCOMPARE(move.first()[1].toFloat(), 0.0F);
    QTest::mouseRelease(right, Qt::LeftButton); QCOMPARE(stop.count(), 1);
}
void MiniVmsUiTests::trackingStatusDoesNotSendCommand() {
    TrackingPanel panel;
    QSignalSpy changed(&panel, &TrackingPanel::trackingChanged);
    TrackingInfo info; info.enabled = true; info.detected = true; info.status = QStringLiteral("TRACKING");
    panel.updateTrackingInfo(info); QCOMPARE(changed.count(), 0); QVERIFY(panel.trackingEnabled());
    panel.setTrackingEnabled(false); QCOMPARE(changed.count(), 1);
}
void MiniVmsUiTests::eventFilterAndRecordingLookup() {
    DummyDataProvider provider;
    QList<EventInfo> events; int recordings = 0; QDateTime target;
    connect(&provider, &DummyDataProvider::eventListReceived, this, [&](const QList<EventInfo> &items) { events = items; });
    connect(&provider, &DummyDataProvider::recordingResolved, this, [&](const RecordingInfo &recording, const QDateTime &time) {
        QVERIFY(recording.simulated); QVERIFY(recording.playbackUri.isEmpty()); ++recordings; target = time;
    });
    provider.setEnabled(true);
    const QDate day = QDate::currentDate(); const QDateTime start(day, QTime(0, 0)), end(day, QTime(23, 59, 59));
    provider.requestEvents(QStringLiteral("CAM01"), start, end, QStringLiteral("PERSON_DETECTED")); QCOMPARE(events.size(), 2);
    provider.requestEvents(QStringLiteral("CAM02"), start, end, QStringLiteral("ALL")); QVERIFY(events.isEmpty());
    const QDateTime eventTime(day, QTime(15, 32, 10)); provider.requestRecordingAt(QStringLiteral("CAM01"), eventTime);
    QCOMPARE(recordings, 1); QCOMPARE(target, eventTime);
    provider.requestRecordingAt(QStringLiteral("CAM01"), QDateTime(day, QTime(18, 0))); QCOMPARE(recordings, 1);
    provider.setEnabled(false); provider.requestEvents(QStringLiteral("CAM01"), start, end, QStringLiteral("ALL")); QVERIFY(events.isEmpty());
}
void MiniVmsUiTests::eventOpensPlaybackAtTimestamp() {
    MainWindow window;
    const QDateTime time(QDate::currentDate(), QTime(15, 31, 22));
    window.findChild<EventSearchWidget *>()->eventPlaybackRequested(QStringLiteral("CAM01"), time);
    auto *playback = window.findChild<PlaybackWidget *>();
    QCOMPARE(window.findChild<QTabWidget *>()->currentWidget(), static_cast<QWidget *>(playback));
    QCOMPARE(playback->findChild<QSlider *>()->value(), 1882);
    QVERIFY(playback->findChild<QSlider *>()->isEnabled());
    window.findChild<QCheckBox *>()->setChecked(false); QVERIFY(!playback->findChild<QSlider *>()->isEnabled());
}
void MiniVmsUiTests::keyboardStopsAndIgnoresInputFields() {
    QWidget window; QLineEdit input(&window); input.move(0, 0); window.show();
    window.activateWindow();
    QTRY_COMPARE(QApplication::activeWindow(), &window);
    PtzKeyboardController keyboard(&window); keyboard.setEnabled(true);
    QSignalSpy move(&keyboard, &PtzKeyboardController::moveRequested), stop(&keyboard, &PtzKeyboardController::stopRequested);
    input.setFocus(); QTest::keyClick(&input, Qt::Key_Right); QCOMPARE(move.count(), 0);
    input.clearFocus(); window.setFocus(); QTest::keyPress(&window, Qt::Key_Right); QCOMPARE(move.count(), 1);
    QTest::keyRelease(&window, Qt::Key_Right); QCOMPARE(stop.count(), 1);
    QTest::keyPress(&window, Qt::Key_Up); QCOMPARE(move.count(), 2);
    keyboard.setEnabled(false); QCOMPARE(stop.count(), 2);
}
void MiniVmsUiTests::unsupportedVmsControlsAndStreamAddress() {
    MainWindow window(nullptr, false);
    QVERIFY(!window.findChild<PTZControlWidget *>()->isEnabled());
    QVERIFY(!window.findChild<TrackingPanel *>()->isEnabled());
    QVERIFY(!window.findChild<QTabWidget *>()->isTabEnabled(0));
    auto *view = window.findChild<CameraViewWidget *>();
    auto *method = view->findChild<QComboBox *>();
    QCOMPARE(method->count(), 2);
    QCOMPARE(method->itemText(0), QStringLiteral("RTSP TCP"));
    QCOMPARE(method->itemText(1), QStringLiteral("RTSP UDP"));
    QCOMPARE(method->findText(QStringLiteral("WebRTC")), -1);
    QCOMPARE(method->currentData().toString(), QStringLiteral("tcp"));
    QVERIFY(method->model()->flags(method->model()->index(1, 0)) & Qt::ItemIsEnabled);
    QSignalSpy streamError(view, &CameraViewWidget::playbackError);
    method->setCurrentIndex(1);
    view->startStream(QUrl(QStringLiteral("http://127.0.0.1:8554/cam")));
    QCOMPARE(streamError.count(), 1); QVERIFY(!view->isPlaying());
    method->setCurrentIndex(0);
    QSignalSpy movement(window.findChild<PtzKeyboardController *>(), &PtzKeyboardController::moveRequested);
    QTest::keyClick(&window, Qt::Key_Right); QCOMPARE(movement.count(), 0);

    auto *device = window.findChild<DeviceInfoWidget *>();
    CameraInfo camera; camera.id = QStringLiteral("CAM01"); device->setCamera(camera);
    QVERIFY(!device->findChild<QPushButton *>(QStringLiteral("getVmsRtspUri")));
    QVERIFY(!device->findChild<QPushButton *>(QStringLiteral("copyVmsRtspUri")));
    auto *uri = device->findChild<QLineEdit *>(QStringLiteral("vmsRtspUri"));
    auto *devices = device->findChild<QComboBox *>(QStringLiteral("onvifDevices"));
    auto *state = device->findChild<QLabel *>(QStringLiteral("discoveredStreamState"));
    device->setDiscoveredCameras({
        {QStringLiteral("http://camera1/onvif"), QStringLiteral("First"), QStringLiteral("camera1"), QStringLiteral("test"),
         QStringLiteral("CAM01"), QStringLiteral("rtsp://127.0.0.1:8555/CAM01"), false, false},
        {QStringLiteral("http://camera2/onvif"), QStringLiteral("Second"), QStringLiteral("camera2"), QStringLiteral("test"),
         QStringLiteral("CAM02"), QStringLiteral("rtsp://127.0.0.1:8555/CAM02"), false, false}});
    QVERIFY(uri->isReadOnly()); QCOMPARE(uri->text(), QStringLiteral("rtsp://127.0.0.1:8555/CAM01"));
    QCOMPARE(state->text(), QStringLiteral("Not added")); QVERIFY(!view->isPlaying());
    devices->setCurrentIndex(1); QCOMPARE(uri->text(), QStringLiteral("rtsp://127.0.0.1:8555/CAM02"));
    device->setStreamUri(QStringLiteral("CAM01"), QUrl(QStringLiteral("rtsp://127.0.0.1:8555/CAM01")));
    QCOMPARE(uri->text(), QStringLiteral("rtsp://127.0.0.1:8555/CAM02"));
    QSignalSpy registration(device, &DeviceInfoWidget::registerRequested);
    device->findChild<QPushButton *>(QStringLiteral("registerOnvifCamera"))->click();
    QCOMPARE(registration.count(), 1); QCOMPARE(registration.first()[0].toString(), QStringLiteral("http://camera2/onvif"));
    device->cameraRegistered(QStringLiteral("CAM02")); QCOMPARE(state->text(), QStringLiteral("Connecting"));
    device->setStreamUri(QStringLiteral("CAM02"), QUrl(QStringLiteral("rtsp://127.0.0.1:8555/CAM02")));
    QCOMPARE(state->text(), QStringLiteral("Ready"));
    device->setStreamUri(QStringLiteral("CAM02"), QUrl(QStringLiteral("rtsp://fixture:token@camera2:8554/cam")));
    QCOMPARE(uri->text(), QStringLiteral("rtsp://camera2:8554/cam"));
    QVERIFY(!uri->toolTip().contains(QStringLiteral("token")));
    device->setDiscoveryState(true, QStringLiteral("Searching...")); QVERIFY(uri->text().isEmpty());
    device->setDiscoveredCameras({{QStringLiteral("http://camera/onvif"), QStringLiteral("Unsafe"), QStringLiteral("camera"),
        QStringLiteral("test"), QStringLiteral("CAM03"), QStringLiteral("rtsp://user:password@127.0.0.1/CAM03"), false, false}});
    QVERIFY(uri->text().isEmpty());
}

void MiniVmsUiTests::missingRecordingDoesNotEnablePlayback() {
    PlaybackWidget playback;
    playback.setLocalVms(true);
    CameraInfo camera; camera.id = QStringLiteral("CAM01"); playback.setCameras({camera});
    RecordingInfo recording; recording.cameraId = camera.id;
    recording.startTime = QDateTime(QDate::currentDate(), QTime(10, 0));
    recording.endTime = recording.startTime.addSecs(600); recording.filePath = QStringLiteral("C:/VMS/recordings/CAM01/test.mkv");
    playback.setRecordings({recording});
    auto *count = playback.findChild<QLabel *>(QStringLiteral("recordingResultCount"));
    QVERIFY(count->text().startsWith(QStringLiteral("1 results"))); QVERIFY(count->text().contains(QStringLiteral("100")));
    auto *info = playback.findChild<QPushButton *>(QStringLiteral("openRecording"));
    QCOMPARE(info->text(), QStringLiteral("OPEN"));
    QVERIFY(!info->isEnabled());
    playback.findChild<QComboBox *>(QStringLiteral("recordingResults"))->setCurrentIndex(0);
    QSignalSpy failed(&playback, &PlaybackWidget::playbackFailed);
    info->click(); QCOMPARE(failed.count(), 1);
    QVERIFY(playback.findChild<QLabel *>(QStringLiteral("recordedVideo"))->text().contains(recording.filePath));
    QVERIFY(!playback.findChild<QSlider *>()->isEnabled());
    for (auto *button : playback.findChildren<QPushButton *>())
        if (button->text() == QStringLiteral("Play")) QVERIFY(!button->isEnabled());
    QSignalSpy invalidated(&playback, &PlaybackWidget::queryInvalidated);
    playback.findChild<QDateEdit *>()->setDate(QDate::currentDate().addDays(-1));
    QCOMPARE(invalidated.count(), 1); QVERIFY(!info->isEnabled());
    playback.setSearchError(QStringLiteral("Server unavailable"));
    QVERIFY(count->text().contains(QStringLiteral("Search failed"))); QVERIFY(!info->isEnabled());
}

void MiniVmsUiTests::recordingFilePlaysAndSeeks() {
    const QString ffmpeg = QStandardPaths::findExecutable(QStringLiteral("ffmpeg"));
    const auto fixture=qEnvironmentVariable("VMS_TEST_RECORDING_FILE");
    if (ffmpeg.isEmpty() && fixture.isEmpty()) QSKIP("ffmpeg or VMS_TEST_RECORDING_FILE is required");
    QTemporaryDir directory; QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("녹화 테스트.mkv"));
    QProcess generator;
    if (!fixture.isEmpty()) { QVERIFY(QFile::copy(fixture,path)); }
    else {
    generator.start(ffmpeg, {QStringLiteral("-v"), QStringLiteral("error"), QStringLiteral("-f"), QStringLiteral("lavfi"),
        QStringLiteral("-i"), QStringLiteral("testsrc2=size=160x120:rate=10"), QStringLiteral("-t"), QStringLiteral("4"),
        QStringLiteral("-c:v"), QStringLiteral("libx264"), QStringLiteral("-pix_fmt"), QStringLiteral("yuv420p"), QStringLiteral("-y"), path});
    QVERIFY(generator.waitForFinished(10000)); QCOMPARE(generator.exitCode(), 0);
    }
    PlaybackWidget playback; playback.setLocalVms(true); playback.show();
    RecordingInfo recording; recording.cameraId = QStringLiteral("CAM01"); recording.filePath = path;
    recording.startTime = QDateTime(QDate::currentDate(), QTime(10, 0)); recording.endTime = recording.startTime.addSecs(4);
    playback.setRecordings({recording});
    auto *results = playback.findChild<QComboBox *>(QStringLiteral("recordingResults"));
    QSignalSpy frames(&playback, &PlaybackWidget::playbackFrameReceived), errors(&playback, &PlaybackWidget::playbackFailed);
    results->setCurrentIndex(0); emit results->activated(0);
    QTRY_VERIFY_WITH_TIMEOUT(frames.count() > 0 || errors.count() > 0, 10000);
    QVERIFY2(errors.isEmpty(), errors.isEmpty() ? "" : qPrintable(errors.first()[0].toString()));
    QCOMPARE(frames.first()[0].toSize(), QSize(160, 120));
    auto *player = playback.findChild<QMediaPlayer *>(); QVERIFY(player);
    playback.findChild<QPushButton *>(QStringLiteral("recordingPause"))->click();
    QTRY_COMPARE(player->playbackState(), QMediaPlayer::PausedState);
    auto *timeline = playback.findChild<QSlider *>(QStringLiteral("recordingTimeline"));
    if (player->duration() > 0) {
        QTRY_VERIFY(timeline->isEnabled()); timeline->setValue(2);
        QTRY_VERIFY(player->position() >= 1900);
    } else {
        QVERIFY(!timeline->isEnabled());
        QVERIFY(!playback.findChild<QPushButton *>(QStringLiteral("recordingForward"))->isEnabled());
        qInfo() << "Backend duration unavailable: verified playback/pause/stop; seek correctly disabled";
    }
    playback.findChild<QPushButton *>(QStringLiteral("recordingStop"))->click();
    QTRY_COMPARE(player->playbackState(), QMediaPlayer::StoppedState);
    playback.setLocalVms(false); playback.openRecording(recording, recording.startTime);
    QCOMPARE(errors.count(), 1); QVERIFY(!timeline->isEnabled());
    playback.openLocalFile(path); const int previousFrames = frames.count();
    QTRY_VERIFY_WITH_TIMEOUT(frames.count() > previousFrames, 10000);
    playback.clear(); QVERIFY(!timeline->isEnabled());
}

void MiniVmsUiTests::helpButtonShowsGuide() {
    MainWindow window(nullptr, false); window.show();
    auto *help = window.findChild<QPushButton *>(QStringLiteral("helpButton")); QVERIFY(help);
    QCOMPARE(help->text(), QStringLiteral("?")); help->click();
    auto *dialog = window.findChild<HelpDialog *>(); QVERIFY(dialog); QVERIFY(dialog->isVisible());
    QVERIFY(!dialog->isModal()); QCOMPARE(dialog->findChild<QTabWidget *>()->count(), 3);
    dialog->close(); QVERIFY(!dialog->isVisible()); help->click(); QVERIFY(dialog->isVisible());
    QCOMPARE(window.findChildren<HelpDialog *>().size(), 1);
}
void MiniVmsUiTests::metadataPlaybackUsesMillisecondOffset() {
    const auto ffmpeg=QStandardPaths::findExecutable(QStringLiteral("ffmpeg"));
    const auto fixture=qEnvironmentVariable("VMS_TEST_RECORDING_FILE");
    if (ffmpeg.isEmpty() && fixture.isEmpty()) QSKIP("ffmpeg or VMS_TEST_RECORDING_FILE is required");
    QTemporaryDir directory; QVERIFY(directory.isValid()); const auto path=directory.filePath(QStringLiteral("metadata-search.mkv"));
    if (!fixture.isEmpty()) { QVERIFY(QFile::copy(fixture,path)); }
    else {
        QProcess generator; generator.start(ffmpeg,{"-v","error","-f","lavfi","-i","testsrc2=size=160x120:rate=10","-t","4","-c:v","libx264","-pix_fmt","yuv420p","-y",path});
        QVERIFY(generator.waitForFinished(10000)); QCOMPARE(generator.exitCode(),0);
    }
    PlaybackWidget playback; playback.setLocalVms(true);
    const auto start=QDateTime::currentMSecsSinceEpoch()-4000;
    QJsonObject descriptor{{"cameraId","CAM01"},{"playable",true},{"offsetMs",1234},{"timeMapping","receive_estimated"},
        {"recording",QJsonObject{{"cameraId","CAM01"},{"startTimeMs",start},{"endTimeMs",start+4000},{"duration",4.0},{"filePath",path}}}};
    QSignalSpy frames(&playback,&PlaybackWidget::playbackFrameReceived), errors(&playback,&PlaybackWidget::playbackFailed);
    const bool opened=playback.openPlaybackResult(descriptor);
    QVERIFY2(opened,errors.isEmpty() ? "No playback error" : qPrintable(errors.first()[0].toString()));
    auto *player=playback.findChild<QMediaPlayer*>(); QVERIFY(player);
    QSignalSpy positions(player,&QMediaPlayer::positionChanged);
    QTRY_VERIFY_WITH_TIMEOUT(frames.count()>0 || errors.count()>0,10000);
    if (!errors.isEmpty() && errors.first()[0].toString().contains(QStringLiteral("해당 시각으로 이동할 수 없습니다"))) {
        QCOMPARE(frames.count(),0); QVERIFY(!playback.findChild<QSlider*>()->isEnabled());
        qInfo()<<"Backend cannot seek: confirmed requested event offset is refused, not played from zero"; return;
    }
    QVERIFY2(errors.isEmpty(),errors.isEmpty() ? "" : qPrintable(errors.first()[0].toString()));
    bool exact=false; for (const auto& row:positions) if (row[0].toLongLong()==1234) exact=true;
    QVERIFY(exact); QVERIFY(playback.findChild<QLabel*>(QStringLiteral("recordingResultCount"))->text().contains(QStringLiteral("추정")));
    descriptor["playable"]=false; descriptor["reason"]="NO_RECORDING_AT_TIME";
    QVERIFY(!playback.openPlaybackResult(descriptor));
    QVERIFY(!playback.findChild<QSlider*>()->isEnabled());
}
void MiniVmsUiTests::resultTableNavigationDoesNotMovePtz() {
    QWidget window; QTableView table(&window); window.show(); window.activateWindow(); table.show(); table.setFocus();
    QTRY_COMPARE(QApplication::activeWindow(),&window);
    PtzKeyboardController keyboard(&window); keyboard.setEnabled(true); QSignalSpy movement(&keyboard,&PtzKeyboardController::moveRequested);
    QTest::keyClick(&table,Qt::Key_Down); QTest::keyClick(&table,Qt::Key_W); QCOMPARE(movement.count(),0);
}
void MiniVmsUiTests::ptzRefreshStopsAndKeepsOldCameraIdentity() {
    PtzCommandController controller;
    QSignalSpy moves(&controller, &PtzCommandController::moveRequested), stops(&controller, &PtzCommandController::stopRequested);
    controller.setTarget(QStringLiteral("CAM01"), true);
    controller.setKeyboardMovement(0.5F, 0); QCOMPARE(moves.count(), 1);
    QTRY_VERIFY_WITH_TIMEOUT(moves.count() >= 3, 700);
    controller.setKeyboardMovement(0, 0); QCOMPARE(stops.count(), 1);
    const int count = moves.count(); QTest::qWait(250); QCOMPARE(moves.count(), count);
    controller.setButtonMovement(0, 0.5F);
    controller.setTarget(QStringLiteral("CAM02"), true);
    QCOMPARE(stops.last()[0].toString(), QStringLiteral("CAM01"));
    const int switched = moves.count(); QTest::qWait(250); QCOMPARE(moves.count(), switched);
    controller.setButtonMovement(-0.5F, 0); QCOMPARE(moves.last()[0].toString(), QStringLiteral("CAM02"));
    controller.setTarget(QStringLiteral("CAM02"), false); QCOMPARE(stops.last()[0].toString(), QStringLiteral("CAM02"));
    const int disabled = moves.count(); QTest::qWait(250); QCOMPARE(moves.count(), disabled);
}
void MiniVmsUiTests::wasdIgnoresRepeatReleaseAndCentersWithC() {
    QWidget window; window.show(); window.activateWindow(); QTRY_COMPARE(QApplication::activeWindow(), &window);
    PtzKeyboardController keyboard(&window); keyboard.setEnabled(true);
    QSignalSpy moves(&keyboard, &PtzKeyboardController::moveRequested), stops(&keyboard, &PtzKeyboardController::stopRequested);
    QSignalSpy center(&keyboard, &PtzKeyboardController::centerRequested), cancelled(&keyboard, &PtzKeyboardController::inputCancelled);
    QTest::keyPress(&window, Qt::Key_W); QCOMPARE(moves.count(), 1); QCOMPARE(moves.last()[1].toFloat(), 0.5F);
    QKeyEvent repeatRelease(QEvent::KeyRelease, Qt::Key_W, Qt::NoModifier, QStringLiteral("w"), true);
    QCoreApplication::sendEvent(&window, &repeatRelease); QCOMPARE(stops.count(), 0);
    QKeyEvent repeatPress(QEvent::KeyPress, Qt::Key_W, Qt::NoModifier, QStringLiteral("w"), true);
    QCoreApplication::sendEvent(&window, &repeatPress); QCOMPARE(moves.count(), 1);
    QTest::keyRelease(&window, Qt::Key_W); QCOMPARE(stops.count(), 1);
    QTest::keyClick(&window, Qt::Key_C); QCOMPARE(center.count(), 1);
    QTest::keyPress(&window, Qt::Key_D); QCOMPARE(moves.last()[0].toFloat(), 0.5F);
    QEvent deactivate(QEvent::WindowDeactivate); QCoreApplication::sendEvent(&window, &deactivate);
    QCOMPARE(cancelled.count(), 1); QCOMPARE(stops.count(), 2);
}
QTEST_MAIN(MiniVmsUiTests)
