#include "reqflow/testing/assertions.h"

#include <QJsonDocument>
#include <QJsonValue>
#include <QRegularExpression>
#include <QVariant>

namespace reqflow::testing {
namespace {
QJsonValue jsonAtPath(const QJsonValue &root, const QString &path, bool *found) {
    *found = false;
    if (root.isUndefined()) return {};
    if (path.isEmpty() || path == QStringLiteral("$")) { *found = true; return root; }
    if (!path.startsWith(QLatin1Char('$'))) return {};
    static const QRegularExpression token(QStringLiteral(R"PATH(\.([A-Za-z_][A-Za-z0-9_-]*)|\[(\d+)\]|\['([^']+)'\]|\["([^"]+)"\])PATH"));
    QJsonValue current = root;
    int consumed = 0;
    const QString remainder = path.mid(1);
    auto matches = token.globalMatch(remainder);
    while (matches.hasNext()) {
        const auto match = matches.next();
        if (match.capturedStart() != consumed) return {};
        consumed = match.capturedEnd();
        if (!match.captured(2).isEmpty()) {
            bool ok = false; const int index = match.captured(2).toInt(&ok);
            if (!ok || !current.isArray() || index < 0 || index >= current.toArray().size()) return {};
            current = current.toArray().at(index);
        } else {
            const QString key = !match.captured(1).isEmpty() ? match.captured(1) :
                                !match.captured(3).isEmpty() ? match.captured(3) : match.captured(4);
            if (!current.isObject() || !current.toObject().contains(key)) return {};
            current = current.toObject().value(key);
        }
    }
    if (consumed != remainder.size()) return {};
    *found = true;
    return current;
}

QString display(const QJsonValue &value) {
    if (value.isString()) return value.toString();
    if (value.isObject()) return QString::fromUtf8(QJsonDocument(value.toObject()).toJson(QJsonDocument::Compact));
    if (value.isArray()) return QString::fromUtf8(QJsonDocument(value.toArray()).toJson(QJsonDocument::Compact));
    if (value.isNull()) return QStringLiteral("null");
    if (value.isUndefined()) return QStringLiteral("undefined");
    return value.toVariant().toString();
}

bool compare(const QString &actual, const QString &expected, const QString &op) {
    if (op == QStringLiteral("contains")) return actual.contains(expected);
    if (op == QStringLiteral("not_equals")) return actual != expected;
    return actual == expected;
}

QString jsonType(const QJsonValue &value) {
    if (value.isObject()) return QStringLiteral("object");
    if (value.isArray()) return QStringLiteral("array");
    if (value.isString()) return QStringLiteral("string");
    if (value.isBool()) return QStringLiteral("boolean");
    if (value.isDouble()) return QStringLiteral("number");
    if (value.isNull()) return QStringLiteral("null");
    return QStringLiteral("undefined");
}
}

QVector<AssertionResult> evaluateAssertions(const QJsonArray &assertions, const network::HttpResponse &response) {
    QVector<AssertionResult> results;
    QJsonParseError parseError;
    const QJsonDocument parsed = QJsonDocument::fromJson(response.body, &parseError);
    const QJsonValue jsonRoot = parsed.isObject() ? QJsonValue(parsed.object()) : QJsonValue(parsed.array());
    for (qsizetype i = 0; i < assertions.size(); ++i) {
        const QJsonObject spec = assertions.at(i).toObject();
        AssertionResult result;
        result.name = spec.value(QStringLiteral("name")).toString(QStringLiteral("Assertion %1").arg(i + 1));
        const QString type = spec.value(QStringLiteral("type")).toString();
        const QString expected = spec.value(QStringLiteral("expected")).toVariant().toString();
        const QString op = spec.value(QStringLiteral("operator")).toString(QStringLiteral("equals"));
        if (type == QStringLiteral("status")) {
            result.actual = QString::number(response.status);
            result.passed = compare(result.actual, expected, op);
        } else if (type == QStringLiteral("duration_ms")) {
            result.actual = QString::number(response.elapsedMs);
            bool ok = false; const qint64 limit = expected.toLongLong(&ok);
            result.passed = ok && (op == QStringLiteral("less_than") ? response.elapsedMs < limit : response.elapsedMs <= limit);
        } else if (type == QStringLiteral("header")) {
            result.actual = QString::fromLatin1(response.header(spec.value(QStringLiteral("header")).toString().toLatin1()));
            result.passed = compare(result.actual, expected, op);
        } else if (type == QStringLiteral("json_value") || type == QStringLiteral("json_type")) {
            bool found = false;
            const QJsonValue value = jsonAtPath(jsonRoot, spec.value(QStringLiteral("path")).toString(), &found);
            result.actual = found ? (type == QStringLiteral("json_type") ? jsonType(value) : display(value)) : QStringLiteral("path not found");
            result.passed = found && compare(result.actual, expected, op);
        } else {
            result.message = QStringLiteral("Tipo de aserción no compatible.");
        }
        if (result.message.isEmpty()) result.message = result.passed ? QStringLiteral("OK") : QStringLiteral("No coincide con lo esperado.");
        results.append(result);
    }
    return results;
}

QJsonArray assertionResultsToJson(const QVector<AssertionResult> &results) {
    QJsonArray array;
    for (const auto &result : results)
        array.append(QJsonObject{{QStringLiteral("name"), result.name}, {QStringLiteral("passed"), result.passed},
                                 {QStringLiteral("message"), result.message}});
    return array;
}

} // namespace reqflow::testing
