#include "reqflow/data/environment.h"
#include "reqflow/data/rustbackend.h"

namespace reqflow::data {

bool expandTemplate(const QString &input, const QJsonObject &variables, QString *output, QString *error) {
    return expandTemplateRust(input, variables, output, error);
}

} // namespace reqflow::data
