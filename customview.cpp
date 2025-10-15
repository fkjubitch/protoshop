#include "customview.h"
#include <QDebug>
#include <cmath>
#include <QKeyEvent>
#include <QFileDialog>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QBuffer>

CustomView::CustomView(QWidget *parent)
    : QGraphicsView(parent)
{
    // 视图的一些基本设置
    setRenderHint(QPainter::Antialiasing); // 开启抗锯齿
    // setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    // setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setTransformationAnchor(QGraphicsView::NoAnchor);
    undoStack.push(QJsonArray());
    this->setAlignment(Qt::AlignLeft | Qt::AlignTop);
}

void CustomView::setPainterStatus(const PainterStatus ps) {painterStatus = ps;}

// 辅助函数
static ItemCommon *itemCommonOf(QGraphicsItem *it)
{
    if (!it) return nullptr;
    // 按实际类型依次尝试
    if (auto t = qgraphicsitem_cast<TransformableRectItem*>(it))    return t;
    if (auto t = qgraphicsitem_cast<TransformableEllipseItem*>(it)) return t;
    if (auto t = qgraphicsitem_cast<TransformableLineItem*>(it))    return t;
    if (auto t = qgraphicsitem_cast<TransformablePolygonItem*>(it)) return t;
    if (auto t = qgraphicsitem_cast<TransformablePathItem*>(it))    return t;
    return nullptr;
}

