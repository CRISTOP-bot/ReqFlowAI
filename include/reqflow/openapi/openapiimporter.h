#pragma once

#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QByteArray>
#include <QVariantMap>
#include <QVector>

namespace reqflow::openapi {

struct ImportedRequest {
    QString name;
    QString method;
    QString path;
    QString headers;
    QString body;
};
struct ImportResult {
    QVector<ImportedRequest> requests;
    QStringList warnings;
    QString error;
};

// Supports the documented OpenAPI 3.x JSON/YAML subset; unsupported constructs are warned about, not guessed.
[[nodiscard]] ImportResult parseOpenApiJson(const QByteArray &bytes);
[[nodiscard]] ImportResult parseOpenApiYaml(const QByteArray &bytes);

} // namespace reqflow::openapi
