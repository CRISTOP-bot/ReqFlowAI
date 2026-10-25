#pragma once

#include <QJsonObject>
#include <QString>

namespace reqflow::data {

// Strictly expands {{identifier}} placeholders; unknown names fail without returning partial output.
[[nodiscard]] bool expandTemplate(const QString &input, const QJsonObject &variables,
                                  QString *output, QString *error = nullptr);

} // namespace reqflow::data
