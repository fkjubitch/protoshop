#ifndef PAINTREGION_H
#define PAINTREGION_H

#include <QWidget>
#include <QPainter>
#include <QMouseEvent>
#include <QPainterPath>
#include <QVector>
#include "common.h"

class PaintRegion : public QWidget
{
public:
    PaintRegion(QWidget* parent);
    void setIsTempWidget(const bool isTemp);
    void setPainterStatus(const PainterStatus ps);

    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    bool isTempWidget = false;

    QPen pen;
    QVector<QPainterPath> paths;
    QPainterPath currPath;
    bool is_drawing = false;
    QPointF lineDrawingLastPoint;
    QPointF lineDrawingCurrPoint;
    PainterStatus painterStatus = PainterStatus::PEN;
};

#endif // PAINTREGION_H
