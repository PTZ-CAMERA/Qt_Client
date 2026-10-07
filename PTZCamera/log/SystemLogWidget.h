#pragma once
#include <QPlainTextEdit>
class SystemLogWidget : public QPlainTextEdit {
    Q_OBJECT
public:
    explicit SystemLogWidget(QWidget *parent = nullptr);
    void addLog(const QString &source, const QString &message);
};
