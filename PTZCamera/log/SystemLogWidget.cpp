// 기존 읽기 전용 로그의 시간 형식과 자동 스크롤을 별도 위젯으로 옮겼다.
#include "SystemLogWidget.h"
#include <QDateTime>
#include <QScrollBar>
SystemLogWidget::SystemLogWidget(QWidget *parent) : QPlainTextEdit(parent) {
    setReadOnly(true); setMaximumBlockCount(1000);
}
void SystemLogWidget::addLog(const QString &source, const QString &message) {
    appendPlainText(QStringLiteral("[%1][%2] %3").arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss")), source, message));
    verticalScrollBar()->setValue(verticalScrollBar()->maximum());
}
