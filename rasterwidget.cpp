#include "rasterwidget.h"
#include <QPainter>
#include <QPaintEvent>
#include <QResizeEvent>
#include <QMouseEvent>
#include <QApplication>
#include <QDebug>
#include <cmath>
#include <QStack>
#include <algorithm>
#include <QFileDialog>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QColorDialog>

// --- 用于扫描线填充的辅助结构 ---
struct Edge {
    int ymax;   double x;   double dx;
    Edge(int ymax, double x, double dx) : ymax(ymax), x(x), dx(dx) {}
};
bool compareEdges(const Edge& a, const Edge& b) { return a.x < b.x; }
// ------------------------------

const qreal PI = 3.141592653589793;


// ===================================================================
// --- 1. MyShape 基类实现 ---
// ===================================================================
void MyShape::translate(const QPointF& delta) {
    // 保留但不再使用
    QTransform translation;
    translation.translate(delta.x(), delta.y());
    transform = transform * translation;
}

void MyShape::rotate(qreal angle, const QPointF& origin) {
    // 保留但不再使用
    transform.translate(origin.x(), origin.y());
    transform.rotate(angle);
    transform.translate(-origin.x(), -origin.y());
}

void MyShape::scale(qreal sx, qreal sy, const QPointF& origin) {
    // 保留但不再使用
    transform.translate(origin.x(), origin.y());
    transform.scale(sx, sy);
    transform.translate(-origin.x(), -origin.y());
}

// ===================================================================
// --- 2. MyShape 子类实现 ---
// ===================================================================

// --- MyLine ---
MyLine::MyLine(QPointF p1, QPointF p2, QColor p, int w, Qt::PenStyle s)
    : MyShape(p, Qt::transparent, w, s) {
    QPointF center = (p1 + p2) / 2.0;
    position = center; // 设置位置
    this->p1 = p1 - center;
    this->p2 = p2 - center;
    recomputeTransform(); // 计算变换矩阵
}
void MyLine::draw(RasterWidget* widget) {
    QPointF p1_screen = transform.map(this->p1);
    QPointF p2_screen = transform.map(this->p2);
    widget->rasterDrawLine(p1_screen.x(), p1_screen.y(), p2_screen.x(), p2_screen.y(), penColor, penWidth, penStyle);
}
QRectF MyLine::getBoundingBox() {
    return QPolygonF({transform.map(p1), transform.map(p2)}).boundingRect();
}
bool MyLine::contains(const QPointF& p) {
    QPointF p1_t = transform.map(p1);
    QPointF p2_t = transform.map(p2);
    QLineF line(p1_t, p2_t);
    qreal len = line.length();
    if (len == 0) return QLineF(p, p1_t).length() < 5 + penWidth / 2;
    qreal dot = QPointF::dotProduct(p - p1_t, p2_t - p1_t) / (len * len);
    if (dot < 0 || dot > 1) return false;
    QPointF proj = p1_t + dot * (p2_t - p1_t);
    return QLineF(p, proj).length() < 5 + penWidth / 2;
}


// --- MyRect ---
MyRect::MyRect(QRectF r, QColor p, QColor b, int w, Qt::PenStyle s)
    : MyShape(p, b, w, s) {
    position = r.center(); // 设置位置
    this->rect = QRectF(-r.width() / 2, -r.height() / 2, r.width(), r.height());
    recomputeTransform(); // 计算变换矩阵
}
void MyRect::draw(RasterWidget* widget) {
    QPolygonF local_poly;
    local_poly << rect.topLeft() << rect.topRight() << rect.bottomRight() << rect.bottomLeft();
    QPolygonF transformed_poly = transform.map(local_poly);
    QVector<QPoint> points;
    for(const QPointF& p : transformed_poly) points.append(p.toPoint());
    widget->rasterScanFillPolygon(points, brushColor, penColor, penWidth, penStyle);
}
QRectF MyRect::getBoundingBox() {
    QPolygonF poly;
    poly << transform.map(rect.topLeft()) << transform.map(rect.topRight())
         << transform.map(rect.bottomRight()) << transform.map(rect.bottomLeft());
    return poly.boundingRect();
}
bool MyRect::contains(const QPointF& p) {
    QTransform inv_transform = transform.inverted();
    QPointF p_local = inv_transform.map(p);
    return rect.contains(p_local);
}


// --- MyCircle ---
MyCircle::MyCircle(qreal r, QColor p, QColor b, int w, Qt::PenStyle s)
    : MyShape(p, b, w, s), radius(r) {
    // 位置默认为 (0,0)，由外部设置
    recomputeTransform();
}
void MyCircle::draw(RasterWidget* widget) {
    QVector<QPoint> points;
    int segments = std::max(36, (int)(radius / 2));
    for (int i = 0; i < segments; ++i) {
        qreal angle = 2.0 * PI * i / (qreal)segments;
        QPointF p_local(radius * cos(angle), radius * sin(angle));
        points.append(transform.map(p_local).toPoint());
    }
    widget->rasterScanFillPolygon(points, brushColor, penColor, penWidth, penStyle);
}
QRectF MyCircle::getBoundingBox() {
    QPolygonF poly;
    int segments = 36;
    for (int i = 0; i < segments; ++i) {
        qreal angle = 2.0 * PI * i / (qreal)segments;
        poly << transform.map(QPointF(radius * cos(angle), radius * sin(angle)));
    }
    return poly.boundingRect();
}
bool MyCircle::contains(const QPointF& p) {
    QTransform inv_transform = transform.inverted();
    QPointF p_local = inv_transform.map(p);
    return QLineF(QPointF(0, 0), p_local).length() <= radius;
}


