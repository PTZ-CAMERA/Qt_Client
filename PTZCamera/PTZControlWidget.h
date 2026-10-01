#pragma once

#include <QWidget>

class PTZControlWidget : public QWidget
{
    Q_OBJECT
public:
    explicit PTZControlWidget(QWidget *parent = nullptr);

signals:
    void panLeftRequested();
    void panRightRequested();
    void tiltUpRequested();
    void tiltDownRequested();
    void centerRequested();
};
