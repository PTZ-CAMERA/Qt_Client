#pragma once
#include "model/VmsTypes.h"
#include <QWidget>
#include <QUrl>
class QLabel;
class QComboBox;
class QLineEdit;
class QPushButton;
class DeviceInfoWidget : public QWidget {
    Q_OBJECT
public:
    explicit DeviceInfoWidget(QWidget *parent = nullptr);
    void setDiscoveryState(bool busy, const QString &message);
    void setDiscoveredCameras(const QList<DiscoveredCamera> &devices);
    void setCamera(const CameraInfo &camera);
    void setStreamUri(const QString &cameraId, const QUrl &uri);
    void clearStreamUri();
    void cameraRegistered(const QString &cameraId);
    void setStreamQueryError(const QString &cameraId, const QString &message);
signals:
    void refreshRequested(const QString &cameraId);
    void discoverRequested();
    void listRequested();
    void registerRequested(const QString &serviceUrl, const QString &username, const QString &password, const QString &profileToken);
private:
    QString m_cameraId;
    QList<QLabel *> m_values;
    QComboBox *m_devices = nullptr;
    QLineEdit *m_service = nullptr, *m_username = nullptr, *m_password = nullptr, *m_profile = nullptr;
    QLineEdit *m_streamUri = nullptr;
    QLabel *m_streamState = nullptr;
    QPushButton *m_discover = nullptr;
    QList<DiscoveredCamera> m_discovered;
    QString m_selectedDeviceId;
};