// --- MyEllipse ---
MyEllipse::MyEllipse(qreal rx, qreal ry, QColor p, QColor b, int w, Qt::PenStyle s)
    : MyShape(p, b, w, s), rx(rx), ry(ry) {
    // 位置默认为 (0,0)，由外部设置
    recomputeTransform();
}
void MyEllipse::draw(RasterWidget* widget) {
    QVector<QPoint> points;
    int segments = std::max(36, (int)((rx+ry) / 4));
    for (int i = 0; i < segments; ++i) {
        qreal angle = 2.0 * PI * i / (qreal)segments;
        QPointF p_local(rx * cos(angle), ry * sin(angle));
        points.append(transform.map(p_local).toPoint());
    }
    widget->rasterScanFillPolygon(points, brushColor, penColor, penWidth, penStyle);
}
QRectF MyEllipse::getBoundingBox() {
    QPolygonF poly;
    int segments = 36;
    for (int i = 0; i < segments; ++i) {
        qreal angle = 2.0 * PI * i / (qreal)segments;
        poly << transform.map(QPointF(rx * cos(angle), ry * sin(angle)));
    }
    return poly.boundingRect();
}
bool MyEllipse::contains(const QPointF& p) {
    QTransform inv_transform = transform.inverted();
    QPointF p_local = inv_transform.map(p);
    if (rx == 0 || ry == 0) return false;
    return (p_local.x() * p_local.x()) / (rx * rx) + (p_local.y() * p_local.y()) / (ry * ry) <= 1.0;
}


// --- MyPolygon ---
MyPolygon::MyPolygon(QVector<QPoint> p, QColor pen, QColor brush, int w, Qt::PenStyle s)
    : MyShape(pen, brush, w, s) {
    QPointF center(0, 0);
    if (!p.isEmpty()) {
        for (const QPoint& pt : p) { center += pt; }
        center /= p.size();
    }
    position = center; // 设置位置
    for (const QPoint& pt : p) {
        points.append(QPointF(pt) - center);
    }
    recomputeTransform(); // 计算变换矩阵
}
void MyPolygon::draw(RasterWidget* widget) {
    QPolygonF local_poly(points);
    QPolygonF transformed_poly = transform.map(local_poly);
    QVector<QPoint> screen_points;
    for (const QPointF& p : transformed_poly) {
        screen_points.append(p.toPoint());
    }
    widget->rasterScanFillPolygon(screen_points, brushColor, penColor, penWidth, penStyle);
}
QRectF MyPolygon::getBoundingBox() {
    QPolygonF local_poly(points);
    QPolygonF transformed_poly = transform.map(local_poly);
    return transformed_poly.boundingRect();
}
bool MyPolygon::contains(const QPointF& p) {
    QTransform inv_transform = transform.inverted();
    QPointF p_local = inv_transform.map(p);
    return QPolygonF(points.toVector()).containsPoint(p_local, Qt::OddEvenFill);
}


// --- MyPath ---
MyPath::MyPath(QVector<QPoint> p, QColor pen, int w, Qt::PenStyle s)
    : MyShape(pen, Qt::transparent, w, s) {
    QPointF center(0, 0);
    if (!p.isEmpty()) {
        for (const QPoint& pt : p) { center += pt; }
        center /= p.size();
    }
    position = center; // 设置位置
    for (const QPoint& pt : p) {
        points.append(QPointF(pt) - center);
    }
    recomputeTransform(); // 计算变换矩阵
}
void MyPath::draw(RasterWidget* widget) {
    QPolygonF local_poly(points);
    QPolygonF transformed_poly = transform.map(local_poly);
    for (int i = 0; i < transformed_poly.size() - 1; ++i) {
        QPoint p1 = transformed_poly[i].toPoint();
        QPoint p2 = transformed_poly[i+1].toPoint();
        widget->rasterDrawLine(p1.x(), p1.y(), p2.x(), p2.y(), penColor, penWidth, penStyle);
    }
}
QRectF MyPath::getBoundingBox() {
    QPolygonF local_poly(points);
    QPolygonF transformed_poly = transform.map(local_poly);
    return transformed_poly.boundingRect();
}
bool MyPath::contains(const QPointF& p) {
    QPolygonF transformed_poly = transform.map(QPolygonF(points));
    for (int i = 0; i < transformed_poly.size() - 1; ++i) {
        QLineF line(transformed_poly[i], transformed_poly[i+1]);
        qreal len = line.length();
        if (len == 0) continue;
        qreal dot = QPointF::dotProduct(p - line.p1(), line.p2() - line.p1()) / (len * len);
        if (dot < 0 || dot > 1) continue;
        QPointF proj = line.p1() + dot * (line.p2() - line.p1());
        if (QLineF(p, proj).length() < 5 + penWidth / 2) return true;
    }
    return false;
}

