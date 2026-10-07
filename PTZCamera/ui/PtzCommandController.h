#pragma once
#include <QObject>
#include <QTimer>

// 입력 상태와 갱신 주기만 관리한다. 실제 VMS 송신은 외부 신호 연결로 처리한다.
class PtzCommandController : public QObject {
    Q_OBJECT
public:
    explicit PtzCommandController(QObject *parent = nullptr);
    void setTarget(const QString &cameraId, bool enabled);
    void setButtonMovement(float pan, float tilt);
    void setKeyboardMovement(float pan, float tilt);
    void cancel();
    void center();
signals:
    void moveRequested(const QString &cameraId, float pan, float tilt);
    void stopRequested(const QString &cameraId);
    void centerRequested(const QString &cameraId);
private:
    void updateMovement();
    QTimer m_refresh;
    QString m_cameraId;
    bool m_enabled = false;
    float m_buttonPan = 0, m_buttonTilt = 0, m_keyPan = 0, m_keyTilt = 0;
    float m_pan = 0, m_tilt = 0;
};
