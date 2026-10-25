#include "reqflow/flow/flowengine.h"

#include <QElapsedTimer>
#include <QEventLoop>
#include <QMap>
#include <QList>
#include <algorithm>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonValue>
#include <QRegularExpression>
#include <QSet>
#include <QTimer>
#include <QUrlQuery>

namespace reqflow::flow {
namespace {
const QSet<QString> kNodeTypes = {QStringLiteral("start"), QStringLiteral("request"),
    QStringLiteral("extract"), QStringLiteral("transform"), QStringLiteral("condition"), QStringLiteral("result")};

QVariant lookupPath(QVariant value, const QString &path) {
    if (path == QStringLiteral("$") || path.isEmpty()) return value;
    if (!path.startsWith(QLatin1Char('$'))) return {};
    static const QRegularExpression token(QStringLiteral("\\.([A-Za-z_][A-Za-z0-9_-]*)|\\[(\\d+)\\]|\\['([^']+)'\\]|\\[\\\"([^\\\"]+)\\\"\\]"));
    const QString remainder = path.mid(1);
    auto matches = token.globalMatch(remainder);
    QVariant current = value;
    int consumed = 0;
    while (matches.hasNext()) {
        const auto match = matches.next();
        if (match.capturedStart() != consumed) return {};
        consumed = match.capturedEnd();
        const QString key = !match.captured(1).isEmpty() ? match.captured(1) :
                            !match.captured(2).isEmpty() ? match.captured(2) :
                            !match.captured(3).isEmpty() ? match.captured(3) : match.captured(4);
        if (!match.captured(2).isEmpty()) {
            bool ok = false; const int index = key.toInt(&ok);
            const QVariantList list = current.toList();
            if (!ok || index < 0 || index >= list.size()) return {};
            current = list.at(index);
        } else {
            const QVariantMap map = current.toMap();
            if (!map.contains(key)) return {};
            current = map.value(key);
        }
    }
    return consumed == remainder.size() ? current : QVariant{};
}

QByteArray substitute(const QString &text, const QVariantMap &vars, QString *error) {
    static const QRegularExpression variable(QStringLiteral("\\{\\{\\s*([A-Za-z_][A-Za-z0-9_.-]*)\\s*\\}\\}"));
    QString output = text;
    QList<QRegularExpressionMatch> matches;
    auto iterator = variable.globalMatch(text);
    while (iterator.hasNext()) matches.append(iterator.next());
    for (const auto &match : matches) {
        const QString key = match.captured(1);
        if (!vars.contains(key)) { *error = QStringLiteral("Variable no definida: %1").arg(key); return {}; }
    }
    for (auto it = matches.crbegin(); it != matches.crend(); ++it)
        output.replace(it->capturedStart(), it->capturedLength(), vars.value(it->captured(1)).toString());
    if (output.contains(QStringLiteral("{{")) || output.contains(QStringLiteral("}}"))) {
        *error = QStringLiteral("Plantilla mal formada; se esperaba {{identificador}}.");
        return {};
    }
    return output.toUtf8();
}

QJsonValue redactedJson(const QJsonValue &value) {
    static const QRegularExpression sensitive(QStringLiteral("password|passwd|secret|token|authorization|auth|cookie|api[_-]?key|client[_-]?secret|credential|private[_-]?key"), QRegularExpression::CaseInsensitiveOption);
    if (value.isObject()) {
        QJsonObject output;
        const auto object = value.toObject();
        for (auto it = object.begin(); it != object.end(); ++it)
            output.insert(it.key(), sensitive.match(it.key()).hasMatch() ? QJsonValue(QStringLiteral("[REDACTED]")) : redactedJson(it.value()));
        return output;
    }
    if (value.isArray()) {
        QJsonArray output;
        for (const auto &item : value.toArray()) output.append(redactedJson(item));
        return output;
    }
    return value;
}

QString redactRequestConfig(const QJsonObject &config) {
    static const QRegularExpression sensitive(QStringLiteral("password|passwd|secret|token|authorization|auth|cookie|api[_-]?key|client[_-]?secret|credential|private[_-]?key"), QRegularExpression::CaseInsensitiveOption);
    QJsonObject safe = config;
    for (auto it = safe.begin(); it != safe.end(); ++it) {
        if (sensitive.match(it.key()).hasMatch()) it.value() = QStringLiteral("[REDACTED]");
        else if (it.key() == QStringLiteral("url")) {
            QUrl url(it.value().toString());
            if (url.isValid()) {
                url.setUserInfo(QString{});
                QUrlQuery query(url);
                const auto items = query.queryItems(QUrl::FullyDecoded);
                query.clear();
                for (const auto &item : items) query.addQueryItem(item.first, sensitive.match(item.first).hasMatch() ? QStringLiteral("[REDACTED]") : item.second);
                url.setQuery(query);
                it.value() = url.toString(QUrl::FullyEncoded);
            }
        }
    }
    return QString::fromUtf8(QJsonDocument(safe).toJson(QJsonDocument::Compact));
}

bool containsSecretValue(const QJsonValue &value, const QString &fieldName = {}) {
    static const QRegularExpression sensitiveName(QStringLiteral("password|passwd|secret|token|authorization|auth|cookie|api[_-]?key|client[_-]?secret|credential|private[_-]?key"), QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression credentialValue(QStringLiteral("\\b(?:Bearer|Basic)\\s+[A-Za-z0-9._~+/=-]{8,}|\\bsk-[A-Za-z0-9_-]{12,}|\\b(?:gh[pousr]_[A-Za-z0-9]{20,}|AKIA[A-Z0-9]{16})\\b"), QRegularExpression::CaseInsensitiveOption);
    if (!fieldName.isEmpty() && sensitiveName.match(fieldName).hasMatch()) return true;
    if (value.isObject()) {
        const auto object = value.toObject();
        for (auto it = object.begin(); it != object.end(); ++it)
            if (containsSecretValue(it.value(), it.key())) return true;
    } else if (value.isArray()) {
        for (const auto &item : value.toArray())
            if (containsSecretValue(item)) return true;
    } else if (value.isString()) {
        const QString text = value.toString();
        if (credentialValue.match(text).hasMatch()) return true;
        if (fieldName.compare(QStringLiteral("url"), Qt::CaseInsensitive) == 0) {
            const QUrl url(text);
            if (!url.userInfo().isEmpty()) return true;
            const QUrlQuery query(url);
            for (const auto &item : query.queryItems(QUrl::FullyDecoded))
                if (sensitiveName.match(item.first).hasMatch()) return true;
        }
        if (fieldName.compare(QStringLiteral("headers"), Qt::CaseInsensitive) == 0) {
            for (const QString &line : text.split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
                const int colon = line.indexOf(QLatin1Char(':'));
                if (colon > 0 && sensitiveName.match(line.left(colon)).hasMatch()) return true;
            }
        }
    }
    return false;
}

bool containsSecret(const QJsonObject &config) {
    return containsSecretValue(config);
}
}

FlowDefinition fromJson(const QJsonObject &json) {
    FlowDefinition flow;
    flow.name = json.value(QStringLiteral("name")).toString();
    flow.version = json.value(QStringLiteral("version")).toInt(1);
    for (const auto &value : json.value(QStringLiteral("nodes")).toArray()) {
        const auto object = value.toObject();
        flow.nodes.append({object.value(QStringLiteral("id")).toString(), object.value(QStringLiteral("type")).toString(), object.value(QStringLiteral("config")).toObject()});
    }
    for (const auto &value : json.value(QStringLiteral("edges")).toArray()) {
        const auto object = value.toObject();
        flow.edges.append({object.value(QStringLiteral("source")).toString(), object.value(QStringLiteral("target")).toString(), object.value(QStringLiteral("label")).toString()});
    }
    return flow;
}

QJsonObject toJson(const FlowDefinition &flow) {
    QJsonArray nodes;
    for (const auto &node : flow.nodes) nodes.append(QJsonObject{{QStringLiteral("id"), node.id}, {QStringLiteral("type"), node.type}, {QStringLiteral("config"), node.config}});
    QJsonArray edges;
    for (const auto &edge : flow.edges) edges.append(QJsonObject{{QStringLiteral("source"), edge.source}, {QStringLiteral("target"), edge.target}, {QStringLiteral("label"), edge.label}});
    return {{QStringLiteral("name"), flow.name}, {QStringLiteral("version"), flow.version}, {QStringLiteral("nodes"), nodes}, {QStringLiteral("edges"), edges}};
}

QStringList validate(const FlowDefinition &flow) {
    QStringList errors;
    if (flow.name.trimmed().isEmpty()) errors.append(QStringLiteral("El flujo necesita nombre."));
    if (flow.nodes.isEmpty() || flow.nodes.size() > 200) { errors.append(QStringLiteral("El flujo requiere entre 1 y 200 nodos.")); return errors; }
    QMap<QString, FlowNode> byId;
    int starts = 0, results = 0;
    for (const auto &node : flow.nodes) {
        if (node.id.trimmed().isEmpty() || byId.contains(node.id)) errors.append(QStringLiteral("IDs de nodo vacíos o duplicados: %1").arg(node.id));
        byId.insert(node.id, node);
        if (!kNodeTypes.contains(node.type)) errors.append(QStringLiteral("Tipo de nodo no compatible: %1").arg(node.type));
        if (node.type == QStringLiteral("start")) ++starts;
        if (node.type == QStringLiteral("result")) ++results;
        if (node.type == QStringLiteral("request") && node.config.value(QStringLiteral("url")).toString().trimmed().isEmpty()) errors.append(QStringLiteral("Nodo %1: falta URL.").arg(node.id));
        if ((node.type == QStringLiteral("extract") || node.type == QStringLiteral("transform") || node.type == QStringLiteral("condition")) && node.config.value(QStringLiteral("variable")).toString().isEmpty()) errors.append(QStringLiteral("Nodo %1: falta variable.").arg(node.id));
    }
    if (starts != 1) errors.append(QStringLiteral("Debe existir exactamente un nodo de inicio."));
    if (results < 1) errors.append(QStringLiteral("Agrega un nodo de resultado."));
    QMap<QString, QStringList> graph;
    QMap<QString, int> indegree;
    QMap<QString, QStringList> conditionLabels;
    for (const auto &id : byId.keys()) indegree[id] = 0;
    for (const auto &edge : flow.edges) {
        if (!byId.contains(edge.source) || !byId.contains(edge.target)) { errors.append(QStringLiteral("Conexión a nodo inexistente.")); continue; }
        graph[edge.source].append(edge.target); ++indegree[edge.target];
        if (byId.value(edge.source).type == QStringLiteral("condition")) conditionLabels[edge.source].append(edge.label);
    }
    for (const auto &node : flow.nodes) {
        if (node.type != QStringLiteral("condition")) continue;
        const auto labels = conditionLabels.value(node.id);
        if (labels.size() != 2 || !labels.contains(QStringLiteral("true")) || !labels.contains(QStringLiteral("false")))
            errors.append(QStringLiteral("Condición %1 necesita salida true y false.").arg(node.id));
    }
    QStringList queue;
    for (auto it = indegree.cbegin(); it != indegree.cend(); ++it) if (it.value() == 0) queue.append(it.key());
    int visited = 0;
    while (!queue.isEmpty()) {
        const QString id = queue.takeLast(); ++visited;
        for (const QString &target : graph.value(id)) if (--indegree[target] == 0) queue.append(target);
    }
    if (visited != byId.size()) errors.append(QStringLiteral("El grafo contiene ciclos."));
    if (starts == 1) {
        const QString start = std::find_if(flow.nodes.cbegin(), flow.nodes.cend(), [](const FlowNode &node) { return node.type == QStringLiteral("start"); })->id;
        QSet<QString> reachable; QStringList pending{start};
        while (!pending.isEmpty()) { const auto id = pending.takeLast(); if (reachable.contains(id)) continue; reachable.insert(id); pending.append(graph.value(id)); }
        for (const auto &id : byId.keys()) if (!reachable.contains(id)) errors.append(QStringLiteral("Nodo no alcanzable: %1").arg(id));
    }
    return errors;
}

FlowRun FlowEngine::run(const FlowDefinition &flow, const QVariantMap &inputs,
                        const std::atomic_bool *cancel, int maxSteps) const {
    FlowRun run; run.variables = inputs;
    static const QRegularExpression sensitiveInput(QStringLiteral("password|passwd|secret|token|authorization|auth|cookie|api[_-]?key|client[_-]?secret|credential|private[_-]?key"), QRegularExpression::CaseInsensitiveOption);
    for (auto it = run.variables.begin(); it != run.variables.end(); ++it)
        if (sensitiveInput.match(it.key()).hasMatch()) it.value() = QStringLiteral("[REDACTED]");
    const QStringList errors = validate(flow);
    if (!errors.isEmpty()) { run.status = QStringLiteral("invalid"); run.error = errors.join(QStringLiteral("; ")); return run; }
    QMap<QString, FlowNode> nodes;
    QMap<QString, QList<FlowEdge>> outgoing;
    for (const auto &node : flow.nodes) nodes.insert(node.id, node);
    for (const auto &edge : flow.edges) outgoing[edge.source].append(edge);
    QString current;
    for (const auto &node : flow.nodes) if (node.type == QStringLiteral("start")) current = node.id;
    network::HttpClient client;
    int steps = 0;
    while (!current.isEmpty()) {
        if (cancel && cancel->load()) { run.status = QStringLiteral("cancelled"); return run; }
        if (++steps > maxSteps) { run.status = QStringLiteral("failed"); run.error = QStringLiteral("Límite de pasos excedido."); return run; }
        const auto node = nodes.value(current);
        QElapsedTimer timer; timer.start(); QJsonObject output; QString error;
        QList<FlowEdge> next = outgoing.value(current);
        const auto config = node.config;
        if (node.type == QStringLiteral("request")) {
            network::HttpRequest request;
            if (containsSecret(config)) error = QStringLiteral("Este nodo parece contener credenciales y se bloqueó: no hay bóveda segura.");
            const QString method = config.value(QStringLiteral("method")).toString(QStringLiteral("GET")).trimmed().toUpper();
            static const QSet<QString> allowedMethods{QStringLiteral("GET"), QStringLiteral("POST"), QStringLiteral("PUT"), QStringLiteral("PATCH"), QStringLiteral("DELETE"), QStringLiteral("HEAD"), QStringLiteral("OPTIONS")};
            if (error.isEmpty() && !allowedMethods.contains(method)) error = QStringLiteral("Método HTTP no permitido.");
            request.method = method.toLatin1();
            QByteArray expandedUrl;
            if (error.isEmpty()) expandedUrl = substitute(config.value(QStringLiteral("url")).toString(), run.variables, &error);
            if (error.isEmpty()) {
                request.url = QUrl(QString::fromUtf8(expandedUrl));
                if (!request.url.isValid() || (request.url.scheme() != QStringLiteral("http") && request.url.scheme() != QStringLiteral("https")) || request.url.host().isEmpty())
                    error = QStringLiteral("URL HTTP/HTTPS no válida en el nodo de petición.");
            }
            if (error.isEmpty()) request.body = substitute(config.value(QStringLiteral("body")).toString(), run.variables, &error);
            request.timeoutMs = qBound(1000, config.value(QStringLiteral("timeout_ms")).toInt(20000), 120000);
            if (error.isEmpty()) {
                const QString headerText = QString::fromUtf8(substitute(config.value(QStringLiteral("headers")).toString(), run.variables, &error));
                if (error.isEmpty()) {
                    static const QRegularExpression headerName(QStringLiteral("^[!#$%&'*+.^_`|~0-9A-Za-z-]+$"));
                    for (const QString &line : headerText.split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
                        const int colon = line.indexOf(QLatin1Char(':'));
                        const QString name = colon > 0 ? line.left(colon).trimmed() : QString{};
                        const QString value = colon > 0 ? line.mid(colon + 1).trimmed() : QString{};
                        if (colon <= 0 || !headerName.match(name).hasMatch() || value.contains(QLatin1Char('\r')) || value.contains(QChar::Null)) {
                            error = QStringLiteral("Cabecera HTTP no válida en el nodo de petición.");
                            break;
                        }
                        request.headers.insert(name.toLatin1(), value.toLatin1());
                    }
                }
            }
            network::HttpResponse response;
            if (error.isEmpty()) {
                QEventLoop loop;
                const auto connection = QObject::connect(&client, &network::HttpClient::finished, &loop, [&](const network::HttpResponse &value) { response = value; loop.quit(); });
                client.send(request);
                loop.exec();
                QObject::disconnect(connection);
            }
            if (error.isEmpty() && !response.error.isEmpty()) error = response.error;
            if (error.isEmpty()) {
                run.variables.insert(QStringLiteral("response.status"), response.status);
                run.variables.insert(QStringLiteral("response.body"), QStringLiteral("[REDACTED: body omitted from persistent flow state]"));
                run.variables.insert(QStringLiteral("response.elapsed_ms"), response.elapsedMs);
                const QJsonDocument responseDocument = QJsonDocument::fromJson(response.body);
                if (responseDocument.isObject()) run.variables.insert(QStringLiteral("response.json"), redactedJson(responseDocument.object()).toObject().toVariantMap());
                else if (responseDocument.isArray()) run.variables.insert(QStringLiteral("response.json"), redactedJson(responseDocument.array()).toArray().toVariantList());
                else run.variables.insert(QStringLiteral("response.json"), QVariant{});
                output.insert(QStringLiteral("status"), response.status); output.insert(QStringLiteral("elapsed_ms"), response.elapsedMs); output.insert(QStringLiteral("size_bytes"), response.sizeBytes());
                const int expected = config.value(QStringLiteral("expected_status")).toInt(0);
                if (expected && response.status != expected) error = QStringLiteral("HTTP %1; esperado %2").arg(response.status).arg(expected);
            }
        } else if (node.type == QStringLiteral("extract")) {
            const auto source = config.value(QStringLiteral("source")).toString(QStringLiteral("response.json"));
            const auto value = lookupPath(run.variables.value(source), config.value(QStringLiteral("path")).toString(QStringLiteral("$")));
            if (!value.isValid()) error = QStringLiteral("No se encontró la ruta JSON.");
            else { const auto name=config.value(QStringLiteral("variable")).toString(); run.variables.insert(name,value); output.insert(name,QJsonValue::fromVariant(value)); }
        } else if (node.type == QStringLiteral("transform")) {
            const auto name=config.value(QStringLiteral("variable")).toString(); const auto value=substitute(config.value(QStringLiteral("template")).toString(),run.variables,&error); run.variables.insert(name,QString::fromUtf8(value)); output.insert(name,QString::fromUtf8(value));
        } else if (node.type == QStringLiteral("condition")) {
            const auto name=config.value(QStringLiteral("variable")).toString(); const auto actual=run.variables.value(name); const auto expected=config.value(QStringLiteral("value")).toVariant(); const auto op=config.value(QStringLiteral("operator")).toString(QStringLiteral("equals")); bool passed=false;
            if(op==QStringLiteral("equals")) passed=actual.toString()==expected.toString(); else if(op==QStringLiteral("not_equals")) passed=actual.toString()!=expected.toString(); else if(op==QStringLiteral("contains")) passed=actual.toString().contains(expected.toString()); else if(op==QStringLiteral("greater_than")) passed=actual.toDouble()>expected.toDouble(); else if(op==QStringLiteral("less_than")) passed=actual.toDouble()<expected.toDouble(); else if(op==QStringLiteral("exists")) passed=actual.isValid()&&!actual.isNull(); else error=QStringLiteral("Operador no compatible.");
            const QString label=passed?QStringLiteral("true"):QStringLiteral("false"); next.erase(std::remove_if(next.begin(),next.end(),[&](const FlowEdge &edge){return edge.label!=label;}),next.end()); output.insert(QStringLiteral("condition"),label);
        } else if (node.type == QStringLiteral("result")) {
            const auto message=substitute(config.value(QStringLiteral("message")).toString(QStringLiteral("Flujo terminado")),run.variables,&error); output.insert(QStringLiteral("message"),QString::fromUtf8(message));
        }
        run.logs.append({node.id,node.type,error.isEmpty()?QStringLiteral("success"):QStringLiteral("failed"),timer.elapsed(),error.isEmpty()?QStringLiteral("Paso completado."):error,output});
        if (!error.isEmpty()) { run.status=QStringLiteral("failed"); run.error=error; return run; }
        if (node.type == QStringLiteral("result")) break;
        if (next.size()>1) { run.status=QStringLiteral("failed"); run.error=QStringLiteral("Bifurcación ambigua."); return run; }
        if (next.isEmpty()) { run.status=QStringLiteral("failed"); run.error=QStringLiteral("El flujo llegó a un nodo terminal sin nodo de resultado."); return run; }
        current=next.first().target;
    }
    run.status=QStringLiteral("success");
    return run;
}

} // namespace reqflow::flow
