#pragma once

#include <QWidget>

class QLabel;
class QLineEdit;
class QSpinBox;

class ConnectionStatusWidget : public QWidget
{
    Q_OBJECT
public:
    enum class Status { Disconnected, Connected, Error };
    explicit ConnectionStatusWidget(QWidget *parent = nullptr);
    void setRaspberryPiConnected(bool connected);
    void setCameraConnected(bool connected);
    void setRaspberryPiStatus(Status status);
    void setCameraStatus(Status status);

signals:
    void connectRequested(const QString &host, quint16 port);
    void disconnectRequested();
    void demoStatusRequested();

private:
    static void setStatus(QLabel *indicator, QLabel *text, Status status);
    QLineEdit *m_ip = nullptr;
    QSpinBox *m_port = nullptr;
    QLabel *m_piIndicator = nullptr;
    QLabel *m_piText = nullptr;
    QLabel *m_cameraIndicator = nullptr;
    QLabel *m_cameraText = nullptr;
};
