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

// ----------------- MyBezierCurve 实现 -----------------
MyBezierCurve::MyBezierCurve(const QVector<QPoint>& controlPoints, QColor pen, int w, Qt::PenStyle s)
    : MyShape(pen, Qt::transparent, w, s)
{
    QPointF center(0,0);
    if (!controlPoints.isEmpty()){
        for (const QPoint &pt: controlPoints) center += pt;
        center /= controlPoints.size();
    }
    position = center;
    for (const QPoint &pt: controlPoints) ctrl.append(QPointF(pt) - center);
    recomputeTransform();
}

static QPointF deCasteljau(const QVector<QPointF>& pts, double t) {
    if (pts.isEmpty()) return QPointF();
    QVector<QPointF> tmp = pts;
    int n = tmp.size();
    for (int r = 1; r < n; ++r) {
        for (int i = 0; i < n - r; ++i) {
            tmp[i] = tmp[i] * (1.0 - t) + tmp[i+1] * t;
        }
    }
    return tmp[0];
}

void MyBezierCurve::draw(RasterWidget* widget) {
    if (ctrl.size() < 2) return;
    QPolygonF local;
    for (const QPointF &p: ctrl) local << p;
    QPolygonF transformed = transform.map(local);
    int steps = 120;
    QPointF prev = transform.map(ctrl.first());
    for (int i = 1; i <= steps; ++i) {
        double t = double(i)/steps;
        QPointF pt = transform.map(deCasteljau(ctrl, t));
        widget->rasterDrawLine(round(prev.x()), round(prev.y()), round(pt.x()), round(pt.y()), penColor, penWidth, penStyle);
        prev = pt;
    }
}

bool MyBezierCurve::contains(const QPointF& p) {
    return getBoundingBox().contains(p);
}

QRectF MyBezierCurve::getBoundingBox() {
    if (ctrl.isEmpty()) return QRectF();
    QPolygonF poly;
    for (const QPointF &c: ctrl) poly << transform.map(c);
    return poly.boundingRect();
}

QRectF MyBezierCurve::getLocalBoundingBox() { QPolygonF poly; for (auto &c: ctrl) poly << c; return poly.boundingRect(); }

// ----------------- MyBSplineCurve 实现 -----------------
MyBSplineCurve::MyBSplineCurve(const QVector<QPoint>& controlPoints, QColor pen, int w, Qt::PenStyle s)
    : MyShape(pen, Qt::transparent, w, s)
{
    QPointF center(0,0);
    if (!controlPoints.isEmpty()){
        for (const QPoint &pt: controlPoints) center += pt;
        center /= controlPoints.size();
    }
    position = center;
    for (const QPoint &pt: controlPoints) ctrl.append(QPointF(pt) - center);
    recomputeTransform();
    generateUniformKnots();
}

void MyBSplineCurve::generateUniformKnots() {
    int n = ctrl.size();
    int m = n + degree + 1;
    knots.clear();
    knots.resize(m);
    for (int i = 0; i < m; ++i) knots[i] = 0.0;
    for (int i = 0; i < m; ++i) {
        if (i <= degree) knots[i] = 0.0;
        else if (i >= n) knots[i] = double(n - degree);
        else knots[i] = double(i - degree);
    }
}

double MyBSplineCurve::basisFunction(int i, int k, double t) const {
    // 非递归保护递归实现（小规模 degree 合适）
    if (k == 0) {
        if (i < 0 || i+1 >= knots.size()) return 0.0;
        if (knots[i] < knots[i+1]) {
            if (t >= knots[i] && t < knots[i+1]) return 1.0;
            int last = knots.size()-1;
            if (i+1 == last && std::fabs(t - knots[i+1]) < (PRECISION*10)) return 1.0;
        }
        return 0.0;
    }
    double denom1 = knots[i+k] - knots[i];
    double denom2 = knots[i+k+1] - knots[i+1];
    double left = 0.0, right = 0.0;
    if (denom1 > PRECISION) left = (t - knots[i]) / denom1 * basisFunction(i, k-1, t);
    if (denom2 > PRECISION) right = (knots[i+k+1] - t) / denom2 * basisFunction(i+1, k-1, t);
    return left + right;
}

QPointF MyBSplineCurve::curvePoint(double t) const {
    QPointF p(0,0);
    int n = ctrl.size();
    for (int i = 0; i < n; ++i) {
        double b = basisFunction(i, degree, t);
        p += ctrl[i] * b;
    }
    return p;
}

void MyBSplineCurve::draw(RasterWidget* widget) {
    if (ctrl.size() < 2) return;
    double t_start = knots[degree];
    double t_end = knots[ctrl.size()];
    int steps = 120;
    QPointF prev = transform.map(curvePoint(t_start));
    for (int i = 1; i < steps; ++i) {
        double t = t_start + (t_end - t_start) * double(i) / steps;
        QPointF pt = transform.map(curvePoint(t));
        widget->rasterDrawLine(round(prev.x()), round(prev.y()), round(pt.x()), round(pt.y()), penColor, penWidth, penStyle);
        prev = pt;
    }
}

bool MyBSplineCurve::contains(const QPointF& p) { return getBoundingBox().contains(p); }
QRectF MyBSplineCurve::getBoundingBox() { QPolygonF poly; for (auto &c: ctrl) poly << transform.map(c); return poly.boundingRect(); }
QRectF MyBSplineCurve::getLocalBoundingBox() { QPolygonF poly; for (auto &c: ctrl) poly << c; return poly.boundingRect(); }

// ----------------- MyBezierSurface 实现 -----------------
MyBezierSurface::MyBezierSurface(const QVector<QVector<QPoint>>& grid, QColor pen, QColor brush, int w, Qt::PenStyle s)
    : MyShape(pen, brush, w, s)
{
    // center by average of points
    QPointF center(0,0); int cnt = 0;
    for (int i = 0; i < grid.size(); ++i) for (int j = 0; j < grid[i].size(); ++j) { center += grid[i][j]; cnt++; }
    if (cnt) center /= cnt;
    position = center;
    for (int i = 0; i < grid.size(); ++i) {
        QVector<QPointF> row;
        for (int j = 0; j < grid[i].size(); ++j) row.append(QPointF(grid[i][j]) - center);
        ctrl.append(row);
    }
    recomputeTransform();
}

double MyBezierSurface::bernstein(int i, int n, double t) const {
    if (i < 0 || i > n) return 0.0;
    double coeff = 1.0;
    for (int j = 0; j < i; ++j) coeff *= double(n - j) / double(j + 1);
    return coeff * std::pow(1.0 - t, n - i) * std::pow(t, i);
}

QPointF MyBezierSurface::surfacePoint(double u, double v) const {
    QPointF p(0,0);
    int nu = ctrl.size(); if (nu == 0) return p;
    int nv = ctrl[0].size();
    for (int i = 0; i < nu; ++i) for (int j = 0; j < nv; ++j) {
        double Bu = bernstein(i, nu-1, u);
        double Bv = bernstein(j, nv-1, v);
        p += ctrl[i][j] * (Bu * Bv);
    }
    return p;
}

