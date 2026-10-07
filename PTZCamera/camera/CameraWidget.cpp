// QImage 프레임과 탐지 결과를 직접 그리는 화면이다.
// 현재 실영상은 별도 RTSP/WebRTC 위젯에서 재생하며, 이 위젯은 데모 화면에 사용된다.
#include "CameraWidget.h"

#include <QFont>
#include <QPaintEvent>
#include <QPainter>
#include <QtMath>

// 부모 레이아웃의 남는 공간을 사용하되, 최소 표시 크기는 확보한다.
CameraWidget::CameraWidget(QWidget *parent) : QWidget(parent)
{
    setMinimumSize(640, 360);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

// 새 프레임을 보관하고 다음 화면 갱신을 요청한다.
// update()는 여기서 즉시 그림을 그리지 않고 Qt가 paintEvent()를 호출하도록 예약한다.
void CameraWidget::setFrame(const QImage &frame)
{
    m_frame = frame;
    update();
}

// 상자, 이름, 신뢰도와 중심점은 원본 프레임 좌표로 전달받는다.
// 실제 위젯 크기에 맞추는 좌표 변환은 paintEvent()에서 한 번에 처리한다.
void CameraWidget::setDetection(const Detection &detection)
{
    m_detection = detection;
    m_hasDetection = true;
    update();
}

// 프레임은 유지하면서 탐지 상자와 객체 중심점만 숨긴다.
void CameraWidget::clearDetection()
{
    m_hasDetection = false;
    update();
}

// 창 크기가 바뀌거나 프레임/탐지 정보가 갱신되면 Qt가 호출한다.
// 배경 → 영상 → 프레임 중심 → 객체 상자/중심/이름 순으로 겹쳐 그린다.
void CameraWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillRect(rect(), QColor("#080c11"));

    // 원본 비율을 유지해 영상을 맞추고, 남는 공간은 양쪽에 균등하게 배치한다.
    // view는 영상이 실제로 표시되는 사각형으로, 검은 여백을 포함하지 않는다.
    const QSize sourceSize = m_frame.isNull() ? QSize(640, 480) : m_frame.size();
    const QSizeF fitted = QSizeF(sourceSize).scaled(QSizeF(size()), Qt::KeepAspectRatio);
    const QRectF view((width() - fitted.width()) / 2.0,
                      (height() - fitted.height()) / 2.0,
                      fitted.width(), fitted.height());

    if (m_frame.isNull()) {
        // 영상이 없으면 안내 문구만 표시하고 탐지 오버레이는 그리지 않는다.
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
    // 클리핑과 펜 설정을 저장해 오버레이가 영상 영역 밖으로 그려지지 않게 한다.
    painter.save();
    painter.setClipRect(view);
    const qreal scaleX = view.width() / sourceSize.width();
    const qreal scaleY = view.height() / sourceSize.height();
    const auto mapPoint = [&](const QPointF &point) {
        // 원본 픽셀 좌표에 배율을 곱한 뒤 화면에서 영상이 시작되는 위치를 더한다.
        return QPointF(view.left() + point.x() * scaleX,
                       view.top() + point.y() * scaleY);
    };

    // 프레임 중심은 원본 영상 기준으로 계산하므로 창 크기와 무관하게 중앙에 놓인다.
    const QPointF frameCenter = mapPoint(QPointF(sourceSize.width() / 2.0,
                                                 sourceSize.height() / 2.0));
    painter.setPen(QPen(QColor("#c0ccd7"), 1));
    painter.drawLine(frameCenter + QPointF(-10, 0), frameCenter + QPointF(10, 0));
    painter.drawLine(frameCenter + QPointF(0, -10), frameCenter + QPointF(0, 10));
    painter.drawEllipse(frameCenter, 2, 2);

    if (m_hasDetection) {
        // 탐지 상자의 위치와 크기에 영상과 동일한 배율을 적용한다.
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

        // 신뢰도는 0~1 값을 백분율로 표시한다. 상자 위에 공간이 부족하면 안쪽에 둔다.
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
