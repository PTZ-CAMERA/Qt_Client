// 녹화 목록은 VMS API로 조회한다. 같은 PC의 완료된 파일은 Qt Multimedia로 재생한다.
// 파일 접근과 검색을 분리하며 원격 VMS의 경로를 로컬 파일로 해석하지 않는다.
#include "PlaybackWidget.h"
#include "camera/CameraWidget.h"
#include <QComboBox>
#include <QCheckBox>
#include <QDateEdit>
#include <QTimeEdit>
#include <QFileDialog>
#include <QFileInfo>
#include <QFile>
#include <QDir>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QMediaPlayer>
#include <QVideoSink>
#include <QVideoFrame>
#include <QUrl>
#include <QVBoxLayout>
#include <algorithm>
#include <limits>

PlaybackWidget::PlaybackWidget(QWidget *parent) : QWidget(parent) {
    auto *layout = new QVBoxLayout(this);
    m_preRoll = new QCheckBox(QStringLiteral("탐지 결과는 2초 전부터 재생 (같은 녹화 파일 범위 내)"),this);
    m_preRoll->setObjectName(QStringLiteral("metadataPreRoll")); m_preRoll->setChecked(true); layout->addWidget(m_preRoll);
    auto *searchRow = new QHBoxLayout;
    m_camera = new QComboBox(this); m_date = new QDateEdit(QDate::currentDate(), this);
    m_date->setCalendarPopup(true); m_date->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
    m_fromTime = new QTimeEdit(QTime(0, 0), this); m_toTime = new QTimeEdit(QTime(23, 59, 59), this);
    m_fromTime->setDisplayFormat(QStringLiteral("HH:mm:ss")); m_toTime->setDisplayFormat(QStringLiteral("HH:mm:ss"));
    m_fromTime->setObjectName(QStringLiteral("recordingFromTime")); m_toTime->setObjectName(QStringLiteral("recordingToTime"));
    m_search = new QPushButton(QStringLiteral("Search recordings"), this);
    m_search->setObjectName(QStringLiteral("searchRecordings"));
    m_recordings = new QComboBox(this); m_recordings->setObjectName(QStringLiteral("recordingResults"));
    m_recordings->setPlaceholderText(QStringLiteral("Select a recording to play"));
    m_recordings->setMinimumWidth(0);
    m_recordings->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    m_open = new QPushButton(QStringLiteral("OPEN"), this);
    m_open->setObjectName(QStringLiteral("openRecording")); m_open->setEnabled(false);
    m_open->setToolTip(QStringLiteral("Play the selected completed recording"));
    auto *browse = new QPushButton(QStringLiteral("OPEN FILE"), this);
    browse->setObjectName(QStringLiteral("openRecordingFile"));
    browse->setToolTip(QStringLiteral("Choose a local recording file, including a copy from another VMS"));
    searchRow->addWidget(m_camera); searchRow->addWidget(m_date);
    searchRow->addWidget(m_fromTime); searchRow->addWidget(new QLabel(QStringLiteral("~"), this)); searchRow->addWidget(m_toTime);
    searchRow->addWidget(m_search); searchRow->addWidget(m_recordings, 1); searchRow->addWidget(m_open); searchRow->addWidget(browse);
    layout->addLayout(searchRow);

    auto *resultRow = new QHBoxLayout;
    m_resultCount = new QLabel(QStringLiteral("No search performed"), this);
    m_resultCount->setObjectName(QStringLiteral("recordingResultCount"));
    m_playbackState = new QLabel(this); m_playbackState->setObjectName(QStringLiteral("recordingPlaybackStatus"));
    resultRow->addWidget(m_resultCount); resultRow->addStretch(); resultRow->addWidget(m_playbackState);
    layout->addLayout(resultRow);
    m_videoStack = new QStackedWidget(this);
    m_video = new QLabel(this); m_video->setAlignment(Qt::AlignCenter); m_video->setMinimumHeight(60);
    m_video->setTextFormat(Qt::PlainText); m_video->setWordWrap(true);
    m_video->setTextInteractionFlags(Qt::TextSelectableByMouse); m_video->setObjectName(QStringLiteral("recordedVideo"));
    m_cameraView = new CameraWidget(this); m_cameraView->setMinimumSize(160, 90);
    m_videoStack->addWidget(m_video); m_videoStack->addWidget(m_cameraView); layout->addWidget(m_videoStack, 1);
    auto *timeRow = new QHBoxLayout;
    m_startTime = new QLabel(this); m_currentTime = new QLabel(this); m_endTime = new QLabel(this);
    timeRow->addWidget(m_startTime); timeRow->addStretch(); timeRow->addWidget(m_currentTime);
    timeRow->addStretch(); timeRow->addWidget(m_endTime); layout->addLayout(timeRow);
    m_timeline = new QSlider(Qt::Horizontal, this); m_timeline->setObjectName(QStringLiteral("recordingTimeline"));
    layout->addWidget(m_timeline);
    m_controls = new QWidget(this); auto *buttons = new QHBoxLayout(m_controls); buttons->setContentsMargins(0, 0, 0, 0);
    const auto add = [this, buttons](const QString &text, const QString &name) {
        auto *button = new QPushButton(text, this); button->setObjectName(name); buttons->addWidget(button); return button;
    };
    m_back = add(QStringLiteral("<< -10s"), QStringLiteral("recordingBack"));
    auto *play = add(QStringLiteral("Play"), QStringLiteral("recordingPlay"));
    auto *pause = add(QStringLiteral("Pause"), QStringLiteral("recordingPause"));
    auto *stop = add(QStringLiteral("Stop"), QStringLiteral("recordingStop"));
    m_forward = add(QStringLiteral("+10s >>"), QStringLiteral("recordingForward"));
    layout->addWidget(m_controls);

    // Dummy 타임라인은 실제 플레이어와 분리한다.
    m_timer.setInterval(1000);
    connect(&m_timer, &QTimer::timeout, this, [this] {
        if (m_timeline->value() >= m_timeline->maximum()) { m_timer.stop(); return; }
        m_timeline->setValue(m_timeline->value() + 1);
    });
    connect(m_timeline, &QSlider::valueChanged, this, [this](int seconds) {
        updateTime(seconds);
        if (!m_current.simulated && m_player && m_player->isSeekable() && m_player->duration() > 0) m_player->setPosition(qint64(seconds) * 1000);
    });
    connect(play, &QPushButton::clicked, this, [this] {
        if (m_current.simulated) m_timer.start();
        else if (m_player) m_player->play();
    });
    connect(pause, &QPushButton::clicked, this, [this] {
        m_timer.stop(); if (m_player) m_player->pause();
    });
    connect(stop, &QPushButton::clicked, this, [this] {
        m_timer.stop(); if (m_player) m_player->stop();
        const QSignalBlocker blocker(m_timeline); m_timeline->setValue(0); updateTime(0);
    });
    const auto seek = [this](int delta) {
        if (m_current.simulated) m_timeline->setValue(m_timeline->value() + delta);
        else if (m_player && m_player->isSeekable() && m_player->duration() > 0)
            m_player->setPosition(std::clamp<qint64>(m_player->position() + qint64(delta) * 1000, 0,
                m_durationMs > 0 ? m_durationMs : std::numeric_limits<qint64>::max()));
    };
    connect(m_back, &QPushButton::clicked, this, [seek] { seek(-10); });
    connect(m_forward, &QPushButton::clicked, this, [seek] { seek(10); });
    connect(m_search, &QPushButton::clicked, this, [this] {
        if (m_searchAvailable && !selectedCameraId().isEmpty() && m_fromTime->time() <= m_toTime->time()) {
            clear(); m_results.clear(); m_recordings->clear(); m_open->setEnabled(false);
            m_resultCount->setText(QStringLiteral("Searching VMS..."));
            emit searchRequested(selectedCameraId(), QDateTime(m_date->date(), m_fromTime->time()),
                QDateTime(m_date->date(), m_toTime->time()).addMSecs(999));
        }
    });
    connect(m_open, &QPushButton::clicked, this, &PlaybackWidget::openSelectedRecording);
    // 첫 결과도 명시적으로 선택할 수 있게 activated를 사용한다.
    connect(m_recordings, &QComboBox::activated, this, [this](int) { openSelectedRecording(); });
    connect(m_recordings, &QComboBox::currentIndexChanged, this, [this](int index) {
        m_open->setEnabled(index >= 0 && index < m_results.size());
    });
    connect(browse, &QPushButton::clicked, this, [this] {
        const auto file = QFileDialog::getOpenFileName(this, QStringLiteral("녹화 파일 선택"), QString(),
            QStringLiteral("Video (*.mkv *.mp4 *.avi *.mov *.webm);;All files (*)"));
        if (!file.isEmpty()) openLocalFile(file);
    });
    const auto invalidate = [this] {
        clear(); m_results.clear(); m_recordings->clear(); m_open->setEnabled(false);
        m_resultCount->setText(QStringLiteral("No search performed")); emit queryInvalidated();
    };
    connect(m_camera, &QComboBox::currentIndexChanged, this, invalidate);
    connect(m_date, &QDateEdit::dateChanged, this, invalidate);
    connect(m_fromTime, &QTimeEdit::timeChanged, this, invalidate);
    connect(m_toTime, &QTimeEdit::timeChanged, this, invalidate);
    m_loadTimeout.setSingleShot(true); m_loadTimeout.setInterval(15000);
    connect(&m_loadTimeout, &QTimer::timeout, this, [this] { showPlaybackError(QStringLiteral("No video frame received from the recording")); });
    setSearchAvailable(false); clear();
}