// getLocalBoundingBox
QRectF MyLine::getLocalBoundingBox() {
    return QRectF(p1, p2).normalized();
}
QRectF MyRect::getLocalBoundingBox() {
    return rect;
}
QRectF MyCircle::getLocalBoundingBox() {
    return QRectF(-radius, -radius, radius*2, radius*2);
}
QRectF MyEllipse::getLocalBoundingBox() {
    return QRectF(-rx, -ry, rx*2, ry*2);
}
QRectF MyPolygon::getLocalBoundingBox() {
    return QPolygonF(points).boundingRect();
}
QRectF MyPath::getLocalBoundingBox() {
    return QPolygonF(points).boundingRect();
}

// ===================================================================
// --- 3. RasterWidget 构造/析构/初始化 ---
// ===================================================================

RasterWidget::RasterWidget(QWidget *parent)
    : QWidget(parent)
{
    m_painterStatus = PainterStatus::SELECT;
    m_colorType = ColorType::BOARD;
    m_penColor = Qt::black;
    m_brushColor = QColor(255, 255, 255, 0);
    m_penWidth = 1;
    m_penStyle = Qt::SolidLine;
    m_isDrawing = false;
    m_isTransforming = false;
    m_activeHandle = nullptr;
    m_currentOpHandlePos = HandlePosition::Center;
    isChosen = true;
    m_isFilling = false;

    resizeBuffer(QSize(800, 600));
    setMouseTracking(true);
    setFocusPolicy(Qt::ClickFocus);
}

RasterWidget::~RasterWidget()
{
    qDeleteAll(m_shapeList);
    qDeleteAll(m_handles);
}

void RasterWidget::resizeBuffer(const QSize &size)
{
    if (m_canvasBuffer.size() == size) return;
    QImage newCanvasBuffer(size, QImage::Format_ARGB32_Premultiplied);
    newCanvasBuffer.fill(Qt::white);
    QPainter p_canvas(&newCanvasBuffer);
    p_canvas.end();
    m_canvasBuffer = newCanvasBuffer;
    qInfo() << "Raster canvas resized to" << size;
}

void RasterWidget::resizeEvent(QResizeEvent *event)
{
    resizeBuffer(event->size());
    redrawAllShapes();
}


// ===================================================================
// --- 4. 核心绘图 (PaintEvent 和 Redraw) ---
// ===================================================================

void RasterWidget::redrawAllShapes()
{
    m_canvasBuffer.fill(Qt::white);
    QPainter p(&m_canvasBuffer);
    p.setRenderHint(QPainter::Antialiasing);

    for (MyShape* shape : m_shapeList) {
        shape->draw(this);
    }

    clearHandles();
    if (!m_selectedShapes.isEmpty()) {
        p.setPen(QPen(Qt::blue, 1, Qt::DashLine));
        p.setBrush(Qt::NoBrush);

        if (m_selectedShapes.count() == 1) {
            MyShape* shape = m_selectedShapes.first();

            // 获取旋转后的多边形 (OBB)
            QPolygonF localRect(shape->getLocalBoundingBox());
            QPolygonF obb = shape->transform.map(localRect);

            createHandles(obb);

            // 绘制旋转后的边框
            p.drawPolygon(obb);

            // 绘制控制点
            p.setPen(Qt::black);
            p.setBrush(Qt::white);
            for (ControlHandle* h : m_handles) {
                p.drawRect(h->getRect(obb));
            }

            // 绘制旋转手柄的连线
            ControlHandle* rotH = m_handles.last(); // Rotate is last
            ControlHandle* topH = m_handles[1];     // Top is index 1
            if (rotH && topH) {
                p.setPen(Qt::black);
                p.drawLine(rotH->getRect(obb).center(), topH->getRect(obb).center());
            }

        } else {
            // 多选时保持 AABB (简单处理)
            for (MyShape* shape : m_selectedShapes) {
                p.drawRect(shape->getBoundingBox());
            }
        }
    }
    p.end();
    update();
}

void RasterWidget::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.drawImage(0, 0, m_canvasBuffer);

    if (m_isDrawing && m_painterStatus == PainterStatus::SELECT) {
        painter.setPen(QPen(Qt::blue, 1, Qt::DashLine));
        painter.setBrush(QColor(0, 0, 100, 30));
        painter.drawRect(QRect(m_startPoint, m_currentPoint).normalized());
    }
    else if (m_isDrawing && m_painterStatus != PainterStatus::SELECT) {
        painter.setPen(QPen(m_penColor, m_penWidth, m_penStyle));
        painter.setBrush(m_brushColor.alpha() == 0 ? Qt::NoBrush : m_brushColor);

        switch (m_painterStatus) {
        case PainterStatus::LINE:
            painter.drawLine(m_startPoint, m_currentPoint);
            break;
        case PainterStatus::RECT:
            painter.drawRect(QRect(m_startPoint, m_currentPoint).normalized());
            break;
        case PainterStatus::CIRCLE: {
            QRectF rect = QRect(m_startPoint, m_currentPoint).normalized();
            qreal r = std::max(rect.width(), rect.height()) / 2.0;
            painter.drawEllipse(rect.center(), r, r);
            break;
        }
        case PainterStatus::ELLIPSE:
            painter.drawEllipse(QRect(m_startPoint, m_currentPoint).normalized());
            break;
        case PainterStatus::PEN: // (FALLTHROUGH)
        case PainterStatus::POLYGON:
            if (!m_tempPoints.isEmpty()) {
                QPolygon poly = m_tempPoints;
                poly.append(m_currentPoint);
                painter.drawPolyline(poly);
            }
            break;
        default: break;
        }
    }
}


