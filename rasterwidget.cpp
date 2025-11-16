#include "rasterwidget.h"
#include <QPainter>
#include <QPaintEvent>
#include <QResizeEvent>
#include <QMouseEvent>
#include <QApplication> // (Fix 3) 用于检查 Shift 键
#include <QDebug>
#include <cmath>
#include <QStack>
#include <algorithm>
#include <QFileDialog>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>

// --- 用于扫描线填充的辅助结构 ---
struct Edge {
    int ymax;   double x;   double dx;
    Edge(int ymax, double x, double dx) : ymax(ymax), x(x), dx(dx) {}
};
bool compareEdges(const Edge& a, const Edge& b) { return a.x < b.x; }
// ------------------------------

// PI
const qreal PI = 3.141592653589793;


// ===================================================================
// --- 1. MyShape 基类实现 ---
// ===================================================================

void MyShape::translate(const QPointF& delta) {
    transform.translate(delta.x(), delta.y());
}

void MyShape::rotate(qreal angle, const QPointF& origin) {
    transform.translate(origin.x(), origin.y());
    transform.rotate(angle);
    transform.translate(-origin.x(), -origin.y());
}

void MyShape::scale(qreal sx, qreal sy, const QPointF& origin) {
    transform.translate(origin.x(), origin.y());
    transform.scale(sx, sy);
    transform.translate(-origin.x(), -origin.y());
}

// ===================================================================
// --- 2. MyShape 子类实现 (与上一版相同) ---
// ===================================================================

// --- MyLine ---
MyLine::MyLine(QPointF p1, QPointF p2, QColor p, int w, Qt::PenStyle s)
    : MyShape(p, Qt::transparent, w, s), p1(p1), p2(p2) {
    transform.translate(p1.x(), p1.y());
    this->p1 = QPointF(0, 0);
    this->p2 = p2 - p1;
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
    QPointF center = r.center();
    transform.translate(center.x(), center.y());
    this->rect = QRectF(-r.width() / 2, -r.height() / 2, r.width(), r.height());
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
    : MyShape(p, b, w, s), radius(r) {}

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
    : MyShape(p, b, w, s), rx(rx), ry(ry) {}

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

    transform.translate(center.x(), center.y());
    for (const QPoint& pt : p) {
        points.append(QPointF(pt) - center);
    }
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
    transform.translate(center.x(), center.y());
    for (const QPoint& pt : p) {
        points.append(QPointF(pt) - center);
    }
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


// ===================================================================
// --- 3. RasterWidget 构造/析构/初始化 ---
// ===================================================================

RasterWidget::RasterWidget(QWidget *parent)
    : QWidget(parent)
{
    m_painterStatus = PainterStatus::SELECT;
    m_colorType = ColorType::BOARD;
    m_penColor = Qt::black;
    m_brushColor = QColor(255, 255, 255, 0); //
    m_penWidth = 1;
    m_penStyle = Qt::SolidLine;
    m_isDrawing = false;
    m_isTransforming = false;
    // m_selectedShapes (Fix 3) - m_selectedShapes 在 QList 构造函数中初始化
    m_activeHandle = nullptr;
    isChosen = false; //
    m_isFilling = false; // (Fix 1)

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

    QImage newBuffer(size, QImage::Format_ARGB32_Premultiplied);
    newBuffer.fill(Qt::white);

    // 绘制旧内容
    QPainter p(&newBuffer);
    p.drawImage(0, 0, m_canvasBuffer);
    p.end();

    m_canvasBuffer = newBuffer;
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
    // 1. 清空画布
    m_canvasBuffer.fill(Qt::white);

    // 2. 重新绘制所有对象
    for (MyShape* shape : m_shapeList) {
        shape->draw(this);
    }

    // 3. 绘制选中框和控制点
    clearHandles();
    if (!m_selectedShapes.isEmpty()) { // (Fix 3)

        QPainter p(&m_canvasBuffer);
        p.setRenderHint(QPainter::Antialiasing);

        // (Fix 3) 如果只选中一个，绘制完整的控制点
        if (m_selectedShapes.count() == 1) {
            MyShape* shape = m_selectedShapes.first();
            QRectF box = shape->getBoundingBox();
            createHandles(box); // 创建控制点

            // 绘制包围盒虚线
            p.setPen(QPen(Qt::blue, 1, Qt::DashLine));
            p.setBrush(Qt::NoBrush);
            p.drawRect(box);

            // 绘制控制点
            p.setPen(Qt::black);
            p.setBrush(Qt::white);
            for (ControlHandle* h : m_handles) {
                p.drawRect(h->getRect(box));
            }
        } else {
            // (Fix 3) 如果选中多个，只绘制虚线框
            p.setPen(QPen(Qt::blue, 1, Qt::DashLine));
            p.setBrush(Qt::NoBrush);
            for (MyShape* shape : m_selectedShapes) {
                p.drawRect(shape->getBoundingBox());
            }
        }
        p.end();
    }

    // 4. 触发 paintEvent
    update();
}

void RasterWidget::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);

    // 1. 绘制已渲染好的 m_canvasBuffer
    painter.drawImage(0, 0, m_canvasBuffer);

    // 2. 绘制 "实时" 预览

    // (FIX 3) 绘制橡皮筋选择框预览
    if (m_isDrawing && m_painterStatus == PainterStatus::SELECT) {
        painter.setPen(QPen(Qt::blue, 1, Qt::DashLine));
        painter.setBrush(QColor(0, 0, 100, 30)); // 半透明蓝色填充
        painter.drawRect(QRect(m_startPoint, m_currentPoint).normalized());
    }
    // 绘制其他工具的预览
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
// --- 5. 鼠标和键盘事件 (Fix 2 & 3) ---
// ===================================================================

