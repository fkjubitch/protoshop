#include "rasterwidget.h"
#include <QPainter>       // 核心绘图类 (仅用于预览)
#include <QPaintEvent>    // 绘制事件
#include <QResizeEvent>   // 窗口大小改变事件
#include <QMouseEvent>    // 鼠标事件
#include <QDebug>         // 调试输出
#include <cmath>          // 用于数学计算
#include <QStack>         // 用于泛洪填充
#include <QVector>
#include <algorithm>      // 用于排序

// --- 用于扫描线填充的辅助结构 ---

// 边 (Edge)
struct Edge {
    int ymax;   // 边的最大y值
    double x;   // 边与当前扫描线的交点x坐标
    double dx;  // 1/m, x每次随y+1的增量
    Edge(int ymax, double x, double dx) : ymax(ymax), x(x), dx(dx) {}
};

// 比较函数，用于AET排序
bool compareEdges(const Edge& a, const Edge& b) {
    return a.x < b.x;
}

// ------------------------------


RasterWidget::RasterWidget(QWidget *parent)
    : QWidget(parent),
    m_painterStatus(PainterStatus::SELECT), // 默认是选择工具
    m_colorType(ColorType::BOARD),          //
    m_penColor(Qt::black),                  //
    m_brushColor(QColor(0,0,0,0)),          // 默认透明填充
    m_penWidth(1),                          //
    m_penStyle(Qt::SolidLine),              //
    m_isDrawing(false)                      //
{
    // (第1步: 初始化)
    // 构造时先给一个默认大小
    resizeBuffer(QSize(800, 600));

    // 开启鼠标跟踪，以便 mouseMoveEvent 即使没有按下按钮也能触发
    // (如果需要实现"悬停"效果，请取消注释)
    // setMouseTracking(true);
}

// --- 1. 核心事件 (初始化、显示) ---

void RasterWidget::resizeBuffer(const QSize &size)
{
    // 检查大小是否真的改变了
    if (m_canvasBuffer.size() == size) {
        return;
    }

    // (第1步: 初始化)
    // 创建一个新的 QImage，大小为窗口大小
    QImage newBuffer(size, QImage::Format_ARGB32_Premultiplied);

    // 用白色填充新画布
    newBuffer.fill(Qt::white);

    // (可选) 如果您希望在调整大小时保留旧内容:
    QPainter painter(&newBuffer);
    painter.drawImage(0, 0, m_canvasBuffer);
    painter.end();

    // 替换旧的缓冲区
    m_canvasBuffer = newBuffer;

    qInfo() << "Raster canvas resized to" << size;
}

void RasterWidget::resizeEvent(QResizeEvent *event)
{
    // (第1步: 初始化)
    // 当窗口大小改变时，调用我们的辅助函数来重置缓冲区
    resizeBuffer(event->size());

    // (重要) 调用基类的 resizeEvent
    QWidget::resizeEvent(event);
}

void RasterWidget::paintEvent(QPaintEvent *event)
{
    // (第3步: 显示)
    QPainter painter(this); // 在 *this*(RasterWidget) 上创建 QPainter

    // 1. (核心) 将我们已经绘制好的 "固化" 像素缓冲区 "贴" 到小部件上
    //    这会绘制所有 "已确定" 的图形
    painter.drawImage(0, 0, m_canvasBuffer);

    // 2. (无闪烁预览) 如果用户正在拖拽绘制新图形...
    if (m_isDrawing) {
        // ...我们直接在 "贴" 完缓冲区的 *上层* 绘制 "实时" 预览图形
        // 这里的绘制 *不* 作用于 m_canvasBuffer，它只在这次 paintEvent 中可见
        // 这就是无闪烁预览的秘诀
        painter.setPen(QPen(m_penColor, m_penWidth, m_penStyle));
        painter.setBrush(m_brushColor.alpha() == 0 ? Qt::NoBrush : m_brushColor);

        // 根据当前工具绘制不同的预览
        switch (m_painterStatus) {
        case PainterStatus::PEN:
            // (画笔工具没有“实时”预览，它在 mouseMove 里直接画)
            break;
        case PainterStatus::LINE:
            painter.drawLine(m_startPoint, m_currentPoint);
            break;
        case PainterStatus::RECT:
            painter.drawRect(QRect(m_startPoint, m_currentPoint).normalized());
            break;
        case PainterStatus::CIRCLE: {
            // 计算半径 (x, y 方向的最大值，模拟 customview.cpp 的行为)
            QPoint delta = m_currentPoint - m_startPoint;
            int radius = std::max(std::abs(delta.x()), std::abs(delta.y()));
            QPoint topLeft(m_startPoint.x() - (delta.x() < 0 ? radius : 0),
                           m_startPoint.y() - (delta.y() < 0 ? radius : 0));
            painter.drawEllipse(QRect(topLeft, QSize(radius, radius)));
            break;
        }
        case PainterStatus::ELLIPSE:
            painter.drawEllipse(QRect(m_startPoint, m_currentPoint).normalized());
            break;
        case PainterStatus::POLYGON:
            // 绘制已有的点和到鼠标的最后一条线
            if (!m_currentPolygonPoints.isEmpty()) {
                painter.drawPolyline(m_currentPolygonPoints);
                painter.drawLine(m_currentPolygonPoints.last(), m_currentPoint);
            }
            break;
        default:
            break;
        }
    }
}


