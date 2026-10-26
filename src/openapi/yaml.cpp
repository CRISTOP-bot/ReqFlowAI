#include "reqflow/openapi/yaml.h"

#include <QJsonArray>
#include <QJsonValue>
#include <QVariant>

#include <yaml-cpp/yaml.h>

namespace reqflow::openapi {
namespace {
// B3: un documento pequeño con aliases anidados («billion laughs») puede pedir
// miles de millones de visitas al convertir el árbol compartido a JSON. El
// presupuesto corta la expansión y devuelve un error claro.
constexpr int kMaxYamlNodes = 100'000;
struct NodeBudgetExceeded {};

QJsonValue yamlValue(const YAML::Node &node, int &budget) {
    if (budget <= 0) throw NodeBudgetExceeded{};
    --budget;
    if (!node || node.IsNull()) return QJsonValue(QJsonValue::Null);
    if (node.IsSequence()) {
        QJsonArray array;
        for (const auto &value : node) array.append(yamlValue(value, budget));
        return array;
    }
    if (node.IsMap()) {
        QJsonObject object;
        for (const auto &entry : node) {
            if (!entry.first.IsScalar()) continue;
            object.insert(QString::fromStdString(entry.first.Scalar()), yamlValue(entry.second, budget));
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
        int budget = kMaxYamlNodes;
        const QJsonValue value = yamlValue(root, budget);
        if (!value.isObject()) {
            if (error) *error = QStringLiteral("La especificación YAML debe tener un objeto en la raíz.");
            return {};
        }
        if (error) error->clear();
        return value.toObject();
    } catch (const NodeBudgetExceeded &) {
        if (error)
            *error = QStringLiteral("El documento YAML supera el presupuesto de %1 nodos expandibles; posible bomba de aliases.")
                         .arg(kMaxYamlNodes);
        return {};
    } catch (const YAML::Exception &exception) {
        if (error) *error = QString::fromUtf8(exception.what());
        return {};
    }
}

} // namespace reqflow::openapi
