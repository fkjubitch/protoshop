#include "parametriccurves.h"
#include <QBrush>
#include <QPen>
#include <QGraphicsScene>
#include <QtMath>
#include <algorithm>

// ============ ControlPointItem 实现 ============
ControlPointItem::ControlPointItem(const QPointF &pos, QGraphicsItem *parent)
    : QGraphicsObject(parent)
{
    const int r = 6;
    setPos(pos);
    setFlags(ItemIsMovable | ItemSendsGeometryChanges | ItemIsSelectable);
    setZValue(1000);
}

QRectF ControlPointItem::boundingRect() const {
    return QRectF(-3, -3, 6, 6);
}

void ControlPointItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
    Q_UNUSED(option);
    Q_UNUSED(widget);
    painter->setBrush(Qt::white);
    painter->setPen(QPen(Qt::black, 1));
    painter->drawEllipse(QRectF(-3, -3, 6, 6));
}

QVariant ControlPointItem::itemChange(GraphicsItemChange change, const QVariant &value) {
    if (change == ItemPositionHasChanged) {
        // 避免在未加入 scene 或构造阶段过早触发信号（可能导致槽在不安全时被调用）
        if (scene() == nullptr) {
            return QGraphicsObject::itemChange(change, value);
        }
        emit moved(this);
    }
    return QGraphicsObject::itemChange(change, value);
}

// ============ BezierCurveItem 实现 ============
BezierCurveItem::BezierCurveItem(const QVector<QPointF> &controlPoints, QGraphicsItem *parent)
    : QGraphicsObject(parent), m_ctrl(controlPoints)
{
    setFlags(ItemIsSelectable | ItemIsMovable | ItemIsFocusable);
    setAcceptHoverEvents(true);
    
    for (const auto &p : m_ctrl) {
        ControlPointItem *h = new ControlPointItem(p, this);
        connect(h, &ControlPointItem::moved, this, &BezierCurveItem::onControlMoved);
        m_handles.push_back(h);
    }
    
    sampleAndUpdatePath(120);
    setCacheMode(DeviceCoordinateCache);
}

QRectF BezierCurveItem::boundingRect() const {
    return m_boundingBox.adjusted(-10, -10, 10, 10);
}

void BezierCurveItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
    Q_UNUSED(option);
    Q_UNUSED(widget);
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setPen(QPen(penColor, penWidth, penStyle));
    if (!m_path.isEmpty()) painter->drawPath(m_path);
}

void BezierCurveItem::setControlPoints(const QVector<QPointF> &pts) {
    m_ctrl = pts;
    for (int i = 0; i < m_handles.size() && i < pts.size(); ++i) {
        m_handles[i]->setPos(pts[i]);
    }
    sampleAndUpdatePath();
}

QPointF BezierCurveItem::deCasteljau(const QVector<QPointF> &pts, double t) const {
    if (pts.size() < 2) return pts.isEmpty() ? QPointF() : pts[0];
    QVector<QPointF> tmp = pts;
    int n = tmp.size();
    for (int r = 1; r < n; ++r) {
        for (int i = 0; i < n - r; ++i) {
            tmp[i] = tmp[i] * (1.0 - t) + tmp[i + 1] * t;
        }
    }
    return tmp[0];
}

void BezierCurveItem::sampleAndUpdatePath(int steps) {
    if (m_updating) { return; }
    m_updating = true;
    // 在修改几何之前通知 scene
    prepareGeometryChange();
    m_path = QPainterPath();
    if (m_ctrl.size() < 2) {
        m_boundingBox = QRectF();
        m_updating = false;
        return;
    }

    m_path.moveTo(m_ctrl[0]);
    for (int i = 1; i <= steps; ++i) {
        double t = static_cast<double>(i) / steps;
        QPointF pt = deCasteljau(m_ctrl, t);
        m_path.lineTo(pt);
    }

    m_boundingBox = m_path.boundingRect();
    m_updating = false;
}

void BezierCurveItem::receiveSceneMousePosition(const QPointF &scenePos, MouseLeftClickStatus status){
    Q_UNUSED(scenePos);
    Q_UNUSED(status);
}

