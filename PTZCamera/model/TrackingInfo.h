#pragma once

#include <QString>
#include <QMetaType>

struct TrackingInfo
{
    // VMS가 전달할 추적 상태다. 기존 필드는 legacy 화면/프로토콜 호환을 위해 유지한다.
    bool enabled = false;
    QString target = QStringLiteral("Person");
    QString status = QStringLiteral("IDLE");
    bool detected = false;
    QString label;
    float confidence = 0.0F;
    int objectX = 0;
    int objectY = 0;
    int errorX = 0;
    int errorY = 0;
    float panAngle = 90.0F;
    float tiltAngle = 90.0F;
    int frameWidth = 640;
    int frameHeight = 480;
};
Q_DECLARE_METATYPE(TrackingInfo)
