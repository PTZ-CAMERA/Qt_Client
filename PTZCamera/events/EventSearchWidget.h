#pragma once
#include "model/VmsTypes.h"
#include <QWidget>
class QComboBox;
class QDateEdit;
class QTimeEdit;
class QLabel;
class QTableView;
class QStandardItemModel;
class EventSearchWidget : public QWidget {
    Q_OBJECT
public:
    explicit EventSearchWidget(QWidget *parent = nullptr);
    void setCameras(const QList<CameraInfo> &cameras);
    void setSelectedCamera(const QString &cameraId);
    void setEvents(const QList<EventInfo> &events);
signals:
    void searchRequested(const QString &cameraId, const QDateTime &start, const QDateTime &end, const QString &type);
    void eventPlaybackRequested(const QString &cameraId, const QDateTime &timestamp);
private:
    QComboBox *m_camera = nullptr;
    QComboBox *m_type = nullptr;
    QDateEdit *m_date = nullptr;
    QTimeEdit *m_start = nullptr;
    QTimeEdit *m_end = nullptr;
    QLabel *m_result = nullptr;
    QTableView *m_table = nullptr;
    QStandardItemModel *m_model = nullptr;
};