void BezierCurveItem::onControlMoved(ControlPointItem *cp) {
    if (!cp) { return; }
    if (m_handles.isEmpty()) { return; }
    for (int i = 0; i < m_handles.size(); ++i) {
        if (m_handles[i] == cp) {
            m_ctrl[i] = cp->pos();
            break;
        }
    }
    sampleAndUpdatePath();
    update();
}

// ============ BSplineCurveItem 实现 ============
BSplineCurveItem::BSplineCurveItem(const QVector<QPointF> &controlPoints, QGraphicsItem *parent)
    : QGraphicsObject(parent), m_ctrl(controlPoints)
{
    setFlags(ItemIsSelectable | ItemIsMovable | ItemIsFocusable);
    setAcceptHoverEvents(true);
    generateUniformKnots();
    
    for (const auto &p : m_ctrl) {
        ControlPointItem *h = new ControlPointItem(p, this);
        connect(h, &ControlPointItem::moved, this, &BSplineCurveItem::onControlMoved);
        m_handles.push_back(h);
    }
    
    sampleAndUpdatePath(120);
    setCacheMode(DeviceCoordinateCache);
}

QRectF BSplineCurveItem::boundingRect() const {
    return m_boundingBox.adjusted(-10, -10, 10, 10);
}

void BSplineCurveItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
    Q_UNUSED(option);
    Q_UNUSED(widget);
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setPen(QPen(penColor, penWidth, penStyle));
    painter->drawPath(m_path);
}

void BSplineCurveItem::setControlPoints(const QVector<QPointF> &pts) {
    m_ctrl = pts;
    generateUniformKnots();
    for (int i = 0; i < m_handles.size() && i < pts.size(); ++i) {
        m_handles[i]->setPos(pts[i]);
    }
    sampleAndUpdatePath();
}

void BSplineCurveItem::generateUniformKnots() {
    int n = m_ctrl.size();
    int m = n + m_degree + 1;
    m_knots.clear();
    m_knots.resize(m);
    
    for (int i = 0; i <= m_degree; ++i) {
        m_knots[i] = 0.0;
    }
    for (int i = m_degree + 1; i < n; ++i) {
        m_knots[i] = static_cast<double>(i - m_degree);
    }
    for (int i = n; i < m; ++i) {
        m_knots[i] = static_cast<double>(n - m_degree);
    }
}

double BSplineCurveItem::basisFunction(int i, int k, double t, const QVector<double> &knots) const {
    // 特殊处理：若 t 与最小 knot 重合，则第 0 个基函数为 1
    if (!knots.isEmpty()) {
        if (std::fabs(t - knots.front()) < (PRECISION * 10)) {
            return (i == 0) ? 1.0 : 0.0;
        }
    }
    if (k == 0) {
        // 基本区间：包含左端、不包含右端；对于最后一个区间允许包含右端
        if (knots[i] < knots[i + 1]) {
            if (t >= knots[i] && t < knots[i + 1]) return 1.0;
        } else {
            // 重复节点区间长度为0，视为 0
            return 0.0;
        }
        int lastIdx = knots.size() - 1;
        if (i + 1 == lastIdx && std::fabs(t - knots[i + 1]) < (PRECISION * 10)) {
            return 1.0;
        }
        return 0.0;
    }
    
    double denom1 = knots[i + k] - knots[i] + PRECISION;
    double denom2 = knots[i + k + 1] - knots[i + 1] + PRECISION;
    double left = (t - knots[i]) / denom1 * basisFunction(i, k - 1, t, knots);
    double right = (knots[i + k + 1] - t) / denom2 * basisFunction(i + 1, k - 1, t, knots);
    return left + right;
}

QPointF BSplineCurveItem::curvePoint(double t) const {
    QPointF p(0, 0);
    int n = m_ctrl.size();
    for (int i = 0; i < n; ++i) {
        double basis = basisFunction(i, m_degree, t, m_knots);
        p += m_ctrl[i] * basis;
    }
    return p;
}