void MyBezierSurface::draw(RasterWidget* widget) {
    if (ctrl.isEmpty()) return;
    int uSteps = 30, vSteps = 30;
    // build vertex grid
    QVector<QVector<QPoint>> verts(uSteps+1, QVector<QPoint>(vSteps+1));
    for (int iu = 0; iu <= uSteps; ++iu) {
        double u = double(iu) / uSteps;
        for (int iv = 0; iv <= vSteps; ++iv) {
            double v = double(iv) / vSteps;
            QPointF sp = transform.map(surfacePoint(u, v));
            verts[iu][iv] = QPoint(round(sp.x()), round(sp.y()));
        }
    }
    // fill triangles (scanline) per quad
    for (int i = 0; i < uSteps; ++i) {
        for (int j = 0; j < vSteps; ++j) {
            // triangle 1
            QVector<QPoint> tri1 = { verts[i][j], verts[i+1][j], verts[i][j+1] };
            if (showFill) {
                QColor c1(static_cast<int>(255 * (1.0 - static_cast<double>(i) / uSteps)),
                          static_cast<int>(255 * (1.0 - static_cast<double>(j) / vSteps)), 150);
                widget->rasterScanFillPolygon(tri1, c1, Qt::transparent, 0, penStyle);
            }
            if (showWire) {
                widget->rasterDrawLine(tri1[0].x(), tri1[0].y(), tri1[1].x(), tri1[1].y(), penColor, penWidth, penStyle);
                widget->rasterDrawLine(tri1[1].x(), tri1[1].y(), tri1[2].x(), tri1[2].y(), penColor, penWidth, penStyle);
                widget->rasterDrawLine(tri1[2].x(), tri1[2].y(), tri1[0].x(), tri1[0].y(), penColor, penWidth, penStyle);
            }
            // triangle 2
            QVector<QPoint> tri2 = { verts[i+1][j], verts[i+1][j+1], verts[i][j+1] };
            if (showFill) {
                QColor c2(static_cast<int>(255 * (1.0 - static_cast<double>(i) / uSteps)),
                          static_cast<int>(255 * (1.0 - static_cast<double>(j) / vSteps)), 150);
                widget->rasterScanFillPolygon(tri2, c2, Qt::transparent, 0, penStyle);
            }
            if (showWire) {
                widget->rasterDrawLine(tri2[0].x(), tri2[0].y(), tri2[1].x(), tri2[1].y(), penColor, penWidth, penStyle);
                widget->rasterDrawLine(tri2[1].x(), tri2[1].y(), tri2[2].x(), tri2[2].y(), penColor, penWidth, penStyle);
                widget->rasterDrawLine(tri2[2].x(), tri2[2].y(), tri2[0].x(), tri2[0].y(), penColor, penWidth, penStyle);
            }
        }
    }
}

bool MyBezierSurface::contains(const QPointF& p) { return getBoundingBox().contains(p); }
QRectF MyBezierSurface::getBoundingBox() {
    QPolygonF poly;
    for (auto &row: ctrl) for (auto &c: row) poly << transform.map(c);
    return poly.boundingRect();
}
QRectF MyBezierSurface::getLocalBoundingBox() { QPolygonF poly; for (auto &row: ctrl) for (auto &c: row) poly << c; return poly.boundingRect(); }


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
    m_currentTargetBuffer = &m_canvasBuffer;
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
    m_antialiasing = true; // 默认启用FXAA反走样

    resizeBuffer(QSize(800, 600));
    setMouseTracking(true);
    setFocusPolicy(Qt::ClickFocus);
    saveSceneState();
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
    m_canvasBuffer = newCanvasBuffer;

    // 同时调整预览缓冲
    m_previewBuffer = QImage(size, QImage::Format_ARGB32_Premultiplied);
    m_previewBuffer.fill(Qt::transparent);
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
void RasterWidget::drawPreview()
{
    // 1. 总是清空预览缓冲区
    m_previewBuffer.fill(Qt::transparent);

    // 2. 如果没有在绘制，直接返回
    if (!m_isDrawing) return;

    // 3. 设置绘制目标为预览缓冲区
    m_currentTargetBuffer = &m_previewBuffer;

    // 4. 根据模式绘制预览（全部使用光栅化算法）
    switch (m_painterStatus) {
    case PainterStatus::SELECT: {
        // 手动绘制橡皮筋选择框的四条边
        QRect rect = QRect(m_startPoint, m_currentPoint).normalized();
        int left = rect.left(), right = rect.right();
        int top = rect.top(), bottom = rect.bottom();

        // 使用rasterDrawLine绘制四条边（虚线）
        rasterDrawLine(left, top, right, top, Qt::blue, 1, Qt::DashLine);     // 上边
        rasterDrawLine(right, top, right, bottom, Qt::blue, 1, Qt::DashLine); // 右边
        rasterDrawLine(right, bottom, left, bottom, Qt::blue, 1, Qt::DashLine); // 下边
        rasterDrawLine(left, bottom, left, top, Qt::blue, 1, Qt::DashLine);   // 左边
        break;
    }
    case PainterStatus::LINE:
        rasterDrawLine(m_startPoint.x(), m_startPoint.y(),
                       m_currentPoint.x(), m_currentPoint.y(),
                       m_penColor, m_penWidth, m_penStyle);
        break;
    case PainterStatus::RECT: {
        QRect rect = QRect(m_startPoint, m_currentPoint).normalized();
        MyRect tempRect(rect, m_penColor, m_brushColor, m_penWidth, m_penStyle);
        tempRect.position = rect.center();
        tempRect.recomputeTransform();
        tempRect.draw(this);
        break;
    }
    case PainterStatus::CIRCLE: {
        QRectF rect = QRect(m_startPoint, m_currentPoint).normalized();
        qreal r = std::max(rect.width(), rect.height()) / 2.0;
        MyCircle tempCircle(r, m_penColor, m_brushColor, m_penWidth, m_penStyle);
        tempCircle.position = rect.center();
        tempCircle.recomputeTransform();
        tempCircle.draw(this);
        break;
    }
    case PainterStatus::ELLIPSE: {
        QRectF rect = QRect(m_startPoint, m_currentPoint).normalized();
        MyEllipse tempEllipse(rect.width()/2, rect.height()/2, m_penColor, m_brushColor, m_penWidth, m_penStyle);
        tempEllipse.position = rect.center();
        tempEllipse.recomputeTransform();
        tempEllipse.draw(this);
        break;
    }
    case PainterStatus::PEN:
    case PainterStatus::POLYGON:
        if (!m_tempPoints.isEmpty()) {
            // 绘制临时路径
            for (int i = 0; i < m_tempPoints.size() - 1; ++i) {
                rasterDrawLine(m_tempPoints[i].x(), m_tempPoints[i].y(),
                               m_tempPoints[i+1].x(), m_tempPoints[i+1].y(),
                               m_penColor, m_penWidth, m_penStyle);
            }
            // 绘制到最后一个临时点
            if (m_tempPoints.size() > 0) {
                rasterDrawLine(m_tempPoints.last().x(), m_tempPoints.last().y(),
                               m_currentPoint.x(), m_currentPoint.y(),
                               m_penColor, m_penWidth, m_penStyle);
            }
        }
        break;
    default: break;
    }

    // 5. 恢复绘制目标为主缓冲区
    m_currentTargetBuffer = &m_canvasBuffer;
}

