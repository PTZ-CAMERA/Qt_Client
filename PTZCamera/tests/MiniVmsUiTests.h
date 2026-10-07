#pragma once
#include <QObject>
// cameraId 격리, 명령 피드백 루프와 입력 정지 동작을 검증한다.
class MiniVmsUiTests : public QObject {
    Q_OBJECT
private slots:
    void cameraSelectionAndDummyIsolation();
    void ptzPressRelease();
    void trackingStatusDoesNotSendCommand();
    void eventFilterAndRecordingLookup();
    void eventOpensPlaybackAtTimestamp();
    void keyboardStopsAndIgnoresInputFields();
    void unsupportedVmsControlsAndStreamAddress();
    void missingRecordingDoesNotEnablePlayback();
    void recordingFilePlaysAndSeeks();
    void helpButtonShowsGuide();
    void ptzRefreshStopsAndKeepsOldCameraIdentity();
    void wasdIgnoresRepeatReleaseAndCentersWithC();
};
