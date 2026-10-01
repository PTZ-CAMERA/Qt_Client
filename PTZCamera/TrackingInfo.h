#pragma once

#include <QString>

struct TrackingInfo
{
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