void RasterWidget::redrawAllShapes()
{
    // 1. 设置绘制目标为主缓冲区
    m_currentTargetBuffer = &m_canvasBuffer;

    // 2. 清空主缓冲
    m_canvasBuffer.fill(Qt::white);

    // 3. 绘制所有形状
    for (MyShape* shape : m_shapeList) {
        shape->draw(this);
    }

    // 4. 绘制选择框和控制点（使用光栅化）
    if (!m_selectedShapes.isEmpty()) {
        if (m_selectedShapes.count() == 1) {
            MyShape* shape = m_selectedShapes.first();
            QPolygonF localRect(shape->getLocalBoundingBox());
            QPolygonF obb = shape->transform.map(localRect);

            // 对于曲线/曲面，将选择矩形在屏幕空间放大，避免控制点与缩放把手重合
            bool isCurveOrSurface = dynamic_cast<MyBezierCurve*>(shape) || dynamic_cast<MyBSplineCurve*>(shape) || dynamic_cast<MyBezierSurface*>(shape);
            if (isCurveOrSurface) {
                QRectF bbox = obb.boundingRect();
                const int pad = 12; // 扩展像素量，确保把手不与控制点重合
                bbox = bbox.adjusted(-pad, -pad, pad, pad);
                QPolygonF expanded;
                expanded << bbox.topLeft() << bbox.topRight() << bbox.bottomRight() << bbox.bottomLeft();
                obb = expanded;
            }

            // 绘制选择框边框（使用光栅化）
            QVector<QPoint> borderPoints;
            for (const QPointF& p : obb) {
                borderPoints.append(p.toPoint());
            }
            // 闭合边框
            if (!borderPoints.isEmpty()) {
                borderPoints.append(borderPoints.first());
            }
            for (int i = 0; i < borderPoints.size() - 1; ++i) {
                rasterDrawLine(borderPoints[i].x(), borderPoints[i].y(),
                               borderPoints[i+1].x(), borderPoints[i+1].y(),
                               Qt::blue, 1, Qt::DashLine);
            }

            // 绘制控制点
            createHandles(obb);
            for (ControlHandle* h : m_handles) {
                QRectF handleRect = h->getRect(obb);
                // 用填充和边框绘制小矩形
                MyRect tempRect(handleRect.toRect(), Qt::black, Qt::white, 1, Qt::SolidLine);
                tempRect.draw(this);
            }

            // 绘制旋转手柄连线
            ControlHandle* rotH = m_handles.last();
            ControlHandle* topH = m_handles[1];
            if (rotH && topH) {
                QPointF p1 = rotH->getRect(obb).center();
                QPointF p2 = topH->getRect(obb).center();
                rasterDrawLine(p1.x(), p1.y(), p2.x(), p2.y(), Qt::black, 1, Qt::SolidLine);
            }

            // 如果选中的是曲线或曲面，绘制控制点（只在选中时显示）
            MyShape* sel = m_selectedShapes.first();
            if (sel) {
                // 贝塞尔曲线
                if (auto *bc = dynamic_cast<MyBezierCurve*>(sel)) {
                    for (const QPointF &c : bc->ctrl) {
                        QPointF sp = bc->transform.map(c);
                        int cx = int(std::round(sp.x()));
                        int cy = int(std::round(sp.y()));
                            // 圆形控制点：黑色外环 + 白色内核（更大）
                            drawFilledCircle(cx, cy, 5, Qt::black);
                            drawFilledCircle(cx, cy, 3, Qt::white);
                    }
                }
                // B样条曲线
                else if (auto *bs = dynamic_cast<MyBSplineCurve*>(sel)) {
                    for (const QPointF &c : bs->ctrl) {
                        QPointF sp = bs->transform.map(c);
                        int cx = int(std::round(sp.x()));
                        int cy = int(std::round(sp.y()));
                        // 圆形控制点：黑色外环 + 白色内核（更大）
                        drawFilledCircle(cx, cy, 5, Qt::black);
                        drawFilledCircle(cx, cy, 3, Qt::white);
                    }
                }
                // Bézier 曲面
                else if (auto *bsf = dynamic_cast<MyBezierSurface*>(sel)) {
                    for (const auto &row : bsf->ctrl) {
                        for (const QPointF &c : row) {
                            QPointF sp = bsf->transform.map(c);
                            int cx = int(std::round(sp.x()));
                            int cy = int(std::round(sp.y()));
                            // 圆形控制点：黑色外环 + 白色内核（更大）
                            drawFilledCircle(cx, cy, 5, Qt::black);
                            drawFilledCircle(cx, cy, 3, Qt::white);
                        }
                    }
                }
            }
        } else {
            // 多选时绘制简单边框
            for (MyShape* shape : m_selectedShapes) {
                QRectF bbox = shape->getBoundingBox();
                MyRect tempRect(bbox, Qt::blue, Qt::transparent, 1, Qt::DashLine);
                tempRect.position = bbox.center();
                tempRect.recomputeTransform();
                tempRect.draw(this);
            }
        }
    }

    // 5. 应用FXAA后处理反走样（性能优化版）
    if (m_antialiasing && m_needsAntialiasing) {
        applyPostprocessAntialiasing();
    }

    update();
}

// **FXAA快速近似反走样（替换原有高斯模糊）**
void RasterWidget::applyPostprocessAntialiasing() {
    if (!m_currentTargetBuffer || !m_antialiasing) return;

    QImage& img = *m_currentTargetBuffer;
    QImage temp = img.copy(); // 采样源

    const int w = img.width();
    const int h = img.height();

    // FXAA优化参数（平衡质量与速度）
    const float FXAA_SPAN_MAX = 8.0f;
    const float FXAA_REDUCE_MUL = 1.0f/8.0f;
    const float FXAA_REDUCE_MIN = 1.0f/128.0f;

    // 处理有效区域（排除边界）
    for (int y = 1; y < h - 1; ++y) {
        for (int x = 1; x < w - 1; ++x) {
            // 采样3x3邻域亮度（灰度转换）
            auto getLum = [&](int px, int py) -> float {
                QColor c = temp.pixelColor(px, py);
                return 0.299f * c.red() + 0.587f * c.green() + 0.114f * c.blue();
            };

            float rgbNW = getLum(x-1, y-1);
            float rgbNE = getLum(x+1, y-1);
            float rgbSW = getLum(x-1, y+1);
            float rgbSE = getLum(x+1, y+1);
            float rgbM = getLum(x, y);

            // 计算边缘对比度
            float lumaMin = qMin(rgbM, qMin(qMin(rgbNW, rgbNE), qMin(rgbSW, rgbSE)));
            float lumaMax = qMax(rgbM, qMax(qMax(rgbNW, rgbNE), qMax(rgbSW, rgbSE)));
            float lumaRange = lumaMax - lumaMin;

            // 对比度阈值判断
            if (lumaRange < qMax(FXAA_REDUCE_MIN, lumaMax * FXAA_REDUCE_MUL)) {
                continue;
            }

            // 计算边缘方向
            float rgbNS = rgbNW + rgbNE - rgbSW - rgbSE;
            float rgbWE = rgbNW + rgbSW - rgbNE - rgbSE;

            // 选择最短响应方向进行模糊
            bool isEdge = fabs(rgbNS) >= fabs(rgbWE);
            float blendFactor = 0.5f - lumaRange / 510.0f;
            blendFactor *= 0.75f;

            // 采样两个方向像素
            QColor c1 = temp.pixelColor(x + (isEdge ? 0 : 1), y + (isEdge ? 1 : 0));
            QColor c2 = temp.pixelColor(x + (isEdge ? 0 : -1), y + (isEdge ? -1 : 0));

            // 混合当前像素
            int r = img.pixelColor(x, y).red() * (1-blendFactor) +
                    (c1.red() + c2.red()) * 0.5f * blendFactor;
            int g = img.pixelColor(x, y).green() * (1-blendFactor) +
                    (c1.green() + c2.green()) * 0.5f * blendFactor;
            int b = img.pixelColor(x, y).blue() * (1-blendFactor) +
                    (c1.blue() + c2.blue()) * 0.5f * blendFactor;

            img.setPixelColor(x, y, QColor(r, g, b));
        }
    }
}

void RasterWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);

    // 绘制主缓冲
    painter.drawImage(0, 0, m_canvasBuffer);

    // 绘制预览缓冲
    painter.drawImage(0, 0, m_previewBuffer);
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

        // 尝试拾取曲线/曲面控制点以开始拖拽
        if (m_selectedShapes.count() == 1) {
            if (tryPickCurveControl(event->pos())) {
                m_isMovingCurveControl = true;
                m_dragStartPosition = event->pos();
                setCursor(Qt::ClosedHandCursor);
                return;
            }
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
    case PainterStatus::CURVE:
    case PainterStatus::BSPLINE:
    case PainterStatus::SURFACE:
        // 记录起点，释放时创建具体 shape
        m_isDrawing = true;
        m_startPoint = event->pos();
        m_currentPoint = event->pos();
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
    {
        MyShape* shape = getShapeAt(event->pos());
        if (shape) {
            if (m_colorType == ColorType::BOARD) {
                shape->penColor = m_penColor;  // 修改边框颜色
            } else {
                shape->brushColor = m_brushColor;  // 修改填充颜色
            }
            redrawAllShapes();
            saveSceneState();
        }
        break;
    }
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
            QPointF shapeCenter = s->position;
            qreal angle1 = QLineF(shapeCenter, m_dragStartPosition).angle();
            qreal angle2 = QLineF(shapeCenter, m_currentPoint).angle();
            qreal angleDiff = angle1 - angle2;
            s->rotation += angleDiff; // 累加旋转角度
            break;
        }

        default: { // 缩放 - 允许翻转
            if (m_selectedShapes.count() != 1) break;
            MyShape* s = m_selectedShapes.first();
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

            // 允许负缩放实现翻转
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

    // 如果正在拖拽曲线/曲面的控制点，更新对应控制点坐标并重绘
    if (m_isMovingCurveControl && m_activeCurveControl.shape) {
        MyShape* s = m_activeCurveControl.shape;
        QTransform inv = s->transform.inverted();
        QPointF local = inv.map(event->pos());
        if (!m_activeCurveControl.isSurface) {
            if (auto *bc = dynamic_cast<MyBezierCurve*>(s)) {
                if (m_activeCurveControl.i >=0 && m_activeCurveControl.i < bc->ctrl.size()) {
                    bc->ctrl[m_activeCurveControl.i] = local;
                }
            } else if (auto *bs = dynamic_cast<MyBSplineCurve*>(s)) {
                if (m_activeCurveControl.i >=0 && m_activeCurveControl.i < bs->ctrl.size()) {
                    bs->ctrl[m_activeCurveControl.i] = local;
                }
            }
        } else {
            if (auto *sf = dynamic_cast<MyBezierSurface*>(s)) {
                int i = m_activeCurveControl.i;
                int j = m_activeCurveControl.j;
                if (i>=0 && i<sf->ctrl.size() && j>=0 && j<sf->ctrl[i].size()) {
                    sf->ctrl[i][j] = local;
                }
            }
        }
        redrawAllShapes();
        return;
    }

    if (m_isDrawing && m_painterStatus == PainterStatus::PEN) {
        m_tempPoints.append(event->pos());
        drawPreview();
        update();
        return; // 避免执行后续逻辑
    }

    // 更新预览
    if (m_isDrawing) {
        drawPreview();
        update();
    }

    if (m_painterStatus == PainterStatus::SELECT) {
        // 如果悬停在曲线/曲面控制点上，显示手形光标
        if (isOverCurveControl(event->pos())) {
            setCursor(Qt::PointingHandCursor);
        } else {
            ControlHandle* h = getHandleAt(event->pos());
            if(h) setCursorForHandle(h->pos);
            else if (getShapeAt(event->pos())) setCursor(Qt::SizeAllCursor);
            else setCursor(Qt::ArrowCursor);
        }
    } else {
        setCursor(Qt::CrossCursor);
    }
}

void RasterWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) return;

    if (m_isTransforming) {
        m_needsAntialiasing = true;
        if (m_currentOpHandlePos == HandlePosition::Center && m_activeHandle) {
            delete m_activeHandle;
        }
        m_isTransforming = false;
        m_activeHandle = nullptr;
        m_originalParams.clear(); // 清除备份
        saveSceneState();
    }

    if (m_isDrawing) {
        m_needsAntialiasing = true;
        if (m_painterStatus == PainterStatus::SELECT) {
            m_isDrawing = false;
            QRectF rubberBandRect = QRectF(m_startPoint, m_currentPoint).normalized();
            m_selectedShapes.clear();
            for (int i = 0; i < m_shapeList.size(); ++i) {
                if (rubberBandRect.intersects(m_shapeList[i]->getBoundingBox())) {
                    m_selectedShapes.append(m_shapeList[i]);
                }
            }
            m_previewBuffer.fill(Qt::transparent);
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
        case PainterStatus::CURVE: {
            // 创建 4 控制点三次贝塞尔（与 CustomView 保持一致的默认布局）
            QVector<QPoint> ctrl = {
                m_startPoint,
                m_startPoint + QPoint(50, -50),
                m_startPoint + QPoint(100, 50),
                m_startPoint + QPoint(150, 0)
            };
            newShape = new MyBezierCurve(ctrl, m_penColor, m_penWidth, m_penStyle);
            break;
        }
        case PainterStatus::BSPLINE: {
            QVector<QPoint> ctrl = {
                m_startPoint,
                m_startPoint + QPoint(40, -60),
                m_startPoint + QPoint(100, -50),
                m_startPoint + QPoint(150, 20),
                m_startPoint + QPoint(170, 80)
            };
            newShape = new MyBSplineCurve(ctrl, m_penColor, m_penWidth, m_penStyle);
            break;
        }
        case PainterStatus::SURFACE: {
            // 4x4 控制网格
            QVector<QVector<QPoint>> grid(4, QVector<QPoint>(4));
            for (int i = 0; i < 4; ++i) for (int j = 0; j < 4; ++j) {
                grid[i][j] = m_startPoint + QPoint(i * 60, j * 60) + QPoint(0, (i + j) * 20);
            }
            // 强制曲面使用与笔色相关且不可由用户更改的填充色
            QColor fill = m_penColor;
            fill.setAlpha(200);
            newShape = new MyBezierSurface(grid, m_penColor, fill, m_penWidth, m_penStyle);
            if (auto *sf = dynamic_cast<MyBezierSurface*>(newShape)) {
                sf->showFill = true;
            }
            break;
        }
        default: break;
        }

        if (newShape) {
            m_shapeList.append(newShape);
            m_selectedShapes.clear();
            m_selectedShapes.append(newShape);

            m_previewBuffer.fill(Qt::transparent); // 清空预览
        }
    }

    // 如果刚刚结束的是控制点拖拽，保存状态并结束拖拽
    if (m_isMovingCurveControl) {
        m_isMovingCurveControl = false;
        m_activeCurveControl = CurveControlPick();
        saveSceneState();
    }

    if (m_needsAntialiasing) {
        redrawAllShapes(); // 这次重绘会应用抗锯齿
        m_needsAntialiasing = false; // 重置
        saveSceneState();
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
    saveSceneState();
}

void RasterWidget::onDeleteActionClicked()
{
    deleteSelectedShapes();
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
    m_penColor = color;
    for (MyShape* s : m_selectedShapes) s->penColor = color;
    if(!m_selectedShapes.isEmpty()) {
        redrawAllShapes();
        saveSceneState();
    }
}

void RasterWidget::setBrushColor(const QColor &color) {
    m_brushColor = color;
    for (MyShape* s : m_selectedShapes) {
        // 曲面的填充不允许用户修改
        if (dynamic_cast<MyBezierSurface*>(s)) continue;
        s->brushColor = color;
    }
    if(!m_selectedShapes.isEmpty()) {
        redrawAllShapes();
        saveSceneState();
    }
}

void RasterWidget::setColorType(const ColorType type) {
    m_colorType = type;
}

void RasterWidget::setPenWidth(int width) {
    m_penWidth = width;
    for (MyShape* s : m_selectedShapes) {
        // 不允许修改曲面的线宽
        if (dynamic_cast<MyBezierSurface*>(s)) continue;
        s->penWidth = width;
    }
    if(!m_selectedShapes.isEmpty()) {
        redrawAllShapes();
        saveSceneState();
    }
}