PlaybackWidget::~PlaybackWidget() { stopPlayer(); }

void PlaybackWidget::setCameras(const QList<CameraInfo> &cameras) {
    m_camera->clear(); for (const auto &camera : cameras) m_camera->addItem(camera.id, camera.id);
}
void PlaybackWidget::setSelectedCamera(const QString &id) {
    const int index = m_camera->findData(id); if (index >= 0) m_camera->setCurrentIndex(index);
}
void PlaybackWidget::setRecordings(const QList<RecordingInfo> &recordings) {
    clear(); m_results = recordings; m_recordings->clear();
    for (const auto &recording : recordings)
        m_recordings->addItem(QStringLiteral("%1 — %2%3").arg(recording.startTime.toString(QStringLiteral("HH:mm:ss")),
            recording.endTime.toString(QStringLiteral("HH:mm:ss")), recording.simulated ? QStringLiteral(" (DUMMY)")
                : QStringLiteral(" / ") + QFileInfo(recording.filePath).fileName()));
    m_recordings->setCurrentIndex(-1); m_open->setEnabled(false);
    m_resultCount->setText(QStringLiteral("%1 results / maximum 100 per VMS query").arg(recordings.size()));
    if (!recordings.isEmpty() && recordings.first().simulated)
        m_resultCount->setText(QStringLiteral("%1 DUMMY results / simulated timeline only").arg(recordings.size()));
    m_recordings->setToolTip(QStringLiteral("Select a file to start playback, or use OPEN"));
}
void PlaybackWidget::openSelectedRecording() {
    const int index = m_recordings->currentIndex();
    if (index >= 0 && index < m_results.size()) openRecording(m_results[index], m_results[index].startTime);
}
bool PlaybackWidget::openPlaybackResult(const QJsonObject &result) {
    clear();
    const QSignalBlocker cameraBlocker(m_camera), dateBlocker(m_date);
    if (!result.value(QStringLiteral("playable")).isBool() || !result.value(QStringLiteral("playable")).toBool()) {
        const auto reason=result.value(QStringLiteral("reason")).toString(QStringLiteral("UNKNOWN"));
        showPlaybackError(reason==QStringLiteral("RECORDING_IN_PROGRESS") ? QStringLiteral("녹화 중 · 파일 확정 대기. 녹화 중지 또는 구간 완료 후 다시 조회하세요.")
            : reason==QStringLiteral("NO_RECORDING_AT_TIME") ? QStringLiteral("탐지 기록은 있지만 해당 시각의 녹화가 없습니다.")
            : QStringLiteral("녹화 재생 불가: %1").arg(reason)); return false;
    }
    const auto record=result.value(QStringLiteral("recording")).toObject();
    const auto offset=result.value(QStringLiteral("offsetMs"));
    const auto start=record.value(QStringLiteral("startTimeMs")), end=record.value(QStringLiteral("endTimeMs"));
    const auto camera=result.value(QStringLiteral("cameraId")).toString();
    if (camera.isEmpty() || record.value(QStringLiteral("cameraId")).toString()!=camera || !offset.isDouble() || offset.toInteger(-1)<0
        || !start.isDouble() || !end.isDouble() || end.toInteger()<=start.toInteger()
        || !record.value(QStringLiteral("duration")).isDouble() || offset.toDouble()>=record.value(QStringLiteral("duration")).toDouble()*1000
        || record.value(QStringLiteral("filePath")).toString().isEmpty()) {
        showPlaybackError(QStringLiteral("Invalid VMS playback result")); return false;
    }
    m_current.cameraId=camera; m_current.startTime=QDateTime::fromMSecsSinceEpoch(start.toInteger(),Qt::UTC).toLocalTime();
    m_current.endTime=QDateTime::fromMSecsSinceEpoch(end.toInteger(),Qt::UTC).toLocalTime(); m_current.filePath=record.value(QStringLiteral("filePath")).toString();
    setSelectedCamera(camera); m_date->setDate(m_current.startTime.date());
    m_startTime->setText(m_current.startTime.toString(QStringLiteral("HH:mm:ss"))); m_endTime->setText(m_current.endTime.toString(QStringLiteral("HH:mm:ss")));
    if (!m_localVms) { showPlaybackError(QStringLiteral("원격 VMS 파일은 로컬에서 바로 열 수 없습니다. OPEN FILE로 복사본을 선택하세요.")); return false; }
    // 서버의 추정 위치를 보존하고 UI에서 선택한 사전 재생 시간만 뺀다. 이전 파일로 넘기지 않는다.
    const auto playbackOffset=std::max<qint64>(0,offset.toInteger()-(m_preRoll->isChecked() ? 2000 : 0));
    startFile(m_current.filePath,playbackOffset);
    if (!m_player) return false;
    m_resultCount->setText(result.value(QStringLiteral("timeMapping")).toString()==QStringLiteral("receive_estimated")
        ? QStringLiteral("탐지 기록 재생 · 추정 시각 %1 ms · 재생 시작 %2 ms").arg(offset.toInteger()).arg(playbackOffset) : QStringLiteral("탐지 기록 재생"));
    return m_player!=nullptr;
}
void PlaybackWidget::openRecording(const RecordingInfo &recording, const QDateTime &targetTime) {
    clear();
    const qint64 duration = recording.startTime.secsTo(recording.endTime);
    if (!recording.startTime.isValid() || !recording.endTime.isValid() || duration <= 0) {
        showPlaybackError(QStringLiteral("Invalid recording time range")); return;
    }
    m_current = recording;
    const QSignalBlocker cameraBlocker(m_camera), dateBlocker(m_date);
    setSelectedCamera(recording.cameraId); m_date->setDate(recording.startTime.date());
    m_startTime->setText(recording.startTime.toString(QStringLiteral("HH:mm:ss")));
    m_endTime->setText(recording.endTime.toString(QStringLiteral("HH:mm:ss")));
    const qint64 offset = targetTime.isValid() ? recording.startTime.secsTo(targetTime) : 0;
    if (recording.simulated) {
        m_video->setText(QStringLiteral("%1 / DUMMY PLAYBACK / TIMELINE PREVIEW\nNo recorded video is being decoded").arg(recording.cameraId));
        m_timeline->setRange(0, static_cast<int>(std::min<qint64>(duration, std::numeric_limits<int>::max())));
        m_timeline->setEnabled(true); m_controls->setEnabled(true); m_back->setEnabled(true); m_forward->setEnabled(true);
        m_timeline->setValue(static_cast<int>(std::clamp<qint64>(offset, 0, m_timeline->maximum())));
        updateTime(m_timeline->value()); m_playbackState->setText(QStringLiteral("DUMMY")); return;
    }
    if (!m_localVms) {
        showPlaybackError(QStringLiteral("The VMS is on another PC. Its file path is not local.\nUse OPEN FILE to play a local copy.\nVMS file: %1").arg(recording.filePath)); return;
    }
    startFile(recording.filePath, std::max<qint64>(0, offset) * 1000);
}
void PlaybackWidget::openLocalFile(const QString &path) {
    clear(); m_current.filePath = path; startFile(path, 0);
}
void PlaybackWidget::startFile(const QString &path, qint64 offsetMs) {
    QString localPath = QDir::fromNativeSeparators(path);
#ifdef Q_OS_WIN
    // 같은 PC의 WSL VMS가 반환한 Windows 드라이브 경로도 열 수 있다.
    if (localPath.startsWith(QStringLiteral("/mnt/")) && localPath.size() > 7 && localPath.at(6) == QLatin1Char('/')
        && localPath.at(5).toLower() >= QLatin1Char('a') && localPath.at(5).toLower() <= QLatin1Char('z'))
        localPath = localPath.mid(5, 1).toUpper() + QStringLiteral(":/") + localPath.mid(7);
#endif
    const QFileInfo file(localPath);
    if (!file.isAbsolute() || !file.isFile() || !file.isReadable() || file.fileName().endsWith(QStringLiteral(".part"), Qt::CaseInsensitive)) {
        showPlaybackError(QStringLiteral("Recording file is missing, unreadable or not finalized:\n%1\nUse OPEN FILE to select the saved MKV/MP4.").arg(path)); return;
    }
    stopPlayer(); const auto generation = m_generation; m_pendingSeekMs = offsetMs;
    m_receivedFrame = false;
    // 일부 백엔드는 duration을 늦게/0으로 보고한다. 서버 검색의 확정된 길이를 먼저 사용한다.
    m_durationMs = m_current.startTime.isValid() && m_current.endTime.isValid()
        ? std::max<qint64>(0, m_current.startTime.msecsTo(m_current.endTime)) : 0;
    { const QSignalBlocker blocker(m_timeline);
      m_timeline->setRange(0, static_cast<int>(std::min<qint64>(m_durationMs / 1000, std::numeric_limits<int>::max()))); }
    m_video->setText(QStringLiteral("Loading recording...\n%1").arg(file.absoluteFilePath()));
    m_videoStack->setCurrentWidget(m_video); m_playbackState->setText(QStringLiteral("Loading..."));
    m_playbackState->setToolTip(file.absoluteFilePath());
    m_player = new QMediaPlayer(this); m_sink = new QVideoSink(this); m_player->setVideoSink(m_sink);
    // 파일은 Qt가 직접 열어 한글/공백 경로도 처리한다. 플레이어가 파일 수명을 소유한다.
    auto *source = new QFile(file.absoluteFilePath(), m_player);
    if (!source->open(QIODevice::ReadOnly)) {
        showPlaybackError(QStringLiteral("Cannot open recording: %1").arg(source->errorString())); return;
    }
    connect(m_sink, &QVideoSink::videoFrameChanged, this, [this, generation](const QVideoFrame &frame) {
        if (generation != m_generation || !frame.isValid()) return;
        const auto image = frame.toImage(); if (image.isNull()) return;
        if (m_pendingSeekMs>0) {
            if (!m_player->isSeekable() || m_player->duration()<=0) {
                return; // Loaded 후 metadata가 준비될 수 있다. 시각 이동 전 frame은 표시하지 않는다.
            }
            const auto offset=m_pendingSeekMs; m_pendingSeekMs=0; m_player->setPosition(offset); return;
        }
        m_loadTimeout.stop(); m_receivedFrame = true;
        m_cameraView->setFrame(image); m_videoStack->setCurrentWidget(m_cameraView); m_controls->setEnabled(true);
        const bool seekable = m_player->isSeekable() && m_player->duration() > 0;
        m_timeline->setEnabled(seekable); m_back->setEnabled(seekable); m_forward->setEnabled(seekable);
        m_timeline->setToolTip(seekable ? QString() : QStringLiteral("This backend has not provided a seekable duration"));
        emit playbackFrameReceived(image.size());
    });
    connect(m_player, &QMediaPlayer::durationChanged, this, [this, generation](qint64 duration) {
        if (generation != m_generation || duration <= 0) return;
        m_durationMs = duration;
        const QSignalBlocker blocker(m_timeline);
        m_timeline->setRange(0, static_cast<int>(std::min<qint64>(duration / 1000, std::numeric_limits<int>::max())));
        m_timeline->setEnabled(m_receivedFrame && m_player->isSeekable());
        m_back->setEnabled(m_player->isSeekable()); m_forward->setEnabled(m_player->isSeekable());
        if (!m_current.startTime.isValid()) {
            m_startTime->setText(QStringLiteral("00:00:00")); m_endTime->setText(QTime(0, 0).addSecs(int(duration / 1000)).toString(QStringLiteral("HH:mm:ss")));
        }
    });
    connect(m_player, &QMediaPlayer::positionChanged, this, [this, generation](qint64 position) {
        if (generation != m_generation) return;
        const QSignalBlocker blocker(m_timeline);
        m_timeline->setValue(static_cast<int>(std::min<qint64>(position / 1000, std::numeric_limits<int>::max())));
        updateTime(m_timeline->value());
    });
    connect(m_player, &QMediaPlayer::seekableChanged, this, [this, generation](bool seekable) {
        if (generation != m_generation) return;
        const bool canSeek = seekable && m_player->duration() > 0;
        m_timeline->setEnabled(canSeek && m_receivedFrame); m_back->setEnabled(canSeek); m_forward->setEnabled(canSeek);
    });
    connect(m_player, &QMediaPlayer::mediaStatusChanged, this, [this, generation](QMediaPlayer::MediaStatus status) {
        if (generation != m_generation) return;
        if (status == QMediaPlayer::LoadedMedia && m_pendingSeekMs > 0 && m_player->isSeekable() && m_player->duration() > 0) {
            if (m_pendingSeekMs>=m_player->duration()) { showPlaybackError(QStringLiteral("검색 시각이 실제 파일 재생 범위 밖입니다.")); return; }
            const auto offset=m_pendingSeekMs; m_pendingSeekMs=0; m_player->setPosition(offset);
        }
        if ((status==QMediaPlayer::LoadedMedia || status==QMediaPlayer::BufferedMedia) && m_pendingSeekMs>0) {
            QTimer::singleShot(500,this,[this,generation]{
                if (generation!=m_generation || !m_player || m_pendingSeekMs<=0) return;
                if (m_player->isSeekable() && m_player->duration()>0 && m_pendingSeekMs<m_player->duration()) {
                    const auto offset=m_pendingSeekMs; m_pendingSeekMs=0; m_player->setPosition(offset);
                } else showPlaybackError(QStringLiteral("이 재생 backend는 해당 시각으로 이동할 수 없습니다. 파일 처음부터 재생을 원하면 OPEN FILE을 사용하세요."));
            });
        }
        if (status == QMediaPlayer::EndOfMedia) m_playbackState->setText(QStringLiteral("Finished"));
    });
    connect(m_player, &QMediaPlayer::playbackStateChanged, this, [this, generation](QMediaPlayer::PlaybackState state) {
        if (generation != m_generation) return;
        m_playbackState->setText(state == QMediaPlayer::PlayingState ? QStringLiteral("Playing")
            : state == QMediaPlayer::PausedState ? QStringLiteral("Paused") : QStringLiteral("Stopped"));
    });
    connect(m_player, &QMediaPlayer::errorOccurred, this, [this, generation](QMediaPlayer::Error, const QString &message) {
        if (generation == m_generation) showPlaybackError(QStringLiteral("Cannot play recording: %1").arg(message));
    });
    m_loadTimeout.start();
    auto *player = m_player;
    // Qt 6.4 sourceDevice는 duration/seek가 없을 수 있어, 시각 검색은 로컬 URL로 연다.
    if (offsetMs>0) player->setSource(QUrl::fromLocalFile(file.absoluteFilePath()));
    else player->setSourceDevice(source, QUrl::fromLocalFile(file.absoluteFilePath()));
    // setSource도 즉시 오류 신호를 낼 수 있어, 정리된 플레이어를 다시 호출하지 않는다.
    if (generation == m_generation && m_player == player) player->play();
}
void PlaybackWidget::stopPlayer() {
    ++m_generation; m_loadTimeout.stop(); m_receivedFrame = false;
    if (m_player) { m_player->disconnect(this); m_player->stop(); m_player->deleteLater(); m_player = nullptr; }
    if (m_sink) { m_sink->disconnect(this); m_sink->deleteLater(); m_sink = nullptr; }
}
void PlaybackWidget::showPlaybackError(const QString &message) {
    stopPlayer(); m_timer.stop(); m_controls->setEnabled(false); m_timeline->setEnabled(false);
    m_video->setText(message); m_videoStack->setCurrentWidget(m_video); m_playbackState->setText(QStringLiteral("Playback unavailable"));
    emit playbackFailed(message);
}
void PlaybackWidget::clear() {
    m_timer.stop(); stopPlayer(); m_current = RecordingInfo{};
    const QSignalBlocker blocker(m_timeline); m_timeline->setValue(0); m_timeline->setEnabled(false); m_controls->setEnabled(false);
    m_cameraView->setFrame(QImage()); m_videoStack->setCurrentWidget(m_video);
    m_video->setText(QStringLiteral("RECORDED VIDEO\nSearch recordings, then select a file to play\nOr use OPEN FILE to choose a saved video"));
    m_playbackState->setText(QStringLiteral("No recording selected")); m_playbackState->setToolTip(QString());
    m_startTime->setText(QStringLiteral("--:--:--")); m_endTime->setText(QStringLiteral("--:--:--")); m_currentTime->setText(QStringLiteral("--:--:--"));
}
void PlaybackWidget::updateTime(int seconds) {
    m_currentTime->setText(m_current.startTime.isValid() ? m_current.startTime.addSecs(seconds).toString(QStringLiteral("HH:mm:ss"))
        : QTime(0, 0).addSecs(seconds).toString(QStringLiteral("HH:mm:ss")));
}
QString PlaybackWidget::selectedCameraId() const { return m_camera->currentData().toString(); }
void PlaybackWidget::setSearchAvailable(bool available) {
    m_searchAvailable = available; m_search->setEnabled(available);
    m_search->setToolTip(available ? QString() : QStringLiteral("Connect to a VMS supporting recording search"));
    if (!available) { setRecordings({}); m_resultCount->setText(QStringLiteral("Recording search unavailable / OPEN FILE remains available")); }
}
void PlaybackWidget::setSearchError(const QString &message) {
    setRecordings({}); m_resultCount->setText(QStringLiteral("Search failed: %1").arg(message));
}