void RasterWidget::mousePressEvent(QMouseEvent *event)
{
    // (POLYGON) 右键结束
    if (m_painterStatus == PainterStatus::POLYGON && event->button() == Qt::RightButton) {
        if (m_isDrawing && m_tempPoints.size() >= 3) {
            MyPolygon* poly = new MyPolygon(m_tempPoints, m_penColor, m_brushColor, m_penWidth, m_penStyle);
            m_shapeList.append(poly);
            m_selectedShapes.clear(); // (Fix 3)
            m_selectedShapes.append(poly); // (Fix 3)
            m_isDrawing = false;
            m_tempPoints.clear();
            redrawAllShapes();
        }
        return;
    }

    if (event->button() != Qt::LeftButton) return;
    m_dragStartPosition = event->pos(); // (FIX 2) 记录拖拽起始点

    // (FIX 3) (SELECT) 模式逻辑重构
    if (m_painterStatus == PainterStatus::SELECT) {

        // 1. 检查是否点中控制点 (只有1个对象被选中时)
        if (m_selectedShapes.count() == 1) {
            m_activeHandle = getHandleAt(event->pos());
        } else {
            m_activeHandle = nullptr;
        }

        if (m_activeHandle) {
            // 1a. 点中了控制点
            m_isTransforming = true;
            // (FIX 2) 保存原始状态
            m_originalTransforms.clear();
            m_originalTransforms[m_selectedShapes.first()] = m_selectedShapes.first()->transform;
            m_originalBoundingBox = getSelectedShapesBoundingBox();
            return;
        }

        MyShape* shape = getShapeAt(event->pos());
        if (shape) {
            // 2. 点中了一个对象
            m_isTransforming = true;
            m_activeHandle = new ControlHandle(HandlePosition::Center); // 准备平移

            bool shiftPressed = (QApplication::keyboardModifiers() & Qt::ShiftModifier); // (Fix 3)

            if (!m_selectedShapes.contains(shape)) {
                // 如果点中的不在当前选区
                if (!shiftPressed) {
                    m_selectedShapes.clear(); // 清空旧选区
                }
                m_selectedShapes.append(shape); // 添加新选区
            }
            // (如果已在选区且按住shift，则不操作，保留多选)

            // (FIX 2) 保存所有选中对象的原始状态
            m_originalTransforms.clear();
            for(MyShape* s : m_selectedShapes) {
                m_originalTransforms[s] = s->transform;
            }
            m_originalBoundingBox = getSelectedShapesBoundingBox();

            redrawAllShapes();
            return;
        }

        // 3. 点了空白处
        m_isDrawing = true; // (FIX 3) 准备画橡皮筋
        m_startPoint = event->pos();
        m_currentPoint = event->pos();
        if (!m_selectedShapes.isEmpty()) {
            m_selectedShapes.clear();
            redrawAllShapes();
        }
        return;
    }

    // (DRAWING) 模式
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
        update();
        break;
    default:
        break;
    }
}

