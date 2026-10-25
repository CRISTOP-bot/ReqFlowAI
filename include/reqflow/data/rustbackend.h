#pragma once

#include <QJsonObject>
#include <QString>

namespace reqflow::data {

// Expands {{identifier}} templates in the Rust backend; returns false without
// exposing partial output when parsing or expansion fails.
[[nodiscard]] bool expandTemplateRust(const QString &input, const QJsonObject &variables,
                                      QString *output, QString *error = nullptr);

} // namespace reqflow::data
