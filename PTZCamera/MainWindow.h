#pragma once

#include "TrackingInfo.h"

#include <QMainWindow>

class CameraPlaybackWidget;
class ConnectionStatusWidget;
class QLabel;
class NetworkClient;
class PTZControlWidget;
class QPlainTextEdit;
class TrackingPanel;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    void updateTrackingInfo(const TrackingInfo &info);
    void addLog(const QString &message);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    QWidget *makePanel(const QString &title, QWidget *content);
    QWidget *makeInformationPanel();
    void setDemoStatus(bool enabled);
    void showDemoDetection();
    void sendPtz(const QString &direction);
    void clearTrackingInfo();

    CameraPlaybackWidget *m_camera = nullptr;
    PTZControlWidget *m_ptz = nullptr;
    TrackingPanel *m_tracking = nullptr;
    ConnectionStatusWidget *m_connection = nullptr;
    NetworkClient *m_network = nullptr;
    QPlainTextEdit *m_log = nullptr;
    QLabel *m_mode = nullptr;
    QLabel *m_object = nullptr;
    QLabel *m_confidence = nullptr;
    QLabel *m_x = nullptr;
    QLabel *m_y = nullptr;
    QLabel *m_errorX = nullptr;
    QLabel *m_errorY = nullptr;
    QLabel *m_pan = nullptr;
    QLabel *m_tilt = nullptr;
    QLabel *m_frameSize = nullptr;
    bool m_demoStatus = false;
    bool m_demoEnabled = true;
    bool m_videoActive = false;
};