// --- 2. 鼠标事件 (交互) ---

void RasterWidget::mousePressEvent(QMouseEvent *event)
{
    // 多边形工具的右键点击用于结束绘制
    if (m_painterStatus == PainterStatus::POLYGON && event->button() == Qt::RightButton) {
        if (m_isDrawing && m_currentPolygonPoints.size() >= 3) {
            // "固化" 多边形
            rasterScanFillPolygon(m_currentPolygonPoints, m_brushColor, m_penColor, m_penWidth, m_penStyle);

            // 清理状态
            m_isDrawing = false;
            m_currentPolygonPoints.clear();
            update(); // 刷新
        }
        return;
    }

    // 只响应鼠标左键
    if (event->button() != Qt::LeftButton) {
        return;
    }

    // 根据不同工具处理
    switch (m_painterStatus) {
    case PainterStatus::SELECT:
        // TODO: (作业要求) 实现选择和变换
        break;

    case PainterStatus::PEN:
    case PainterStatus::LINE:
    case PainterStatus::RECT:
    case PainterStatus::CIRCLE:
    case PainterStatus::ELLIPSE:
        m_isDrawing = true;
        m_startPoint = event->pos();
        m_currentPoint = event->pos();
        break;

    case PainterStatus::POLYGON:
        m_isDrawing = true; // 进入“多边形绘制中”状态
        m_currentPolygonPoints.append(event->pos());
        m_currentPoint = event->pos(); // 更新当前点
        update(); // 触发 paintEvent 刷新预览
        break;

    case PainterStatus::FILLSELECT:
        // (作业要求) 实现填充
        rasterFloodFill(event->pos().x(), event->pos().y(), m_brushColor);
        update(); // 填充完成后立即刷新
        break;

    default:
        break;
    }
}

void RasterWidget::mouseMoveEvent(QMouseEvent *event)
{
    // 更新当前点
    m_currentPoint = event->pos();

    if (m_painterStatus == PainterStatus::POLYGON && m_isDrawing && !m_currentPolygonPoints.isEmpty()) {
        // 多边形模式下，即使鼠标没按下（但m_isDrawing=true），也更新预览
        update(); // 触发 paintEvent 更新预览
        return;
    }

    if (!m_isDrawing) {
        return;
    }

    if (m_painterStatus == PainterStatus::PEN) {
        // (特殊) 画笔工具在拖拽时就"固化"
        rasterDrawLine(m_startPoint.x(), m_startPoint.y(),
                       m_currentPoint.x(), m_currentPoint.y(),
                       m_penColor, m_penWidth, m_penStyle);
        // 画完后，新的起点就是当前点
        m_startPoint = m_currentPoint;

        // 画笔工具需要立即刷新缓冲区
        update();
    } else {
        // 其他工具只刷新“预览”
        update(); // 触发 paintEvent (这不会修改 m_canvasBuffer)
    }
}

