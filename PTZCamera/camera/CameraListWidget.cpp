// cameraIdを表示名と分離して保持する。リスト選択はControllerに通知するだけである。
#include "CameraListWidget.h"
#include <QListWidget>
#include <QSignalBlocker>
#include <QVBoxLayout>
namespace {
void displayCamera(QListWidgetItem *item, const CameraInfo &camera) {
    item->setData(Qt::UserRole, camera.id);
    item->setText(QStringLiteral("%1\n%2\n● %3   %4")
        .arg(camera.id, camera.name, camera.online ? QStringLiteral("ONLINE") : QStringLiteral("OFFLINE"),
             camera.recording ? QStringLiteral("● REC") : QStringLiteral("REC OFF")));
    item->setForeground(camera.online ? QColor("#dce5ed") : QColor("#8795a3"));
}
}
CameraListWidget::CameraListWidget(QWidget *parent) : QWidget(parent) {
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    m_list = new QListWidget(this);
    m_list->setObjectName(QStringLiteral("cameraList"));
    m_list->setMinimumWidth(180);
    layout->addWidget(m_list);
    connect(m_list, &QListWidget::currentItemChanged, this, [this](QListWidgetItem *item) {
        emit cameraSelected(item ? item->data(Qt::UserRole).toString() : QString());
    });
}
// 목록 갱신 후에도 같은 cameraId의 선택을 유지하고, 없으면 선두 장치를 선택한다.
void CameraListWidget::setCameras(const QList<CameraInfo> &cameras) {
    const QString previous = selectedCameraId();
    {
        QSignalBlocker blocker(m_list);
        m_list->clear();
        int selected = 0;
        for (int i = 0; i < cameras.size(); ++i) {
            auto *item = new QListWidgetItem(m_list);
            displayCamera(item, cameras[i]);
            if (cameras[i].id == previous) selected = i;
        }
        if (!cameras.isEmpty()) m_list->setCurrentRow(selected);
    }
    emit cameraSelected(selectedCameraId());
}
void CameraListWidget::updateCamera(const CameraInfo &camera) {
    for (int i = 0; i < m_list->count(); ++i)
        if (m_list->item(i)->data(Qt::UserRole).toString() == camera.id)
            displayCamera(m_list->item(i), camera);
}
QString CameraListWidget::selectedCameraId() const {
    return m_list->currentItem() ? m_list->currentItem()->data(Qt::UserRole).toString() : QString();
}

void CameraListWidget::selectCamera(const QString &id) {
    for (int row = 0; row < m_list->count(); ++row)
        if (m_list->item(row)->data(Qt::UserRole).toString() == id) { m_list->setCurrentRow(row); return; }
}
