#include "paintregion.h"

PaintRegion::PaintRegion(QWidget* parent) :
    QWidget(parent),
    pen(Qt::white, 1, Qt::SolidLine)
{
    setAttribute(Qt::WA_OpaquePaintEvent); // 加快重绘(废弃使用，因为会导致背景色不能修改)
}

void PaintRegion::setIsTempWidget(const bool isTemp)
{
    this->isTempWidget = isTemp;
}

void PaintRegion::setPainterStatus(const PainterStatus ps)
{
    this->painterStatus = ps;
}

void PaintRegion::paintEvent(QPaintEvent* event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing); // 设置抗锯齿
    painter.setPen(pen);

    if (painterStatus == PainterStatus::PEN)
    {
        if (!isTempWidget && !currPath.isEmpty())
        {
            painter.drawPath(currPath);
        }
    }
    else
    {
        if (isTempWidget)
        {
            painter.fillRect(rect(), Qt::white);
            if (!is_drawing)
            {
                return;
            }
        }
        else
        {
            if (is_drawing)
            {
                return;
            }
        }
        if (painterStatus == PainterStatus::LINE)
        {
            painter.drawLine(lineDrawingLastPoint, lineDrawingCurrPoint);
        }
    }
}

void PaintRegion::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton)
    {
        is_drawing = true;
        currPath.moveTo(event->position());
        lineDrawingLastPoint = event->position();
        lineDrawingCurrPoint = event->position();
        update();
        return;
    }
}

void PaintRegion::mouseMoveEvent(QMouseEvent* event)
{
    if (is_drawing && (event->buttons() & Qt::LeftButton))
    {
        switch (painterStatus) {
            case PainterStatus::PEN:
                currPath.lineTo(event->position());
                break;
            case PainterStatus::LINE:
                lineDrawingCurrPoint = event->position();
                break;
            default:
                break;
        }
        update();
        return;
    }
}

void PaintRegion::mouseReleaseEvent(QMouseEvent* event)
{
    if (is_drawing && event->button() == Qt::LeftButton)
    {
        is_drawing = false;
        paths.append(currPath);
        currPath.clear();
        update();
        return;
    }
}