void RasterWidget::mouseReleaseEvent(QMouseEvent *event)
{
    // 只响应鼠标左键释放
    if (event->button() != Qt::LeftButton || !m_isDrawing) {
        return;
    }

    // 多边形工具不在这里结束 (它在 press 右键时结束)
    if (m_painterStatus == PainterStatus::POLYGON) {
        return;
    }

    // --- (核心) “固化”图形 ---
    // 鼠标释放，"实时" 预览图形需要被 "固化" 到 m_canvasBuffer 上

    switch (m_painterStatus) {
    case PainterStatus::LINE:
        rasterDrawLine(m_startPoint.x(), m_startPoint.y(),
                       m_currentPoint.x(), m_currentPoint.y(),
                       m_penColor, m_penWidth, m_penStyle);
        break;
    case PainterStatus::RECT:
    {
        QRect rect = QRect(m_startPoint, m_currentPoint).normalized();
        if (m_brushColor.alpha() != 0) { // 检查是否需要填充
            QVector<QPoint> points = { rect.topLeft(), rect.topRight(), rect.bottomRight(), rect.bottomLeft() };
            rasterScanFillPolygon(points, m_brushColor, m_penColor, m_penWidth, m_penStyle);
        } else {
            // 仅绘制边框
            rasterDrawLine(rect.left(), rect.top(), rect.right(), rect.top(), m_penColor, m_penWidth, m_penStyle);
            rasterDrawLine(rect.right(), rect.top(), rect.right(), rect.bottom(), m_penColor, m_penWidth, m_penStyle);
            rasterDrawLine(rect.right(), rect.bottom(), rect.left(), rect.bottom(), m_penColor, m_penWidth, m_penStyle);
            rasterDrawLine(rect.left(), rect.bottom(), rect.left(), rect.top(), m_penColor, m_penWidth, m_penStyle);
        }
    }
    break;
    case PainterStatus::CIRCLE:
    {
        // 模拟 customview.cpp 的行为
        QPoint delta = m_currentPoint - m_startPoint;
        int radius = std::max(std::abs(delta.x()), std::abs(delta.y()));
        QPoint topLeft(m_startPoint.x() - (delta.x() < 0 ? radius : 0),
                       m_startPoint.y() - (delta.y() < 0 ? radius : 0));
        QRect rect(topLeft, QSize(radius, radius));

        if (m_brushColor.alpha() != 0) {
            // TODO: 实现圆的填充
            qInfo() << "Circle Fill not implemented yet. Drawing border only.";
        }
        rasterDrawCircle(rect.center().x(), rect.center().y(), radius / 2, m_penColor, m_penWidth, m_penStyle);
    }
    break;
    case PainterStatus::ELLIPSE:
    {
        QRect rect = QRect(m_startPoint, m_currentPoint).normalized();
        if (m_brushColor.alpha() != 0) {
            // TODO: 实现椭圆的填充
            qInfo() << "Ellipse Fill not implemented yet. Drawing border only.";
        }
        rasterDrawEllipse(rect.center().x(), rect.center().y(), rect.width()/2, rect.height()/2, m_penColor, m_penWidth, m_penStyle);
    }
    break;
    default:
        break;
    }

    // 绘制完成
    m_isDrawing = false;

    // (重要) "固化" 操作修改了 m_canvasBuffer,
    // 调用一次 update() 来显示最终结果
    update();
}


// --- 3. 槽函数 (连接UI) ---

void RasterWidget::setPainterStatus(const PainterStatus ps)
{
    //
    m_painterStatus = ps;
    qInfo() << "RasterWidget: Status set to" << ps;

    // 如果切换到非多边形工具，则清空多边形临时点
    if (ps != PainterStatus::POLYGON && m_isDrawing && !m_currentPolygonPoints.isEmpty()) {
        // 如果正在画多边形时切换了工具，就"固化"它
        if(m_currentPolygonPoints.size() >= 3) {
            rasterScanFillPolygon(m_currentPolygonPoints, m_brushColor, m_penColor, m_penWidth, m_penStyle);
        }
        m_isDrawing = false;
        m_currentPolygonPoints.clear();
        update(); // 清除预览
    }
}

void RasterWidget::setPenColor(const QColor &color)
{
    //
    if (m_colorType == ColorType::BOARD) {
        m_penColor = color;
    } else {
        m_brushColor = color;
    }
}

void RasterWidget::setBrushColor(const QColor &color)
{
    //
    // 这个槽函数在 mainwindow.cpp 中没有被直接调用
    // mainwindow.cpp 通过 palatteButtonClicked -> setPenColor 来设置
    m_brushColor = color;
}

void RasterWidget::setColorType(const ColorType type)
{
    //
    m_colorType = type;
}

void RasterWidget::setPenWidth(int width)
{
    //
    m_penWidth = width;
}

void RasterWidget::setPenStyle(Qt::PenStyle style)
{
    //
    m_penStyle = style;
}

void RasterWidget::clearCanvas()
{
    //
    m_canvasBuffer.fill(Qt::white);
    m_currentPolygonPoints.clear();
    m_isDrawing = false;
    update(); // 刷新
}


// --- 4. 基础 API 和光栅化算法 (作业核心) ---

/**
 * @brief (基础) 在画布缓冲区的指定坐标绘制一个像素
 */
void RasterWidget::drawPixel(int x, int y, const QColor &color)
{
    // (第2步: 绘图 - 基础)
    // (重要) 边界检查
    if (m_canvasBuffer.rect().contains(x, y)) {
        // 在缓冲区上设置像素颜色
        m_canvasBuffer.setPixelColor(x, y, color);
    }
}