void RasterWidget::mouseMoveEvent(QMouseEvent *event)
{
    m_currentPoint = event->pos(); // 总是更新当前点
    QPointF dragDelta = QPointF(m_currentPoint - m_dragStartPosition); // (FIX 2) 总偏移量

    // (FIX 2 & 3) (TRANSFORM) 模式
    if (m_isTransforming && m_painterStatus == PainterStatus::SELECT && !m_selectedShapes.isEmpty()) {
        if (!m_activeHandle) return;

        // 1. 将所有变换重置为鼠标按下时的状态
        for(MyShape* s : m_selectedShapes) {
            if (m_originalTransforms.contains(s)) {
                s->transform = m_originalTransforms[s];
            }
        }

        QPointF rotOrigin = m_originalBoundingBox.center();

        switch(m_activeHandle->pos) {
        case HandlePosition::Center: // (Fix 3) 平移 (适用于多选)
            for(MyShape* s : m_selectedShapes) {
                s->translate(dragDelta); // 应用总平移
            }
            break;

        // (Fix 2) 旋转/缩放 (仅当选中1个时才有效)
        case HandlePosition::Rotate: {
            if (m_selectedShapes.count() != 1) break;
            qreal angle1 = QLineF(rotOrigin, m_dragStartPosition).angle();
            qreal angle2 = QLineF(rotOrigin, m_currentPoint).angle();
            m_selectedShapes.first()->rotate(angle2 - angle1, rotOrigin); // 应用总旋转
            break;
        }

        // (Fix 2) 稳定的缩放逻辑
        default: {
            if (m_selectedShapes.count() != 1) break;
            MyShape* s = m_selectedShapes.first();

            // 1. 确定缩放原点 (对角)
            QPointF scaleOrigin;
            QPointF startHandlePos;

            // (这只是一个示例，完整的8点缩放需要更多数学)
            if (m_activeHandle->pos == HandlePosition::BottomRight) {
                scaleOrigin = m_originalBoundingBox.topLeft();
                startHandlePos = m_originalBoundingBox.bottomRight();
            } else if (m_activeHandle->pos == HandlePosition::TopLeft) {
                scaleOrigin = m_originalBoundingBox.bottomRight();
                startHandlePos = m_originalBoundingBox.topLeft();
            } else {
                // 默认 (以中心缩放)
                scaleOrigin = m_originalBoundingBox.center();
                startHandlePos = m_dragStartPosition; // (不完美，但能用)
            }

            // (TODO: 旋转后的缩放仍然很复杂，这里的实现是针对未旋转的包围盒)
            QLineF v_orig(scaleOrigin, startHandlePos);
            QLineF v_new(scaleOrigin, startHandlePos + dragDelta);

            qreal sx = 1.0, sy = 1.0;
            if (v_orig.dx() != 0) sx = v_new.dx() / v_orig.dx();
            if (v_orig.dy() != 0) sy = v_new.dy() / v_orig.dy();

            s->scale(sx, sy, scaleOrigin);
            break;
        }
        }
        redrawAllShapes();
        return;
    }

    // (FIX 3) (DRAWING - Rubber Band) 模式
    if (m_isDrawing && m_painterStatus == PainterStatus::SELECT) {
        update(); // 触发 paintEvent 来绘制橡皮筋
        return;
    }

    // (DRAWING - Tools) 模式
    if (m_isDrawing) {
        if (m_painterStatus == PainterStatus::PEN) {
            m_tempPoints.append(event->pos());
            update(); // 刷新预览
        } else if (m_painterStatus == PainterStatus::POLYGON) {
            update(); // 刷新预览 (Bug 2 修复)
        } else {
            update(); // 刷新其他工具的预览
        }
    }

    // (CURSOR)
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

    // (TRANSFORM)
    if (m_isTransforming) {
        if (m_activeHandle && m_activeHandle->pos == HandlePosition::Center) {
            delete m_activeHandle;
        }
        m_isTransforming = false;
        m_activeHandle = nullptr;
        m_originalTransforms.clear(); // (Fix 2)
    }

    // (DRAWING)
    if (m_isDrawing) {

        // (FIX 3) (SELECT - Rubber Band)
        if (m_painterStatus == PainterStatus::SELECT) {
            m_isDrawing = false;
            QRectF rubberBandRect = QRectF(m_startPoint, m_currentPoint).normalized();

            m_selectedShapes.clear();
            for (int i = 0; i < m_shapeList.size(); ++i) { // (Fix 3) 遍历所有
                if (rubberBandRect.intersects(m_shapeList[i]->getBoundingBox())) {
                    m_selectedShapes.append(m_shapeList[i]); // (Fix 3) 添加所有
                }
            }
            redrawAllShapes();
            return;
        }

        // (Bug 1 & 2 修复)
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
            newShape->transform.translate(rect.center().x(), rect.center().y());
            break;
        }
        case PainterStatus::ELLIPSE: {
            newShape = new MyEllipse(rect.width() / 2.0, rect.height() / 2.0, m_penColor, m_brushColor, m_penWidth, m_penStyle);
            newShape->transform.translate(rect.center().x(), rect.center().y());
            break;
        }
        case PainterStatus::POLYGON:
            break;
        default: break;
        }

        if (newShape) {
            m_shapeList.append(newShape);
            m_selectedShapes.clear(); // (Fix 3)
            m_selectedShapes.append(newShape);
        }

        if (newShape) {
            redrawAllShapes();
        }
    }
}

