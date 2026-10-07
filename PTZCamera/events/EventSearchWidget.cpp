// 검색 조건과 결과 표만 담당한다. 서버/DB 조회는 searchRequested를 받은 외부 객체가 수행한다.
#include "EventSearchWidget.h"
#include <QComboBox>
#include <QDateEdit>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStandardItemModel>
#include <QTableView>
#include <QTimeEdit>
#include <QVBoxLayout>
EventSearchWidget::EventSearchWidget(QWidget *parent) : QWidget(parent) {
    auto *layout = new QVBoxLayout(this);
    auto *filters = new QHBoxLayout;
    m_camera = new QComboBox(this);
    m_date = new QDateEdit(QDate::currentDate(), this);
    m_date->setCalendarPopup(true); m_date->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
    m_start = new QTimeEdit(QTime(0, 0), this);
    m_end = new QTimeEdit(QTime(23, 59, 59), this);
    m_start->setDisplayFormat(QStringLiteral("HH:mm:ss")); m_end->setDisplayFormat(QStringLiteral("HH:mm:ss"));
    m_type = new QComboBox(this);
    m_type->addItems({QStringLiteral("ALL"), QStringLiteral("PERSON_DETECTED"), QStringLiteral("PERSON_LOST"),
        QStringLiteral("TRACKING_STARTED"), QStringLiteral("TRACKING_STOPPED"),
        QStringLiteral("CAMERA_CONNECTED"), QStringLiteral("CAMERA_DISCONNECTED")});
    auto *search = new QPushButton(QStringLiteral("SEARCH"), this);
    filters->addWidget(new QLabel(QStringLiteral("Camera"), this)); filters->addWidget(m_camera);
    filters->addWidget(m_date); filters->addWidget(m_start); filters->addWidget(m_end);
    filters->addWidget(m_type, 1); filters->addWidget(search);
    layout->addLayout(filters);
    m_model = new QStandardItemModel(this);
    m_model->setHorizontalHeaderLabels({QStringLiteral("TIME"), QStringLiteral("CAMERA"), QStringLiteral("TYPE"),
        QStringLiteral("CONFIDENCE"), QStringLiteral("PAN"), QStringLiteral("TILT")});
    m_table = new QTableView(this); m_table->setModel(m_model);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_table->verticalHeader()->hide();
    layout->addWidget(m_table, 1);
    m_result = new QLabel(QStringLiteral("No event results"), this); layout->addWidget(m_result);
    connect(search, &QPushButton::clicked, this, [this] {
        const QDateTime start(m_date->date(), m_start->time()), end(m_date->date(), m_end->time());
        if (m_camera->currentData().toString().isEmpty() || start > end) {
            m_result->setText(QStringLiteral("Select a camera and a valid time range")); return;
        }
        emit searchRequested(m_camera->currentData().toString(), start, end, m_type->currentText());
    });
    connect(m_table, &QTableView::doubleClicked, this, [this](const QModelIndex &index) {
        const auto first = m_model->index(index.row(), 0);
        emit eventPlaybackRequested(first.data(Qt::UserRole).toString(), first.data(Qt::UserRole + 1).toDateTime());
    });
}
void EventSearchWidget::setCameras(const QList<CameraInfo> &cameras) {
    const QString selected = m_camera->currentData().toString(); m_camera->clear();
    for (const auto &camera : cameras) m_camera->addItem(camera.id, camera.id);
    setSelectedCamera(selected);
}
void EventSearchWidget::setSelectedCamera(const QString &id) {
    const int index = m_camera->findData(id); if (index >= 0) m_camera->setCurrentIndex(index);
}
void EventSearchWidget::setEvents(const QList<EventInfo> &events) {
    m_model->removeRows(0, m_model->rowCount());
    for (const auto &event : events) {
        QList<QStandardItem *> row;
        for (const auto &text : {event.timestamp.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")), event.cameraId, event.type,
             event.type == QStringLiteral("PERSON_DETECTED") ? QStringLiteral("%1%").arg(qRound(event.confidence * 100)) : QStringLiteral("—"),
             QString::number(event.panAngle, 'f', 0), QString::number(event.tiltAngle, 'f', 0)})
            row.append(new QStandardItem(text));
        row.first()->setData(event.cameraId, Qt::UserRole);
        row.first()->setData(event.timestamp, Qt::UserRole + 1);
        m_model->appendRow(row);
    }
    m_result->setText(QStringLiteral("%1 events").arg(events.size()));
}