void BSplineCurveItem::sampleAndUpdatePath(int steps) {
    if (m_updating) { return; }
    m_updating = true;
    m_path = QPainterPath();
    if (m_ctrl.size() < 2) {
        m_boundingBox = QRectF();
        prepareGeometryChange();
        m_updating = false;
        return;
    }
    
    double t_start = m_knots[m_degree];
    double t_end = m_knots[m_ctrl.size()];
    
    // 在修改几何之前通知 scene
    prepareGeometryChange();
    m_path.moveTo(curvePoint(t_start));
    for (int i = 1; i < steps; ++i) {
        double t = t_start + (t_end - t_start) * static_cast<double>(i) / steps;
        QPointF pt = curvePoint(t);
        m_path.lineTo(pt);
    }

    m_boundingBox = m_path.boundingRect();
    m_updating = false;
}

void BSplineCurveItem::onControlMoved(ControlPointItem *cp) {
    for (int i = 0; i < m_handles.size(); ++i) {
        if (m_handles[i] == cp) {
            m_ctrl[i] = cp->pos();
            break;
        }
    }
    sampleAndUpdatePath();
    update();
}

void BSplineCurveItem::receiveSceneMousePosition(const QPointF &scenePos, MouseLeftClickStatus status) {
    Q_UNUSED(scenePos);
    Q_UNUSED(status);
}

// ============ BezierSurfaceItem 实现 ============
BezierSurfaceItem::BezierSurfaceItem(const QVector<QVector<QPointF>> &grid, QGraphicsItem *parent)
    : QGraphicsObject(parent), m_ctrl(grid)
{
    setFlags(ItemIsSelectable | ItemIsMovable | ItemIsFocusable);
    setAcceptHoverEvents(true);
    
    for (int u = 0; u < m_ctrl.size(); ++u) {
        for (int v = 0; v < m_ctrl[u].size(); ++v) {
            ControlPointItem *h = new ControlPointItem(m_ctrl[u][v], this);
            connect(h, &ControlPointItem::moved, this, &BezierSurfaceItem::onHandleMoved);
            m_handles.push_back(h);
        }
    }
    
    rebuildMesh();
    setCacheMode(DeviceCoordinateCache);
}

QRectF BezierSurfaceItem::boundingRect() const {
    return m_boundingBox.adjusted(-10, -10, 10, 10);
}

void BezierSurfaceItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
    Q_UNUSED(option);
    Q_UNUSED(widget);
    
    painter->setRenderHint(QPainter::Antialiasing);
    
    if (m_showFill) {
        for (int i = 0; i < m_fillTriangles.size(); ++i) {
            painter->setBrush(QBrush(m_triangleColors.value(i, brushColor)));
            painter->setPen(Qt::NoPen);
            painter->drawPolygon(m_fillTriangles[i]);
        }
    }
    
    if (m_showWire) {
        painter->setPen(QPen(penColor, penWidth, penStyle));
        painter->setBrush(Qt::NoBrush);
        for (const auto &quad : m_meshQuads) {
            painter->drawPolyline(quad);
        }
    }
}

void BezierSurfaceItem::setControlGrid(const QVector<QVector<QPointF>> &grid) {
    m_ctrl = grid;
    int idx = 0;
    for (int u = 0; u < m_ctrl.size(); ++u) {
        for (int v = 0; v < m_ctrl[u].size(); ++v) {
            if (idx < m_handles.size()) {
                m_handles[idx]->setPos(m_ctrl[u][v]);
            }
            ++idx;
        }
    }
    rebuildMesh();
}

double BezierSurfaceItem::bernstein(int i, int n, double t) const {
    if (i < 0 || i > n) return 0;
    
    double coeff = 1.0;
    for (int j = 0; j < i; ++j) {
        coeff *= static_cast<double>(n - j) / (j + 1);
    }
    
    return coeff * std::pow(1.0 - t, n - i) * std::pow(t, i);
}

QPointF BezierSurfaceItem::surfacePoint(double u, double v) const {
    QPointF p(0, 0);
    for (int i = 0; i < m_ctrl.size(); ++i) {
        for (int j = 0; j < m_ctrl[i].size(); ++j) {
            double B_u = bernstein(i, 3, u);
            double B_v = bernstein(j, 3, v);
            p += m_ctrl[i][j] * (B_u * B_v);
        }
    }
    return p;
}

