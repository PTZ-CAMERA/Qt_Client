// 자동 추적 ON/OFF, 대상 종류와 추적 상태를 표시한다.
// 추적 알고리즘은 실행하지 않으며 외부에서 받은 상태를 화면에 반영한다.
#include "TrackingPanel.h"

#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>
#include <cmath>

// 토글 버튼과 대상/상태 행을 구성한다. 현재 선택 가능한 대상은 Person 하나다.
TrackingPanel::TrackingPanel(QWidget *parent) : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);
    auto *modeRow = new QHBoxLayout;
    modeRow->addWidget(new QLabel(QStringLiteral("Tracking mode"), this));
    modeRow->addStretch();
    m_toggle = new QPushButton(QStringLiteral("OFF"), this);
    m_toggle->setCheckable(true);
    m_toggle->setObjectName(QStringLiteral("trackingToggle"));
    m_toggle->setMinimumWidth(94);
    modeRow->addWidget(m_toggle);
    layout->addLayout(modeRow);

    auto *targetRow = new QHBoxLayout;
    targetRow->addWidget(new QLabel(QStringLiteral("Target"), this));
    targetRow->addStretch();
    auto *target = new QComboBox(this);
    m_target = target;
    target->addItem(QStringLiteral("Person"));
    target->setMinimumWidth(130);
    targetRow->addWidget(target);
    layout->addLayout(targetRow);

    auto *stateRow = new QHBoxLayout;
    stateRow->addWidget(new QLabel(QStringLiteral("Status"), this));
    stateRow->addStretch();
    m_state = new QLabel(QStringLiteral("IDLE"), this);
    m_state->setObjectName(QStringLiteral("trackingState"));
    stateRow->addWidget(m_state);
    layout->addLayout(stateRow);
    m_details = new QWidget(this);
    auto *details = new QFormLayout(m_details);
    details->setContentsMargins(0, 0, 0, 0);
    const auto addValue = [this, details](const QString &name, QLabel *&value) {
        value = new QLabel(QStringLiteral("—"), this);
        value->setObjectName(QStringLiteral("infoValue"));
        details->addRow(name, value);
    };
    addValue(QStringLiteral("Confidence"), m_confidence);
    addValue(QStringLiteral("Error X"), m_errorX);
    addValue(QStringLiteral("Error Y"), m_errorY);
    addValue(QStringLiteral("Pan command"), m_pan);
    addValue(QStringLiteral("Tilt command"), m_tilt);
    layout->addWidget(m_details);
    m_commandStatus = new QLabel(this); m_commandStatus->setObjectName(QStringLiteral("trackingCommandStatus"));
    m_commandStatus->setWordWrap(true); layout->addWidget(m_commandStatus);
    m_confirmation.setSingleShot(true); m_confirmation.setInterval(20000);
    connect(&m_confirmation,&QTimer::timeout,this,[this]{ cancelCommand(QStringLiteral("추적 상태 확인 시간 초과 · 실제 상태 미확인")); });
    // legacy 화면에서는 기존 Object Information 패널을 사용하므로 추가 상세 행을 숨긴다.
    m_details->hide();
    setState(State::Idle);

    connect(m_toggle, &QPushButton::toggled, this, [this](bool enabled) {
        // 사용자가 추적을 요청했다는 뜻이다. 장치에서 추적을 시작했다는 확인은 아니다.
        // 상태를 일단 IDLE로 두고 이후 탐지/소실 신호에 따라 TRACKING/LOST로 갱신한다.
        m_enabled = enabled;
        m_toggle->setText(enabled ? QStringLiteral("ON") : QStringLiteral("OFF"));
        setState(State::Idle);
        emit trackingChanged(enabled);
    });
}
void TrackingPanel::setControlAvailable(bool available) {
    m_available=available;
    if (!available && m_waiting) cancelCommand(QStringLiteral("추적 제어 연결을 확인하세요."));
    m_toggle->setEnabled(available && !m_waiting); m_target->setEnabled(available && !m_waiting);
}
void TrackingPanel::beginCommand(bool enabled) {
    m_desired=enabled; m_waiting=true; m_confirmation.start();
    m_commandStatus->setText(enabled ? QStringLiteral("추적 ON 요청 · VMS/Pi 응답 대기") : QStringLiteral("추적 OFF 요청 · VMS/Pi 응답 대기"));
    setControlAvailable(m_available);
}
void TrackingPanel::commandPhase(const QString &phase) {
    if (!m_waiting) return;
    if (phase==QStringLiteral("ACCEPTED")) m_commandStatus->setText(QStringLiteral("VMS 접수 · Pi 응답 대기"));
    else if (phase==QStringLiteral("PI_ACKNOWLEDGED")) m_commandStatus->setText(QStringLiteral("Pi ONVIF 응답 확인 · 실제 추적 상태 알림 대기"));
    else cancelCommand(QStringLiteral("추적 명령 실패/취소 · 확인된 상태 유지"));
}
void TrackingPanel::cancelCommand(const QString &reason) {
    m_waiting=false; m_confirmation.stop(); m_commandStatus->setText(reason);
    m_toggle->setEnabled(m_available); m_target->setEnabled(m_available);
}
void TrackingPanel::updateMetadata(const QJsonObject &data) {
    m_details->show(); const QSignalBlocker blocker(m_toggle);
    const auto enabled=data.value(QStringLiteral("tracking"));
    m_toggle->setText(enabled.isBool() ? enabled.toBool() ? QStringLiteral("ON") : QStringLiteral("OFF") : QStringLiteral("—"));
    m_enabled=enabled.isBool() && enabled.toBool(); m_toggle->setChecked(m_enabled);
    m_state->setText(data.value(QStringLiteral("targetState")).toString(enabled.isBool() ? m_enabled ? QStringLiteral("ENABLED") : QStringLiteral("IDLE") : QStringLiteral("UNKNOWN")));
    if (m_waiting && enabled.isBool() && m_enabled==m_desired) cancelCommand(QStringLiteral("Pi 메타데이터로 추적 %1 확인").arg(m_enabled ? QStringLiteral("ON") : QStringLiteral("OFF")));
    const auto number=[&](const char *key,const QString &suffix) {
        const auto value=data.value(QString::fromLatin1(key));
        return value.isDouble() && std::isfinite(value.toDouble()) ? QString::number(value.toDouble(),'f',0)+suffix : QStringLiteral("—");
    };
    const auto confidence=data.value(QStringLiteral("confidence"));
    m_confidence->setText(confidence.isDouble() && data.value(QStringLiteral("detected")).toBool() ? QString::number(confidence.toDouble()*100,'f',0)+QStringLiteral("%") : QStringLiteral("—"));
    m_errorX->setText(number("errorX",{})); m_errorY->setText(number("errorY",{}));
    m_pan->setText(number("panCommandAngle",QStringLiteral("°"))); m_tilt->setText(number("tiltCommandAngle",QStringLiteral("°")));
    setToolTip(QStringLiteral("ONVIF metadata · angles are commanded values · overlay is not frame-synchronized"));
}

