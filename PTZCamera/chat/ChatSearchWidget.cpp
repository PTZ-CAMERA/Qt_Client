#include "ChatSearchWidget.h"
#include <QDateTime>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QStandardItemModel>
#include <QTableView>
#include <QHeaderView>
#include <QVBoxLayout>
#include <cmath>
ChatSearchWidget::ChatSearchWidget(QWidget *parent):QWidget(parent) {
    auto *layout=new QVBoxLayout(this);
    auto *hint=new QLabel(QStringLiteral("예: 오늘 사람이 나온 기록을 신뢰도 90% 이상으로 찾아줘 · 시간 기준 Asia/Seoul · 영상 분석은 Pi, 조건 해석은 Gemini가 담당합니다."),this);
    hint->setWordWrap(true); layout->addWidget(hint);
    m_history=new QPlainTextEdit(this); m_history->setReadOnly(true); m_history->setMaximumBlockCount(300);
    m_history->setObjectName(QStringLiteral("chatHistory")); m_history->setMaximumHeight(110); layout->addWidget(m_history);
    auto *entry=new QHBoxLayout;
    m_input=new QLineEdit(this); m_input->setObjectName(QStringLiteral("chatInput")); m_input->setMaxLength(512);
    m_input->setPlaceholderText(QStringLiteral("검색할 날짜·시간·조건을 입력하세요. 두 번째 결과 재생 같은 후속 질문도 가능합니다."));
    m_send=new QPushButton(QStringLiteral("검색"),this); m_send->setObjectName(QStringLiteral("chatSend"));
    entry->addWidget(m_input,1); entry->addWidget(m_send); layout->addLayout(entry);
    m_model=new QStandardItemModel(this); m_model->setHorizontalHeaderLabels({QStringLiteral("시각"),QStringLiteral("카메라"),QStringLiteral("기록"),QStringLiteral("신뢰도"),QStringLiteral("녹화")});
    m_table=new QTableView(this); m_table->setObjectName(QStringLiteral("chatResults")); m_table->setModel(m_model);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows); m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers); m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch); m_table->verticalHeader()->hide();
    layout->addWidget(m_table,1);
    auto *bottom=new QHBoxLayout;
    m_status=new QLabel(QStringLiteral("VMS에 연결하고 카메라를 선택하세요."),this); m_status->setWordWrap(true);
    m_next=new QPushButton(QStringLiteral("다음 페이지"),this); m_next->setObjectName(QStringLiteral("chatNext"));
    m_play=new QPushButton(QStringLiteral("선택 녹화 재생"),this); m_play->setObjectName(QStringLiteral("chatPlay"));
    bottom->addWidget(m_status,1); bottom->addWidget(m_next); bottom->addWidget(m_play); layout->addLayout(bottom);
    connect(m_send,&QPushButton::clicked,this,[this]{submit(m_input->text());});
    connect(m_input,&QLineEdit::returnPressed,m_send,&QPushButton::click);
    connect(m_next,&QPushButton::clicked,this,[this]{submit(QStringLiteral("다음 페이지 보여줘"));});
    connect(m_play,&QPushButton::clicked,this,&ChatSearchWidget::openSelected);
    connect(m_table,&QTableView::doubleClicked,this,[this]{openSelected();});
    connect(m_table->selectionModel(),&QItemSelectionModel::selectionChanged,this,[this]{updateButtons();});
    m_next->setProperty("hasNext",false); updateButtons();
}
void ChatSearchWidget::setContext(const QString &camera,bool available) {
    const bool changed=m_cameraId!=camera || m_available!=(available && !camera.isEmpty());
    if (m_cameraId!=camera || !available) {
        m_requestId.clear(); m_records={}; m_model->removeRows(0,m_model->rowCount()); m_next->setProperty("hasNext",false);
        m_history->clear(); m_input->clear();
    }
    m_cameraId=camera; m_available=available && !camera.isEmpty();
    if (changed && m_requestId.isEmpty()) m_status->setText(m_available ? QStringLiteral("%1 · 검색 결과 재생은 추정 시각 기준입니다.").arg(camera) : QStringLiteral("VMS 연결 및 chatSearch capability가 필요합니다. Dummy에서는 실제 채팅을 보내지 않습니다."));
    updateButtons();
}
void ChatSearchWidget::updateButtons() {
    const bool ready=m_available && m_requestId.isEmpty(); m_send->setEnabled(ready); m_input->setEnabled(ready);
    m_next->setEnabled(ready && m_next->property("hasNext").toBool());
    const int row=m_table->currentIndex().row();
    m_play->setEnabled(ready && row>=0 && row<m_records.size() && m_records[row].toObject().value(QStringLiteral("playback")).toObject().value(QStringLiteral("playable")).toBool());
}
void ChatSearchWidget::submit(const QString &message) {
    if (!m_available || !m_requestId.isEmpty() || message.trimmed().isEmpty()) return;
    m_history->appendPlainText(QStringLiteral("나: %1").arg(message)); m_input->clear(); emit searchRequested(message);
}
void ChatSearchWidget::beginRequest(const QString &id) {
    m_requestId=id; m_status->setText(id.isEmpty() ? QStringLiteral("요청을 보내지 못했습니다.") : QStringLiteral("Gemini 조건 해석 및 VMS 검색 중…")); updateButtons();
}
void ChatSearchWidget::requestError(const QString &id,const QString &message) {
    if (id!=m_requestId) return;
    m_requestId.clear(); m_history->appendPlainText(QStringLiteral("오류: %1").arg(message)); m_status->setText(message); updateButtons();
}
void ChatSearchWidget::acceptResponse(const QString &id,const QJsonObject &data) {
    if (id!=m_requestId) return;
    m_requestId.clear(); m_history->appendPlainText(QStringLiteral("VMS: %1").arg(data.value(QStringLiteral("answer")).toString()));
    const auto action=data.value(QStringLiteral("action")).toString();
    if (action==QStringLiteral("search") || action==QStringLiteral("next_page")) {
        m_records=data.value(QStringLiteral("results")).toArray(); m_model->removeRows(0,m_model->rowCount());
        for (const auto &value:m_records) {
            const auto record=value.toObject(), playback=record.value(QStringLiteral("playback")).toObject();
            const auto confidence=record.value(QStringLiteral("confidence"));
            QList<QStandardItem*> row;
            for (const auto &text:{QDateTime::fromMSecsSinceEpoch(record.value(QStringLiteral("searchTimeMs")).toInteger(),Qt::UTC).toLocalTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz")),
                record.value(QStringLiteral("cameraId")).toString(),record.value(QStringLiteral("type")).toString(),
                confidence.isDouble() && std::isfinite(confidence.toDouble()) ? QString::number(confidence.toDouble()*100,'f',0)+QStringLiteral("%") : QStringLiteral("—"),
                playback.value(QStringLiteral("playable")).toBool() ? QStringLiteral("추정 시각 재생 가능") : QStringLiteral("녹화 없음")}) row.append(new QStandardItem(text));
            m_model->appendRow(row);
        }
        m_next->setProperty("hasNext",data.value(QStringLiteral("nextCursor")).isObject());
    }
    m_status->setText(data.value(QStringLiteral("answer")).toString()); updateButtons();
    if (action==QStringLiteral("select_result")) {
        const auto playback=data.value(QStringLiteral("playback")).toObject();
        if (playback.value(QStringLiteral("playable")).toBool()) emit selectedPlaybackReceived(playback);
    }
}
void ChatSearchWidget::openSelected() {
    if (!m_play->isEnabled()) return;
    const int row=m_table->currentIndex().row();
    if (row>=0 && row<m_records.size()) emit recordPlaybackRequested(m_records[row].toObject());
}
