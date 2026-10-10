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
#include <QSpinBox>
#include <QSignalBlocker>
#include <cmath>
EventSearchWidget::EventSearchWidget(QWidget *parent) : QWidget(parent) {
    auto *layout = new QVBoxLayout(this);
    auto *filters = new QHBoxLayout;
    m_camera = new QComboBox(this);
    m_camera->setObjectName(QStringLiteral("eventCamera"));
    m_date = new QDateEdit(QDate::currentDate(), this);
    m_date->setCalendarPopup(true); m_date->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
    m_start = new QTimeEdit(QTime(0, 0), this);
    m_end = new QTimeEdit(QTime(23, 59, 59), this);
    m_start->setDisplayFormat(QStringLiteral("HH:mm:ss")); m_end->setDisplayFormat(QStringLiteral("HH:mm:ss"));
    m_type = new QComboBox(this);
    m_kind = new QComboBox(this); m_kind->setObjectName(QStringLiteral("eventRecordKind"));
    m_kind->addItem(QStringLiteral("탐지 샘플"),QStringLiteral("detections")); m_kind->addItem(QStringLiteral("상태 이력"),QStringLiteral("events"));
    m_confidence = new QSpinBox(this); m_confidence->setObjectName(QStringLiteral("eventMinConfidence"));
    m_confidence->setRange(0,100); m_confidence->setSuffix(QStringLiteral("%")); m_confidence->setSpecialValueText(QStringLiteral("제한 없음"));
    m_confidence->setToolTip(QStringLiteral("confidence가 없는 기록은 최소 신뢰도 조건에 포함되지 않습니다."));
    m_type->addItems({QStringLiteral("ALL"), QStringLiteral("PERSON_DETECTED"), QStringLiteral("PERSON_LOST"),
        QStringLiteral("TRACKING_STARTED"), QStringLiteral("TRACKING_STOPPED"),
        QStringLiteral("CAMERA_CONNECTED"), QStringLiteral("CAMERA_DISCONNECTED")});
    auto *search = new QPushButton(QStringLiteral("SEARCH"), this);
    m_search=search; search->setObjectName(QStringLiteral("eventSearchButton"));
    m_next = new QPushButton(QStringLiteral("다음 페이지"),this); m_next->setObjectName(QStringLiteral("eventNextPage")); m_next->setEnabled(false);
    filters->addWidget(new QLabel(QStringLiteral("Camera"), this)); filters->addWidget(m_camera);
    filters->addWidget(m_date); filters->addWidget(m_start); filters->addWidget(m_end);
    filters->addWidget(m_kind); filters->addWidget(m_type, 1); filters->addWidget(m_confidence); filters->addWidget(search); filters->addWidget(m_next);
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
    connect(search,&QPushButton::clicked,this,[this]{submit(false);});
    connect(m_next,&QPushButton::clicked,this,[this]{submit(true);});
    connect(m_table, &QTableView::doubleClicked, this, [this](const QModelIndex &index) {
        if (m_realMode) { if (index.row()>=0 && index.row()<m_records.size()) emit recordPlaybackRequested(m_records[index.row()].toObject()); return; }
        const auto first = m_model->index(index.row(), 0);
        emit eventPlaybackRequested(first.data(Qt::UserRole).toString(), first.data(Qt::UserRole + 1).toDateTime());
    });
    const auto invalidate=[this]{resetResults();emit queryInvalidated();};
    connect(m_camera,&QComboBox::currentIndexChanged,this,invalidate);
    connect(m_type,&QComboBox::currentIndexChanged,this,invalidate);
    connect(m_kind,&QComboBox::currentIndexChanged,this,[this,invalidate]{setRealMode(m_realMode);invalidate();});
    connect(m_confidence,&QSpinBox::valueChanged,this,invalidate);
    connect(m_date,&QDateEdit::dateChanged,this,invalidate);
    connect(m_start,&QTimeEdit::timeChanged,this,invalidate); connect(m_end,&QTimeEdit::timeChanged,this,invalidate);
    setRealMode(false);
}
void EventSearchWidget::setCameras(const QList<CameraInfo> &cameras) {
    const QSignalBlocker blocker(m_camera);
    const QString selected = m_camera->currentData().toString(); m_camera->clear();
    for (const auto &camera : cameras) m_camera->addItem(camera.id, camera.id);
    setSelectedCamera(selected);
    if (selected!=m_camera->currentData().toString()) { resetResults(); emit queryInvalidated(); }
}
void EventSearchWidget::setRealMode(bool real) {
    m_realMode=real; const QSignalBlocker blocker(m_type); const auto selected=m_type->currentText();
    m_type->clear(); m_type->addItems({QStringLiteral("ALL"),QStringLiteral("PERSON_DETECTED"),QStringLiteral("PERSON_LOST"),QStringLiteral("TRACKING_STARTED"),QStringLiteral("TRACKING_STOPPED")});
    if (!real) m_type->addItems({QStringLiteral("CAMERA_CONNECTED"),QStringLiteral("CAMERA_DISCONNECTED")});
    const auto index=m_type->findText(selected); if (index>=0) m_type->setCurrentIndex(index);
    m_kind->setVisible(real); m_confidence->setVisible(real); m_next->setVisible(real);
    m_type->setEnabled(!real || m_kind->currentData()==QStringLiteral("events"));
}
void EventSearchWidget::setSearchAvailable(bool available) {
    m_available=available; m_search->setEnabled(available && m_requestId.isEmpty());
    m_next->setEnabled(available && m_requestId.isEmpty() && m_cursor.isObject());
    if (!available) resetResults();
}
void EventSearchWidget::resetResults() {
    m_requestId.clear(); m_cursor=QJsonValue(); m_records={}; m_model->removeRows(0,m_model->rowCount());
    m_next->setEnabled(false); m_search->setEnabled(m_available); m_result->setText(QStringLiteral("검색 조건을 선택하세요. 녹화 재생은 추정 시각 기준입니다."));
}
void EventSearchWidget::submit(bool next) {
    const QDateTime start(m_date->date(),m_start->time()), end(m_date->date(),m_end->time());
    if (!m_available || !m_requestId.isEmpty() || m_camera->currentData().toString().isEmpty() || start>end) {
        m_result->setText(QStringLiteral("Select a camera and a valid time range / connect to VMS")); return;
    }
    if (!m_realMode) { emit searchRequested(m_camera->currentData().toString(),start,end,m_type->currentText()); return; }
    // 화면 입력은 초 단위이므로 마지막 초의 999ms도 검색 범위에 포함한다.
    emit recordSearchRequested(m_camera->currentData().toString(),start,end.addMSecs(999),m_kind->currentData()==QStringLiteral("detections"),m_type->currentText(),m_confidence->value()/100.0,next ? m_cursor : QJsonValue());
}
void EventSearchWidget::beginRequest(const QString &id) {
    m_requestId=id; m_search->setEnabled(id.isEmpty() && m_available); m_next->setEnabled(false);
    m_result->setText(id.isEmpty() ? QStringLiteral("검색 요청을 보내지 못했습니다.") : QStringLiteral("VMS에서 검색 중…"));
}
void EventSearchWidget::requestError(const QString &id,const QString &message) {
    if (id!=m_requestId) return;
    m_requestId.clear(); m_search->setEnabled(m_available); m_next->setEnabled(false); m_result->setText(message);
}
void EventSearchWidget::acceptResults(const QString &id,const QJsonArray &records,const QJsonValue &cursor) {
    if (id!=m_requestId || !m_realMode) return;
    m_requestId.clear(); m_records=records; m_cursor=cursor;
    m_model->removeRows(0,m_model->rowCount());
    for (const auto &value:records) {
        const auto record=value.toObject();
        const auto optional=[](const QJsonValue &v,bool percent=false){return v.isDouble() && std::isfinite(v.toDouble()) ? QString::number(v.toDouble()*(percent ? 100 : 1),'f',0)+(percent ? QStringLiteral("%") : QString()) : QStringLiteral("—");};
        QList<QStandardItem*> row;
        for (const auto &text:{QDateTime::fromMSecsSinceEpoch(record.value(QStringLiteral("searchTimeMs")).toInteger(),Qt::UTC).toLocalTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz")),
            record.value(QStringLiteral("cameraId")).toString(),record.value(QStringLiteral("type")).toString(),optional(record.value(QStringLiteral("confidence")),true),
            optional(record.value(QStringLiteral("panCommandAngle"))),optional(record.value(QStringLiteral("tiltCommandAngle")))}) row.append(new QStandardItem(text));
        m_model->appendRow(row);
    }
    m_search->setEnabled(m_available); m_next->setEnabled(m_available && cursor.isObject());
    m_result->setText(QStringLiteral("현재 페이지 %1개 · 더블클릭하여 녹화 위치 조회 · 추정 시각").arg(records.size()));
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