void CustomView::mousePressEvent(QMouseEvent *event)
{
    // 为所有items广播鼠标坐标
    if (event->button() == Qt::LeftButton)
    {
        QPointF scenePos = mapToScene(event->pos());
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
        case PainterStatus::PEN:
        {
            if (event->button() == Qt::LeftButton) {
                m_startPoint = mapToScene(event->pos());
                m_isDrawing = true;
                m_livePath = QPainterPath();
                m_livePath.moveTo(m_startPoint);

                m_currentPathItem = new TransformablePathItem(m_livePath);
                m_currentPathItem->setPen(QPen(penColor, penWidth, penStyle));
                m_currentPathItem->penColor = penColor;
                m_currentPathItem->penWidth = penWidth;
                m_currentPathItem->brushColor = brushColor;
                m_currentPathItem->penStyle = penStyle;
                scene()->addItem(m_currentPathItem);
            } else {
                QGraphicsView::mousePressEvent(event);
            }
            break;
        }
        case PainterStatus::LINE:
        {
            if (event->button() == Qt::LeftButton) {
                m_startPoint = mapToScene(event->pos());
                m_isDrawing = true;

                // 创建一个新的矩形项，但暂时是空的
                m_currentLineItem = new TransformableLineItem(QLineF(m_startPoint, m_startPoint));
                m_currentLineItem->setPen(QPen(penColor, penWidth, penStyle)); // 设置画笔
                m_currentLineItem->penColor = penColor;
                m_currentLineItem->penWidth = penWidth;
                m_currentLineItem->brushColor = brushColor;
                m_currentLineItem->penStyle = penStyle;

                // 将新项添加到场景中
                scene()->addItem(m_currentLineItem);
            } else {
                // 如果是其他按键，则调用基类的处理方式（例如右键菜单、中键拖拽视图）
                QGraphicsView::mousePressEvent(event);
            }
            break;
        }
        case PainterStatus::RECT:
        {
            if (event->button() == Qt::LeftButton) {
                m_startPoint = mapToScene(event->pos());
                m_isDrawing = true;

                // 创建一个新的矩形项，但暂时是空的
                m_currentRectItem = new TransformableRectItem(QRectF(m_startPoint, m_startPoint));
                m_currentRectItem->setPen(QPen(penColor, penWidth, penStyle)); // 设置画笔
                m_currentRectItem->setBrush(QBrush(brushColor)); // 设置填充
                m_currentRectItem->penColor = penColor;
                m_currentRectItem->penWidth = penWidth;
                m_currentRectItem->brushColor = brushColor;
                m_currentRectItem->penStyle = penStyle;

                // 将新项添加到场景中
                scene()->addItem(m_currentRectItem);
            } else {
                // 如果是其他按键，则调用基类的处理方式（例如右键菜单、中键拖拽视图）
                QGraphicsView::mousePressEvent(event);
            }
            break;
        }
        case PainterStatus::POLYGON:
        {
            if (event->button() == Qt::LeftButton) {
                QPointF scenePos = mapToScene(event->pos());
                m_livePolygon << scenePos;
                m_isDrawing = true;

                if (!m_currentPolygonItem) {   // 第一次点击：新建
                    m_currentPolygonItem = new TransformablePolygonItem(m_livePolygon);
                    m_currentPolygonItem->setPen(QPen(penColor, penWidth, penStyle));
                    m_currentPolygonItem->setBrush(QBrush(brushColor));
                    m_currentPolygonItem->penColor = penColor;
                    m_currentPolygonItem->penWidth = penWidth;
                    m_currentPolygonItem->brushColor = brushColor;
                    m_currentPolygonItem->penStyle = penStyle;
                    scene()->addItem(m_currentPolygonItem);
                } else {                       // 后续点击：追加顶点
                    m_currentPolygonItem->setPolygon(m_livePolygon);
                }
            }
            break;
        }
        case PainterStatus::CIRCLE:
        case PainterStatus::ELLIPSE:
        {
            if (event->button() == Qt::LeftButton) {
                m_startPoint = mapToScene(event->pos());
                m_isDrawing = true;
                if (painterStatus == PainterStatus::CIRCLE)
                    m_currentEllipseItem = new TransformableEllipseItem(QRectF(m_startPoint, m_startPoint), nullptr, true);
                else
                    m_currentEllipseItem = new TransformableEllipseItem(QRectF(m_startPoint, m_startPoint));
                m_currentEllipseItem->setPen(QPen(penColor, penWidth, penStyle));
                m_currentEllipseItem->setBrush(QBrush(brushColor));
                m_currentEllipseItem->penColor = penColor;
                m_currentEllipseItem->penWidth = penWidth;
                m_currentEllipseItem->brushColor = brushColor;
                m_currentEllipseItem->penStyle = penStyle;
                scene()->addItem(m_currentEllipseItem);
            } else {
                QGraphicsView::mousePressEvent(event);
            }
            break;
        }
        case PainterStatus::FILLSELECT:
        {
            if (event->button() == Qt::LeftButton) {
                QGraphicsItem *hit = nullptr;
                for (auto it : scene()->items(mapToScene(event->pos()))) {
                    if (itemCommonOf(it)) { hit = it; break; }
                }
                if (!hit) break;

                auto *c = itemCommonOf(hit);
                if (auto *rect = qgraphicsitem_cast<TransformableRectItem*>(hit)) {
                    if (colorType == BOARD) { c->penColor = penColor; rect->setPen(QPen(penColor, c->penWidth, c->penStyle)); }
                    else                    { c->brushColor = brushColor; rect->setBrush(QBrush(brushColor)); }
                } else if (auto *ell = qgraphicsitem_cast<TransformableEllipseItem*>(hit)) {
                    if (colorType == BOARD) { c->penColor = penColor; ell->setPen(QPen(penColor, c->penWidth, c->penStyle)); }
                    else                    { c->brushColor = brushColor; ell->setBrush(QBrush(brushColor)); }
                } else if (auto *ln = qgraphicsitem_cast<TransformableLineItem*>(hit)) {
                    if (colorType == BOARD) { c->penColor = penColor; ln->setPen(QPen(penColor, c->penWidth, c->penStyle)); }
                    // Line 没有 brush，忽略 FILL
                } else if (auto *pl = qgraphicsitem_cast<TransformablePolygonItem*>(hit)) {
                    if (colorType == BOARD) { c->penColor = penColor; pl->setPen(QPen(penColor, c->penWidth, c->penStyle)); }
                    else                    { c->brushColor = brushColor; pl->setBrush(QBrush(brushColor)); }
                } else if (auto *pa = qgraphicsitem_cast<TransformablePathItem*>(hit)) {
                    if (colorType == BOARD) { c->penColor = penColor; pa->setPen(QPen(penColor, c->penWidth, c->penStyle)); }
                    // Path 没有 brush，忽略 FILL
                }

                hit->update();
                saveSceneState();
            }
            break;
        }
        default:
        {
            setDragMode(QGraphicsView::RubberBandDrag);
            QGraphicsView::mousePressEvent(event);
        }
    }
}

