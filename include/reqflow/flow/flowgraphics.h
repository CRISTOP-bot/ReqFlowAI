#pragma once

#include "reqflow/ui/theme.h"

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
        setToolTip(QStringLiteral("%1 · nodo %2\nArrastra para moverlo; clic para editarlo.").arg(type_, id_));
    }
    [[nodiscard]] QString id() const { return id_; }
    [[nodiscard]] QRectF boundingRect() const override { return {0, 0, 190, 74}; }
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *) override {
        // Colores desde los tokens del tema (theme.h), nunca literales hex.
        const auto token = [](const char *value) { return QColor(QString::fromLatin1(value)); };
        painter->setPen(QPen(token(reqflow::ui::theme::kBorderStrong), 2));
        painter->setBrush(isSelected() ? token(reqflow::ui::theme::kAccentDark)
                                       : token(reqflow::ui::theme::kSurface2));
        painter->drawRoundedRect(boundingRect(), 8, 8);
        painter->setPen(token(reqflow::ui::theme::kAccentSoft));
        painter->drawText(QRectF(12, 10, 165, 20), Qt::AlignLeft, type_.toUpper());
        painter->setPen(token(reqflow::ui::theme::kText));
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
