#ifndef RASTERWIDGET_H
#define RASTERWIDGET_H

#include <QWidget>
#include <QImage>
#include <QColor>
#include <QPoint>
#include <QVector>
#include <QList>
#include <QRectF>
#include <QTransform>
#include <QKeyEvent>
#include <QMap>
#include "common.h"

class QPaintEvent;
class QResizeEvent;
class QMouseEvent;
class RasterWidget;

// --- 1. 变换控制点 ---
enum class HandlePosition {
    TopLeft, Top, TopRight, Right, BottomRight, Bottom, BottomLeft, Left,
    Rotate,
    Center
};

class MyShape;

class ControlHandle {
public:
    HandlePosition pos;
    ControlHandle(HandlePosition p) : pos(p) {}
    QRectF getRect(const QPolygonF& obb);
};


// --- 2. 形状基类 ---
enum class ShapeType { Line, Rect, Circle, Ellipse, Polygon, Path };

class MyShape {
public:
    QColor penColor = Qt::black;
    QColor brushColor = QColor(0, 0, 0, 0);
    int penWidth = 1;
    Qt::PenStyle penStyle = Qt::SolidLine;
    QTransform transform;

public:
    MyShape(QColor p, QColor b, int w, Qt::PenStyle s)
        : penColor(p), brushColor(b), penWidth(w), penStyle(s) {}
    virtual ~MyShape() {}

    virtual void draw(RasterWidget* widget) = 0;
    virtual bool contains(const QPointF& p) = 0;
    virtual QRectF getBoundingBox() = 0;
    virtual QRectF getLocalBoundingBox() = 0; // 获取局部坐标系下的包围盒
    virtual ShapeType getType() = 0;

    void translate(const QPointF& delta);
    void rotate(qreal angle, const QPointF& origin);
    void scale(qreal sx, qreal sy, const QPointF& origin);
};

// --- 3. 形状子类 (声明不变) ---

class MyLine : public MyShape {
public:
    QPointF p1, p2;
    MyLine(QPointF p1, QPointF p2, QColor p, int w, Qt::PenStyle s);
    void draw(RasterWidget* widget) override;
    bool contains(const QPointF& p) override;
    QRectF getBoundingBox() override;
    QRectF getLocalBoundingBox() override;
    ShapeType getType() override { return ShapeType::Line; }
};

class MyRect : public MyShape {
public:
    QRectF rect;
    MyRect(QRectF r, QColor p, QColor b, int w, Qt::PenStyle s);
    void draw(RasterWidget* widget) override;
    bool contains(const QPointF& p) override;
    QRectF getBoundingBox() override;
    QRectF getLocalBoundingBox() override;
    ShapeType getType() override { return ShapeType::Rect; }
};

class MyCircle : public MyShape {
public:
    qreal radius;
    MyCircle(qreal r, QColor p, QColor b, int w, Qt::PenStyle s);
    void draw(RasterWidget* widget) override;
    bool contains(const QPointF& p) override;
    QRectF getBoundingBox() override;
    QRectF getLocalBoundingBox() override;
    ShapeType getType() override { return ShapeType::Circle; }
};

class MyEllipse : public MyShape {
public:
    qreal rx, ry;
    MyEllipse(qreal rx, qreal ry, QColor p, QColor b, int w, Qt::PenStyle s);
    void draw(RasterWidget* widget) override;
    bool contains(const QPointF& p) override;
    QRectF getBoundingBox() override;
    QRectF getLocalBoundingBox() override;
    ShapeType getType() override { return ShapeType::Ellipse; }
};

class MyPolygon : public MyShape {
public:
    QVector<QPointF> points;
    MyPolygon(QVector<QPoint> p, QColor pen, QColor brush, int w, Qt::PenStyle s);
    void draw(RasterWidget* widget) override;
    bool contains(const QPointF& p) override;
    QRectF getBoundingBox() override;
    QRectF getLocalBoundingBox() override;
    ShapeType getType() override { return ShapeType::Polygon; }
};

class MyPath : public MyShape {
public:
    QVector<QPointF> points;
    MyPath(QVector<QPoint> p, QColor pen, int w, Qt::PenStyle s);
    void draw(RasterWidget* widget) override;
    bool contains(const QPointF& p) override;
    QRectF getBoundingBox() override;
    QRectF getLocalBoundingBox() override;
    ShapeType getType() override { return ShapeType::Path; }
};


// --- 4. RasterWidget 主类 ---

class RasterWidget : public QWidget
{
    Q_OBJECT

public:
    explicit RasterWidget(QWidget *parent = nullptr);
    ~RasterWidget();

public slots:
    void setPainterStatus(const PainterStatus ps);
    void setPenColor(const QColor &color);
    void setBrushColor(const QColor &color);
    void setColorType(const ColorType type);
    void setPenWidth(int width);
    void setPenStyle(Qt::PenStyle style);
    void clearCanvas();
    void onOpen();
    void onSaveAs();
    void palatteButtonClicked();

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

public:
    // 光栅化算法
    void rasterDrawLine(int x0, int y0, int x1, int y1, const QColor &color, int width, Qt::PenStyle style);
    void rasterDrawCircle(int xc, int yc, int radius, const QColor &color, int width, Qt::PenStyle style);
    void rasterDrawEllipse(int xc, int yc, int rx, int ry, const QColor &color, int width, Qt::PenStyle style);
    void rasterScanFillPolygon(const QVector<QPoint> &points, const QColor &fillColor, const QColor &borderColor, int width, Qt::PenStyle style);
    void rasterFloodFill(int x, int y, const QColor &fillColor);

private:
    // --- 内部辅助函数 ---
    void drawPixel(int x, int y, const QColor &color);
    void drawThickPixel(int x, int y, int width, const QColor &color);
    void resizeBuffer(const QSize &size);
    void redrawAllShapes();
    void deleteSelectedShapes();

    // 变换相关
    MyShape* getShapeAt(const QPoint& p);
    ControlHandle* getHandleAt(const QPoint& p);
    void createHandles(const QPolygonF& obb);
    void clearHandles();
    void setCursorForHandle(HandlePosition pos);
    QRectF getSelectedShapesBoundingBox();

private:
    QImage m_canvasBuffer;

    // --- 状态 ---
    PainterStatus m_painterStatus = PainterStatus::SELECT;
    ColorType m_colorType = ColorType::BOARD;
    QColor m_penColor = Qt::black;
    QColor m_brushColor = QColor(255, 255, 255, 0);
    int m_penWidth = 1;
    Qt::PenStyle m_penStyle = Qt::SolidLine;

    // --- “实时”绘图的状态 ---
    bool m_isDrawing = false;
    QPoint m_startPoint;
    QPoint m_currentPoint;
    QVector<QPoint> m_tempPoints;

    // --- (核心) 对象列表 和 变换状态 ---
    QList<MyShape*> m_shapeList;
    QList<MyShape*> m_selectedShapes;
    QList<ControlHandle*> m_handles;
    ControlHandle* m_activeHandle = nullptr; // 注意：在Move中不要使用它

    // [Fix] 新增变量：按值存储当前操作的手柄位置，防止野指针
    HandlePosition m_currentOpHandlePos = HandlePosition::Center;

    bool m_isTransforming = false;
    QPoint m_dragStartPosition;

    QMap<MyShape*, QTransform> m_originalTransforms; // 存储每个对象的原始变换
    QRectF m_originalBoundingBox; // 存储组的原始包围盒
    bool m_isFilling = false;

public:
    bool isChosen = true;

signals:
    void sendMousePos(QPointF pos);
};

#endif