// ===================================================================
// --- 5. 鼠标和键盘事件 ---
// ===================================================================
void RasterWidget::mousePressEvent(QMouseEvent *event)
{
    if (m_painterStatus == PainterStatus::POLYGON && event->button() == Qt::RightButton) {
        if (m_isDrawing && m_tempPoints.size() >= 3) {
            MyPolygon* poly = new MyPolygon(m_tempPoints, m_penColor, m_brushColor, m_penWidth, m_penStyle);
            m_shapeList.append(poly);
            m_selectedShapes.clear();
            m_selectedShapes.append(poly);
            m_isDrawing = false;
            m_tempPoints.clear();
            redrawAllShapes();
        }
        return;
    }

    if (event->button() != Qt::LeftButton) return;
    m_dragStartPosition = event->pos();

    if (m_painterStatus == PainterStatus::SELECT) {

        // 1. 检查控制点
        if (m_selectedShapes.count() == 1) {
            m_activeHandle = getHandleAt(event->pos());
        } else {
            m_activeHandle = nullptr;
        }

        if (m_activeHandle) {
            m_isTransforming = true;
            m_currentOpHandlePos = m_activeHandle->pos;

            // 备份当前所有分解参数
            m_originalParams.clear();
            for(MyShape* s : m_selectedShapes) {
                TransformParams params;
                params.position = s->position;
                params.rotation = s->rotation;
                params.scaleX = s->scaleX;
                params.scaleY = s->scaleY;
                params.transform = s->transform; // 用于鼠标坐标转换
                m_originalParams[s] = params;
            }
            m_originalBoundingBox = getSelectedShapesBoundingBox();
            return;
        }

        MyShape* shape = getShapeAt(event->pos());
        if (shape) {
            m_isTransforming = true;
            m_activeHandle = new ControlHandle(HandlePosition::Center);
            m_currentOpHandlePos = HandlePosition::Center;

            bool shiftPressed = (QApplication::keyboardModifiers() & Qt::ShiftModifier);

            if (!m_selectedShapes.contains(shape)) {
                if (!shiftPressed) {
                    m_selectedShapes.clear();
                }
                m_selectedShapes.append(shape);
            }

            // 备份当前所有分解参数
            m_originalParams.clear();
            for(MyShape* s : m_selectedShapes) {
                TransformParams params;
                params.position = s->position;
                params.rotation = s->rotation;
                params.scaleX = s->scaleX;
                params.scaleY = s->scaleY;
                params.transform = s->transform;
                m_originalParams[s] = params;
            }
            m_originalBoundingBox = getSelectedShapesBoundingBox();

            redrawAllShapes();
            return;
        }

        m_isDrawing = true;
        m_startPoint = event->pos();
        m_currentPoint = event->pos();
        if (!m_selectedShapes.isEmpty()) {
            m_selectedShapes.clear();
            redrawAllShapes();
        }
        return;
    }

    if (!m_selectedShapes.isEmpty()) {
        m_selectedShapes.clear();
        redrawAllShapes();
    }

    switch (m_painterStatus) {
    case PainterStatus::PEN:
        m_isDrawing = true;
        m_tempPoints.clear();
        m_tempPoints.append(event->pos());
        break;
    case PainterStatus::LINE:
    case PainterStatus::RECT:
    case PainterStatus::CIRCLE:
    case PainterStatus::ELLIPSE:
        m_isDrawing = true;
        m_startPoint = event->pos();
        m_currentPoint = event->pos();
        break;
    case PainterStatus::POLYGON:
        m_isDrawing = true;
        m_tempPoints.append(event->pos());
        m_currentPoint = event->pos();
        update();
        break;
    case PainterStatus::FILLSELECT:
        rasterFloodFill(event->pos().x(), event->pos().y(), m_brushColor);
        break;
    default:
        break;
    }
}