QColor BezierSurfaceItem::barycentricColor(const QPointF &p, const QVector<QPointF> &tri,
                                           const QVector<QColor> &colors) const {
    if (tri.size() < 3 || colors.size() < 3) return brushColor;
    
    QPointF v0 = tri[1] - tri[0];
    QPointF v1 = tri[2] - tri[0];
    QPointF v2 = p - tri[0];
    
    double dot00 = v0.x() * v0.x() + v0.y() * v0.y();
    double dot01 = v0.x() * v1.x() + v0.y() * v1.y();
    double dot02 = v0.x() * v2.x() + v0.y() * v2.y();
    double dot11 = v1.x() * v1.x() + v1.y() * v1.y();
    double dot12 = v1.x() * v2.x() + v1.y() * v2.y();
    
    double denom = dot00 * dot11 - dot01 * dot01 + PRECISION;
    double invDenom = 1.0 / denom;
    double u = (dot11 * dot02 - dot01 * dot12) * invDenom;
    double v = (dot00 * dot12 - dot01 * dot02) * invDenom;
    double w = 1.0 - u - v;
    
    u = std::clamp(u, 0.0, 1.0);
    v = std::clamp(v, 0.0, 1.0);
    w = std::clamp(w, 0.0, 1.0);
    double sum = u + v + w + PRECISION;
    u /= sum;
    v /= sum;
    w /= sum;
    
    QColor c = colors[0];
    c.setRed(static_cast<int>(c.red() * u + colors[1].red() * v + colors[2].red() * w));
    c.setGreen(static_cast<int>(c.green() * u + colors[1].green() * v + colors[2].green() * w));
    c.setBlue(static_cast<int>(c.blue() * u + colors[1].blue() * v + colors[2].blue() * w));
    return c;
}

void BezierSurfaceItem::rebuildMesh(int uSteps, int vSteps) {
    if (m_updating) { return; }
    m_updating = true;
    m_meshQuads.clear();
    // 在修改网格与包围盒之前通知 scene
    prepareGeometryChange();
    m_meshQuads.clear();
    m_fillTriangles.clear();
    m_triangleColors.clear();
    m_boundingBox = QRectF();
    QVector<QVector<QPointF>> vertices(uSteps + 1, QVector<QPointF>(vSteps + 1));
    for (int i = 0; i <= uSteps; ++i) {
        for (int j = 0; j <= vSteps; ++j) {
            double u = static_cast<double>(i) / uSteps;
            double v = static_cast<double>(j) / vSteps;
            vertices[i][j] = surfacePoint(u, v);
            m_boundingBox |= QRectF(vertices[i][j], QSizeF(1, 1));
        }
    }
    
    for (int i = 0; i < uSteps; ++i) {
        for (int j = 0; j < vSteps; ++j) {
            QPolygonF quad;
            quad << vertices[i][j] << vertices[i + 1][j] 
                 << vertices[i + 1][j + 1] << vertices[i][j + 1];
            m_meshQuads.push_back(quad);
            
            QPolygonF tri1;
            tri1 << vertices[i][j] << vertices[i + 1][j] << vertices[i][j + 1];
            m_fillTriangles.push_back(tri1);
            
            QColor c1(static_cast<int>(255 * (1.0 - static_cast<double>(i) / uSteps)),
                      static_cast<int>(255 * (1.0 - static_cast<double>(j) / vSteps)), 150);
            m_triangleColors << c1;
            
            QPolygonF tri2;
            tri2 << vertices[i + 1][j] << vertices[i + 1][j + 1] << vertices[i][j + 1];
            m_fillTriangles.push_back(tri2);
            m_triangleColors << c1;
        }
    }
    
    update();
    m_updating = false;
}

void BezierSurfaceItem::onHandleMoved(ControlPointItem *cp) {
    int idx = 0;
    for (int u = 0; u < m_ctrl.size(); ++u) {
        for (int v = 0; v < m_ctrl[u].size(); ++v) {
            if (idx < m_handles.size() && m_handles[idx] == cp) {
                m_ctrl[u][v] = cp->pos();
                rebuildMesh();
                update();
                return;
            }
            ++idx;
        }
    }
}

void BezierSurfaceItem::receiveSceneMousePosition(const QPointF &scenePos, MouseLeftClickStatus status) {
    Q_UNUSED(scenePos);
    Q_UNUSED(status);
}
