#ifndef CUSTOMVIEW_H
#define CUSTOMVIEW_H

#include <QGraphicsView>
#include <QMouseEvent>
#include <QGraphicsRectItem>
#include <QColorDialog>
#include <QVector>
#include <QStack>
#include <QJsonArray>
#include "common.h"
#include "transformablepathitem.h"
#include "transformablelineitem.h"
#include "transformablerectitem.h"
#include "transformablepolygonitem.h"
#include "transformableellipseitem.h"

class CustomView : public QGraphicsView
{
    Q_OBJECT
public:
    explicit CustomView(QWidget *parent = nullptr);

    void setPainterStatus(const PainterStatus ps);
    //撤销重做相关
    void saveSceneState();   // 保存当前场景状态
    void restoreSceneState(const QJsonArray &state); // 恢复场景状态

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    void deleteSelectedItem();

private:
    PainterStatus painterStatus = PainterStatus::SELECT;

    QPointF m_startPoint; // 记录鼠标按下的起始点
    TransformablePathItem *m_currentPathItem = nullptr;
    QPainterPath m_livePath;
    TransformableLineItem *m_currentLineItem = nullptr;
    TransformableRectItem *m_currentRectItem = nullptr; // 当前正在绘制的矩形
    TransformablePolygonItem *m_currentPolygonItem = nullptr;
    QPolygonF m_livePolygon;   // 正在采集的顶点
    TransformableEllipseItem *m_currentEllipseItem = nullptr;
    bool m_isDrawing = false; // 是否正在绘制的标志

    // 画笔相关
    QColor penColor = Qt::black;
    QColor brushColor = QColor(255,255,255,0); // 填充色

    // 旋转相关
    bool isRotateCursor = false;

    // 撤销重做相关
    const int maxUndoSteps = 50; // 最大撤销步数
    QStack<QJsonArray> undoStack; // 重做栈
    QStack<QJsonArray> redoStack; // 撤销栈

public:
    int penWidth = 1; // 线宽
    Qt::PenStyle penStyle = Qt::SolidLine; // 画笔类型
    ColorType colorType = BOARD; // 着色类型

signals:
    void sendMousePos(QPointF pos);

public slots:
    void palatteButtonClicked();
    void onDeleteActionClicked();
    void onSaveAs();     // 弹出对话框 → 选 *.png / *.json
    void onOpen();       // 弹出对话框 → 选 *.json → 还原
    void onRevoke(); // 撤销
    void onUndo();   // 重做
};

#endif // CUSTOMVIEW_H
