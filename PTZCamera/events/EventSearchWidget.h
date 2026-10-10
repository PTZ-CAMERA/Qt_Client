#pragma once
#include "model/VmsTypes.h"
#include <QWidget>
#include <QJsonArray>
#include <QJsonObject>
class QComboBox;
class QDateEdit;
class QTimeEdit;
class QLabel;
class QTableView;
class QStandardItemModel;
class QPushButton;
class QSpinBox;
class EventSearchWidget : public QWidget {
    Q_OBJECT
public:
    explicit EventSearchWidget(QWidget *parent = nullptr);
    void setCameras(const QList<CameraInfo> &cameras);
    void setSelectedCamera(const QString &cameraId);
    void setEvents(const QList<EventInfo> &events);
    void setRealMode(bool real);
    void setSearchAvailable(bool available);
    void beginRequest(const QString &id);
    void acceptResults(const QString &id,const QJsonArray &records,const QJsonValue &cursor);
    void requestError(const QString &id,const QString &message);
    void resetResults();
signals:
    void queryInvalidated();
    void recordSearchRequested(const QString &cameraId,const QDateTime &start,const QDateTime &end,
        bool detections,const QString &type,double minConfidence,const QJsonValue &cursor);
    void recordPlaybackRequested(const QJsonObject &record);
    void searchRequested(const QString &cameraId, const QDateTime &start, const QDateTime &end, const QString &type);
    void eventPlaybackRequested(const QString &cameraId, const QDateTime &timestamp);
private:
    void submit(bool next);
    QComboBox *m_camera = nullptr;
    QComboBox *m_type = nullptr;
    QDateEdit *m_date = nullptr;
    QTimeEdit *m_start = nullptr;
    QTimeEdit *m_end = nullptr;
    QLabel *m_result = nullptr;
    QTableView *m_table = nullptr;
    QStandardItemModel *m_model = nullptr;
    QComboBox *m_kind = nullptr;
    QSpinBox *m_confidence = nullptr;
    QPushButton *m_search = nullptr,*m_next = nullptr;
    bool m_realMode = false,m_available = true;
    QString m_requestId;
    QJsonArray m_records;
    QJsonValue m_cursor;
};
