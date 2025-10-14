#ifndef CUSTOMVIEW_H
#define CUSTOMVIEW_H

#include <QGraphicsView>
#include <QMouseEvent>
#include <QGraphicsRectItem>
#include <QColorDialog>
#include "common.h"
#include "transformablelineitem.h"
#include "transformablerectitem.h"
#include "transformableellipseitem.h"
#include "mainwindow.h"

class CustomView : public QGraphicsView
{
    Q_OBJECT
public:
    explicit CustomView(QWidget *parent = nullptr);

    void setPainterStatus(const PainterStatus ps);

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
    TransformableLineItem *m_currentLineItem = nullptr;
    TransformableRectItem *m_currentRectItem = nullptr; // 当前正在绘制的矩形
    TransformableEllipseItem *m_currentEllipseItem = nullptr;
    bool m_isDrawing = false; // 是否正在绘制的标志

    // 画笔相关
    QColor penColor = Qt::black;
    QColor brushColor = QColor(255,255,255,0); // 填充色
    Qt::PenStyle penStyle = Qt::SolidLine; // 画笔类型
    ColorType colorType = BOARD; // 着色类型

public:
    int penWidth = 1; // 线宽

signals:
    void sendMousePos(QPointF pos);

public slots:
    void palatteButtonClicked();
    void boardButtonChecked(bool checked);
    void fillButtonChecked(bool checked);
    void onDeleteActionClicked();
};

#endif // CUSTOMVIEW_H
