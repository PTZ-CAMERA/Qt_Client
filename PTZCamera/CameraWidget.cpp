#include "CameraWidget.h"

#include <QFont>
#include <QPaintEvent>
#include <QPainter>
#include <QtMath>

CameraWidget::CameraWidget(QWidget *parent) : QWidget(parent)
{
    setMinimumSize(640, 360);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void CameraWidget::setFrame(const QImage &frame)
{
    m_frame = frame;
    update();
}

void CameraWidget::setDetection(const Detection &detection)
{
    m_detection = detection;
    m_hasDetection = true;
    update();
}

void CameraWidget::clearDetection()
{
    m_hasDetection = false;
    update();
}

void CameraWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillRect(rect(), QColor("#080c11"));

    const QSize sourceSize = m_frame.isNull() ? QSize(640, 480) : m_frame.size();
    const QSizeF fitted = QSizeF(sourceSize).scaled(QSizeF(size()), Qt::KeepAspectRatio);
    const QRectF view((width() - fitted.width()) / 2.0,
                      (height() - fitted.height()) / 2.0,
                      fitted.width(), fitted.height());

    if (m_frame.isNull()) {
        painter.setPen(QColor("#738190"));
        QFont font = painter.font();
        font.setPixelSize(22);
        font.setBold(true);
        font.setLetterSpacing(QFont::AbsoluteSpacing, 2.0);
        painter.setFont(font);
        painter.drawText(view, Qt::AlignCenter, QStringLiteral("NO CAMERA SIGNAL"));
        return;
    }

    painter.drawImage(view, m_frame);
    painter.save();
    painter.setClipRect(view);
    const qreal scaleX = view.width() / sourceSize.width();
    const qreal scaleY = view.height() / sourceSize.height();
    const auto mapPoint = [&](const QPointF &point) {
        return QPointF(view.left() + point.x() * scaleX,
                       view.top() + point.y() * scaleY);
    };

    const QPointF frameCenter = mapPoint(QPointF(sourceSize.width() / 2.0,
                                                 sourceSize.height() / 2.0));
    painter.setPen(QPen(QColor("#c0ccd7"), 1));
    painter.drawLine(frameCenter + QPointF(-10, 0), frameCenter + QPointF(10, 0));
    painter.drawLine(frameCenter + QPointF(0, -10), frameCenter + QPointF(0, 10));
    painter.drawEllipse(frameCenter, 2, 2);

    if (m_hasDetection) {
        const QRectF box(view.left() + m_detection.boundingBox.x() * scaleX,
                         view.top() + m_detection.boundingBox.y() * scaleY,
                         m_detection.boundingBox.width() * scaleX,
                         m_detection.boundingBox.height() * scaleY);
        const QColor accent("#52d7a5");
        painter.setPen(QPen(accent, 2));
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(box);

        const QPointF objectCenter = mapPoint(m_detection.center);
        painter.drawLine(objectCenter + QPointF(-7, 0), objectCenter + QPointF(7, 0));
        painter.drawLine(objectCenter + QPointF(0, -7), objectCenter + QPointF(0, 7));

        const QString caption = QStringLiteral("%1  %2%")
                                    .arg(m_detection.label)
                                    .arg(qRound(m_detection.confidence * 100.0F));
        QFont font = painter.font();
        font.setPixelSize(13);
        font.setBold(true);
        painter.setFont(font);
        const int captionWidth = painter.fontMetrics().horizontalAdvance(caption) + 16;
        const qreal captionY = box.top() >= view.top() + 26 ? box.top() - 25 : box.top();
        QRectF captionRect(box.left(), captionY, captionWidth, 24);
        painter.fillRect(captionRect, accent);
        painter.setPen(QColor("#081410"));
        painter.drawText(captionRect, Qt::AlignCenter, caption);
    }
    painter.restore();
}
