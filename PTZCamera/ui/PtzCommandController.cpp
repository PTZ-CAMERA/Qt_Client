#include "PtzCommandController.h"
#include <algorithm>
#include <cmath>

PtzCommandController::PtzCommandController(QObject *parent) : QObject(parent) {
    m_refresh.setInterval(200);
    connect(&m_refresh, &QTimer::timeout, this, [this] {
        if (m_enabled && !m_cameraId.isEmpty()) emit moveRequested(m_cameraId, m_pan, m_tilt);
    });
}
void PtzCommandController::setTarget(const QString &cameraId, bool enabled) {
    if (cameraId != m_cameraId || (!enabled && m_enabled)) cancel();
    m_cameraId = cameraId; m_enabled = enabled && !cameraId.isEmpty();
}
void PtzCommandController::setButtonMovement(float pan, float tilt) {
    if (!m_enabled || !std::isfinite(pan) || !std::isfinite(tilt)) return;
    m_buttonPan = pan; m_buttonTilt = tilt; updateMovement();
}
void PtzCommandController::setKeyboardMovement(float pan, float tilt) {
    if (!m_enabled || !std::isfinite(pan) || !std::isfinite(tilt)) return;
    m_keyPan = pan; m_keyTilt = tilt; updateMovement();
}
void PtzCommandController::updateMovement() {
    const float pan = std::clamp(m_buttonPan + m_keyPan, -0.5F, 0.5F);
    const float tilt = std::clamp(m_buttonTilt + m_keyTilt, -0.5F, 0.5F);
    if (pan == 0 && tilt == 0) {
        const bool moving = m_refresh.isActive(); m_refresh.stop(); m_pan = m_tilt = 0;
        if (moving) emit stopRequested(m_cameraId);
        return;
    }
    if (!m_refresh.isActive() || pan != m_pan || tilt != m_tilt) {
        m_pan = pan; m_tilt = tilt; m_refresh.start(); emit moveRequested(m_cameraId, pan, tilt);
    }
}
void PtzCommandController::cancel() {
    const bool stop = m_enabled && !m_cameraId.isEmpty();
    m_refresh.stop(); m_buttonPan = m_buttonTilt = m_keyPan = m_keyTilt = m_pan = m_tilt = 0;
    if (stop) emit stopRequested(m_cameraId);
}
void PtzCommandController::center() {
    if (!m_enabled || m_cameraId.isEmpty()) return;
    // CENTER 자체가 이동 모드를 바꾼다. STOP 응답을 기다리지 않고 두 명령을 연속 송신하지 않는다.
    m_refresh.stop(); m_buttonPan = m_buttonTilt = m_keyPan = m_keyTilt = m_pan = m_tilt = 0;
    emit centerRequested(m_cameraId);
}
