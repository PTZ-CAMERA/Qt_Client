#include "TrackingPanel.h"

#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

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
    setState(State::Idle);

    connect(m_toggle, &QPushButton::toggled, this, [this](bool enabled) {
        m_enabled = enabled;
        m_toggle->setText(enabled ? QStringLiteral("ON") : QStringLiteral("OFF"));
        setState(State::Idle);
        emit trackingChanged(enabled);
    });
}

void TrackingPanel::setTrackingEnabled(bool enabled)
{
    m_toggle->setChecked(enabled);
}

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

bool TrackingPanel::trackingEnabled() const
{
    return m_enabled;
}