void RasterWidget::mouseMoveEvent(QMouseEvent *event)
{
    emit sendMousePos(event->pos());
    m_currentPoint = event->pos();
    QPointF dragDelta = QPointF(m_currentPoint - m_dragStartPosition);

    if (m_isTransforming && m_painterStatus == PainterStatus::SELECT && !m_selectedShapes.isEmpty()) {

        // 1. 还原到原始分解参数
        for(MyShape* s : m_selectedShapes) {
            if (m_originalParams.contains(s)) {
                s->position = m_originalParams[s].position;
                s->rotation = m_originalParams[s].rotation;
                s->scaleX = m_originalParams[s].scaleX;
                s->scaleY = m_originalParams[s].scaleY;
            }
        }

        switch(m_currentOpHandlePos) {
        case HandlePosition::Center: { // 平移
            for(MyShape* s : m_selectedShapes) {
                s->position += dragDelta;
            }
            break;
        }

        case HandlePosition::Rotate: { // 旋转
            if (m_selectedShapes.count() != 1) break;
            MyShape* s = m_selectedShapes.first();

            // 使用当前transform计算形状中心（因为已经还原到原始状态）
            QPointF shapeCenter = s->position;

            // 计算角度
            qreal angle1 = QLineF(shapeCenter, m_dragStartPosition).angle();
            qreal angle2 = QLineF(shapeCenter, m_currentPoint).angle();
            qreal angleDiff = angle1 - angle2;

            s->rotation += angleDiff; // 累加旋转角度
            break;
        }

        default: { // 缩放
            if (m_selectedShapes.count() != 1) break;
            MyShape* s = m_selectedShapes.first();

            // 获取形状的中心
            QPointF shapeCenter = s->position;

            // 计算鼠标在原始变换空间中的向量
            QTransform invTrans = m_originalParams[s].transform.inverted();
            QPointF localStart = invTrans.map(m_dragStartPosition);
            QPointF localCurr = invTrans.map(m_currentPoint);

            qreal sx = 1.0;
            qreal sy = 1.0;
            const qreal EPSILON = 7.0;

            if (std::abs(localStart.x()) > EPSILON) {
                sx = localCurr.x() / localStart.x();
            }
            if (std::abs(localStart.y()) > EPSILON) {
                sy = localCurr.y() / localStart.y();
            }

            // 限制最小缩放值，防止翻转或消失
            sx = std::max(0.01, sx);
            sy = std::max(0.01, sy);

            // 应用缩放（直接设置，而不是累乘）
            s->scaleX = m_originalParams[s].scaleX * sx;
            s->scaleY = m_originalParams[s].scaleY * sy;
            break;
        }
        }

        // 2. 为所有受影响的形状重新计算变换矩阵
        for(MyShape* s : m_selectedShapes) {
            s->recomputeTransform();
        }

        redrawAllShapes();
        return;
    }

    if (m_isDrawing && m_painterStatus == PainterStatus::SELECT) { update(); return; }
    if (m_isDrawing) {
        if (m_painterStatus == PainterStatus::PEN) { m_tempPoints.append(event->pos()); update(); }
        else if (m_painterStatus == PainterStatus::POLYGON) { update(); }
        else { update(); }
    }
    if (m_painterStatus == PainterStatus::SELECT) {
        ControlHandle* h = getHandleAt(event->pos());
        if(h) setCursorForHandle(h->pos);
        else if (getShapeAt(event->pos())) setCursor(Qt::SizeAllCursor);
        else setCursor(Qt::ArrowCursor);
    } else {
        setCursor(Qt::CrossCursor);
    }
}

void RasterWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) return;

    if (m_isTransforming) {
        if (m_currentOpHandlePos == HandlePosition::Center && m_activeHandle) {
            delete m_activeHandle;
        }
        m_isTransforming = false;
        m_activeHandle = nullptr;
        m_originalParams.clear(); // 清除备份
    }

    if (m_isDrawing) {
        if (m_painterStatus == PainterStatus::SELECT) {
            m_isDrawing = false;
            QRectF rubberBandRect = QRectF(m_startPoint, m_currentPoint).normalized();
            m_selectedShapes.clear();
            for (int i = 0; i < m_shapeList.size(); ++i) {
                if (rubberBandRect.intersects(m_shapeList[i]->getBoundingBox())) {
                    m_selectedShapes.append(m_shapeList[i]);
                }
            }
            redrawAllShapes();
            return;
        }

        if (m_painterStatus != PainterStatus::POLYGON) {
            m_isDrawing = false;
        }

        MyShape* newShape = nullptr;
        QRectF rect = QRect(m_startPoint, m_currentPoint).normalized();

        switch (m_painterStatus) {
        case PainterStatus::PEN:
            if (m_tempPoints.size() > 1) {
                newShape = new MyPath(m_tempPoints, m_penColor, m_penWidth, m_penStyle);
            }
            m_tempPoints.clear();
            break;
        case PainterStatus::LINE:
            newShape = new MyLine(m_startPoint, m_currentPoint, m_penColor, m_penWidth, m_penStyle);
            break;
        case PainterStatus::RECT:
            newShape = new MyRect(rect, m_penColor, m_brushColor, m_penWidth, m_penStyle);
            break;
        case PainterStatus::CIRCLE: {
            qreal r = std::max(rect.width(), rect.height()) / 2.0;
            newShape = new MyCircle(r, m_penColor, m_brushColor, m_penWidth, m_penStyle);
            newShape->position = rect.center(); // 设置位置
            newShape->recomputeTransform(); // 重新计算
            break;
        }
        case PainterStatus::ELLIPSE: {
            newShape = new MyEllipse(rect.width() / 2.0, rect.height() / 2.0, m_penColor, m_brushColor, m_penWidth, m_penStyle);
            newShape->position = rect.center(); // 设置位置
            newShape->recomputeTransform(); // 重新计算
            break;
        }
        case PainterStatus::POLYGON:
            break;
        default: break;
        }

        if (newShape) {
            m_shapeList.append(newShape);
            m_selectedShapes.clear();
            m_selectedShapes.append(newShape);
        }

        if (newShape) {
            redrawAllShapes();
        }
    }
}

void RasterWidget::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Delete && !m_selectedShapes.isEmpty()) {
        deleteSelectedShapes();
    } else {
        QWidget::keyPressEvent(event);
    }
}

void RasterWidget::deleteSelectedShapes()
{
    if (m_selectedShapes.isEmpty()) return;
    for (MyShape* shape : m_selectedShapes) {
        m_shapeList.removeOne(shape);
        delete shape;
    }
    clearHandles();
    m_selectedShapes.clear();
    redrawAllShapes();
}

