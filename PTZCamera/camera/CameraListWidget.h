#pragma once
#include "model/VmsTypes.h"
#include <QWidget>
class QListWidget;
class CameraListWidget : public QWidget {
    Q_OBJECT
public:
    explicit CameraListWidget(QWidget *parent = nullptr);
    void setCameras(const QList<CameraInfo> &cameras);
    void updateCamera(const CameraInfo &camera);
    void selectCamera(const QString &cameraId);
    QString selectedCameraId() const;
signals:
    void cameraSelected(const QString &cameraId);
private:
    QListWidget *m_list = nullptr;
};
