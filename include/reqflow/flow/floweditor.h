#pragma once

#include "reqflow/data/projectstore.h"
#include "reqflow/flow/flowengine.h"

#include <QGraphicsScene>
#include <QGraphicsView>
#include <QJsonObject>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QWidget>
#include <QMap>
#include <QComboBox>

namespace reqflow::flow {

class FlowEditor final : public QWidget {
    Q_OBJECT
public:
    FlowEditor(data::ProjectStore *store, qint64 projectId, QWidget *parent = nullptr);

private slots:
    void addNode();
    void applyConfig();
    void saveFlow();
    void validateFlow();
    void runFlow();
    void loadSelectedFlow(int index);
    void selectedNodeChanged();
    void nodeMoved(const QString &nodeId, const QPointF &position);

private:
    void rebuildCanvas();
    void appendLog(const QString &text);
    [[nodiscard]] bool safeToPersist() const;
    [[nodiscard]] QJsonObject currentJson() const;

    data::ProjectStore *store_;
    qint64 projectId_;
    qint64 flowId_ = -1;
    FlowDefinition flow_;
    QMap<QString, QPointF> positions_;
    QGraphicsScene *scene_ = nullptr;
    QGraphicsView *view_ = nullptr;
    QPlainTextEdit *config_ = nullptr;
    QPlainTextEdit *log_ = nullptr;
    QLineEdit *name_ = nullptr;
    QComboBox *type_ = nullptr;
    QComboBox *savedFlows_ = nullptr;
    QString selectedNodeId_;
};

} // namespace reqflow::flow
