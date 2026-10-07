// 기존 QPainter CameraWidget을 감싸 카메라 이름과 독립적인 상태 표시를 추가한다.
// M1에는 서버 스트림 재생을 시작하지 않으며 UI 요청만 외부로 전달한다.
#include "CameraViewWidget.h"
#include "CameraWidget.h"
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QMediaPlayer>
#include <QVideoSink>
#include <QVideoFrame>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>
#include <QStandardItemModel>
CameraViewWidget::CameraViewWidget(QWidget *parent) : QWidget(parent) {
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    auto *top = new QHBoxLayout;
    m_name = new QLabel(QStringLiteral("Select a camera"), this);
    m_name->setObjectName(QStringLiteral("cameraHeading"));
    m_online = new QLabel(this); m_live = new QLabel(this); m_rec = new QLabel(this);
    top->addWidget(m_name, 1); top->addWidget(m_online); top->addWidget(m_live); top->addWidget(m_rec);
    layout->addLayout(top);
    auto *controls = new QHBoxLayout;
    controls->addWidget(new QLabel(QStringLiteral("Stream"), this));
    m_method = new QComboBox(this);
    m_method->setObjectName(QStringLiteral("rtspTransport"));
    m_method->addItem(QStringLiteral("RTSP TCP"), QStringLiteral("tcp"));
    m_method->addItem(QStringLiteral("RTSP UDP"), QStringLiteral("udp"));
    m_method->setToolTip(QStringLiteral("VMS RTSP transport: TCP interleaved or UDP RTP/RTCP"));
    controls->addWidget(m_method);
    auto *start = new QPushButton(QStringLiteral("Start"), this);
    auto *stop = new QPushButton(QStringLiteral("Stop"), this);
    controls->addWidget(start); controls->addWidget(stop);
    m_startRec = new QPushButton(QStringLiteral("REC START"), this); m_startRec->setObjectName(QStringLiteral("recordingStart"));
    m_stopRec = new QPushButton(QStringLiteral("REC STOP"), this); m_stopRec->setObjectName(QStringLiteral("recordingStop"));
    m_startRec->setEnabled(false); m_stopRec->setEnabled(false);
    controls->addWidget(m_startRec); controls->addWidget(m_stopRec); controls->addStretch();
    connect(m_startRec, &QPushButton::clicked, this, &CameraViewWidget::startRecordingRequested);
    connect(m_stopRec, &QPushButton::clicked, this, &CameraViewWidget::stopRecordingRequested);
    layout->addLayout(controls);
    m_view = new CameraWidget(this);
    // VMS의 좌측 목록/하단 탭과 함께 배치해도 영상 위젯이 창의 최소 크기를 과도하게 키우지 않는다.
    m_view->setMinimumSize(320, 180);
    layout->addWidget(m_view, 1);
    connect(start, &QPushButton::clicked, this, [this] { emit startRequested(m_method->currentText()); });
    connect(stop, &QPushButton::clicked, this, &CameraViewWidget::stopRequested);
    connect(m_method, &QComboBox::currentTextChanged, this, [this](const QString &method) {
        // 전송 방식 변경 시 이전 스트림과 진행 중인 URI 요청을 정리한다.
        stopStream(); emit stopRequested(); emit streamMethodChanged(method);
    });
    m_frameTimeout = new QTimer(this); m_frameTimeout->setSingleShot(true); m_frameTimeout->setInterval(15000);
    connect(m_frameTimeout, &QTimer::timeout, this, [this] { stopStream(); emit playbackError(QStringLiteral("No video frame received from VMS")); });
    setOnline(false); setRecording(false); setLive(false);
}
void CameraViewWidget::setDetection(const DetectionInfo &info) {
    if (!info.detected) { clearDetection(); return; }
    m_view->setDetection({info.boundingBox, info.label, info.confidence, info.objectCenter});
}
void CameraViewWidget::clearDetection() { m_view->clearDetection(); }
void CameraViewWidget::setCameraName(const QString &name) { m_name->setText(name); }
void CameraViewWidget::setRecording(bool recording) {
    m_rec->setText(recording ? QStringLiteral("● REC") : QStringLiteral("REC OFF"));
    m_rec->setStyleSheet(recording ? QStringLiteral("color: #ef7272;") : QStringLiteral("color: #8795a3;"));
}
void CameraViewWidget::setOnline(bool online) {
    m_online->setText(online ? QStringLiteral("● ONLINE") : QStringLiteral("● OFFLINE"));
    m_online->setStyleSheet(online ? QStringLiteral("color: #52d7a5;") : QStringLiteral("color: #8795a3;"));
}
void CameraViewWidget::setFrame(const QImage &frame) { m_view->setFrame(frame); }
void CameraViewWidget::setLive(bool live, bool simulated) {
    m_hasLiveFrame = live;
    m_live->setText(live ? (simulated ? QStringLiteral("LIVE (DUMMY)") : QStringLiteral("LIVE"))
                         : QStringLiteral("NO STREAM"));
    m_live->setStyleSheet(live ? QStringLiteral("color: #52d7a5;") : QStringLiteral("color: #8795a3;"));
}

void CameraViewWidget::startStream(const QUrl &uri) {
    stopStream();
    if (!uri.isValid() || uri.scheme() != QStringLiteral("rtsp") || !uri.userInfo().isEmpty()) {
        emit playbackError(QStringLiteral("Invalid VMS stream address")); return;
    }
    auto transport = QUrlQuery(uri).queryItemValue(QStringLiteral("transport"));
    if (transport.isEmpty()) transport = selectedTransport();
    if (transport != QStringLiteral("tcp") && transport != QStringLiteral("udp")) {
        emit playbackError(QStringLiteral("Invalid RTSP transport")); return;
    }
    qputenv("QT_FFMPEG_RTSP_TRANSPORT", transport.toUtf8());
    m_playing = true; const auto generation = m_generation;
    m_player = new QMediaPlayer(this); m_sink = new QVideoSink(this); m_player->setVideoSink(m_sink);
    connect(m_sink, &QVideoSink::videoFrameChanged, this, [this, generation](const QVideoFrame &frame) {
        if (generation != m_generation || !m_playing || !frame.isValid()) return;
        const auto image = frame.toImage(); if (image.isNull()) return;
        m_frameTimeout->stop(); setFrame(image); setLive(true); emit videoFrameReceived(image.size());
    });
    connect(m_player, &QMediaPlayer::errorOccurred, this, [this, generation](QMediaPlayer::Error, const QString &) {
        if (generation != m_generation || !m_playing) return;
        stopStream(); emit playbackError(QStringLiteral("VMS RTSP playback failed"));
    });
    m_frameTimeout->start();
    auto *player = m_player; player->setSource(uri);
    if (generation == m_generation && m_player == player) player->play();
}
void CameraViewWidget::stopStream() {
    ++m_generation; m_playing = false;
    if (m_frameTimeout) m_frameTimeout->stop();
    if (m_player) { m_player->disconnect(this); m_player->stop(); m_player->deleteLater(); m_player = nullptr; }
    if (m_sink) { m_sink->disconnect(this); m_sink->deleteLater(); m_sink = nullptr; }
    setLive(false); setFrame(QImage()); clearDetection();
}

void CameraViewWidget::setRecordingControls(bool canStart, bool canStop) { m_startRec->setEnabled(canStart); m_stopRec->setEnabled(canStop); }

QString CameraViewWidget::selectedTransport() const { return m_method->currentData().toString(); }
