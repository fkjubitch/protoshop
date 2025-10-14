#ifndef COMMON_H
#define COMMON_H

#include <QPointF>
#include <QColor>

enum PainterStatus {SELECT, PEN, LINE, CURVE, RECT, POLYGON, CIRCLE, ELLIPSE};
enum MouseLeftClickStatus {PRESS, MOVE, RELEASE};
enum ColorType {BOARD, FILL};

// 广播鼠标在画布上的坐标给各个item的接口
class IMousePositionReceiver {
public:
    virtual ~IMousePositionReceiver() = default;
    // 任何希望接收场景坐标的 item 都需要实现这个函数
    virtual void receiveSceneMousePosition(const QPointF &scenePos, const MouseLeftClickStatus mouseLeftClickStatus) = 0;
};

//
class ItemCommon {
public:
    // 是否处于旋转点
    bool isRotateHandle = false;
    // 是否正在旋转
    bool isRotateHandling = false;
    // 控制点的大小
    const int HANDLE_SIZE = 10;
    // 旋转控制点距离顶部的偏移
    const float ROTATE_HANDLE_OFFSET = 15;
    // 图形属性
    QColor penColor = Qt::black;
    QColor brushColor = Qt::white; // 填充色
    int penWidth = 1; // 线宽
    Qt::PenStyle penStyle = Qt::SolidLine; // 画笔类型
};

#endif // COMMON_H
