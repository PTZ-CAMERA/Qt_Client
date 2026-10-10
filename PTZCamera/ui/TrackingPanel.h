#pragma once

#include <QWidget>
#include "model/TrackingInfo.h"
#include <QJsonObject>
#include <QTimer>

class QLabel;
class QPushButton;
class QComboBox;

class TrackingPanel : public QWidget
{
    Q_OBJECT
public:
    enum class State { Idle, Tracking, Lost };
    explicit TrackingPanel(QWidget *parent = nullptr);
    void setTrackingEnabled(bool enabled);
    void setState(State state);
    bool trackingEnabled() const;
    void updateTrackingInfo(const TrackingInfo &info);
    void updateMetadata(const QJsonObject &metadata);
    void setControlAvailable(bool available);
    void beginCommand(bool enabled);
    void commandPhase(const QString &phase);
    void cancelCommand(const QString &reason);

signals:
    void trackingChanged(bool enabled);

private:
    QPushButton *m_toggle = nullptr;
    QLabel *m_state = nullptr;
    QComboBox *m_target = nullptr;
    QWidget *m_details = nullptr;
    QLabel *m_confidence = nullptr;
    QLabel *m_errorX = nullptr;
    QLabel *m_errorY = nullptr;
    QLabel *m_pan = nullptr;
    QLabel *m_tilt = nullptr;
    bool m_enabled = false;
    bool m_available = false, m_waiting = false, m_desired = false;
    QLabel *m_commandStatus = nullptr;
    QTimer m_confirmation;
};
