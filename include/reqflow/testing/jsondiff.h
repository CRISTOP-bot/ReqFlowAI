#pragma once

#include <QJsonArray>
#include <QJsonValue>
#include <QString>
#include <QVector>

namespace reqflow::testing {

enum class JsonChangeKind { Added, Removed, Modified };

struct JsonChange {
    QString path;
    JsonChangeKind kind = JsonChangeKind::Modified;
    QJsonValue before;
    QJsonValue after;
};

struct JsonDiffResult {
    QVector<JsonChange> changes;
    bool truncated = false;
};

// Compares parsed JSON values without mutating either input. Paths use a readable JSONPath-like notation.
[[nodiscard]] JsonDiffResult compareJson(const QJsonValue &before, const QJsonValue &after,
                                         int maxChanges = 5000, int maxDepth = 64);
[[nodiscard]] QString jsonValueForDiff(const QJsonValue &value);
[[nodiscard]] QString jsonChangeKindName(JsonChangeKind kind);

} // namespace reqflow::testing
