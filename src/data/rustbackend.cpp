#include "reqflow/data/rustbackend.h"

#include <QByteArray>
#include <QJsonDocument>

#include <cstddef>

extern "C" char *reqflow_expand_template(const char *input, std::size_t input_len, const char *variables_json, std::size_t variables_json_len, std::size_t *output_len_out, char **error_out, std::size_t *error_len_out);
extern "C" void reqflow_string_free(char *value, std::size_t len);

namespace reqflow::data {

bool expandTemplateRust(const QString &input, const QJsonObject &variables, QString *output, QString *error) {
    const QByteArray inputUtf8 = input.toUtf8();
    const QByteArray variablesUtf8 = QJsonDocument(variables).toJson(QJsonDocument::Compact);
    std::size_t rustOutputLen = 0;
    char *rustError = nullptr;
    std::size_t rustErrorLen = 0;
    char *rustOutput = reqflow_expand_template(inputUtf8.constData(), static_cast<std::size_t>(inputUtf8.size()), variablesUtf8.constData(), static_cast<std::size_t>(variablesUtf8.size()), &rustOutputLen, &rustError, &rustErrorLen);
    if (!rustOutput) {
        if (error) {
            *error = rustError ? QString::fromUtf8(rustError, static_cast<qsizetype>(rustErrorLen)) : QStringLiteral("El backend Rust no pudo procesar la plantilla.");
        }
        if (rustError) reqflow_string_free(rustError, rustErrorLen);
        return false;
    }
    if (output) *output = QString::fromUtf8(rustOutput, static_cast<qsizetype>(rustOutputLen));
    reqflow_string_free(rustOutput, rustOutputLen);
    if (rustError) reqflow_string_free(rustError, rustErrorLen);
    if (error) error->clear();
    return true;
}

} // namespace reqflow::data