// 외부 코드에서도 버튼을 클릭했을 때와 같은 상태 변경 경로를 사용하도록 한다.
// 체크 상태가 실제로 바뀌면 toggled 신호를 통해 trackingChanged도 전달된다.
void TrackingPanel::setTrackingEnabled(bool enabled)
{
    m_toggle->setChecked(enabled);
}

// 추적 상태를 문구와 색상으로 표시한다. 이 함수 자체는 추적 ON/OFF를 바꾸지 않는다.
void TrackingPanel::setState(State state)
{
    QString text;
    QString color;
    switch (state) {
    case State::Tracking: text = QStringLiteral("TRACKING"); color = QStringLiteral("#52d7a5"); break;
    case State::Lost: text = QStringLiteral("LOST"); color = QStringLiteral("#efb35d"); break;
    case State::Idle: text = QStringLiteral("IDLE"); color = QStringLiteral("#8795a3"); break;
    }
    m_state->setText(text);
    m_state->setStyleSheet(QStringLiteral("color: %1; font-weight: 700;").arg(color));
}

// 현재 UI에서 자동 추적을 요청한 상태인지 반환한다.
bool TrackingPanel::trackingEnabled() const
{
    return m_enabled;
}

// 외부 상태를 표시할 때는 사용자 토글 신호를 막아 같은 명령이 다시 송신되지 않게 한다.
void TrackingPanel::updateTrackingInfo(const TrackingInfo &info)
{
    m_details->show();
    QSignalBlocker blocker(m_toggle);
    m_enabled = info.enabled;
    m_toggle->setChecked(info.enabled);
    m_toggle->setText(info.enabled ? QStringLiteral("ON") : QStringLiteral("OFF"));
    m_target->setItemText(0, info.target);
    setState(info.status == QStringLiteral("TRACKING") ? State::Tracking
             : info.status == QStringLiteral("LOST") ? State::Lost : State::Idle);
    const auto signedValue = [](int value) { return value >= 0 ? QStringLiteral("+%1").arg(value) : QString::number(value); };
    m_confidence->setText(info.detected ? QStringLiteral("%1%").arg(qRound(info.confidence * 100)) : QStringLiteral("—"));
    m_errorX->setText(info.detected ? signedValue(info.errorX) : QStringLiteral("—"));
    m_errorY->setText(info.detected ? signedValue(info.errorY) : QStringLiteral("—"));
    m_pan->setText(QStringLiteral("%1°").arg(info.panAngle, 0, 'f', 0));
    m_tilt->setText(QStringLiteral("%1°").arg(info.tiltAngle, 0, 'f', 0));
    // 메타데이터 API가 없는 VMS에서는 기본 각도를 실제 상태처럼 표시하지 않는다.
    if (info.status == QStringLiteral("UNSUPPORTED")) {
        m_state->setText(QStringLiteral("UNSUPPORTED"));
        m_pan->setText(QStringLiteral("—")); m_tilt->setText(QStringLiteral("—"));
    }
}