void RasterWidget::setPenStyle(Qt::PenStyle style) {
    m_penStyle = style;
    for (MyShape* s : m_selectedShapes) {
        s->penStyle = style;
    }
    if(!m_selectedShapes.isEmpty()) {
        redrawAllShapes();
        saveSceneState();
    }
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
    m_previewBuffer.fill(Qt::transparent);
    update();
    saveSceneState();
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
        saveSceneState();
    }
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

// 尝试在当前被选中的单个 shape 上拾取控制点（曲线或曲面）
bool RasterWidget::tryPickCurveControl(const QPoint &p)
{
    if (m_selectedShapes.count() != 1) return false;
    MyShape* sel = m_selectedShapes.first();
    // 贝塞尔曲线
    if (auto *bc = dynamic_cast<MyBezierCurve*>(sel)) {
        int r = 10; // 控制点拾取半径像素（加大以匹配更大的可视控制点）
        for (int idx = 0; idx < bc->ctrl.size(); ++idx) {
            QPointF sp = bc->transform.map(bc->ctrl[idx]);
            int dx = int(std::round(sp.x())) - p.x();
            int dy = int(std::round(sp.y())) - p.y();
            if (dx*dx + dy*dy <= r*r) {
                m_activeCurveControl.shape = bc;
                m_activeCurveControl.i = idx;
                m_activeCurveControl.j = -1;
                m_activeCurveControl.isSurface = false;
                return true;
            }
        }
        return false;
    }
    // B-spline
    if (auto *bs = dynamic_cast<MyBSplineCurve*>(sel)) {
        int r = 10;
        for (int idx = 0; idx < bs->ctrl.size(); ++idx) {
            QPointF sp = bs->transform.map(bs->ctrl[idx]);
            int dx = int(std::round(sp.x())) - p.x();
            int dy = int(std::round(sp.y())) - p.y();
            if (dx*dx + dy*dy <= r*r) {
                m_activeCurveControl.shape = bs;
                m_activeCurveControl.i = idx;
                m_activeCurveControl.j = -1;
                m_activeCurveControl.isSurface = false;
                return true;
            }
        }
        return false;
    }
    // Bézier 曲面
    if (auto *s = dynamic_cast<MyBezierSurface*>(sel)) {
        int r = 10;
        for (int i = 0; i < s->ctrl.size(); ++i) {
            for (int j = 0; j < s->ctrl[i].size(); ++j) {
                QPointF sp = s->transform.map(s->ctrl[i][j]);
                int dx = int(std::round(sp.x())) - p.x();
                int dy = int(std::round(sp.y())) - p.y();
                if (dx*dx + dy*dy <= r*r) {
                    m_activeCurveControl.shape = s;
                    m_activeCurveControl.i = i;
                    m_activeCurveControl.j = j;
                    m_activeCurveControl.isSurface = true;
                    return true;
                }
            }
        }
        return false;
    }
    return false;
}

bool RasterWidget::isOverCurveControl(const QPoint &p) const {
    if (m_selectedShapes.count() != 1) return false;
    MyShape* sel = m_selectedShapes.first();
    int r = 10;
    if (auto *bc = dynamic_cast<MyBezierCurve*>(sel)) {
        for (int idx = 0; idx < bc->ctrl.size(); ++idx) {
            QPointF sp = bc->transform.map(bc->ctrl[idx]);
            int dx = int(std::round(sp.x())) - p.x();
            int dy = int(std::round(sp.y())) - p.y();
            if (dx*dx + dy*dy <= r*r) return true;
        }
        return false;
    }
    if (auto *bs = dynamic_cast<MyBSplineCurve*>(sel)) {
        for (int idx = 0; idx < bs->ctrl.size(); ++idx) {
            QPointF sp = bs->transform.map(bs->ctrl[idx]);
            int dx = int(std::round(sp.x())) - p.x();
            int dy = int(std::round(sp.y())) - p.y();
            if (dx*dx + dy*dy <= r*r) return true;
        }
        return false;
    }
    if (auto *s = dynamic_cast<MyBezierSurface*>(sel)) {
        for (int i = 0; i < s->ctrl.size(); ++i) {
            for (int j = 0; j < s->ctrl[i].size(); ++j) {
                QPointF sp = s->transform.map(s->ctrl[i][j]);
                int dx = int(std::round(sp.x())) - p.x();
                int dy = int(std::round(sp.y())) - p.y();
                if (dx*dx + dy*dy <= r*r) return true;
            }
        }
        return false;
    }
    return false;
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
    QPolygonF localPoly(s->getLocalBoundingBox());
    QPolygonF obb = s->transform.map(localPoly);

    // 如果当前是曲线或曲面，使用与 redrawAllShapes 中相同的扩展逻辑
    bool isCurveOrSurface = dynamic_cast<MyBezierCurve*>(s) || dynamic_cast<MyBSplineCurve*>(s) || dynamic_cast<MyBezierSurface*>(s);
    if (isCurveOrSurface) {
        QRectF bbox = obb.boundingRect();
        const int pad = 12; // 与 redrawAllShapes 中的 pad 保持一致
        bbox = bbox.adjusted(-pad, -pad, pad, pad);
        QPolygonF expanded;
        expanded << bbox.topLeft() << bbox.topRight() << bbox.bottomRight() << bbox.bottomLeft();
        obb = expanded;
    }

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
        QPointF topMid = (tl + tr) / 2.0;
        QLineF upVec(bl, tl);
        if (upVec.length() > 0) {
            upVec.setLength(20);
            center = topMid + (upVec.p2() - upVec.p1());
        } else {
            center = topMid + QPointF(0, -20);
        }
        break;
    }
    case HandlePosition::Center:
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
// --- 8. 光栅化算法 (核心实现) ---
// ===================================================================
void RasterWidget::drawPixel(int x, int y, const QColor &color) {
    if (!m_currentTargetBuffer) return;
    if (m_currentTargetBuffer->rect().contains(x, y)) {
        // 优化：alpha预乘提升混合性能
        if (color.alpha() == 255) {
            m_currentTargetBuffer->setPixelColor(x, y, color);
        } else {
            // 手动alpha混合（比QPainter更快）
            QColor bg = m_currentTargetBuffer->pixelColor(x, y);
            qreal a = color.alphaF();
            int r = color.red() * a + bg.red() * (1 - a);
            int g = color.green() * a + bg.green() * (1 - a);
            int b = color.blue() * a + bg.blue() * (1 - a);
            m_currentTargetBuffer->setPixelColor(x, y, QColor(r, g, b));
        }
    }
}

void RasterWidget::drawPixelAA(int x, int y, qreal alpha, const QColor &color) {
    if (!m_currentTargetBuffer) return;
    if (!m_currentTargetBuffer->rect().contains(x, y)) return;

    // 简单的alpha混合
    QColor bg = m_currentTargetBuffer->pixelColor(x, y);
    int r = bg.red() * (1-alpha) + color.red() * alpha;
    int g = bg.green() * (1-alpha) + color.green() * alpha;
    int b = bg.blue() * (1-alpha) + color.blue() * alpha;
    m_currentTargetBuffer->setPixelColor(x, y, QColor(r, g, b, 255));
}

void RasterWidget::drawThickPixel(int x, int y, int width, const QColor &color) {
    int r = width / 2;
    for (int iy = -r; iy <= r; ++iy) {
        for (int ix = -r; ix <= r; ++ix) {
            drawPixel(x + ix, y + iy, color);
        }
    }
}

