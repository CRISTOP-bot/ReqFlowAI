#pragma once

#include <QGraphicsObject>
#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QStyleOptionGraphicsItem>
#include <utility>

namespace reqflow::flow {

class FlowNodeItem final : public QGraphicsObject {
    Q_OBJECT
public:
    FlowNodeItem(QString id, QString type, QGraphicsItem *parent = nullptr)
        : QGraphicsObject(parent), id_(std::move(id)), type_(std::move(type)) {
        setFlags(ItemIsMovable | ItemIsSelectable | ItemSendsGeometryChanges);
        setCacheMode(DeviceCoordinateCache);
    }
    [[nodiscard]] QString id() const { return id_; }
    [[nodiscard]] QRectF boundingRect() const override { return {0, 0, 190, 74}; }
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *) override {
        painter->setPen(QPen(QColor(QStringLiteral("#8192ad")), 2));
        painter->setBrush(isSelected() ? QColor(QStringLiteral("#4338ca")) : QColor(QStringLiteral("#1e2b40")));
        painter->drawRoundedRect(boundingRect(), 8, 8);
        painter->setPen(QColor(QStringLiteral("#a99cff")));
        painter->drawText(QRectF(12, 10, 165, 20), Qt::AlignLeft, type_.toUpper());
        painter->setPen(QColor(QStringLiteral("#f1f5f9")));
        painter->drawText(QRectF(12, 36, 165, 24), Qt::AlignLeft, id_);
    }
signals:
    void moved(const QString &id, const QPointF &position);
    void selected(const QString &id);
protected:
    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override {
        if (change == ItemPositionHasChanged) emit moved(id_, value.toPointF());
        return QGraphicsObject::itemChange(change, value);
    }
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override {
        emit selected(id_);
        QGraphicsObject::mousePressEvent(event);
    }
private:
    QString id_;
    QString type_;
};

} // namespace reqflow::flow
