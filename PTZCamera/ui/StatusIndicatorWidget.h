#pragma once
#include <QLabel>
class StatusIndicatorWidget : public QLabel {
    Q_OBJECT
public:
    enum class State { Inactive, Active, Error };
    explicit StatusIndicatorWidget(const QString &name, QWidget *parent = nullptr);
    void setState(const QString &text, State state);
    void setName(const QString &name);
private:
    QString m_name;
    QString m_text;
    State m_state = State::Inactive;
};
