#ifndef CUSTOMVIEW_H
#define CUSTOMVIEW_H

#include <QGraphicsView>
#include <QMouseEvent>
#include <QGraphicsRectItem>
#include "common.h"
#include "transformablerectitem.h"

class CustomView : public QGraphicsView
{
    Q_OBJECT
public:
    explicit CustomView(QWidget *parent = nullptr);

    void setPainterStatus(const PainterStatus ps);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    PainterStatus painterStatus = PainterStatus::SELECT;

    QPointF m_startPoint; // 记录鼠标按下的起始点
    TransformableRectItem *m_currentRectItem; // 当前正在绘制的矩形
    bool m_isDrawing; // 是否正在绘制的标志

signals:
    void sendMousePos(QPointF pos);
};

#endif // CUSTOMVIEW_H
