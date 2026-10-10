// 방향키의 누름/놓음을 PTZ 신호로 변환한다. 앱 비활성화와 포커스 이탈 시에도 정지한다.
#include "PtzKeyboardController.h"
#include <QAbstractItemView>
#include <QAbstractSpinBox>
#include <QApplication>
#include <QComboBox>
#include <QEvent>
#include <QKeyEvent>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QTextEdit>
#include <QWidget>
PtzKeyboardController::PtzKeyboardController(QWidget *window) : QObject(window), m_window(window) {
    qApp->installEventFilter(this);
    connect(qApp, &QApplication::focusChanged, this, [this] {
        if (!m_enabled) return;
        stop(); emit inputCancelled();
    });
}
void PtzKeyboardController::setEnabled(bool enabled) { if (!enabled) stop(); m_enabled = enabled; }
void PtzKeyboardController::stop() {
    if (m_pressed.isEmpty()) return;
    m_pressed.clear(); emit stopRequested();
}
void PtzKeyboardController::updateMovement() {
    const float pan = 0.5F * (int(m_pressed.contains(Qt::Key_Right) || m_pressed.contains(Qt::Key_D))
        - int(m_pressed.contains(Qt::Key_Left) || m_pressed.contains(Qt::Key_A)));
    const float tilt = 0.5F * (int(m_pressed.contains(Qt::Key_Up) || m_pressed.contains(Qt::Key_W))
        - int(m_pressed.contains(Qt::Key_Down) || m_pressed.contains(Qt::Key_S)));
    if (pan == 0 && tilt == 0) emit stopRequested(); else emit moveRequested(pan, tilt);
}
bool PtzKeyboardController::eventFilter(QObject *watched, QEvent *event) {
    Q_UNUSED(watched)
    // 마우스 이동 버튼을 누른 채 앱을 떠난 경우에도 정지 의도를 전달한다.
    if (m_enabled && (event->type() == QEvent::ApplicationDeactivate
        || (event->type() == QEvent::WindowDeactivate && watched == m_window))) {
        if (m_pressed.isEmpty()) emit stopRequested(); else stop();
        emit inputCancelled();
    }
    if (event->type() != QEvent::KeyPress && event->type() != QEvent::KeyRelease) return false;
    auto *key = static_cast<QKeyEvent *>(event);
    if (key->isAutoRepeat()) return m_pressed.contains(key->key());
    // 포커스가 바뀌어도 이미 처리한 방향키의 놓음은 반드시 처리한다.
    if (event->type() == QEvent::KeyRelease && m_pressed.remove(key->key())) { updateMovement(); return true; }
    if (!m_enabled || QApplication::activeWindow() != m_window || event->type() != QEvent::KeyPress) return false;
    QWidget *focus = QApplication::focusWidget();
    // 검색 결과의 방향키 탐색을 카메라 이동으로 해석하지 않는다.
    if (qobject_cast<QAbstractItemView*>(focus) || (focus && qobject_cast<QAbstractItemView*>(focus->parentWidget()))) return false;
    if (qobject_cast<QLineEdit *>(focus) || qobject_cast<QAbstractSpinBox *>(focus) || qobject_cast<QComboBox *>(focus)
        || qobject_cast<QTextEdit *>(focus) || qobject_cast<QPlainTextEdit *>(focus)) return false;
    if (key->modifiers() != Qt::NoModifier && key->modifiers() != Qt::ShiftModifier) return false;
    if (key->key() == Qt::Key_R || key->key() == Qt::Key_C) { emit centerRequested(); return true; }
    if (key->key() == Qt::Key_Left || key->key() == Qt::Key_Right || key->key() == Qt::Key_Up || key->key() == Qt::Key_Down
        || key->key() == Qt::Key_A || key->key() == Qt::Key_D || key->key() == Qt::Key_W || key->key() == Qt::Key_S) {
        m_pressed.insert(key->key()); updateMovement(); return true;
    }
    return false;
}
