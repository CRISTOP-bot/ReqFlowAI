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

// Supports OpenAPI 3.x JSON documents; unsupported constructs are reported, not guessed.
[[nodiscard]] ImportResult parseOpenApiJson(const QByteArray &bytes);

} // namespace reqflow::openapi
