// VMS 접속, 카메라 온라인, 영상 재생, 녹화 상태를 각각 독립적으로 표시한다.
#include "StatusIndicatorWidget.h"
StatusIndicatorWidget::StatusIndicatorWidget(const QString &name, QWidget *parent) : QLabel(parent), m_name(name) {
    setMinimumWidth(130); setState(QStringLiteral("Disconnected"), State::Inactive);
}
void StatusIndicatorWidget::setState(const QString &text, State state) {
    m_text = text; m_state = state;
    setText(QStringLiteral("%1 ● %2").arg(m_name, text));
    const QString color = state == State::Active ? QStringLiteral("#52d7a5")
        : state == State::Error ? QStringLiteral("#ef7272") : QStringLiteral("#8795a3");
    setStyleSheet(QStringLiteral("color: %1; padding: 3px 8px;").arg(color));
}
void StatusIndicatorWidget::setName(const QString &name) { m_name = name; setState(m_text, m_state); }
