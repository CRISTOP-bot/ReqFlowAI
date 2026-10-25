#include "reqflow/data/projectstore.h"

#include <QDir>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QUuid>
#include <QVariant>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QJsonValue>
#include <QJsonObject>
#include <QRegularExpression>
#include <QStringList>
#include <QUrl>
#include <QUrlQuery>

namespace {
const QRegularExpression sensitiveField(QStringLiteral("password|passwd|secret|token|authorization|cookie|api[_-]?key|client[_-]?secret"), QRegularExpression::CaseInsensitiveOption);
QString safeHistoryUrl(QString value) {
    QUrl url(value); if (!url.isValid()) return QStringLiteral("[invalid URL omitted]");
    url.setUserInfo(QString{}); QUrlQuery query(url); const auto items=query.queryItems(QUrl::FullyDecoded); query.clear();
    for(const auto &item:items) query.addQueryItem(item.first,sensitiveField.match(item.first).hasMatch()?QStringLiteral("[REDACTED]"):item.second);
    url.setQuery(query); return url.toString(QUrl::FullyEncoded);
}
bool jsonHasSensitiveKey(const QJsonValue &value) {
    if (value.isObject()) { const auto object=value.toObject(); for(auto it=object.begin();it!=object.end();++it) if(sensitiveField.match(it.key()).hasMatch()||jsonHasSensitiveKey(it.value())) return true; }
    if (value.isArray()) for(const auto &item:value.toArray()) if(jsonHasSensitiveKey(item)) return true;
    return false;
}
QJsonValue safeJson(const QJsonValue &value) {
    if(value.isObject()){QJsonObject out;const auto object=value.toObject();for(auto it=object.begin();it!=object.end();++it)out.insert(it.key(),sensitiveField.match(it.key()).hasMatch()?QJsonValue(QStringLiteral("[REDACTED]")):safeJson(it.value()));return out;}
    if(value.isArray()){QJsonArray out;for(const auto &item:value.toArray())out.append(safeJson(item));return out;}
    return value;
}
QString safeHistoryBody(QString value) {
    if(value.isEmpty()) return {};
    QJsonParseError error; const auto doc=QJsonDocument::fromJson(value.toUtf8(),&error);
    if(error.error!=QJsonParseError::NoError||doc.isNull()) return QStringLiteral("[non-JSON response omitted from history]");
    if(doc.isObject()) return QString::fromUtf8(QJsonDocument(safeJson(doc.object()).toObject()).toJson(QJsonDocument::Compact));
    return QString::fromUtf8(QJsonDocument(safeJson(doc.array()).toArray()).toJson(QJsonDocument::Compact));
}
QString safeHistoryHeaders(QString value) {
    QStringList out; for(const auto &line:value.split(QLatin1Char('\n'),Qt::SkipEmptyParts)){const int colon=line.indexOf(QLatin1Char(':'));if(colon<0){out.append(QStringLiteral("[malformed header omitted]"));continue;}const QString name=line.left(colon).trimmed();out.append(name+QStringLiteral(": ")+(sensitiveField.match(name).hasMatch()?QStringLiteral("[REDACTED]"):line.mid(colon+1).trimmed()));} return out.join(QLatin1Char('\n'));
}
}