// ===================================================================
// --- 6. 槽函数实现 ---
// ===================================================================
void RasterWidget::setPainterStatus(const PainterStatus ps)
{
    if (m_isDrawing && (m_painterStatus == PainterStatus::POLYGON || m_painterStatus == PainterStatus::PEN)) {
        m_isDrawing = false;
        m_tempPoints.clear();
    }

    m_painterStatus = ps;
    if (m_painterStatus != PainterStatus::SELECT) {
        if (!m_selectedShapes.isEmpty()) {
            m_selectedShapes.clear();
            redrawAllShapes();
        }
    }
}

void RasterWidget::setPenColor(const QColor &color)
{
    if (m_colorType == ColorType::BOARD) {
        m_penColor = color;
        for (MyShape* s : m_selectedShapes) s->penColor = color;
    } else {
        m_brushColor = color;
        for (MyShape* s : m_selectedShapes) s->brushColor = color;
    }
    if(!m_selectedShapes.isEmpty()) redrawAllShapes();
}

void RasterWidget::setBrushColor(const QColor &color) {
    m_brushColor = color;
    for (MyShape* s : m_selectedShapes) {
        s->brushColor = color;
    }
    if(!m_selectedShapes.isEmpty()) redrawAllShapes();
}

void RasterWidget::setColorType(const ColorType type) {
    m_colorType = type;
}

void RasterWidget::setPenWidth(int width) {
    m_penWidth = width;
    for (MyShape* s : m_selectedShapes) {
        s->penWidth = width;
    }
    if(!m_selectedShapes.isEmpty()) redrawAllShapes();
}

void RasterWidget::setPenStyle(Qt::PenStyle style) {
    m_penStyle = style;
    for (MyShape* s : m_selectedShapes) {
        s->penStyle = style;
    }
    if(!m_selectedShapes.isEmpty()) redrawAllShapes();
}

void RasterWidget::clearCanvas()
{
    qDeleteAll(m_shapeList);
    m_shapeList.clear();
    m_selectedShapes.clear();
    m_isDrawing = false;
    m_tempPoints.clear();
    m_isTransforming = false;
    m_canvasBuffer.fill(Qt::white);
    update();
}

void RasterWidget::onSaveAs()
{
    if(!isChosen) return;
    QString fileName = QFileDialog::getSaveFileName(this, "保存为", "", "JSON 文件 (*.json);;PNG 图片 (*.png)");
    if (fileName.isEmpty()) return;

    if (fileName.endsWith(".png")) {
        QList<MyShape*> temp = m_selectedShapes;
        m_selectedShapes.clear();
        redrawAllShapes();
        m_canvasBuffer.save(fileName);
        m_selectedShapes = temp;
        redrawAllShapes();
    } else if (fileName.endsWith(".json")) {
        qWarning() << "JSON save not implemented for RasterWidget yet.";
    }
}

void RasterWidget::palatteButtonClicked()
{
    if(!isChosen) return;
    QString title = (m_colorType == ColorType::BOARD) ? "选择边框颜色" : "选择填充颜色";
    QColor color = QColorDialog::getColor(Qt::black, nullptr, title, QColorDialog::ShowAlphaChannel);

    if(color.isValid()) {
        if (m_colorType == ColorType::BOARD) {
            setPenColor(color);
        } else {
            setBrushColor(color);
        }
    }
}

void RasterWidget::onOpen()
{
    if(!isChosen) return;
    qWarning() << "JSON open not implemented for RasterWidget yet.";
}


// ===================================================================
// --- 7. 变换/控制点 辅助函数 ---
// ===================================================================
MyShape* RasterWidget::getShapeAt(const QPoint& p) {
    for (int i = m_shapeList.size() - 1; i >= 0; --i) {
        if (m_shapeList[i]->contains(p)) {
            return m_shapeList[i];
        }
    }
    return nullptr;
}

QRectF RasterWidget::getSelectedShapesBoundingBox() {
    if (m_selectedShapes.isEmpty()) return QRectF();
    if (m_selectedShapes.count() == 1) return m_selectedShapes.first()->getBoundingBox();

    QRectF totalBox = m_selectedShapes.first()->getBoundingBox();
    for (int i = 1; i < m_selectedShapes.count(); ++i) {
        totalBox = totalBox.united(m_selectedShapes[i]->getBoundingBox());
    }
    return totalBox;
}

ControlHandle* RasterWidget::getHandleAt(const QPoint& p) {
    if (m_selectedShapes.count() != 1) return nullptr;

    MyShape* s = m_selectedShapes.first();
    // 获取旋转后的 OBB 多边形
    QPolygonF localPoly(s->getLocalBoundingBox());
    QPolygonF obb = s->transform.map(localPoly);

    for (ControlHandle* h : m_handles) {
        if (h->getRect(obb).contains(p)) {
            return h;
        }
    }
    return nullptr;
}

void RasterWidget::clearHandles() {
    qDeleteAll(m_handles);
    m_handles.clear();
}

