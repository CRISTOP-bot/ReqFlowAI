#include "reqflow/openapi/yaml.h"

#include <QJsonArray>
#include <QJsonValue>
#include <QVariant>

#include <yaml-cpp/yaml.h>

namespace reqflow::openapi {
namespace {
QJsonValue yamlValue(const YAML::Node &node) {
    if (!node || node.IsNull()) return QJsonValue(QJsonValue::Null);
    if (node.IsSequence()) {
        QJsonArray array;
        for (const auto &value : node) array.append(yamlValue(value));
        return array;
    }
    if (node.IsMap()) {
        QJsonObject object;
        for (const auto &entry : node) {
            if (!entry.first.IsScalar()) continue;
            object.insert(QString::fromStdString(entry.first.Scalar()), yamlValue(entry.second));
        }
        return object;
    }
    const QString text = QString::fromStdString(node.Scalar());
    const QString lowered = text.toLower();
    if (lowered == QStringLiteral("true")) return true;
    if (lowered == QStringLiteral("false")) return false;
    if (lowered == QStringLiteral("null") || text == QStringLiteral("~")) return QJsonValue(QJsonValue::Null);
    bool integerOk = false;
    const qlonglong integer = text.toLongLong(&integerOk);
    if (integerOk) return static_cast<double>(integer);
    bool numberOk = false;
    const double number = text.toDouble(&numberOk);
    if (numberOk) return number;
    return text;
}
}

QJsonObject parseYamlObject(const QByteArray &bytes, QString *error) {
    try {
        const YAML::Node root = YAML::Load(bytes.constData());
        const QJsonValue value = yamlValue(root);
        if (!value.isObject()) {
            if (error) *error = QStringLiteral("La especificación YAML debe tener un objeto en la raíz.");
            return {};
        }
        if (error) error->clear();
        return value.toObject();
    } catch (const YAML::Exception &exception) {
        if (error) *error = QString::fromUtf8(exception.what());
        return {};
    }
}

} // namespace reqflow::openapi
