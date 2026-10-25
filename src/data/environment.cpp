#include "reqflow/data/environment.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonValue>
#include <QRegularExpression>

namespace reqflow::data {

bool expandTemplate(const QString &input, const QJsonObject &variables, QString *output, QString *error) {
    static const QRegularExpression token(QStringLiteral(R"(\{\{([A-Za-z_][A-Za-z0-9_.-]*)\}\})"));
    QString result;
    qsizetype cursor = 0;
    auto matches = token.globalMatch(input);
    while (matches.hasNext()) {
        const auto match = matches.next();
        result += input.mid(cursor, match.capturedStart() - cursor);
        const QString name = match.captured(1);
        if (!variables.contains(name)) {
            if (error) *error = QStringLiteral("La variable {{%1}} no existe en el entorno seleccionado.").arg(name);
            return false;
        }
        const QJsonValue value = variables.value(name);
        if (value.isString()) result += value.toString();
        else if (value.isObject()) result += QString::fromUtf8(QJsonDocument(value.toObject()).toJson(QJsonDocument::Compact));
        else if (value.isArray()) result += QString::fromUtf8(QJsonDocument(value.toArray()).toJson(QJsonDocument::Compact));
        else if (!value.isNull() && !value.isUndefined()) result += value.toVariant().toString();
        cursor = match.capturedEnd();
    }
    result += input.mid(cursor);
    if (result.contains(QStringLiteral("{{")) || result.contains(QStringLiteral("}}"))) {
        if (error) *error = QStringLiteral("Marcador de plantilla mal formado o anidado; se esperaba {{identificador}}.");
        return false;
    }
    if (output) *output = result;
    return true;
}

} // namespace reqflow::data