void RasterWidget::createHandles(const QPolygonF& obb) {
    if (obb.isEmpty()) return;
    clearHandles();
    m_handles << new ControlHandle(HandlePosition::TopLeft);
    m_handles << new ControlHandle(HandlePosition::Top);
    m_handles << new ControlHandle(HandlePosition::TopRight);
    m_handles << new ControlHandle(HandlePosition::Right);
    m_handles << new ControlHandle(HandlePosition::BottomRight);
    m_handles << new ControlHandle(HandlePosition::Bottom);
    m_handles << new ControlHandle(HandlePosition::BottomLeft);
    m_handles << new ControlHandle(HandlePosition::Left);
    m_handles << new ControlHandle(HandlePosition::Rotate);
}

QRectF ControlHandle::getRect(const QPolygonF& obb) {
    const int SIZE = 10;
    QPointF center;

    // obb[0]=TL, obb[1]=TR, obb[2]=BR, obb[3]=BL
    if (obb.count() < 4) return QRectF();

    QPointF tl = obb[0];
    QPointF tr = obb[1];
    QPointF br = obb[2];
    QPointF bl = obb[3];

    switch(pos) {
    case HandlePosition::TopLeft:     center = tl; break;
    case HandlePosition::TopRight:    center = tr; break;
    case HandlePosition::BottomRight: center = br; break;
    case HandlePosition::BottomLeft:  center = bl; break;

    case HandlePosition::Top:         center = (tl + tr) / 2.0; break;
    case HandlePosition::Right:       center = (tr + br) / 2.0; break;
    case HandlePosition::Bottom:      center = (bl + br) / 2.0; break;
    case HandlePosition::Left:        center = (bl + tl) / 2.0; break;

    case HandlePosition::Rotate: {
        // 旋转手柄放在 Top 手柄上方 20px 处 (沿着局部Y轴方向)
        QPointF topMid = (tl + tr) / 2.0;
        // 计算向上的向量 (利用 TopLeft -> BottomLeft 向量取反)
        QLineF upVec(bl, tl);
        // 归一化并延伸
        if (upVec.length() > 0) {
            upVec.setLength(20);
            center = topMid + (upVec.p2() - upVec.p1());
        } else {
            center = topMid + QPointF(0, -20);
        }
        break;
    }
    case HandlePosition::Center:
        // 平均值求中心
        center = (tl + tr + br + bl) / 4.0;
        break;
    }
    return QRectF(center.x() - SIZE/2, center.y() - SIZE/2, SIZE, SIZE);
}

void RasterWidget::setCursorForHandle(HandlePosition pos) {
    switch(pos) {
    case HandlePosition::TopLeft:
    case HandlePosition::BottomRight: setCursor(Qt::SizeFDiagCursor); break;
    case HandlePosition::TopRight:
    case HandlePosition::BottomLeft:  setCursor(Qt::SizeBDiagCursor); break;
    case HandlePosition::Top:
    case HandlePosition::Bottom:      setCursor(Qt::SizeVerCursor); break;
    case HandlePosition::Left:
    case HandlePosition::Right:       setCursor(Qt::SizeHorCursor); break;
    case HandlePosition::Rotate:      setCursor(Qt::CrossCursor); break;
    default: setCursor(Qt::ArrowCursor); break;
    }
}


// ===================================================================
// --- 8. 光栅化算法 (保持不变) ---
// ===================================================================
void RasterWidget::drawPixel(int x, int y, const QColor &color) {
    if (m_canvasBuffer.rect().contains(x, y)) {
        m_canvasBuffer.setPixelColor(x, y, color);
    }
}
void RasterWidget::drawThickPixel(int x, int y, int width, const QColor &color) {
    int r = width / 2;
    for (int iy = -r; iy <= r; ++iy) {
        for (int ix = -r; ix <= r; ++ix) {
            drawPixel(x + ix, y + iy, color);
        }
    }
}

