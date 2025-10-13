#ifndef COMMON_H
#define COMMON_H

#include <QPointF>

enum PainterStatus {SELECT, PEN, LINE, CURVE, RECT, POLYGON, CIRCLE, ELLIPSE};
enum MouseLeftClickStatus {PRESS, MOVE, RELEASE};

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
    bool isRotateHandle;
    // 是否正在旋转
    bool isRotateHandling;
    // 控制点的大小
    const int HANDLE_SIZE = 10;
    // 旋转控制点距离顶部的偏移
    const float ROTATE_HANDLE_OFFSET = 15;
};

#endif // COMMON_H