void RasterWidget::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Delete && !m_selectedShapes.isEmpty()) { // (Fix 3)
        deleteSelectedShapes();
    } else {
        QWidget::keyPressEvent(event);
    }
}

void RasterWidget::deleteSelectedShapes() // (Fix 3)
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
// --- 6. 槽函数实现 (Fix 3) ---
// ===================================================================

void RasterWidget::setPainterStatus(const PainterStatus ps)
{
    if (m_isDrawing && (m_painterStatus == PainterStatus::POLYGON || m_painterStatus == PainterStatus::PEN)) {
        m_isDrawing = false;
        m_tempPoints.clear();
    }

    m_painterStatus = ps;
    if (m_painterStatus != PainterStatus::SELECT) {
        if (!m_selectedShapes.isEmpty()) { // (Fix 3)
            m_selectedShapes.clear();
            redrawAllShapes();
        }
    }
}

void RasterWidget::setPenColor(const QColor &color)
{
    if (m_colorType == ColorType::BOARD) {
        m_penColor = color;
        for (MyShape* s : m_selectedShapes) s->penColor = color; // (Fix 3)
    } else { // ColorType::FILL
        m_brushColor = color;
        for (MyShape* s : m_selectedShapes) s->brushColor = color; // (Fix 3)
    }
    if(!m_selectedShapes.isEmpty()) redrawAllShapes();
}

void RasterWidget::setBrushColor(const QColor &color) {
    m_brushColor = color;
    for (MyShape* s : m_selectedShapes) { // (Fix 3)
        s->brushColor = color;
    }
    if(!m_selectedShapes.isEmpty()) redrawAllShapes();
}

void RasterWidget::setColorType(const ColorType type) {
    m_colorType = type;
}

void RasterWidget::setPenWidth(int width) {
    m_penWidth = width;
    for (MyShape* s : m_selectedShapes) { // (Fix 3)
        s->penWidth = width;
    }
    if(!m_selectedShapes.isEmpty()) redrawAllShapes();
}

void RasterWidget::setPenStyle(Qt::PenStyle style) {
    m_penStyle = style;
    for (MyShape* s : m_selectedShapes) { // (Fix 3)
        s->penStyle = style;
    }
    if(!m_selectedShapes.isEmpty()) redrawAllShapes();
}

