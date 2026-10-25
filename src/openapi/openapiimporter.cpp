#include "reqflow/openapi/openapiimporter.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QJsonValue>
#include <QRegularExpression>

namespace reqflow::openapi {
namespace {
QString methodTitle(QString method) { if(method.isEmpty()) return method; method[0]=method[0].toUpper(); return method; }
QString schemaSample(const QJsonObject &schema) {
    const QString type=schema.value(QStringLiteral("type")).toString();
    if(type==QStringLiteral("object")) {
        QJsonObject object;
        const auto props=schema.value(QStringLiteral("properties")).toObject();
        for(auto it=props.begin();it!=props.end();++it) {
            const auto child=it.value().toObject(); const QString childType=child.value(QStringLiteral("type")).toString();
            if(childType==QStringLiteral("string")) object.insert(it.key(),QStringLiteral("string"));
            else if(childType==QStringLiteral("integer")||childType==QStringLiteral("number")) object.insert(it.key(),0);
            else if(childType==QStringLiteral("boolean")) object.insert(it.key(),false);
        }
        return QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Indented));
    }
    return {};
}
}
ImportResult parseOpenApiJson(const QByteArray &bytes) {
    ImportResult result; QJsonParseError parseError; const auto doc=QJsonDocument::fromJson(bytes,&parseError);
    if(parseError.error!=QJsonParseError::NoError||!doc.isObject()){result.error=QStringLiteral("Documento JSON inválido: ")+parseError.errorString();return result;}
    const auto root=doc.object(); const QString version=root.value(QStringLiteral("openapi")).toString();
    if(!version.startsWith(QStringLiteral("3."))){result.error=QStringLiteral("Solo se admite OpenAPI 3.x JSON en esta versión.");return result;}
    const auto servers = root.value(QStringLiteral("servers")).toArray();
    const QString base = servers.isEmpty() ? QString{} : servers.first().toObject().value(QStringLiteral("url")).toString();
    const auto paths=root.value(QStringLiteral("paths")).toObject();
    if(paths.isEmpty()) result.warnings.append(QStringLiteral("La especificación no contiene paths."));
    static const QStringList methods={"get","post","put","patch","delete","head","options"};
    for(auto pathIt=paths.begin();pathIt!=paths.end();++pathIt){
        const auto pathObject=pathIt.value().toObject();
        for(const QString &method:methods){
            if(!pathObject.value(method).isObject()) continue;
            const auto operation=pathObject.value(method).toObject();
            QString operationName=operation.value(QStringLiteral("summary")).toString();
            if(operationName.isEmpty()) operationName=operation.value(QStringLiteral("operationId")).toString();
            if(operationName.isEmpty()) operationName=methodTitle(method)+QStringLiteral(" ")+pathIt.key();
            ImportedRequest request; request.name=operationName; request.method=method.toUpper(); request.path=pathIt.key();
            const auto parameters=operation.value(QStringLiteral("parameters")).toArray(); QStringList queryParams;
            for(const auto &paramValue:parameters){const auto param=paramValue.toObject(); const QString pname=param.value(QStringLiteral("name")).toString(); const QString where=param.value(QStringLiteral("in")).toString();
                if(pname.isEmpty()) continue;
                if(where==QStringLiteral("query")) queryParams.append(pname+QStringLiteral("="));
                else if(where==QStringLiteral("header")) { request.headers+=pname+QStringLiteral(": \n"); result.warnings.append(QStringLiteral("%1: completa manualmente la cabecera %2 antes de enviar.").arg(operationName,pname)); }
                else if(where==QStringLiteral("path")) result.warnings.append(QStringLiteral("%1: el parámetro de ruta {%2} requiere variable manual.").arg(operationName,pname));
            }
            if(!queryParams.isEmpty()){request.path+=request.path.contains(QLatin1Char('?'))?QLatin1Char('&'):QLatin1Char('?');request.path+=queryParams.join(QLatin1Char('&'));}
            const auto content=operation.value(QStringLiteral("requestBody")).toObject().value(QStringLiteral("content")).toObject();
            const auto jsonContent=content.value(QStringLiteral("application/json")).toObject();
            if(!jsonContent.isEmpty()){request.body=schemaSample(jsonContent.value(QStringLiteral("schema")).toObject());if(!request.body.isEmpty()) request.headers+=QStringLiteral("Content-Type: application/json\n");}
            if(!content.isEmpty()&&jsonContent.isEmpty()) result.warnings.append(QStringLiteral("%1: tipo de cuerpo no compatible; no importado.").arg(operationName));
            if(!base.isEmpty()) request.path=base+request.path;
            result.requests.append(request);
        }
    }
    if(root.value(QStringLiteral("components")).isObject()) result.warnings.append(QStringLiteral("Las referencias $ref no se expanden en esta importación inicial."));
    return result;
}
} // namespace reqflow::openapi
