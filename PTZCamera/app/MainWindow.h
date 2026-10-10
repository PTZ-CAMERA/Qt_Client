#pragma once
#include "model/VmsTypes.h"
#include <QMainWindow>
#include <QJsonObject>
#include <QTimer>
#include <QHash>
class CameraListWidget;
class CameraViewWidget;
class ConnectionStatusWidget;
class DeviceInfoWidget;
class DummyDataProvider;
class EventSearchWidget;
class PlaybackWidget;
class PTZControlWidget;
class PtzKeyboardController;
class StatusIndicatorWidget;
class SystemLogWidget;
class TrackingPanel;
class VmsClient;
class QCheckBox;
class QTabWidget;
class HelpDialog;
class PtzCommandController;
class QLabel;
class ChatSearchWidget;
// 화면 배치, cameraId 선택과 Dummy/VMS 데이터 연결을 담당한다.
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr, bool dummyMode = true);
    ~MainWindow() override;
    void addLog(const QString &source, const QString &message);
private:
    QWidget *makePanel(const QString &title, QWidget *content);
    void selectCamera(const QString &cameraId);
    void applyCameraStatus(const CameraInfo &camera);
    void requestStop();
    void requestCenter();
    void applyMetadata(const QString &cameraId,const QJsonObject &data);
    void openMetadataPlayback(const QJsonObject &data);
    void requestMetadataPlayback(const QJsonObject &record);
    CameraListWidget *m_list = nullptr;
    CameraViewWidget *m_view = nullptr;
    PTZControlWidget *m_ptz = nullptr;
    TrackingPanel *m_tracking = nullptr;
    ConnectionStatusWidget *m_connection = nullptr;
    DeviceInfoWidget *m_device = nullptr;
    EventSearchWidget *m_events = nullptr;
    PlaybackWidget *m_playback = nullptr;
    SystemLogWidget *m_log = nullptr;
    DummyDataProvider *m_dummy = nullptr;
    VmsClient *m_client = nullptr;
    PtzKeyboardController *m_keyboard = nullptr;
    PtzCommandController *m_ptzCommands = nullptr;
    QLabel *m_ptzNotice = nullptr;
    QCheckBox *m_dummyToggle = nullptr;
    QTabWidget *m_tabs = nullptr;
    HelpDialog *m_help = nullptr;
    ChatSearchWidget *m_chat = nullptr;
    StatusIndicatorWidget *m_headerVms = nullptr;
    StatusIndicatorWidget *m_vmsStatus = nullptr;
    StatusIndicatorWidget *m_cameraStatus = nullptr;
    StatusIndicatorWidget *m_streamStatus = nullptr;
    StatusIndicatorWidget *m_recStatus = nullptr;
    QList<CameraInfo> m_cameras;
    CameraInfo m_current;
    QString m_registerSelection;
    QString m_discoveryRequestId;
    QString m_playbackRequestId;
    QJsonObject m_metadata;
    QHash<QString,qint64> m_metadataSourceTimes;
    QTimer m_metadataExpiry;
    bool m_liveWanted = false, m_streamRequestPending = false;
};
