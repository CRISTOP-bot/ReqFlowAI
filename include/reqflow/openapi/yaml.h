#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QString>

namespace reqflow::openapi {

// Converts YAML into JSON-compatible OpenAPI data. Returns an empty object on invalid YAML.
[[nodiscard]] QJsonObject parseYamlObject(const QByteArray &bytes, QString *error = nullptr);

} // namespace reqflow::openapi