void CustomView::mouseMoveEvent(QMouseEvent *event)
{
    // 给坐标标签发送鼠标位置
    emit sendMousePos(mapToScene(event->pos()));

    // 为所有items广播鼠标坐标
    QPointF scenePos = mapToScene(event->pos());
    // 判断是否需要将鼠标设为旋转指针
    isRotateCursor = false;

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
    else if (painterStatus == FILLSELECT) {
        viewport()->setCursor(Qt::CrossCursor);
        return;   // 后面逻辑全部跳过
    }
    else {
        this->viewport()->setCursor(Qt::ArrowCursor);
    }

    switch (painterStatus)
    {
        case PainterStatus::PEN:
        {
            if (m_isDrawing) {
                QPointF currentPoint = mapToScene(event->pos());
                m_livePath.lineTo(currentPoint);
                m_currentPathItem->setPath(m_livePath);
            } else {
                QGraphicsView::mouseMoveEvent(event);
            }
            break;
        }
        case PainterStatus::LINE:
        {
            if (m_isDrawing) {
                QPointF currentPoint = mapToScene(event->pos());
                QLineF line(m_startPoint, currentPoint);
                m_currentLineItem->setLine(line);
            } else {
                QGraphicsView::mouseMoveEvent(event);
            }
            break;
        }
        case PainterStatus::RECT:
        {
            if (m_isDrawing) {
                QPointF currentPoint = mapToScene(event->pos());
                QRectF rect(m_startPoint, currentPoint);
                m_currentRectItem->setRect(rect.normalized()); // normalized()保证左上角坐标小于右下角
            } else {
                QGraphicsView::mouseMoveEvent(event);
            }
            break;
        }
        case PainterStatus::POLYGON:
        {
            if (m_isDrawing) {
                m_currentPolygonItem->setPolygon(m_livePolygon);
                QPolygonF poly = m_currentPolygonItem->polygon();    // 简单示意：让所有共点跟随鼠标
                poly << mapToScene(event->pos());
                m_currentPolygonItem->setPolygon(poly);
            } else {
                QGraphicsView::mouseMoveEvent(event);
            }
            break;
        }
        case PainterStatus::CIRCLE: {
            if (!m_isDrawing) {
                QGraphicsView::mouseMoveEvent(event);
                break;
            }
            QPointF currentPoint = mapToScene(event->pos());
            QPointF delta = currentPoint - m_startPoint;

            // 边长 = 最大移动距离，保证圆能跟随鼠标
            qreal side = std::max(std::fabs(delta.x()), std::fabs(delta.y()));

            // 让起点始终是正方形中心（或左上，按你需求改）
            // qreal signX = std::copysign(1.0, delta.x());
            // qreal signY = std::copysign(1.0, delta.y());
            QPointF topLeft(m_startPoint.x() - (delta.x() < 0 ? side : 0),
                            m_startPoint.y() - (delta.y() < 0 ? side : 0));
            QRectF rect(topLeft, QSizeF(side, side));
            m_currentEllipseItem->setRect(rect);
            break;
        }
        case PainterStatus::ELLIPSE:
        {
            if (m_isDrawing) {
                QPointF currentPoint = mapToScene(event->pos());
                QRectF rect(m_startPoint, currentPoint);
                m_currentEllipseItem->setRect(rect.normalized());
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
        QPointF scenePos = mapToScene(event->pos());
        for (QGraphicsItem *item : scene()->items()) {
            IMousePositionReceiver *receiver = dynamic_cast<IMousePositionReceiver*>(item);
            if (receiver) {
                receiver->receiveSceneMousePosition(scenePos, MouseLeftClickStatus::RELEASE);
            }
        }
    }

    switch (painterStatus)
    {
        case PainterStatus::PEN:
        {
            if (event->button() == Qt::LeftButton && m_isDrawing) {
                m_isDrawing = false;
                if (m_currentPathItem && m_currentPathItem->path().isEmpty()) {
                    scene()->removeItem(m_currentPathItem);
                    delete m_currentPathItem;
                }
                m_currentPathItem = nullptr;
            }
            break;
        }
        case PainterStatus::LINE:
        {
            if (event->button() == Qt::LeftButton && m_isDrawing) {
                m_isDrawing = false;
                if (m_currentLineItem && m_currentLineItem->line().isNull()) {
                    scene()->removeItem(m_currentLineItem);
                    delete m_currentLineItem;
                }
                m_currentLineItem = nullptr;

            } else {
                QGraphicsView::mouseReleaseEvent(event);
            }
            break;
        }
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
        case PainterStatus::POLYGON:
        {
            if (event->button() == Qt::RightButton && m_isDrawing) {
                m_isDrawing = false;
                if (m_currentPolygonItem &&
                    m_currentPolygonItem->polygon().boundingRect().isEmpty()) {
                    scene()->removeItem(m_currentPolygonItem);
                    delete m_currentPolygonItem;
                }
                m_livePolygon.clear();
                m_currentPolygonItem = nullptr;
            } else {
                QGraphicsView::mouseReleaseEvent(event);
            }
            break;
        }
        case PainterStatus::CIRCLE:
        case PainterStatus::ELLIPSE:
        {
            if (event->button() == Qt::LeftButton && m_isDrawing) {
                m_isDrawing = false;
                if (m_currentEllipseItem && m_currentEllipseItem->rect().isEmpty()) {
                    scene()->removeItem(m_currentEllipseItem);
                    delete m_currentEllipseItem;
                }
                m_currentEllipseItem = nullptr;
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

    if (m_isDrawing == false){
        saveSceneState();
    }
}

void CustomView::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Delete) {
        deleteSelectedItem();
    } else {
        QGraphicsView::keyPressEvent(event); // 其他按键交给基类处理
    }
}

void CustomView::deleteSelectedItem()
{
    // 获取所有选中的图元
    QList<QGraphicsItem*> selectedItems = scene()->selectedItems();

    // 删除选中的图元
    for (QGraphicsItem *item : selectedItems) {
        scene()->removeItem(item);
        delete item;
    }

    saveSceneState();
}

void CustomView::palatteButtonClicked()
{
    QColor color = QColorDialog::getColor(Qt::black, nullptr, "选择颜色", QColorDialog::ShowAlphaChannel);
    if(color.isValid()) {
        switch(colorType){
        case BOARD:{
            penColor = color;
            break;
        }
        case FILL:{
            brushColor = color;
            break;
        }
        }
    }
}

void CustomView::onDeleteActionClicked()
{
    deleteSelectedItem();
}

void CustomView::onSaveAs()
{
    QString filter = "PNG 图片 (*.png);;JSON 源码 (*.json)";
    QString fileName = QFileDialog::getSaveFileName(this, "保存为", "", filter);
    if (fileName.isEmpty()) return;

    if (fileName.endsWith(".png", Qt::CaseInsensitive)) {
        // 1. 画布截屏
        QRectF r = scene()->sceneRect();
        QImage img(r.size().toSize(), QImage::Format_ARGB32);
        img.fill(Qt::transparent);
        qDebug() << r;
        QPainter painter(&img);
        scene()->render(&painter);
        img.save(fileName);
    } else if (fileName.endsWith(".json", Qt::CaseInsensitive)) {
        // 2. 导出 JSON
        QJsonArray array;
        for (QGraphicsItem *it : scene()->items())
            array.append(itemToJson(it));

        QJsonDocument doc(array);
        QFile file(fileName);
        if (file.open(QIODevice::WriteOnly))
            file.write(doc.toJson());
    }
}

void CustomView::onOpen()
{
    QString fileName = QFileDialog::getOpenFileName(this, "打开", "", "JSON 源码 (*.json)");
    if (fileName.isEmpty()) return;

    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) return;

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isArray()) return;

    scene()->clear();          // 先清空
    for (const QJsonValue &v : doc.array())
        jsonToItem(v.toObject(), scene());

    saveSceneState();
}

void CustomView::saveSceneState()
{
    QJsonArray state;
    for (QGraphicsItem *it : scene()->items())
        state.append(itemToJson(it));

    undoStack.push(state);
    if (undoStack.size() > maxUndoSteps)
        undoStack.pop_front();

    redoStack.clear(); // 新操作后清空重做栈
}

void CustomView::restoreSceneState(const QJsonArray &state)
{
    scene()->clear();
    for (const QJsonValue &v : state)
        jsonToItem(v.toObject(), scene());
}

void CustomView::onRevoke()
{
    if (undoStack.size() <= 1) return;

    redoStack.push(undoStack.pop());
    restoreSceneState(undoStack.top());
}

void CustomView::onUndo()
{
    if (redoStack.isEmpty()) return;

    undoStack.push(redoStack.pop());
    restoreSceneState(undoStack.top());
}
