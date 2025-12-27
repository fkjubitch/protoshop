#ifndef PARAMETRICCURVES_H
#define PARAMETRICCURVES_H

#include <QGraphicsObject>
#include <QVector>
#include <QPainter>
#include <QGraphicsSceneMouseEvent>
#include <QStyleOptionGraphicsItem>
#include "common.h"

// ============== ControlPointItem ==============
class ControlPointItem : public QGraphicsObject {
    Q_OBJECT
public:
    explicit ControlPointItem(const QPointF &pos, QGraphicsItem *parent = nullptr);
    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;
signals:
    void moved(ControlPointItem* cp);
protected:
    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;
};

// ============== BezierCurveItem ==============
class BezierCurveItem : public QGraphicsObject, public IMousePositionReceiver, public ItemCommon {
    Q_OBJECT
public:
    explicit BezierCurveItem(const QVector<QPointF> &controlPoints, QGraphicsItem *parent = nullptr);
    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;
    void setControlPoints(const QVector<QPointF> &pts);
    QVector<QPointF> controlPoints() const { return m_ctrl; }
    void sampleAndUpdatePath(int steps = 100);
    void receiveSceneMousePosition(const QPointF &scenePos, MouseLeftClickStatus status) override;
private slots:
    void onControlMoved(ControlPointItem *cp);
private:
    QPointF deCasteljau(const QVector<QPointF> &pts, double t) const;
    QVector<QPointF> m_ctrl;
    QVector<ControlPointItem*> m_handles;
    QPainterPath m_path;
    QRectF m_boundingBox;
    bool m_updating = false;
};

// ============== BSplineCurveItem ==============
class BSplineCurveItem : public QGraphicsObject, public IMousePositionReceiver, public ItemCommon {
    Q_OBJECT
public:
    explicit BSplineCurveItem(const QVector<QPointF> &controlPoints, QGraphicsItem *parent = nullptr);
    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;
    void setControlPoints(const QVector<QPointF> &pts);
    QVector<QPointF> controlPoints() const { return m_ctrl; }
    void sampleAndUpdatePath(int steps = 100);
    void receiveSceneMousePosition(const QPointF &scenePos, MouseLeftClickStatus status) override;
private slots:
    void onControlMoved(ControlPointItem *cp);
private:
    double basisFunction(int i, int k, double t, const QVector<double> &knots) const;
    QPointF curvePoint(double t) const;
    void generateUniformKnots();
    QVector<QPointF> m_ctrl;
    QVector<ControlPointItem*> m_handles;
    QVector<double> m_knots;
    int m_degree = 3;
    QPainterPath m_path;
    QRectF m_boundingBox;
    bool m_updating = false;
};

// ============== BezierSurfaceItem ==============
class BezierSurfaceItem : public QGraphicsObject, public IMousePositionReceiver, public ItemCommon {
    Q_OBJECT
public:
    explicit BezierSurfaceItem(const QVector<QVector<QPointF>> &grid, QGraphicsItem *parent = nullptr);
    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;
    void setControlGrid(const QVector<QVector<QPointF>> &grid);
    QVector<QVector<QPointF>> controlGrid() const { return m_ctrl; }
    void setShowWireMesh(bool b) { m_showWire = b; update(); }
    void setShowFill(bool b) { m_showFill = b; update(); }
    void rebuildMesh(int uSteps = 30, int vSteps = 30);
    void receiveSceneMousePosition(const QPointF &scenePos, MouseLeftClickStatus status) override;
private slots:
    void onHandleMoved(ControlPointItem *cp);
private:
    double bernstein(int i, int n, double t) const;
    QPointF surfacePoint(double u, double v) const;
    QColor barycentricColor(const QPointF &p, const QVector<QPointF> &tri, const QVector<QColor> &colors) const;
    QVector<QVector<QPointF>> m_ctrl;
    QVector<ControlPointItem*> m_handles;
    QVector<QPolygonF> m_meshQuads;
    QVector<QPolygonF> m_fillTriangles;
    QVector<QColor> m_triangleColors;
    QRectF m_boundingBox;
    bool m_showWire = true;
    bool m_showFill = false;
    bool m_updating = false;
};

#endif // PARAMETRICCURVES_H
