#pragma once
#include <QObject>
#include <QSet>
class QWidget;
class PtzKeyboardController : public QObject {
    Q_OBJECT
public:
    explicit PtzKeyboardController(QWidget *window);
    void stop();
    void setEnabled(bool enabled);
signals:
    void inputCancelled();
    void moveRequested(float panVelocity, float tiltVelocity);
    void stopRequested();
    void centerRequested();
protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
private:
    void updateMovement();
    QWidget *m_window = nullptr;
    QSet<int> m_pressed;
    bool m_enabled = false;
};