// 辅助函数：绘制一个"粗"像素
void RasterWidget::drawThickPixel(int x, int y, int width, const QColor &color)
{
    int r = width / 2;
    for (int iy = -r; iy <= r; ++iy) {
        for (int ix = -r; ix <= r; ++ix) {
            // (可选) 可以使用半径检查来画圆形的粗点
            // if (ix*ix + iy*iy <= r*r) {
            drawPixel(x + ix, y + iy, color);
            // }
        }
    }
}


/**
 * @brief (作业要求) 使用Bresenham算法绘制直线
 */
void RasterWidget::rasterDrawLine(int x0, int y0, int x1, int y1, const QColor &color, int width, Qt::PenStyle style)
{
    // Bresenham's Line Algorithm
    int dx = std::abs(x1 - x0);
    int dy = -std::abs(y1 - y0);
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx + dy;
    int e2;

    // 线型计数器
    int dashPattern = 0;
    int dashLength = 10; // 虚线/点线的长度
    bool drawing = true;

    while (true) {
        // --- 处理线型 ---
        switch (style) {
        case Qt::SolidLine: //
            drawing = true;
            break;
        case Qt::DashLine: //
            drawing = (dashPattern / dashLength) % 2 == 0;
            break;
        case Qt::DotLine: //
            drawing = (dashPattern % dashLength) == 0;
            break;
        case Qt::DashDotLine: //
        {
            int segment = (dashPattern / dashLength) % 4;
            drawing = (segment == 0 || segment == 2); // 0=Dash, 1=Gap, 2=Dot, 3=Gap
            if (segment == 2) drawing = (dashPattern % (dashLength/2)) == 0; // 点
        }
        break;
        default:
            drawing = true;
            break;
        }
        dashPattern++;
        // ----------------

        if (drawing) {
            // drawPixel(x0, y0, color);
            // --- 处理线宽 ---
            drawThickPixel(x0, y0, width, color);
        }

        if (x0 == x1 && y0 == y1) break;
        e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

/**
 * @brief (作业要求) 使用中点圆算法绘制圆形
 */
void RasterWidget::rasterDrawCircle(int xc, int yc, int radius, const QColor &color, int width, Qt::PenStyle style)
{
    // Midpoint Circle Algorithm
    int x = radius;
    int y = 0;
    int p = 1 - radius;

    // 绘制第一个点
    drawThickPixel(xc + x, yc - y, width, color);

    if (radius > 0) {
        drawThickPixel(xc - x, yc + y, width, color);
        drawThickPixel(xc + y, yc + x, width, color);
        drawThickPixel(xc - y, yc - x, width, color);
    }

    while (x > y) {
        y++;
        if (p <= 0) {
            p = p + 2 * y + 1;
        } else {
            x--;
            p = p + 2 * y - 2 * x + 1;
        }

        if (x < y) break;

        // (TODO: 中点圆算法的线型实现比较复杂，这里暂时忽略)
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

/**
 * @brief (作业要求) 使用中点椭圆算法绘制椭圆
 */
void RasterWidget::rasterDrawEllipse(int xc, int yc, int rx, int ry, const QColor &color, int width, Qt::PenStyle style)
{
    // Midpoint Ellipse Algorithm
    long rx2 = (long)rx * rx;
    long ry2 = (long)ry * ry;
    long twoRx2 = 2 * rx2;
    long twoRy2 = 2 * ry2;

    // Region 1
    int x = 0;
    int y = ry;
    long p = (long)std::round(ry2 - rx2 * ry + 0.25 * rx2);
    long px = 0;
    long py = twoRx2 * y;

    while (px < py) {
        drawThickPixel(xc + x, yc + y, width, color);
        drawThickPixel(xc - x, yc + y, width, color);
        drawThickPixel(xc + x, yc - y, width, color);
        drawThickPixel(xc - x, yc - y, width, color);

        x++;
        px += twoRy2;
        if (p < 0) {
            p += ry2 + px;
        } else {
            y--;
            py -= twoRx2;
            p += ry2 + px - py;
        }
    }

    // Region 2
    p = (long)std::round(ry2 * (x + 0.5) * (x + 0.5) + rx2 * (y - 1) * (y - 1) - rx2 * ry2);

    while (y >= 0) {
        drawThickPixel(xc + x, yc + y, width, color);
        drawThickPixel(xc - x, yc + y, width, color);
        drawThickPixel(xc + x, yc - y, width, color);
        drawThickPixel(xc - x, yc - y, width, color);

        y--;
        py -= twoRx2;
        if (p > 0) {
            p += rx2 - py;
        } else {
            x++;
            px += twoRy2;
            p += rx2 - py + px;
        }
    }
}

/**
 * @brief (作业要求) 使用扫描线算法填充多边形
 */
void RasterWidget::rasterScanFillPolygon(const QVector<QPoint> &points, const QColor &fillColor, const QColor &borderColor, int width, Qt::PenStyle style)
{
    if (points.size() < 3) return;

    // --- 1. 确定Y的范围并创建边表 (ET) ---
    int yMin = points[0].y();
    int yMax = points[0].y();
    for (const QPoint &p : points) {
        if (p.y() < yMin) yMin = p.y();
        if (p.y() > yMax) yMax = p.y();
    }

    // 边表 (ET)，使用 y-yMin 作为索引
    QVector<QList<Edge>> ET(yMax - yMin + 1);

    for (int i = 0; i < points.size(); ++i) {
        const QPoint &p1 = points[i];
        const QPoint &p2 = points[(i + 1) % points.size()];

        // 跳过水平边
        if (p1.y() == p2.y()) continue;

        int ymin_edge = std::min(p1.y(), p2.y());
        int ymax_edge = std::max(p1.y(), p2.y());
        double x_at_ymin = (p1.y() < p2.y()) ? p1.x() : p2.x();
        double dx = (double)(p2.x() - p1.x()) / (double)(p2.y() - p1.y());

        // 将边放入边表
        ET[ymin_edge - yMin].append(Edge(ymax_edge, x_at_ymin, dx));
    }

    // --- 2. 循环扫描线y，维护活动边表 (AET) ---
    QList<Edge> AET;
    for (int y = yMin; y <= yMax; ++y) {
        // 2a. 将新边从 ET 移入 AET
        int et_index = y - yMin;
        for (const Edge &e : ET[et_index]) {
            AET.append(e);
        }

        // 2b. 移除 AET 中 ymax == y 的边
        AET.erase(std::remove_if(AET.begin(), AET.end(),
                                 [y](const Edge &e) { return e.ymax == y; }), AET.end());

        // 2c. 对 AET 按 x 排序
        std::sort(AET.begin(), AET.end(), compareEdges);

        // 2d. 填充 AET 中成对的 x 之间的像素
        if (fillColor.alpha() != 0) { // 检查填充色是否透明
            for (int i = 0; i < AET.size(); i += 2) {
                if (i + 1 < AET.size()) {
                    int xStart = std::ceil(AET[i].x);
                    int xEnd = std::floor(AET[i + 1].x);
                    for (int x = xStart; x <= xEnd; ++x) {
                        drawPixel(x, y, fillColor);
                    }
                }
            }
        }

        // 2e. 更新 AET 中每条边的 x
        for (Edge &e : AET) {
            e.x += e.dx;
        }
    }

    // --- 3. 绘制边框 (在填充后) ---
    if (borderColor.alpha() != 0 && width > 0) {
        for (int i = 0; i < points.size(); ++i) {
            const QPoint &p1 = points[i];
            const QPoint &p2 = points[(i + 1) % points.size()];
            rasterDrawLine(p1.x(), p1.y(), p2.x(), p2.y(), borderColor, width, style);
        }
    }
}


/**
 * @brief (作业要求) 使用泛洪算法（或边界填充）进行填充
 */
void RasterWidget::rasterFloodFill(int x, int y, const QColor &fillColor)
{
    // 4-connected Stack-based Flood Fill

    // 1. 检查边界
    if (!m_canvasBuffer.rect().contains(x, y)) {
        return;
    }

    // 2. 获取目标颜色
    QColor targetColor = m_canvasBuffer.pixelColor(x, y);

    // 3. 如果点击的颜色就是填充色，或者已经是目标色，则返回
    if (fillColor == targetColor) {
        return;
    }

    QStack<QPoint> stack;
    stack.push(QPoint(x, y));

    while (!stack.isEmpty()) {
        QPoint p = stack.pop();
        int cx = p.x();
        int cy = p.y();

        // 检查边界和颜色
        if (m_canvasBuffer.rect().contains(cx, cy) && m_canvasBuffer.pixelColor(cx, cy) == targetColor)
        {
            // 4. 填充
            drawPixel(cx, cy, fillColor);

            // 5. 推入邻居
            stack.push(QPoint(cx + 1, cy));
            stack.push(QPoint(cx - 1, cy));
            stack.push(QPoint(cx, cy + 1));
            stack.push(QPoint(cx, cy - 1));
        }
    }
}
