#ifndef PREVIEWCANVAS_H
#define PREVIEWCANVAS_H

#include <QPoint>
#include <QWidget>

#include "petproject.h"

class PreviewCanvas : public QWidget
{
    Q_OBJECT

public:
    explicit PreviewCanvas(QWidget *parent = nullptr);

    void setPreview(const PetProject *project, const PetAction *action, int frameIndex);
    void setOnionSkin(bool previous, bool next);
    QRect canvasRect() const;
    QPoint widgetToCanvas(const QPoint &widgetPoint) const;
    QPoint canvasToWidget(const QPoint &canvasPoint) const;

signals:
    void anchorPicked(const QPoint &canvasPoint);
    void stateOffsetDragged(const QPoint &delta);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    QRect fitRect(const QSize &sourceSize) const;
    QPixmap composedFrame() const;

    const PetProject *m_project {nullptr};
    const PetAction *m_action {nullptr};
    int m_frameIndex {0};
    bool m_showPreviousOnion {false};
    bool m_showNextOnion {false};
    bool m_dragging {false};
    QPoint m_lastCanvasPoint;
};

#endif // PREVIEWCANVAS_H