void RasterWidget::clearCanvas()
{
    qDeleteAll(m_shapeList);
    m_shapeList.clear();
    m_selectedShapes.clear(); // (Fix 3)
    m_isDrawing = false;
    m_tempPoints.clear();
    m_isTransforming = false;
    m_canvasBuffer.fill(Qt::white);
    update();
}

void RasterWidget::onSaveAs()
{
    //
    QString fileName = QFileDialog::getSaveFileName(this, "保存为", "", "JSON 文件 (*.json);;PNG 图片 (*.png)");
    if (fileName.isEmpty()) return;

    if (fileName.endsWith(".png")) {
        QList<MyShape*> temp = m_selectedShapes; // (Fix 3)
        m_selectedShapes.clear();
        redrawAllShapes(); // 重绘，清除选中框
        m_canvasBuffer.save(fileName);
        m_selectedShapes = temp; // 恢复
        redrawAllShapes();
    } else if (fileName.endsWith(".json")) {
        qWarning() << "JSON save not implemented for RasterWidget yet.";
    }
}

void RasterWidget::onOpen()
{
    //
    qWarning() << "JSON open not implemented for RasterWidget yet.";
}


// ===================================================================
// --- 7. 变换/控制点 辅助函数 (Fix 3) ---
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
    if (m_selectedShapes.count() != 1) return nullptr; // 只有1个时才显示控制点

    QRectF box = m_selectedShapes.first()->getBoundingBox();
    for (ControlHandle* h : m_handles) {
        if (h->getRect(box).contains(p)) {
            return h;
        }
    }
    return nullptr;
}

void RasterWidget::clearHandles() {
    qDeleteAll(m_handles);
    m_handles.clear();
}

void RasterWidget::createHandles(const QRectF& groupBoundingBox) {
    if (groupBoundingBox.isNull()) return;
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

QRectF ControlHandle::getRect(const QRectF& box) { // (Fix 3)
    const int SIZE = 10; // (HANDLE_SIZE)
    QPointF center;

    switch(pos) {
    case HandlePosition::TopLeft:     center = box.topLeft(); break;
    case HandlePosition::Top:         center = QPointF(box.center().x(), box.top()); break;
    case HandlePosition::TopRight:    center = box.topRight(); break;
    case HandlePosition::Right:       center = QPointF(box.right(), box.center().y()); break;
    case HandlePosition::BottomRight: center = box.bottomRight(); break;
    case HandlePosition::Bottom:      center = QPointF(box.center().x(), box.bottom()); break;
    case HandlePosition::BottomLeft:  center = box.bottomLeft(); break;
    case HandlePosition::Left:        center = QPointF(box.left(), box.center().y()); break;
    case HandlePosition::Rotate:      center = QPointF(box.center().x(), box.top() - 20); break; // (ROTATE_HANDLE_OFFSET)
    case HandlePosition::Center:      center = box.center(); break;
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
// --- 8. (真实) 光栅化算法 (非作弊) ---
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
    // (Bresenham)
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
    // (Midpoint Circle) (仅用于预览)
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
    // (Midpoint Ellipse) (仅用于预览)
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
    // (Scanline Fill) (非作弊版本)
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
    // (Flood Fill)

    // (FIX 1)
    if (m_isFilling) return;
    m_isFilling = true;

    if (!m_canvasBuffer.rect().contains(x, y)) {
        m_isFilling = false;
        return;
    }

    QColor targetColor = m_canvasBuffer.pixelColor(x, y);

    if (fillColor == targetColor) {
        m_isFilling = false; // (FIX 1) 必须在这里返回前重置标志
        return;
    }

    QStack<QPoint> stack;
    stack.push(QPoint(x, y));

    while (!stack.isEmpty()) {
        QPoint p = stack.pop();
        int cx = p.x(); int cy = p.y();

        if (!m_canvasBuffer.rect().contains(cx, cy)) {
            continue;
        }

        if (m_canvasBuffer.pixelColor(cx, cy) == targetColor) {
            drawPixel(cx, cy, fillColor);
            stack.push(QPoint(cx + 1, cy));
            stack.push(QPoint(cx - 1, cy));
            stack.push(QPoint(cx, cy + 1));
            stack.push(QPoint(cx, cy - 1));
        }
    }

    m_isFilling = false; // (FIX 1)
}