// 绘制实心圆（像素级填充）
void RasterWidget::drawFilledCircle(int cx, int cy, int radius, const QColor &color) {
    if (radius <= 0) { drawPixel(cx, cy, color); return; }
    for (int dy = -radius; dy <= radius; ++dy) {
        int h = radius*radius - dy*dy;
        if (h < 0) continue;
        int dxmax = int(std::floor(std::sqrt((double)h)));
        for (int dx = -dxmax; dx <= dxmax; ++dx) {
            drawPixel(cx + dx, cy + dy, color);
        }
    }
}

// 画直线
void RasterWidget::rasterDrawLine(int x0, int y0, int x1, int y1, const QColor &color, int width, Qt::PenStyle style) {
    if (m_lineAlgorithm == LineAlgorithm::DDA) {
        rasterDrawLineDDA(x0, y0, x1, y1, color, width);
        return;
    }

    // 默认Bresenham
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
        // 处理线型
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

// DDA直线算法
void RasterWidget::rasterDrawLineDDA(int x0, int y0, int x1, int y1, const QColor &color, int width) {
    double dx = x1 - x0;
    double dy = y1 - y0;
    double steps = std::max(std::abs(dx), std::abs(dy));

    if (steps == 0) {
        drawThickPixel(x0, y0, width, color);
        return;
    }

    double xIncrement = dx / steps;
    double yIncrement = dy / steps;
    double x = x0;
    double y = y0;

    for (int i = 0; i <= steps; i++) {
        drawThickPixel(round(x), round(y), width, color);
        x += xIncrement;
        y += yIncrement;
    }
}

// **改进的Wu反走样直线算法**
void RasterWidget::rasterDrawLineWu(int x0, int y0, int x1, int y1, const QColor &color, int width) {
    bool steep = abs(y1 - y0) > abs(x1 - x0);
    if (steep) {
        std::swap(x0, y0);
        std::swap(x1, y1);
    }
    if (x0 > x1) {
        std::swap(x0, x1);
        std::swap(y0, y1);
    }

    double dx = x1 - x0;
    double dy = y1 - y0;
    double gradient = (dx == 0) ? 1.0 : dy / dx;

    // 处理端点
    double xEnd = round(x0);
    double yEnd = y0 + gradient * (xEnd - x0);
    double xGap = 1 - (x0 + 0.5);
    int xPixel1 = xEnd;
    int yPixel1 = int(yEnd);

    auto plot = [&](int x, int y, float alpha) {
        QColor blendedColor = colorWithAlpha(color, alpha);
        if (steep) drawPixel(y, x, blendedColor);
        else drawPixel(x, y, blendedColor);
    };

    plot(xPixel1, yPixel1, (1 - (yEnd - floor(yEnd))) * xGap);
    plot(xPixel1, yPixel1 + 1, (yEnd - floor(yEnd)) * xGap);

    double intery = yEnd + gradient;
    xEnd = round(x1);
    yEnd = y1 + gradient * (xEnd - x1);
    xGap = (x1 + 0.5) - xEnd;
    int xPixel2 = xEnd;
    int yPixel2 = int(yEnd);

    plot(xPixel2, yPixel2, (1 - (yEnd - floor(yEnd))) * xGap);
    plot(xPixel2, yPixel2 + 1, (yEnd - floor(yEnd)) * xGap);

    // 主循环
    for (int x = xPixel1 + 1; x <= xPixel2 - 1; ++x) {
        plot(x, int(intery), 1 - (intery - floor(intery)));
        plot(x, int(intery) + 1, intery - floor(intery));
        intery += gradient;
    }

    // 处理线宽
    if (width > 1) {
        int halfWidth = width / 2;
        for (int w = 1; w <= halfWidth; ++w) {
            rasterDrawLineWu(x0, y0 - w, x1, y1 - w, color, 1);
            rasterDrawLineWu(x0, y0 + w, x1, y1 + w, color, 1);
        }
    }
}

// 辅助函数：生成带透明度的颜色
QColor RasterWidget::colorWithAlpha(const QColor& c, float alpha) {
    return QColor(c.red(), c.green(), c.blue(), int(c.alpha() * alpha));
}

// 圆形绘制（添加Wu反走样分支）
void RasterWidget::rasterDrawCircle(int xc, int yc, int radius, const QColor &color, int width, Qt::PenStyle style) {
    if (m_antialiasing && width == 1) {
        rasterDrawCircleWu(xc, yc, radius, color, width);
        return;
    }

    if (m_circleAlgorithm == CircleAlgorithm::Bresenham) {
        rasterDrawCircleBresenham(xc, yc, radius, color, width);
    } else {
        // 中点圆算法（默认）
        int x = radius; int y = 0; int p = 1 - radius;
        auto drawCirclePoints = [&](int x, int y) {
            drawThickPixel(xc + x, yc + y, width, color);
            drawThickPixel(xc - x, yc + y, width, color);
            drawThickPixel(xc + x, yc - y, width, color);
            drawThickPixel(xc - x, yc - y, width, color);
            drawThickPixel(xc + y, yc + x, width, color);
            drawThickPixel(xc - y, yc + x, width, color);
            drawThickPixel(xc + y, yc - x, width, color);
            drawThickPixel(xc - y, yc - x, width, color);
        };

        drawCirclePoints(x, y);
        if (radius > 0) {
            drawThickPixel(xc, yc + radius, width, color);
            drawThickPixel(xc, yc - radius, width, color);
        }

        while (x > y) {
            y++;
            if (p <= 0) { p = p + 2 * y + 1; }
            else { x--; p = p + 2 * y - 2 * x + 1; }
            if (x < y) break;
            drawCirclePoints(x, y);
        }
    }
}

// **Wu反走样圆形算法**
void RasterWidget::rasterDrawCircleWu(int xc, int yc, int radius, const QColor &color, int width) {
    if (radius <= 0) {
        drawPixel(xc, yc, color);
        return;
    }

    auto drawCircleWuPoints = [&](int x, int y, float alpha) {
        if (alpha <= 0) return;
        QColor c = colorWithAlpha(color, alpha);
        drawPixel(xc + x, yc + y, c);
        drawPixel(xc - x, yc + y, c);
        drawPixel(xc + x, yc - y, c);
        drawPixel(xc - x, yc - y, c);
        drawPixel(xc + y, yc + x, c);
        drawPixel(xc - y, yc + x, c);
        drawPixel(xc + y, yc - x, c);
        drawPixel(xc - y, yc - x, c);
    };

    int x = radius;
    int y = 0;
    int d = 1 - radius;

    while (x >= y) {
        // 计算覆盖率实现反走样
        float coverage = 1.0f;
        if (d < 0) {
            coverage = 1.0f - float(-d) / (2.0f * x + 1);
        }

        drawCircleWuPoints(x, y, coverage);

        if (d < 0) {
            d += 2 * y + 3;
        } else {
            d += 2 * (y - x) + 5;
            x--;
        }
        y++;
    }
}

// Bresenham圆算法（第二种）
void RasterWidget::rasterDrawCircleBresenham(int xc, int yc, int radius, const QColor &color, int width) {
    int x = 0;
    int y = radius;
    int d = 3 - 2 * radius;

    auto drawCirclePoints = [&](int x, int y) {
        drawThickPixel(xc + x, yc + y, width, color);
        drawThickPixel(xc - x, yc + y, width, color);
        drawThickPixel(xc + x, yc - y, width, color);
        drawThickPixel(xc - x, yc - y, width, color);
        drawThickPixel(xc + y, yc + x, width, color);
        drawThickPixel(xc - y, yc + x, width, color);
        drawThickPixel(xc + y, yc - x, width, color);
        drawThickPixel(xc - y, yc - x, width, color);
    };

    while (x <= y) {
        drawCirclePoints(x, y);
        if (d < 0) {
            d = d + 4 * x + 6;
        } else {
            d = d + 4 * (x - y) + 10;
            y--;
        }
        x++;
    }
}

// 椭圆绘制（添加Wu反走样分支）
void RasterWidget::rasterDrawEllipse(int xc, int yc, int rx, int ry, const QColor &color, int width, Qt::PenStyle style) {
    if (m_antialiasing && width == 1) {
        rasterDrawEllipseWu(xc, yc, rx, ry, color, width);
        return;
    }

    if (m_ellipseAlgorithm == EllipseAlgorithm::DDA) {
        rasterDrawEllipseDDA(xc, yc, rx, ry, color, width);
        return;
    }

    // 中点椭圆算法（默认）
    long rx2 = (long)rx * rx; long ry2 = (long)ry * ry;
    long twoRx2 = 2 * rx2; long twoRy2 = 2 * ry2;
    int x = 0; int y = ry;
    long p = (long)std::round(ry2 - rx2 * ry + 0.25 * rx2);
    long px = 0; long py = twoRx2 * y;

    auto drawEllipsePoints = [&](int x, int y) {
        drawThickPixel(xc + x, yc + y, width, color);
        drawThickPixel(xc - x, yc + y, width, color);
        drawThickPixel(xc + x, yc - y, width, color);
        drawThickPixel(xc - x, yc - y, width, color);
    };

    while (px < py) {
        drawEllipsePoints(x, y);
        x++; px += twoRy2;
        if (p < 0) { p += ry2 + px; }
        else { y--; py -= twoRx2; p += ry2 + px - py; }
    }

    p = (long)std::round(ry2 * (x + 0.5) * (x + 0.5) + rx2 * (y - 1) * (y - 1) - rx2 * ry2);
    while (y >= 0) {
        drawEllipsePoints(x, y);
        y--; py -= twoRx2;
        if (p > 0) { p += rx2 - py; }
        else { x++; px += twoRy2; p += rx2 - py + px; }
    }
}

// **Wu反走样椭圆算法**
void RasterWidget::rasterDrawEllipseWu(int xc, int yc, int rx, int ry, const QColor &color, int width) {
    if (rx <= 0 || ry <= 0) return;

    auto drawEllipseWuPoints = [&](int x, int y, float alpha) {
        QColor c = colorWithAlpha(color, alpha);
        drawPixel(xc + x, yc + y, c);
        drawPixel(xc - x, yc + y, c);
        drawPixel(xc + x, yc - y, c);
        drawPixel(xc - x, yc - y, c);
    };

    long rx2 = (long)rx * rx;
    long ry2 = (long)ry * ry;
    long twoRx2 = 2 * rx2;
    long twoRy2 = 2 * ry2;
    int x = 0;
    int y = ry;
    long p = (long)round(ry2 - rx2 * ry + 0.25 * rx2);
    long px = 0;
    long py = twoRx2 * y;

    // 区域1
    while (px < py) {
        float coverage = 1.0f;
        if (p < 0) {
            coverage = 1.0f - float(-p) / (float)(px + py);
        }
        drawEllipseWuPoints(x, y, coverage);

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

    // 区域2
    p = (long)round(ry2 * (x + 0.5) * (x + 0.5) + rx2 * (y - 1) * (y - 1) - rx2 * ry2);
    while (y >= 0) {
        drawEllipseWuPoints(x, y, 1.0f);
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

// DDA椭圆算法（第二种）
void RasterWidget::rasterDrawEllipseDDA(int xc, int yc, int rx, int ry, const QColor &color, int width) {
    int segments = std::max(36, (int)((rx + ry) / 4));
    for (int i = 0; i < segments; ++i) {
        qreal angle = 2.0 * PI * i / (qreal)segments;
        int x = round(xc + rx * cos(angle));
        int y = round(yc + ry * sin(angle));
        drawThickPixel(x, y, width, color);
    }
}

// 扫描线填充多边形
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

    // 绘制边界（第二种算法：使用直线绘制边界）
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

// 序列化辅助函数
static QJsonObject shapeToJson(MyShape* shape)
{
    QJsonObject obj;
    obj["penColor"] = shape->penColor.name(QColor::HexArgb);
    obj["brushColor"] = shape->brushColor.name(QColor::HexArgb);
    obj["penWidth"] = shape->penWidth;
    obj["penStyle"] = shape->penStyle;
    obj["position"] = QJsonArray{shape->position.x(), shape->position.y()};
    obj["rotation"] = shape->rotation;
    obj["scaleX"] = shape->scaleX;
    obj["scaleY"] = shape->scaleY;

    // 根据类型存储特定数据
    switch(shape->getType()) {
    case ShapeType::BezierCurve: {
        MyBezierCurve* curve = static_cast<MyBezierCurve*>(shape);
        obj["type"] = "MyBezierCurve";
        QJsonArray pts;
        for (const QPointF &p : curve->ctrl)
            pts.append(QJsonArray{p.x(), p.y()});
        obj["controlPoints"] = pts;
        break;
    }
    case ShapeType::BSplineCurve: {
        MyBSplineCurve* sc = static_cast<MyBSplineCurve*>(shape);
        obj["type"] = "MyBSplineCurve";
        QJsonArray pts;
        for (const QPointF &p : sc->ctrl)
            pts.append(QJsonArray{p.x(), p.y()});
        obj["controlPoints"] = pts;
        obj["degree"] = sc->degree;
        break;
    }
    case ShapeType::BezierSurface: {
        MyBezierSurface* surf = static_cast<MyBezierSurface*>(shape);
        obj["type"] = "MyBezierSurface";
        QJsonArray grid;
        for (const QVector<QPointF> &row : surf->ctrl) {
            QJsonArray r;
            for (const QPointF &p : row)
                r.append(QJsonArray{p.x(), p.y()});
            grid.append(r);
        }
        obj["controlGrid"] = grid;
        obj["showWire"] = surf->showWire;
        obj["showFill"] = surf->showFill;
        break;
    }
    case ShapeType::Line: {
        MyLine* line = static_cast<MyLine*>(shape);
        obj["type"] = "MyLine";
        obj["p1"] = QJsonArray{line->p1.x(), line->p1.y()};
        obj["p2"] = QJsonArray{line->p2.x(), line->p2.y()};
        break;
    }
    case ShapeType::Rect: {
        MyRect* rect = static_cast<MyRect*>(shape);
        obj["type"] = "MyRect";
        obj["rect"] = QJsonArray{rect->rect.x(), rect->rect.y(),
                                 rect->rect.width(), rect->rect.height()};
        break;
    }
    case ShapeType::Circle: {
        MyCircle* circle = static_cast<MyCircle*>(shape);
        obj["type"] = "MyCircle";
        obj["radius"] = circle->radius;
        break;
    }
    case ShapeType::Ellipse: {
        MyEllipse* ellipse = static_cast<MyEllipse*>(shape);
        obj["type"] = "MyEllipse";
        obj["rx"] = ellipse->rx;
        obj["ry"] = ellipse->ry;
        break;
    }
    case ShapeType::Polygon: {
        MyPolygon* polygon = static_cast<MyPolygon*>(shape);
        obj["type"] = "MyPolygon";
        QJsonArray points;
        for(const QPointF& p : polygon->points) {
            points.append(QJsonArray{p.x(), p.y()});
        }
        obj["points"] = points;
        break;
    }
    case ShapeType::Path: {
        MyPath* path = static_cast<MyPath*>(shape);
        obj["type"] = "MyPath";
        QJsonArray points;
        for(const QPointF& p : path->points) {
            points.append(QJsonArray{p.x(), p.y()});
        }
        obj["points"] = points;
        break;
    }
    }

    return obj;
}

static MyShape* jsonToShape(const QJsonObject& obj)
{
    QColor penColor = QColor(obj["penColor"].toString());
    QColor brushColor = QColor(obj["brushColor"].toString());
    int penWidth = obj["penWidth"].toInt();
    Qt::PenStyle penStyle = static_cast<Qt::PenStyle>(obj["penStyle"].toInt());

    QString type = obj["type"].toString();
    MyShape* shape = nullptr;

    if (type == "MyLine") {
        QJsonArray p1Arr = obj["p1"].toArray();
        QJsonArray p2Arr = obj["p2"].toArray();
        QPointF p1(p1Arr[0].toDouble(), p1Arr[1].toDouble());
        QPointF p2(p2Arr[0].toDouble(), p2Arr[1].toDouble());
        shape = new MyLine(p1, p2, penColor, penWidth, penStyle);
    } else if (type == "MyRect") {
        QJsonArray rectArr = obj["rect"].toArray();
        QRectF rect(rectArr[0].toDouble(), rectArr[1].toDouble(),
                    rectArr[2].toDouble(), rectArr[3].toDouble());
        shape = new MyRect(rect, penColor, brushColor, penWidth, penStyle);
    } else if (type == "MyCircle") {
        qreal radius = obj["radius"].toDouble();
        shape = new MyCircle(radius, penColor, brushColor, penWidth, penStyle);
    } else if (type == "MyEllipse") {
        qreal rx = obj["rx"].toDouble();
        qreal ry = obj["ry"].toDouble();
        shape = new MyEllipse(rx, ry, penColor, brushColor, penWidth, penStyle);
    } else if (type == "MyPolygon") {
        QJsonArray pointsArr = obj["points"].toArray();
        QVector<QPoint> points;
        for (const QJsonValue& v : pointsArr) {
            QJsonArray pArr = v.toArray();
            points.append(QPoint(pArr[0].toDouble(), pArr[1].toDouble()));
        }
        shape = new MyPolygon(points, penColor, brushColor, penWidth, penStyle);
    } else if (type == "MyPath") {
        QJsonArray pointsArr = obj["points"].toArray();
        QVector<QPoint> points;
        for (const QJsonValue& v : pointsArr) {
            QJsonArray pArr = v.toArray();
            points.append(QPoint(pArr[0].toDouble(), pArr[1].toDouble()));
        }
        shape = new MyPath(points, penColor, penWidth, penStyle);
    } else if (type == "MyBezierCurve") {
        QJsonArray pts = obj["controlPoints"].toArray();
        QVector<QPoint> ctrl;
        for (const QJsonValue &v : pts) {
            QJsonArray p = v.toArray();
            ctrl.append(QPoint(p[0].toDouble(), p[1].toDouble()));
        }
        shape = new MyBezierCurve(ctrl, penColor, penWidth, penStyle);
    } else if (type == "MyBSplineCurve") {
        QJsonArray pts = obj["controlPoints"].toArray();
        QVector<QPoint> ctrl;
        for (const QJsonValue &v : pts) {
            QJsonArray p = v.toArray();
            ctrl.append(QPoint(p[0].toDouble(), p[1].toDouble()));
        }
        shape = new MyBSplineCurve(ctrl, penColor, penWidth, penStyle);
        if (obj.contains("degree")) {
            if (auto bs = dynamic_cast<MyBSplineCurve*>(shape))
                bs->degree = obj["degree"].toInt();
        }
    } else if (type == "MyBezierSurface") {
        QJsonArray grid = obj["controlGrid"].toArray();
        QVector<QVector<QPoint>> ctrl;
        for (const QJsonValue &rv : grid) {
            QJsonArray row = rv.toArray();
            QVector<QPoint> r;
            for (const QJsonValue &pv : row) {
                QJsonArray p = pv.toArray();
                r.append(QPoint(p[0].toDouble(), p[1].toDouble()));
            }
            ctrl.append(r);
        }
        shape = new MyBezierSurface(ctrl, penColor, brushColor, penWidth, penStyle);
        if (auto sf = dynamic_cast<MyBezierSurface*>(shape)) {
            if (obj.contains("showWire")) sf->showWire = obj["showWire"].toBool();
            if (obj.contains("showFill")) sf->showFill = obj["showFill"].toBool();
        }
    }

    if (shape) {
        QJsonArray posArr = obj["position"].toArray();
        shape->position = QPointF(posArr[0].toDouble(), posArr[1].toDouble());
        shape->rotation = obj["rotation"].toDouble();
        shape->scaleX = obj["scaleX"].toDouble();
        shape->scaleY = obj["scaleY"].toDouble();
        shape->recomputeTransform();
    }

    return shape;
}

// ===================================================================
// --- 9. 撤销重做功能实现 ---
// ===================================================================

void RasterWidget::saveSceneState()
{
    QJsonArray state;
    for (MyShape* shape : m_shapeList) {
        state.append(shapeToJson(shape));
    }

    QJsonDocument doc(state);
    undoStack.push(doc.toJson());

    if (undoStack.size() > maxUndoSteps)
        undoStack.pop_front();

    redoStack.clear(); // 新操作后清空重做栈
}

void RasterWidget::restoreSceneState(const QByteArray &stateData)
{
    // 清除当前形状
    qDeleteAll(m_shapeList);
    m_shapeList.clear();
    m_selectedShapes.clear();
    clearHandles();

    // 反序列化状态
    QJsonDocument doc = QJsonDocument::fromJson(stateData);
    QJsonArray state = doc.array();

    for (const QJsonValue &v : state) {
        MyShape* shape = jsonToShape(v.toObject());
        if (shape) {
            m_shapeList.append(shape);
        }
    }

    redrawAllShapes();
}

void RasterWidget::onRevoke()
{
    if (undoStack.size() <= 1) return; // 至少保留一个初始状态

    redoStack.push(undoStack.pop());
    restoreSceneState(undoStack.top());
}

void RasterWidget::onUndo()
{
    if (redoStack.isEmpty()) return;

    undoStack.push(redoStack.pop());
    restoreSceneState(undoStack.top());
}

void RasterWidget::onSaveAs()
{
    if(!isChosen) return;

    // 参考 CustomView：提供 PNG 和 JSON 两种格式选项
    QString filter = "PNG 图片 (*.png);;JSON 源码 (*.json)";
    QString fileName = QFileDialog::getSaveFileName(this, "保存为", "", filter);
    if (fileName.isEmpty()) return;

    if (fileName.endsWith(".png", Qt::CaseInsensitive)) {
        // PNG 导出：合并画布和预览缓冲区
        QImage finalImage = m_canvasBuffer.copy();
        QPainter p(&finalImage);
        p.drawImage(0, 0, m_previewBuffer);
        p.end();
        finalImage.save(fileName);

    } else if (fileName.endsWith(".json", Qt::CaseInsensitive)) {
        // JSON 导出：序列化所有形状
        QJsonArray array;
        for (MyShape* shape : m_shapeList) {
            array.append(shapeToJson(shape));
        }

        QJsonDocument doc(array);
        QFile file(fileName);
        if (file.open(QIODevice::WriteOnly)) {
            file.write(doc.toJson());
            file.close();
        }
    }
}

void RasterWidget::onOpen()
{
    if(!isChosen) return;

    QString fileName = QFileDialog::getOpenFileName(this, "打开", "", "JSON 源码 (*.json)");
    if (fileName.isEmpty()) return;

    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "无法打开文件:" << fileName;
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();

    if (!doc.isArray()) {
        qWarning() << "无效的 JSON 格式: 根元素必须是数组";
        return;
    }

    // 清空当前状态
    qDeleteAll(m_shapeList);
    m_shapeList.clear();
    m_selectedShapes.clear();
    clearHandles();

    // 加载形状
    QJsonArray array = doc.array();
    for (const QJsonValue &v : array) {
        MyShape* shape = jsonToShape(v.toObject());
        if (shape) {
            m_shapeList.append(shape);
        }
    }

    redrawAllShapes();
    saveSceneState();
}
