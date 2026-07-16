#include "previewcanvas.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>

namespace {
constexpr int kCheckerCell = 12;
constexpr int kDragThreshold = 2;
}

PreviewCanvas::PreviewCanvas(QWidget *parent)
    : QWidget(parent)
{
    setMinimumSize(440, 440);
    setMouseTracking(true);
    setCursor(Qt::CrossCursor);
}

void PreviewCanvas::setPreview(const PetProject *project, const PetAction *action, int frameIndex)
{
    m_project = project;
    m_action = action;
    m_frameIndex = frameIndex;
    update();
}

void PreviewCanvas::setOnionSkin(bool previous, bool next)
{
    m_showPreviousOnion = previous;
    m_showNextOnion = next;
    update();
}

QRect PreviewCanvas::canvasRect() const
{
    if (!m_project || !m_project->canvasSize.isValid()) {
        return QRect(QPoint(), size());
    }
    return fitRect(m_project->canvasSize);
}

QPoint PreviewCanvas::widgetToCanvas(const QPoint &widgetPoint) const
{
    if (!m_project || !m_project->canvasSize.isValid()) {
        return widgetPoint;
    }

    const QRect rect = canvasRect();
    if (rect.isEmpty()) {
        return QPoint();
    }

    const double sx = static_cast<double>(m_project->canvasSize.width()) / rect.width();
    const double sy = static_cast<double>(m_project->canvasSize.height()) / rect.height();
    return QPoint(qRound((widgetPoint.x() - rect.left()) * sx),
                  qRound((widgetPoint.y() - rect.top()) * sy));
}

QPoint PreviewCanvas::canvasToWidget(const QPoint &canvasPoint) const
{
    if (!m_project || !m_project->canvasSize.isValid()) {
        return canvasPoint;
    }

    const QRect rect = canvasRect();
    if (rect.isEmpty()) {
        return QPoint();
    }

    const double sx = static_cast<double>(rect.width()) / m_project->canvasSize.width();
    const double sy = static_cast<double>(rect.height()) / m_project->canvasSize.height();
    return QPoint(rect.left() + qRound(canvasPoint.x() * sx),
                  rect.top() + qRound(canvasPoint.y() * sy));
}

void PreviewCanvas::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton || !m_project || !m_action) {
        QWidget::mousePressEvent(event);
        return;
    }

    const QPoint canvasPoint = widgetToCanvas(event->position().toPoint());
    emit anchorPicked(canvasPoint);
    m_dragging = true;
    m_lastCanvasPoint = canvasPoint;
    event->accept();
}

void PreviewCanvas::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_dragging || !m_project || !m_action || !(event->buttons() & Qt::LeftButton)) {
        QWidget::mouseMoveEvent(event);
        return;
    }

    const QPoint canvasPoint = widgetToCanvas(event->position().toPoint());
    const QPoint delta = canvasPoint - m_lastCanvasPoint;
    if (delta.manhattanLength() >= kDragThreshold) {
        emit stateOffsetDragged(delta);
        m_lastCanvasPoint = canvasPoint;
    }
    event->accept();
}

void PreviewCanvas::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragging = false;
        event->accept();
        return;
    }
    QWidget::mouseReleaseEvent(event);
}

void PreviewCanvas::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.fillRect(rect(), QColor(246, 246, 246));

    for (int y = 0; y < height(); y += kCheckerCell) {
        for (int x = 0; x < width(); x += kCheckerCell) {
            const bool dark = ((x / kCheckerCell) + (y / kCheckerCell)) % 2;
            painter.fillRect(QRect(x, y, kCheckerCell, kCheckerCell),
                             dark ? QColor(218, 218, 218) : QColor(252, 252, 252));
        }
    }

    if (!m_project || !m_project->canvasSize.isValid()) {
        painter.setPen(QColor(85, 85, 85));
        painter.drawText(rect(), Qt::AlignCenter, tr("Create or open a project"));
        return;
    }

    const QRect rect = canvasRect();
    if (m_showPreviousOnion && m_action && m_frameIndex > 0) {
        const int current = m_frameIndex;
        m_frameIndex = current - 1;
        const QPixmap previous = composedFrame();
        m_frameIndex = current;
        if (!previous.isNull()) {
            painter.setOpacity(0.25);
            painter.drawPixmap(rect, previous);
            painter.setOpacity(1.0);
        }
    }

    const QPixmap frame = composedFrame();
    if (!frame.isNull()) {
        painter.drawPixmap(rect, frame);
    } else if (!m_action || m_action->frames.isEmpty()) {
        painter.setPen(QColor(85, 85, 85));
        painter.drawText(this->rect(), Qt::AlignCenter, tr("No frames"));
    }

    if (m_showNextOnion && m_action && m_frameIndex + 1 < m_action->frames.size()) {
        const int current = m_frameIndex;
        m_frameIndex = current + 1;
        const QPixmap next = composedFrame();
        m_frameIndex = current;
        if (!next.isNull()) {
            painter.setOpacity(0.15);
            painter.drawPixmap(rect, next);
            painter.setOpacity(1.0);
        }
    }

    const QPoint anchorPoint = canvasToWidget(m_project->anchor);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(QColor(20, 130, 90), 1, Qt::DashLine));
    painter.drawLine(rect.left(), anchorPoint.y(), rect.right(), anchorPoint.y());
    painter.setPen(QPen(QColor(220, 70, 50), 2));
    painter.drawLine(anchorPoint + QPoint(-8, 0), anchorPoint + QPoint(8, 0));
    painter.drawLine(anchorPoint + QPoint(0, -8), anchorPoint + QPoint(0, 8));
    painter.setBrush(QColor(220, 70, 50));
    painter.drawEllipse(anchorPoint, 3, 3);

    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(QColor(40, 70, 120), 1));
    painter.drawRect(rect.adjusted(0, 0, -1, -1));
}

QRect PreviewCanvas::fitRect(const QSize &sourceSize) const
{
    if (sourceSize.isEmpty() || size().isEmpty()) {
        return QRect(QPoint(), size());
    }

    const QSize fitted = sourceSize.scaled(size(), Qt::KeepAspectRatio);
    return QRect(QPoint((width() - fitted.width()) / 2,
                        (height() - fitted.height()) / 2),
                 fitted);
}

QPixmap PreviewCanvas::composedFrame() const
{
    if (!m_project || !m_action || !m_project->canvasSize.isValid() || m_action->frames.isEmpty()) {
        return QPixmap();
    }

    const int frameIndex = qBound(0, m_frameIndex, m_action->frames.size() - 1);
    QPixmap source(m_project->absolutePathFor(m_action->frames.at(frameIndex).path));
    if (source.isNull()) {
        return QPixmap();
    }

    QPixmap composed(m_project->canvasSize);
    composed.fill(Qt::transparent);
    QPainter painter(&composed);
    const PetFrame &frame = m_action->frames.at(frameIndex);
    painter.drawPixmap(m_action->offset + frame.offset + frame.autoOffset, source);
    return composed;
}
