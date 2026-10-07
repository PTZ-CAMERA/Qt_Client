#pragma once

#include <QWidget>

class QLabel;
class QPushButton;

class TrackingPanel : public QWidget
{
    Q_OBJECT
public:
    enum class State { Idle, Tracking, Lost };
    explicit TrackingPanel(QWidget *parent = nullptr);
    void setTrackingEnabled(bool enabled);
    void setState(State state);
    bool trackingEnabled() const;

signals:
    void trackingChanged(bool enabled);

private:
    QPushButton *m_toggle = nullptr;
    QLabel *m_state = nullptr;
    bool m_enabled = false;
};
