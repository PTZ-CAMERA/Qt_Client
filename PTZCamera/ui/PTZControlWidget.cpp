// PTZ 방향 버튼과 중앙 복귀 버튼을 배치하는 UI이다.
// 이 클래스는 방향 신호만 발생시키며, 실제 명령 송신은 MainWindow가 연결한다.
#include "PTZControlWidget.h"

#include <QGridLayout>
#include <QEvent>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

// 3×3 격자에 방향 버튼을 배치하고 별도의 중앙 복귀 버튼을 추가한다.
PTZControlWidget::PTZControlWidget(QWidget *parent) : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);

    auto *grid = new QGridLayout;
    grid->setHorizontalSpacing(8);
    grid->setVerticalSpacing(8);
    auto makeButton = [this](const QString &text, const QString &tip) {
        // 공통 크기와 스타일 이름을 한곳에서 지정해 버튼 모양을 통일한다.
        auto *button = new QPushButton(text, this);
        button->setObjectName(QStringLiteral("ptzButton"));
        button->setToolTip(tip);
        button->setMinimumSize(64, 42);
        return button;
    };
    auto *up = makeButton(QStringLiteral("▲"), QStringLiteral("Tilt up"));
    auto *down = makeButton(QStringLiteral("▼"), QStringLiteral("Tilt down"));
    auto *left = makeButton(QStringLiteral("◀"), QStringLiteral("Pan left"));
    auto *right = makeButton(QStringLiteral("▶"), QStringLiteral("Pan right"));
    auto *middle = new QLabel(QStringLiteral("PTZ"), this);
    middle->setAlignment(Qt::AlignCenter);
    middle->setObjectName(QStringLiteral("ptzMiddle"));
    grid->addWidget(up, 0, 1);
    grid->addWidget(left, 1, 0);
    grid->addWidget(middle, 1, 1);
    grid->addWidget(right, 1, 2);
    grid->addWidget(down, 2, 1);
    grid->setColumnStretch(0, 1);
    grid->setColumnStretch(1, 1);
    grid->setColumnStretch(2, 1);
    layout->addLayout(grid);

    // R 키는 MainWindow의 이벤트 필터가 처리한다. 버튼과 키보드가 같은 명령 경로를 사용한다.
    auto *center = new QPushButton(QStringLiteral("Reset / Center (C / R)"), this);
    m_center = center;
    center->setObjectName(QStringLiteral("secondaryButton"));
    center->setMinimumHeight(36);
    layout->addWidget(center);

    auto *hint = new QLabel(QStringLiteral("W/A/S/D or arrows  ·  C/R center"), this);
    hint->setAlignment(Qt::AlignCenter);
    hint->setObjectName(QStringLiteral("ptzHint"));
    layout->addWidget(hint);

    // VMS 화면은 누르는 동안 이동 요청, 놓으면 정지 요청을 사용한다.
    // 여기서는 GPIO나 소켓을 다루지 않고 Controller가 cameraId를 붙여 전달한다.
    const auto bindDirection = [this](QPushButton *button, int direction) {
        connect(button, &QPushButton::pressed, this, [this, direction] { m_pressed.insert(direction); updateMovement(); });
        connect(button, &QPushButton::released, this, [this, direction] {
            if (m_pressed.remove(direction)) updateMovement();
        });
    };
    bindDirection(right, Qt::Key_Right); bindDirection(left, Qt::Key_Left);
    bindDirection(up, Qt::Key_Up); bindDirection(down, Qt::Key_Down);
    // legacy 창은 이전 클릭당 방향 명령을 계속 사용할 수 있다.
    connect(up, &QPushButton::clicked, this, &PTZControlWidget::tiltUpRequested);
    connect(down, &QPushButton::clicked, this, &PTZControlWidget::tiltDownRequested);
    connect(left, &QPushButton::clicked, this, &PTZControlWidget::panLeftRequested);
    connect(right, &QPushButton::clicked, this, &PTZControlWidget::panRightRequested);
    connect(center, &QPushButton::clicked, this, &PTZControlWidget::centerRequested);
}

void PTZControlWidget::updateMovement() {
    const float pan = 0.5F * (int(m_pressed.contains(Qt::Key_Right)) - int(m_pressed.contains(Qt::Key_Left)));
    const float tilt = 0.5F * (int(m_pressed.contains(Qt::Key_Up)) - int(m_pressed.contains(Qt::Key_Down)));
    if (pan == 0 && tilt == 0) emit stopRequested(); else emit moveRequested(pan, tilt);
}
void PTZControlWidget::resetPressedState() {
    m_pressed.clear();
    for (auto *button : findChildren<QPushButton *>()) if (button->objectName() == QStringLiteral("ptzButton")) button->setDown(false);
}
void PTZControlWidget::setCenterEnabled(bool enabled) { m_center->setEnabled(enabled); }
void PTZControlWidget::changeEvent(QEvent *event) {
    if (event->type() == QEvent::EnabledChange && !isEnabled() && !m_pressed.isEmpty()) {
        resetPressedState(); emit stopRequested();
    }
    QWidget::changeEvent(event);
}
