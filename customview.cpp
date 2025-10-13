#include "customview.h"
#include <QDebug>

CustomView::CustomView(QWidget *parent)
    : QGraphicsView(parent), m_currentRectItem(nullptr), m_isDrawing(false)
{
    // 视图的一些基本设置
    setRenderHint(QPainter::Antialiasing); // 开启抗锯齿
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setTransformationAnchor(QGraphicsView::NoAnchor);
}

void CustomView::setPainterStatus(const PainterStatus ps) {painterStatus = ps;}

void CustomView::mousePressEvent(QMouseEvent *event)
{
    // 为所有items广播鼠标坐标
    if (event->button() == Qt::LeftButton)
    {
        QPointF scenePos = event->pos();
        for (QGraphicsItem *item : scene()->items()) {
            IMousePositionReceiver *receiver = dynamic_cast<IMousePositionReceiver*>(item);
            if (receiver) {
                receiver->receiveSceneMousePosition(scenePos, MouseLeftClickStatus::PRESS);
            }
        }
    }

    // 如果是鼠标左键按下，开始绘图
    switch (painterStatus)
    {
        case PainterStatus::RECT:
        {
            if (event->button() == Qt::LeftButton) {
                m_startPoint = event->pos();
                m_isDrawing = true;

                // 创建一个新的矩形项，但暂时是空的
                m_currentRectItem = new TransformableRectItem(QRectF(m_startPoint, m_startPoint));
                m_currentRectItem->setPen(QPen(Qt::black, 2)); // 设置画笔
                // m_currentRectItem->setBrush(QBrush(Qt::cyan)); // 设置填充

                // 将新项添加到场景中
                scene()->addItem(m_currentRectItem);
            } else {
                // 如果是其他按键，则调用基类的处理方式（例如右键菜单、中键拖拽视图）
                QGraphicsView::mousePressEvent(event);
            }
            break;
        }
        default:
        {
            setDragMode(QGraphicsView::RubberBandDrag); // 设置拖拽模式为橡皮筋选择，便于选择多个Item
            QGraphicsView::mousePressEvent(event);
        }
    }
}

void CustomView::mouseMoveEvent(QMouseEvent *event)
{
    // 给坐标标签发送鼠标位置
    emit sendMousePos(event->pos());

    // 为所有items广播鼠标坐标
    QPointF scenePos = event->pos();
    // 判断是否需要将鼠标设为旋转指针
    bool isRotateCursor = false;
    for (QGraphicsItem *item : scene()->items()) {
        IMousePositionReceiver *receiver = dynamic_cast<IMousePositionReceiver*>(item);
        if (receiver) {
            receiver->receiveSceneMousePosition(scenePos, MouseLeftClickStatus::MOVE);
        }
        ItemCommon * itemCommon = dynamic_cast<ItemCommon*>(item);

        if (itemCommon->isRotateHandle || itemCommon->isRotateHandling) {
            isRotateCursor = true;
        }
    }

    // 如果需要将鼠标设为旋转指针, 则执行
    if (isRotateCursor) {
        this->viewport()->setCursor(Qt::SizeAllCursor);
    }
    else {
        this->viewport()->setCursor(Qt::ArrowCursor);
    }

    switch (painterStatus)
    {
        case PainterStatus::RECT:
        {
            if (m_isDrawing) {
                // 如果正在绘制，就更新矩形的尺寸
                QPointF currentPoint = event->pos();
                QRectF rect(m_startPoint, currentPoint);
                m_currentRectItem->setRect(rect.normalized()); // normalized()保证左上角坐标小于右下角
            } else {
                QGraphicsView::mouseMoveEvent(event);
            }
            break;
        }
        default:
        {
            if(isRotateCursor){
                setDragMode(QGraphicsView::NoDrag);
            }
            else{
                setDragMode(QGraphicsView::RubberBandDrag);
            }
            QGraphicsView::mouseMoveEvent(event);
        }
    }
}

void CustomView::mouseReleaseEvent(QMouseEvent *event)
{
    // 为所有items广播鼠标坐标
    if (event->button() == Qt::LeftButton)
    {
        QPointF scenePos = event->pos();
        for (QGraphicsItem *item : scene()->items()) {
            IMousePositionReceiver *receiver = dynamic_cast<IMousePositionReceiver*>(item);
            if (receiver) {
                receiver->receiveSceneMousePosition(scenePos, MouseLeftClickStatus::RELEASE);
            }
        }
    }

    switch (painterStatus)
    {
        case PainterStatus::RECT:
        {
            if (event->button() == Qt::LeftButton && m_isDrawing) {
                m_isDrawing = false;
                // 确保绘制的矩形有有效的尺寸，否则可能无法选中
                if (m_currentRectItem && m_currentRectItem->rect().isEmpty()) {
                    scene()->removeItem(m_currentRectItem);
                    delete m_currentRectItem;
                }
                m_currentRectItem = nullptr;

            } else {
                QGraphicsView::mouseReleaseEvent(event);
            }
            break;
        }
        default:
        {
            setDragMode(QGraphicsView::RubberBandDrag);
            QGraphicsView::mouseReleaseEvent(event);
        }
    }
}
