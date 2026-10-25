#pragma once

#include "reqflow/network/httpclient.h"

#include <QJsonObject>
#include <QList>
#include <QString>
#include <QVariantMap>
#include <atomic>

namespace reqflow::flow {

struct FlowNode {
    QString id;
    QString type;
    QJsonObject config;
};
struct FlowEdge {
    QString source;
    QString target;
    QString label;
};
struct FlowDefinition {
    QString name;
    int version = 1;
    QList<FlowNode> nodes;
    QList<FlowEdge> edges;
};
struct NodeLog {
    QString nodeId;
    QString type;
    QString status;
    qint64 elapsedMs = 0;
    QString message;
    QJsonObject output;
};
struct FlowRun {
    QString status;
    QList<NodeLog> logs;
    QVariantMap variables;
    QString error;
};

[[nodiscard]] QStringList validate(const FlowDefinition &flow);
[[nodiscard]] FlowDefinition fromJson(const QJsonObject &json);
[[nodiscard]] QJsonObject toJson(const FlowDefinition &flow);

class FlowEngine final {
public:
    [[nodiscard]] FlowRun run(const FlowDefinition &flow, const QVariantMap &inputs = {},
                              const std::atomic_bool *cancel = nullptr,
                              int maxSteps = 500) const;
};

} // namespace reqflow::flow
