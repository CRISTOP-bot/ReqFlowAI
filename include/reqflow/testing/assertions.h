#pragma once

#include "reqflow/network/httpclient.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QStringList>
#include <QVector>

namespace reqflow::testing {

struct AssertionResult {
    QString name;
    bool passed = false;
    QString actual;
    QString message;
};

// Assertion objects use: {name,type,expected,header,path,operator}.
// Supported types: status, header, json_value, json_type and duration_ms.
[[nodiscard]] QVector<AssertionResult> evaluateAssertions(const QJsonArray &assertions,
                                                           const network::HttpResponse &response);
[[nodiscard]] QJsonArray assertionResultsToJson(const QVector<AssertionResult> &results);

} // namespace reqflow::testing
