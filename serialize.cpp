#include "common.h"
#include "transformablelineitem.h"
#include "transformablerectitem.h"
#include "transformableellipseitem.h"
#include "transformablepolygonitem.h"
#include "transformablepathitem.h"

static QJsonArray pathToArray(const QPainterPath &p)
{
    QJsonArray arr;
    for (int i = 0; i < p.elementCount(); ++i) {
        const QPainterPath::Element &e = p.elementAt(i);
        QJsonArray elem{e.type, e.x, e.y};
        arr.append(elem);
    }
    return arr;
}

static QPainterPath arrayToPath(const QJsonArray &arr)
{
    QPainterPath p;
    for (const QJsonValue &v : arr) {
        QJsonArray e = v.toArray();
        int type = e[0].toInt();
        qreal x  = e[1].toDouble();
        qreal y  = e[2].toDouble();
        switch (type) {
        case QPainterPath::MoveToElement:  p.moveTo(x, y);  break;
        case QPainterPath::LineToElement:  p.lineTo(x, y);  break;
        case QPainterPath::CurveToElement:
            /* 需再读两个控制点 */
            break; // 若用到 cubic 需继续展开
        }
    }
    return p;
}

inline QString itemType(QGraphicsItem *it)
{
    if (qgraphicsitem_cast<TransformablePathItem*>(it))     return "TransformablePathItem";
    if (qgraphicsitem_cast<TransformableLineItem*>(it))     return "TransformableLineItem";
    if (qgraphicsitem_cast<TransformableRectItem*>(it))     return "TransformableRectItem";
    if (qgraphicsitem_cast<TransformableEllipseItem*>(it))  return "TransformableEllipseItem";
    if (qgraphicsitem_cast<TransformablePolygonItem*>(it))  return "TransformablePolygonItem";
    return "Unknown";
}

QJsonObject itemToJson(QGraphicsItem *item)
{
    QJsonObject obj;
    if (!item) return obj;

    /* 公共属性 */
    obj["type"] = itemType(item);
    obj["z"]    = item->zValue();
    obj["rot"]  = item->rotation();
    QJsonArray pos = {item->pos().x(), item->pos().y()};
    obj["pos"]  = pos;

    ItemCommon *c = dynamic_cast<ItemCommon*>(item);
    if (c) {
        obj["penColor"]   = c->penColor.name(QColor::HexArgb);
        obj["brushColor"] = c->brushColor.name(QColor::HexArgb);
        obj["penWidth"]   = c->penWidth;
        obj["penStyle"]   = c->penStyle;
    }

    /* 各类型私有数据 */
    if (auto *l = qgraphicsitem_cast<TransformableLineItem*>(item)) {
        QLineF ln = l->line();
        obj["x1"] = ln.x1(); obj["y1"] = ln.y1();
        obj["x2"] = ln.x2(); obj["y2"] = ln.y2();
    } else if (auto *r = qgraphicsitem_cast<TransformableRectItem*>(item)) {
        QRectF rc = r->rect();
        obj["x"] = rc.x(); obj["y"] = rc.y();
        obj["w"] = rc.width(); obj["h"] = rc.height();
    } else if (auto *e = qgraphicsitem_cast<TransformableEllipseItem*>(item)) {
        QRectF rc = e->rect();
        obj["x"] = rc.x(); obj["y"] = rc.y();
        obj["w"] = rc.width(); obj["h"] = rc.height();
        obj["circle"] = e->isCircle;
    } else if (auto *p = qgraphicsitem_cast<TransformablePolygonItem*>(item)) {
        QJsonArray pts;
        for (const QPointF &pt : p->polygon())
            pts.append(QJsonArray{pt.x(), pt.y()});
        obj["points"] = pts;
    } else if (auto *pa = qgraphicsitem_cast<TransformablePathItem*>(item)) {
        obj["path"] = pathToArray(pa->path());   // QPainterPath 自带字符串序列化
    }
    return obj;
}

void jsonToItem(const QJsonObject &o, QGraphicsScene *scene)
{
    QString type = o["type"].toString();
    qreal z  = o["z"].toDouble();
    qreal rot= o["rot"].toDouble();
    QJsonArray posArr = o["pos"].toArray();
    QPointF pos(posArr[0].toDouble(), posArr[1].toDouble());

    auto applyCommon = [&](QGraphicsItem *it){
        it->setPos(pos);
        it->setRotation(rot);
        it->setZValue(z);
        if (ItemCommon *c = dynamic_cast<ItemCommon*>(it)) {
            c->penColor   = QColor(o["penColor"].toString());
            c->brushColor = QColor(o["brushColor"].toString());
            c->penWidth   = o["penWidth"].toInt();
            c->penStyle   = static_cast<Qt::PenStyle>(o["penStyle"].toInt());
        }
    };

    if (type == "TransformableLineItem") {
        auto *l = new TransformableLineItem(QLineF(
            o["x1"].toDouble(), o["y1"].toDouble(),
            o["x2"].toDouble(), o["y2"].toDouble()));
        applyCommon(l);
        l->setPen(QPen(l->penColor, l->penWidth, l->penStyle));
        scene->addItem(l);
    } else if (type == "TransformableRectItem") {
        auto *r = new TransformableRectItem(QRectF(
            o["x"].toDouble(), o["y"].toDouble(),
            o["w"].toDouble(), o["h"].toDouble()));
        applyCommon(r);
        r->setPen(QPen(r->penColor, r->penWidth, r->penStyle));
        r->setBrush(QBrush(r->brushColor));
        scene->addItem(r);
    } else if (type == "TransformableEllipseItem") {
        auto *e = new TransformableEllipseItem(QRectF(
                                                   o["x"].toDouble(), o["y"].toDouble(),
                                                   o["w"].toDouble(), o["h"].toDouble()), nullptr, o["circle"].toBool());
        applyCommon(e);
        e->setPen(QPen(e->penColor, e->penWidth, e->penStyle));
        e->setBrush(QBrush(e->brushColor));
        scene->addItem(e);
    } else if (type == "TransformablePolygonItem") {
        QJsonArray pts = o["points"].toArray();
        QPolygonF poly;
        for (int i = 0; i < pts.size(); ++i) {
            QJsonArray p = pts[i].toArray();
            poly << QPointF(p[0].toDouble(), p[1].toDouble());
        }
        auto *p = new TransformablePolygonItem(poly);
        applyCommon(p);
        p->setPen(QPen(p->penColor, p->penWidth, p->penStyle));
        p->setBrush(QBrush(p->brushColor));
        scene->addItem(p);
    } else if (type == "TransformablePathItem") {
        QPainterPath path = arrayToPath(o["path"].toArray());
        auto *pa = new TransformablePathItem(path);
        applyCommon(pa);
        pa->setPen(QPen(pa->penColor, pa->penWidth, pa->penStyle));
        scene->addItem(pa);
    }
}
