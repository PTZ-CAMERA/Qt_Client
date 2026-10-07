#pragma once
#include <QDialog>

// 실제 지원 기능과 사용자 조작 순서를 설명하는 비모달 도움말 창이다.
class HelpDialog : public QDialog {
    Q_OBJECT
public:
    explicit HelpDialog(QWidget *parent = nullptr);
};
