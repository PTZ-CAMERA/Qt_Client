#pragma once

#include <QImage>
#include <QPoint>
#include <QRect>
#include <QString>
#include <QWidget>

class CameraWidget : public QWidget
{
    Q_OBJECT
public:
    struct Detection {
        QRect boundingBox;
        QString label;
        float confidence = 0.0F;
        QPoint center;
        QSize imageSize;
        bool hasConfidence = true;
    };

    explicit CameraWidget(QWidget *parent = nullptr);
    void setFrame(const QImage &frame);
    void setDetection(const Detection &detection);
    void clearDetection();
    bool hasDetection() const { return m_hasDetection; }
    Detection detection() const { return m_detection; }

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QImage m_frame;
    Detection m_detection;
    bool m_hasDetection = false;
};