namespace reqflow::data {
namespace {
QVariantMap rowToMap(const QSqlQuery &query) {
    QVariantMap row;
    const QSqlRecord record = query.record();
    for (int i = 0; i < record.count(); ++i)
        row.insert(record.fieldName(i), query.value(i));
    return row;
}
}

ProjectStore::ProjectStore(const QString &filePath) {
    connectionName_ = QStringLiteral("reqflow_%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    database_ = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName_);
    QString path = filePath;
    if (path.isEmpty()) {
        const QString base = QDir::home().filePath(QStringLiteral(".local/share/reqflow-ai"));
        QDir().mkpath(base);
        path = QDir(base).filePath(QStringLiteral("reqflow.sqlite3"));
    } else {
        QDir().mkpath(QFileInfo(path).absolutePath());
    }
    database_.setDatabaseName(path);
    if (!database_.open()) {
        lastError_ = database_.lastError().text();
        return;
    }
    initialize();
}

ProjectStore::~ProjectStore() {
    if (database_.isOpen()) database_.close();
    database_ = QSqlDatabase();
    QSqlDatabase::removeDatabase(connectionName_);
}

bool ProjectStore::isOpen() const { return database_.isOpen() && lastError_.isEmpty(); }
QString ProjectStore::lastError() const { return lastError_; }

bool ProjectStore::initialize() {
    const QStringList schema = {
        QStringLiteral("PRAGMA foreign_keys=ON"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS projects (id INTEGER PRIMARY KEY, name TEXT NOT NULL UNIQUE, created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP)"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS requests (id INTEGER PRIMARY KEY, project_id INTEGER NOT NULL REFERENCES projects(id) ON DELETE CASCADE, name TEXT NOT NULL, method TEXT NOT NULL, url TEXT NOT NULL, params TEXT NOT NULL DEFAULT '', headers TEXT NOT NULL DEFAULT '', body TEXT NOT NULL DEFAULT '', updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP)"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS history (id INTEGER PRIMARY KEY, project_id INTEGER NOT NULL REFERENCES projects(id) ON DELETE CASCADE, request_name TEXT NOT NULL, method TEXT NOT NULL, url TEXT NOT NULL, status INTEGER, elapsed_ms REAL NOT NULL, response_size INTEGER NOT NULL, response_headers TEXT NOT NULL, response_body TEXT NOT NULL, error TEXT NOT NULL DEFAULT '', created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP)"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS flows (id INTEGER PRIMARY KEY, project_id INTEGER NOT NULL REFERENCES projects(id) ON DELETE CASCADE, name TEXT NOT NULL, definition TEXT NOT NULL, version INTEGER NOT NULL DEFAULT 1, updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP, UNIQUE(project_id,name))"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS flow_runs (id INTEGER PRIMARY KEY, flow_id INTEGER NOT NULL REFERENCES flows(id) ON DELETE CASCADE, status TEXT NOT NULL, result TEXT NOT NULL, created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP)"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS environments (id INTEGER PRIMARY KEY, project_id INTEGER NOT NULL REFERENCES projects(id) ON DELETE CASCADE, name TEXT NOT NULL, variables_json TEXT NOT NULL, created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP, updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP, UNIQUE(project_id,name))"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS assertion_suites (id INTEGER PRIMARY KEY, project_id INTEGER NOT NULL REFERENCES projects(id) ON DELETE CASCADE, name TEXT NOT NULL, request_id INTEGER REFERENCES requests(id) ON DELETE SET NULL, definition TEXT NOT NULL, created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP, updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP, UNIQUE(project_id,name))"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS assertion_runs (id INTEGER PRIMARY KEY, suite_id INTEGER NOT NULL REFERENCES assertion_suites(id) ON DELETE CASCADE, status TEXT NOT NULL, result TEXT NOT NULL, created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP)")
    };
    for (const QString &statement : schema) {
        QSqlQuery query(database_);
        if (!query.exec(statement)) { lastError_ = query.lastError().text(); return false; }
    }
    QSqlQuery seed(database_);
    seed.prepare(QStringLiteral("INSERT OR IGNORE INTO projects(name) VALUES (?)"));
    seed.addBindValue(QStringLiteral("Mi proyecto"));
    if (!seed.exec()) { lastError_ = seed.lastError().text(); return false; }
    return true;
}

QVector<QVariantMap> ProjectStore::projects() const {
    QVector<QVariantMap> rows;
    QSqlQuery query(database_);
    if (!query.exec(QStringLiteral("SELECT id,name,created_at FROM projects ORDER BY name COLLATE NOCASE"))) return rows;
    while (query.next()) rows.append(rowToMap(query));
    return rows;
}

qint64 ProjectStore::createProject(const QString &name) {
    const QString normalized = name.trimmed();
    if (normalized.isEmpty()) { lastError_ = QStringLiteral("El proyecto necesita un nombre."); return -1; }
    QSqlQuery query(database_);
    query.prepare(QStringLiteral("INSERT INTO projects(name) VALUES (?)"));
    query.addBindValue(normalized);
    if (!query.exec()) { lastError_ = query.lastError().text(); return -1; }
    return query.lastInsertId().toLongLong();
}

QVector<QVariantMap> ProjectStore::requests(qint64 projectId) const {
    QVector<QVariantMap> rows;
    QSqlQuery query(database_);
    query.prepare(QStringLiteral("SELECT * FROM requests WHERE project_id=? ORDER BY name COLLATE NOCASE"));
    query.addBindValue(projectId);
    if (!query.exec()) return rows;
    while (query.next()) rows.append(rowToMap(query));
    return rows;
}

qint64 ProjectStore::saveRequest(qint64 projectId, qint64 requestId, const QVariantMap &request) {
    const QUrl requestUrl(request.value(QStringLiteral("url")).toString());
    if (!requestUrl.userName().isEmpty() || !requestUrl.password().isEmpty()) { lastError_ = QStringLiteral("No se persisten URL con credenciales embebidas."); return -1; }
    QUrlQuery urlQuery(requestUrl);
    for (const auto &item : urlQuery.queryItems(QUrl::FullyDecoded)) if (sensitiveField.match(item.first).hasMatch()) { lastError_ = QStringLiteral("No se persisten parámetros que parecen secretos."); return -1; }
    for (const QString &field : {QStringLiteral("headers"), QStringLiteral("params")}) {
        for (const QString &line : request.value(field).toString().split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
            const int separator = line.indexOf(field == QStringLiteral("headers") ? QLatin1Char(':') : QLatin1Char('='));
            if (separator > 0 && sensitiveField.match(line.left(separator).trimmed()).hasMatch()) { lastError_ = QStringLiteral("No se persisten cabeceras o parámetros sensibles."); return -1; }
        }
    }
    QJsonParseError bodyError; const auto bodyDoc = QJsonDocument::fromJson(request.value(QStringLiteral("body")).toString().toUtf8(), &bodyError);
    if (bodyError.error == QJsonParseError::NoError && bodyDoc.isObject()) {
        const auto body = bodyDoc.object(); for (auto it=body.begin(); it!=body.end(); ++it) if (sensitiveField.match(it.key()).hasMatch()) { lastError_ = QStringLiteral("No se persisten cuerpos JSON con campos sensibles."); return -1; }
    }
    if (bodyError.error == QJsonParseError::NoError && !bodyDoc.isNull() && jsonHasSensitiveKey(bodyDoc.isObject() ? QJsonValue(bodyDoc.object()) : QJsonValue(bodyDoc.array()))) { lastError_ = QStringLiteral("No se persisten cuerpos JSON con campos sensibles."); return -1; }
    const QStringList fields = {QStringLiteral("name"), QStringLiteral("method"), QStringLiteral("url"),
                                QStringLiteral("params"), QStringLiteral("headers"), QStringLiteral("body")};
    if (requestId > 0) {
        QSqlQuery query(database_);
        query.prepare(QStringLiteral("UPDATE requests SET name=?,method=?,url=?,params=?,headers=?,body=?,updated_at=CURRENT_TIMESTAMP WHERE id=? AND project_id=?"));
        for (const QString &field : fields) query.addBindValue(request.value(field));
        query.addBindValue(requestId); query.addBindValue(projectId);
        if (!query.exec()) { lastError_ = query.lastError().text(); return -1; }
        if (query.numRowsAffected() > 0) return requestId;
    }
    QSqlQuery query(database_);
    query.prepare(QStringLiteral("INSERT INTO requests(project_id,name,method,url,params,headers,body) VALUES (?,?,?,?,?,?,?)"));
    query.addBindValue(projectId);
    for (const QString &field : fields) query.addBindValue(request.value(field));
    if (!query.exec()) { lastError_ = query.lastError().text(); return -1; }
    return query.lastInsertId().toLongLong();
}

bool ProjectStore::deleteRequest(qint64 projectId, qint64 requestId) {
    QSqlQuery query(database_);
    query.prepare(QStringLiteral("DELETE FROM requests WHERE id=? AND project_id=?"));
    query.addBindValue(requestId); query.addBindValue(projectId);
    if (!query.exec()) { lastError_ = query.lastError().text(); return false; }
    return query.numRowsAffected() > 0;
}

qint64 ProjectStore::addHistory(qint64 projectId, const QVariantMap &item) {
    QVariantMap safe = item;
    safe.insert(QStringLiteral("url"), safeHistoryUrl(item.value(QStringLiteral("url")).toString()));
    safe.insert(QStringLiteral("response_headers"), safeHistoryHeaders(item.value(QStringLiteral("response_headers")).toString()));
    safe.insert(QStringLiteral("response_body"), safeHistoryBody(item.value(QStringLiteral("response_body")).toString()));
    safe.insert(QStringLiteral("error"), item.value(QStringLiteral("error")).toString().isEmpty() ? QString{} : QStringLiteral("[network error omitted from history]"));
    QSqlQuery query(database_);
    query.prepare(QStringLiteral("INSERT INTO history(project_id,request_name,method,url,status,elapsed_ms,response_size,response_headers,response_body,error) VALUES (?,?,?,?,?,?,?,?,?,?)"));
    query.addBindValue(projectId); query.addBindValue(safe.value(QStringLiteral("request_name")));
    query.addBindValue(safe.value(QStringLiteral("method"))); query.addBindValue(safe.value(QStringLiteral("url")));
    query.addBindValue(safe.value(QStringLiteral("status"))); query.addBindValue(safe.value(QStringLiteral("elapsed_ms")));
    query.addBindValue(safe.value(QStringLiteral("response_size"))); query.addBindValue(safe.value(QStringLiteral("response_headers")));
    query.addBindValue(safe.value(QStringLiteral("response_body"))); query.addBindValue(safe.value(QStringLiteral("error")));
    if (!query.exec()) { lastError_ = query.lastError().text(); return -1; }
    return query.lastInsertId().toLongLong();
}

QVector<QVariantMap> ProjectStore::history(qint64 projectId, int limit) const {
    QVector<QVariantMap> rows;
    QSqlQuery query(database_);
    query.prepare(QStringLiteral("SELECT * FROM history WHERE project_id=? ORDER BY id DESC LIMIT ?"));
    query.addBindValue(projectId); query.addBindValue(qMax(1, limit));
    if (!query.exec()) return rows;
    while (query.next()) rows.append(rowToMap(query));
    return rows;
}

qint64 ProjectStore::saveFlow(qint64 projectId, qint64 flowId, const QString &name, const QString &definition) {
    QJsonParseError flowError; const auto flowDoc=QJsonDocument::fromJson(definition.toUtf8(),&flowError);
    if(flowError.error!=QJsonParseError::NoError||!flowDoc.isObject()){ lastError_=QStringLiteral("La definición del flujo no es JSON válido."); return -1; }
    const auto nodes=flowDoc.object().value(QStringLiteral("nodes")).toArray();
    for(const auto &nodeValue:nodes){const auto node=nodeValue.toObject();const auto config=node.value(QStringLiteral("config")).toObject();
        if(jsonHasSensitiveKey(config)){lastError_=QStringLiteral("No se persiste la definición del flujo con claves potencialmente sensibles. Esto no sustituye una bóveda segura.");return -1;}
        const QString type=node.value(QStringLiteral("type")).toString();
        if(type==QStringLiteral("request")){QUrl url(config.value(QStringLiteral("url")).toString());QUrlQuery query(url);if(!url.userName().isEmpty()||!url.password().isEmpty()){lastError_=QStringLiteral("El flujo contiene userinfo en la URL.");return -1;}for(const auto &item:query.queryItems(QUrl::FullyDecoded))if(sensitiveField.match(item.first).hasMatch()){lastError_=QStringLiteral("El flujo contiene un parámetro URL potencialmente sensible.");return -1;}}
        const QString serialized=QString::fromUtf8(QJsonDocument(config).toJson(QJsonDocument::Compact));
        if(serialized.contains(QStringLiteral("Authorization:"),Qt::CaseInsensitive)||serialized.contains(QStringLiteral("Cookie:"),Qt::CaseInsensitive)){lastError_=QStringLiteral("El flujo contiene una cabecera potencialmente sensible.");return -1;}
    }
    if (flowId > 0) {
        QSqlQuery update(database_);
        update.prepare(QStringLiteral("UPDATE flows SET name=?,definition=?,version=version+1,updated_at=CURRENT_TIMESTAMP WHERE id=? AND project_id=?"));
        update.addBindValue(name.trimmed()); update.addBindValue(definition); update.addBindValue(flowId); update.addBindValue(projectId);
        if (!update.exec()) { lastError_ = update.lastError().text(); return -1; }
        if (update.numRowsAffected() > 0) return flowId;
    }
    QSqlQuery insert(database_);
    insert.prepare(QStringLiteral("INSERT INTO flows(project_id,name,definition) VALUES (?,?,?)"));
    insert.addBindValue(projectId); insert.addBindValue(name.trimmed()); insert.addBindValue(definition);
    if (!insert.exec()) { lastError_ = insert.lastError().text(); return -1; }
    return insert.lastInsertId().toLongLong();
}

QVector<QVariantMap> ProjectStore::flows(qint64 projectId) const {
    QVector<QVariantMap> rows;
    QSqlQuery query(database_);
    query.prepare(QStringLiteral("SELECT * FROM flows WHERE project_id=? ORDER BY name COLLATE NOCASE"));
    query.addBindValue(projectId);
    if (!query.exec()) return rows;
    while (query.next()) rows.append(rowToMap(query));
    return rows;
}

qint64 ProjectStore::addFlowRun(qint64 flowId, const QString &status, const QString &result) {
    QSqlQuery query(database_);
    query.prepare(QStringLiteral("INSERT INTO flow_runs(flow_id,status,result) VALUES (?,?,?)"));
    query.addBindValue(flowId); query.addBindValue(status); query.addBindValue(result);
    if (!query.exec()) { lastError_ = query.lastError().text(); return -1; }
    return query.lastInsertId().toLongLong();
}

qint64 ProjectStore::saveEnvironment(qint64 projectId, qint64 environmentId, const QString &name, const QString &variablesJson) {
    const QString normalized=name.trimmed();
    QJsonParseError error; const auto doc=QJsonDocument::fromJson(variablesJson.toUtf8(),&error);
    if(normalized.isEmpty()||error.error!=QJsonParseError::NoError||!doc.isObject()){lastError_=QStringLiteral("El entorno requiere nombre y un objeto JSON válido de variables.");return -1;}
    if(jsonHasSensitiveKey(doc.object())){lastError_=QStringLiteral("No guardo valores con nombres que parecen secretos; no hay bóveda del sistema disponible.");return -1;}
    if(environmentId>0){QSqlQuery update(database_);update.prepare(QStringLiteral("UPDATE environments SET name=?,variables_json=?,updated_at=CURRENT_TIMESTAMP WHERE id=? AND project_id=?"));update.addBindValue(normalized);update.addBindValue(QString::fromUtf8(QJsonDocument(doc.object()).toJson(QJsonDocument::Compact)));update.addBindValue(environmentId);update.addBindValue(projectId);if(!update.exec()){lastError_=update.lastError().text();return -1;}if(update.numRowsAffected()>0)return environmentId;}
    QSqlQuery insert(database_);insert.prepare(QStringLiteral("INSERT INTO environments(project_id,name,variables_json) VALUES (?,?,?)"));insert.addBindValue(projectId);insert.addBindValue(normalized);insert.addBindValue(QString::fromUtf8(QJsonDocument(doc.object()).toJson(QJsonDocument::Compact)));if(!insert.exec()){lastError_=insert.lastError().text();return -1;}return insert.lastInsertId().toLongLong();
}

QVector<QVariantMap> ProjectStore::environments(qint64 projectId) const {
    QVector<QVariantMap> rows;QSqlQuery query(database_);query.prepare(QStringLiteral("SELECT * FROM environments WHERE project_id=? ORDER BY name COLLATE NOCASE"));query.addBindValue(projectId);if(!query.exec())return rows;while(query.next())rows.append(rowToMap(query));return rows;
}

bool ProjectStore::deleteEnvironment(qint64 projectId, qint64 environmentId) {
    QSqlQuery query(database_);query.prepare(QStringLiteral("DELETE FROM environments WHERE id=? AND project_id=?"));query.addBindValue(environmentId);query.addBindValue(projectId);if(!query.exec()){lastError_=query.lastError().text();return false;}return query.numRowsAffected()>0;
}

qint64 ProjectStore::saveAssertionSuite(qint64 projectId, qint64 suiteId, const QString &name, qint64 requestId, const QString &definition) {
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(definition.toUtf8(), &error);
    if (name.trimmed().isEmpty() || error.error != QJsonParseError::NoError || !document.isArray()) {
        lastError_ = QStringLiteral("La suite requiere un nombre y un arreglo JSON de aserciones.");
        return -1;
    }
    for (const auto &value : document.array()) {
        if (!value.isObject()) { lastError_ = QStringLiteral("Cada aserción debe ser un objeto JSON."); return -1; }
        const auto spec = value.toObject();
        const QString type = spec.value(QStringLiteral("type")).toString();
        if (!QStringList{QStringLiteral("status"), QStringLiteral("header"), QStringLiteral("json_value"), QStringLiteral("json_type"), QStringLiteral("duration_ms")}.contains(type)) {
            lastError_ = QStringLiteral("Tipo de aserción no compatible: %1").arg(type); return -1;
        }
        if (type == QStringLiteral("header") && spec.value(QStringLiteral("header")).toString().isEmpty()) {
            lastError_ = QStringLiteral("Una aserción de cabecera requiere el campo header."); return -1;
        }
        if ((type == QStringLiteral("json_value") || type == QStringLiteral("json_type")) && spec.value(QStringLiteral("path")).toString().isEmpty()) {
            lastError_ = QStringLiteral("Una aserción JSON requiere el campo path."); return -1;
        }
    }
    const QString normalized = QString::fromUtf8(QJsonDocument(document.array()).toJson(QJsonDocument::Compact));
    if (suiteId > 0) {
        QSqlQuery update(database_);
        update.prepare(QStringLiteral("UPDATE assertion_suites SET name=?,request_id=?,definition=?,updated_at=CURRENT_TIMESTAMP WHERE id=? AND project_id=?"));
        update.addBindValue(name.trimmed());
        update.addBindValue(requestId > 0 ? QVariant(requestId) : QVariant{});
        update.addBindValue(normalized); update.addBindValue(suiteId); update.addBindValue(projectId);
        if (!update.exec()) { lastError_ = update.lastError().text(); return -1; }
        if (update.numRowsAffected() > 0) return suiteId;
    }
    QSqlQuery insert(database_);
    insert.prepare(QStringLiteral("INSERT INTO assertion_suites(project_id,name,request_id,definition) VALUES (?,?,?,?)"));
    insert.addBindValue(projectId); insert.addBindValue(name.trimmed());
    insert.addBindValue(requestId > 0 ? QVariant(requestId) : QVariant{});
    insert.addBindValue(normalized);
    if (!insert.exec()) { lastError_ = insert.lastError().text(); return -1; }
    return insert.lastInsertId().toLongLong();
}

QVector<QVariantMap> ProjectStore::assertionSuites(qint64 projectId) const {
    QVector<QVariantMap> rows;
    QSqlQuery query(database_);
    query.prepare(QStringLiteral("SELECT * FROM assertion_suites WHERE project_id=? ORDER BY name COLLATE NOCASE"));
    query.addBindValue(projectId);
    if (!query.exec()) return rows;
    while (query.next()) rows.append(rowToMap(query));
    return rows;
}

qint64 ProjectStore::addAssertionRun(qint64 suiteId, const QString &status, const QString &result) {
    QSqlQuery query(database_);
    query.prepare(QStringLiteral("INSERT INTO assertion_runs(suite_id,status,result) VALUES (?,?,?)"));
    query.addBindValue(suiteId); query.addBindValue(status); query.addBindValue(result);
    if (!query.exec()) { lastError_ = query.lastError().text(); return -1; }
    return query.lastInsertId().toLongLong();
}

} // namespace reqflow::data
