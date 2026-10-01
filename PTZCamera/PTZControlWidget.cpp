#include "PTZControlWidget.h"

#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

PTZControlWidget::PTZControlWidget(QWidget *parent) : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);

    auto *grid = new QGridLayout;
    grid->setHorizontalSpacing(8);
    grid->setVerticalSpacing(8);
    auto makeButton = [this](const QString &text, const QString &tip) {
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

    auto *center = new QPushButton(QStringLiteral("Reset / Center (R)"), this);
    center->setObjectName(QStringLiteral("secondaryButton"));
    center->setMinimumHeight(36);
    layout->addWidget(center);

    auto *hint = new QLabel(QStringLiteral("Keyboard  ↑  ↓  ←  →  /  R"), this);
    hint->setAlignment(Qt::AlignCenter);
    hint->setObjectName(QStringLiteral("ptzHint"));
    layout->addWidget(hint);

    connect(up, &QPushButton::clicked, this, &PTZControlWidget::tiltUpRequested);
    connect(down, &QPushButton::clicked, this, &PTZControlWidget::tiltDownRequested);
    connect(left, &QPushButton::clicked, this, &PTZControlWidget::panLeftRequested);
    connect(right, &QPushButton::clicked, this, &PTZControlWidget::panRightRequested);
    connect(center, &QPushButton::clicked, this, &PTZControlWidget::centerRequested);
}
