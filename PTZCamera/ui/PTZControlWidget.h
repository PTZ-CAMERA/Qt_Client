#pragma once

#include <QWidget>
#include <QSet>
class QPushButton;

class PTZControlWidget : public QWidget
{
    Q_OBJECT
public:
    explicit PTZControlWidget(QWidget *parent = nullptr);
    void resetPressedState();
    void setCenterEnabled(bool enabled);

signals:
    void moveRequested(float panVelocity, float tiltVelocity);
    void stopRequested();
    // 아래 방향 신호는 이전 직접 연결 창의 호환용이다.
    void panLeftRequested();
    void panRightRequested();
    void tiltUpRequested();
    void tiltDownRequested();
    void centerRequested();
protected:
    void changeEvent(QEvent *event) override;
private:
    void updateMovement();
    QSet<int> m_pressed;
    QPushButton *m_center = nullptr;
};
