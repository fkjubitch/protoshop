#ifndef RASTERWIDGET_H
#define RASTERWIDGET_H

#include <QWidget>
#include <QImage>
#include <QColor>
#include <QPoint>
#include <QVector>
#include "common.h"

class QPaintEvent;
class QResizeEvent;
class QMouseEvent;

class RasterWidget : public QWidget
{
    Q_OBJECT

public:
    explicit RasterWidget(QWidget *parent = nullptr);

public slots:
    // --- 这些是用来从 MainWindow 接收信号的槽函数 ---

    /**
     * @brief 设置当前绘图工具 (连接到 mainwindow.cpp 的 on_..._clicked)
     */
    void setPainterStatus(const PainterStatus ps);

    /**
     * @brief 设置当前画笔颜色 (连接到 mainwindow.cpp 的 palatteButtonClicked)
     */
    void setPenColor(const QColor &color);

    /**
     * @brief 设置当前填充颜色 (连接到 mainwindow.cpp 的 palatteButtonClicked)
     */
    void setBrushColor(const QColor &color);

    /**
     * @brief 设置当前着色类型 (连接到 mainwindow.cpp 的 on_boardButton_clicked / on_fillButton_clicked)
     */
    void setColorType(const ColorType type);

    /**
     * @brief 设置当前线宽 (连接到 mainwindow.cpp 的 on_spinBox_valueChanged)
     */
    void setPenWidth(int width);

    /**
     * @brief 设置当前线型 (连接到 mainwindow.cpp 的 on_..._toggled)
     */
    void setPenStyle(Qt::PenStyle style);

    /**
     * @brief 清空画布
     */
    void clearCanvas();

    // --- 您还需要添加用于撤销/重做、保存/打开的槽函数 ---
    // void onRevoke();
    // void onUndo();
    // void onSaveAs();
    // void onOpen();

protected:
    // --- 重写的 Qt 事件 ---

    /**
     * @brief (第3步: 显示) 核心绘制事件
     */
    void paintEvent(QPaintEvent *event) override;

    /**
     * @brief (第1步: 初始化) 窗口大小改变事件
     */
    void resizeEvent(QResizeEvent *event) override;

    /**
     * @brief (第2步: 绘图) 鼠标按下事件
     */
    void mousePressEvent(QMouseEvent *event) override;

    /**
     * @brief (第2步: 绘图) 鼠标移动事件 (用于实时预览)
     */
    void mouseMoveEvent(QMouseEvent *event) override;

    /**
     * @brief (第2步: 绘图) 鼠标释放事件 (用于“固化”图形)
     */
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    // --- 核心绘图 API ---

    /**
     * @brief (基础) 在画布缓冲区的指定坐标绘制一个像素
     */
    void drawPixel(int x, int y, const QColor &color);

    /**
     * @brief 画一个粗像素
     */
    void drawThickPixel(int x, int y, int width, const QColor &color);

    /**
     * @brief 调整内部 QImage 缓冲区大小的辅助函数
     */
    void resizeBuffer(const QSize &size);

    /**
     * @brief (作业要求) 使用Bresenham或DDA算法绘制直线
     */
    void rasterDrawLine(int x0, int y0, int x1, int y1, const QColor &color, int width, Qt::PenStyle style);

    /**
     * @brief (作业要求) 使用中点圆算法绘制圆形
     */
    void rasterDrawCircle(int xc, int yc, int radius, const QColor &color, int width, Qt::PenStyle style);

    /**
     * @brief (作业要求) 使用中点椭圆算法绘制椭圆
     */
    void rasterDrawEllipse(int xc, int yc, int rx, int ry, const QColor &color, int width, Qt::PenStyle style);

    /**
     * @brief (作业要求) 使用扫描线算法填充多边形
     */
    void rasterScanFillPolygon(const QVector<QPoint> &points, const QColor &fillColor, const QColor &borderColor, int width, Qt::PenStyle style);

    /**
     * @brief (作业要求) 使用泛洪算法（或边界填充）进行填充
     */
    void rasterFloodFill(int x, int y, const QColor &fillColor);


private:
    QImage m_canvasBuffer;

    // --- 存储从 MainWindow 传来的当前状态 ---
    PainterStatus m_painterStatus = PainterStatus::SELECT; // 当前工具
    ColorType m_colorType = ColorType::BOARD; // 当前着色类型 (边框/填充)
    QColor m_penColor = Qt::black;     // 当前画笔 (边框) 颜色
    QColor m_brushColor = Qt::white;   // 当前画刷 (填充) 颜色
    int m_penWidth = 1;                // 当前线宽
    Qt::PenStyle m_penStyle = Qt::SolidLine; // 当前线型

    // --- 存储“实时”绘图的状态 ---
    bool m_isDrawing = false;           // 鼠标是否按下
    QPoint m_startPoint;                // 鼠标按下的起点
    QPoint m_currentPoint;              // 鼠标移动的当前点
    QVector<QPoint> m_currentPolygonPoints; // 用于绘制多边形的临时顶点列表

public:
    bool isChosen = true;
};

#endif // RASTERWIDGET_H