void RasterWidget::rasterDrawLine(int x0, int y0, int x1, int y1, const QColor &color, int width, Qt::PenStyle style) {
    int dx = std::abs(x1 - x0);
    int dy = -std::abs(y1 - y0);
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx + dy;
    int e2;
    int dashPattern = 0;
    int dashLength = 10;
    bool drawing = true;
    while (true) {
        switch (style) {
        case Qt::SolidLine: drawing = true; break;
        case Qt::DashLine: drawing = (dashPattern / dashLength) % 2 == 0; break;
        case Qt::DotLine: drawing = (dashPattern % dashLength) == 0; break;
        case Qt::DashDotLine: {
            int segment = (dashPattern / dashLength) % 4;
            drawing = (segment == 0 || segment == 2);
            if (segment == 2) drawing = (dashPattern % (dashLength/2)) == 0;
            break;
        }
        default: drawing = true; break;
        }
        dashPattern++;
        if (drawing) {
            drawThickPixel(x0, y0, width, color);
        }
        if (x0 == x1 && y0 == y1) break;
        e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void RasterWidget::rasterDrawCircle(int xc, int yc, int radius, const QColor &color, int width, Qt::PenStyle style) {
    int x = radius; int y = 0; int p = 1 - radius;
    drawThickPixel(xc + x, yc - y, width, color);
    if (radius > 0) {
        drawThickPixel(xc - x, yc + y, width, color);
        drawThickPixel(xc + y, yc + x, width, color);
        drawThickPixel(xc - y, yc - x, width, color);
    }
    while (x > y) {
        y++;
        if (p <= 0) { p = p + 2 * y + 1; }
        else { x--; p = p + 2 * y - 2 * x + 1; }
        if (x < y) break;
        drawThickPixel(xc + x, yc - y, width, color);
        drawThickPixel(xc - x, yc - y, width, color);
        drawThickPixel(xc + x, yc + y, width, color);
        drawThickPixel(xc - x, yc + y, width, color);
        if (x != y) {
            drawThickPixel(xc + y, yc - x, width, color);
            drawThickPixel(xc - y, yc - x, width, color);
            drawThickPixel(xc + y, yc + x, width, color);
            drawThickPixel(xc - y, yc + x, width, color);
        }
    }
}

void RasterWidget::rasterDrawEllipse(int xc, int yc, int rx, int ry, const QColor &color, int width, Qt::PenStyle style) {
    long rx2 = (long)rx * rx; long ry2 = (long)ry * ry;
    long twoRx2 = 2 * rx2; long twoRy2 = 2 * ry2;
    int x = 0; int y = ry;
    long p = (long)std::round(ry2 - rx2 * ry + 0.25 * rx2);
    long px = 0; long py = twoRx2 * y;
    while (px < py) {
        drawThickPixel(xc + x, yc + y, width, color);
        drawThickPixel(xc - x, yc + y, width, color);
        drawThickPixel(xc + x, yc - y, width, color);
        drawThickPixel(xc - x, yc - y, width, color);
        x++; px += twoRy2;
        if (p < 0) { p += ry2 + px; }
        else { y--; py -= twoRx2; p += ry2 + px - py; }
    }
    p = (long)std::round(ry2 * (x + 0.5) * (x + 0.5) + rx2 * (y - 1) * (y - 1) - rx2 * ry2);
    while (y >= 0) {
        drawThickPixel(xc + x, yc + y, width, color);
        drawThickPixel(xc - x, yc + y, width, color);
        drawThickPixel(xc + x, yc - y, width, color);
        drawThickPixel(xc - x, yc - y, width, color);
        y--; py -= twoRx2;
        if (p > 0) { p += rx2 - py; }
        else { x++; px += twoRy2; p += rx2 - py + px; }
    }
}

void RasterWidget::rasterScanFillPolygon(const QVector<QPoint> &points, const QColor &fillColor, const QColor &borderColor, int width, Qt::PenStyle style) {
    if (points.size() < 3) return;
    int yMin = points[0].y(); int yMax = points[0].y();
    for (const QPoint &p : points) {
        if (p.y() < yMin) yMin = p.y();
        if (p.y() > yMax) yMax = p.y();
    }
    if (yMin > m_canvasBuffer.height() || yMax < 0) return;
    yMin = std::max(0, yMin);
    yMax = std::min(m_canvasBuffer.height() - 1, yMax);

    QVector<QList<Edge>> ET(yMax - yMin + 1);
    for (int i = 0; i < points.size(); ++i) {
        const QPoint &p1 = points[i];
        const QPoint &p2 = points[(i + 1) % points.size()];
        if (p1.y() == p2.y()) continue;

        int ymin_edge = std::min(p1.y(), p2.y());
        int ymax_edge = std::max(p1.y(), p2.y());
        if (ymax_edge < yMin || ymin_edge > yMax) continue;

        double x_at_ymin = (p1.y() < p2.y()) ? p1.x() : p2.x();
        double dx = (double)(p2.x() - p1.x()) / (double)(p2.y() - p1.y());

        if (ymin_edge < yMin) {
            x_at_ymin += dx * (yMin - ymin_edge);
            ymin_edge = yMin;
        }
        ET[ymin_edge - yMin].append(Edge(ymax_edge, x_at_ymin, dx));
    }

    QList<Edge> AET;
    for (int y = yMin; y <= yMax; ++y) {
        int et_index = y - yMin;
        for (const Edge &e : ET[et_index]) { AET.append(e); }
        AET.erase(std::remove_if(AET.begin(), AET.end(),
                                 [y](const Edge &e) { return e.ymax == y; }), AET.end());
        if (AET.isEmpty()) continue;
        std::sort(AET.begin(), AET.end(), compareEdges);
        if (fillColor.alpha() != 0) {
            for (int i = 0; i < AET.size(); i += 2) {
                if (i + 1 < AET.size()) {
                    int xStart = std::ceil(AET[i].x);
                    int xEnd = std::floor(AET[i + 1].x);
                    xStart = std::max(0, xStart);
                    xEnd = std::min(m_canvasBuffer.width() - 1, xEnd);
                    for (int x = xStart; x <= xEnd; ++x) {
                        drawPixel(x, y, fillColor);
                    }
                }
            }
        }
        for (Edge &e : AET) { e.x += e.dx; }
    }
    if (borderColor.alpha() != 0 && width > 0) {
        for (int i = 0; i < points.size(); ++i) {
            const QPoint &p1 = points[i];
            const QPoint &p2 = points[(i + 1) % points.size()];
            rasterDrawLine(p1.x(), p1.y(), p2.x(), p2.y(), borderColor, width, style);
        }
    }
}

void RasterWidget::rasterFloodFill(int x, int y, const QColor &fillColor) {
    if (m_isFilling) return;
    m_isFilling = true;
    MyShape* shape = getShapeAt(QPoint(x, y));
    if (shape) {
        shape->brushColor = fillColor;
        redrawAllShapes();
    }
    m_isFilling = false;
}
