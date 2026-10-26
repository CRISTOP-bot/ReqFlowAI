#include "reqflow/data/projectstore.h"
#include "reqflow/data/environment.h"
#include "reqflow/flow/flowengine.h"
#include "reqflow/network/httpclient.h"
#include "reqflow/openapi/openapiimporter.h"
#include "reqflow/security/secrets.h"
#include "reqflow/testing/assertions.h"
#include "reqflow/testing/jsondiff.h"

#include <QEventLoop>
#include <QFile>
#include <QHash>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonObject>
#include <QSet>
#include <QStringList>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTimer>
#include <algorithm>
#include <functional>
#include <QtTest>

#ifdef Q_OS_UNIX
#include <sys/stat.h>
#endif

namespace {
// Servidor HTTP mínimo para las pruebas de red: solo acepta conexiones locales
// y responde una vez por conexión con `Connection: close` salvo que se pida
// mantener el socket abierto (prueba del tope de tamaño de respuesta).
class TestHttpServer final : public QObject {
public:
    TestHttpServer() {
        QObject::connect(&server_, &QTcpServer::newConnection, this, [this] {
            while (QTcpSocket *socket = server_.nextPendingConnection()) {
                QObject::connect(socket, &QTcpSocket::readyRead, this, [this, socket] {
                    pending_[socket] += socket->readAll();
                    const QByteArray request = pending_[socket];
                    if (!request.contains("\r\n\r\n") || responded_.contains(socket)) return;
                    responded_.insert(socket);
                    responder(socket, request);
                    socket->flush();
                    if (!keepOpen) socket->disconnectFromHost();
                });
            }
        });
        server_.listen(QHostAddress::LocalHost, 0);
    }
    [[nodiscard]] bool isListening() const { return server_.isListening(); }
    [[nodiscard]] quint16 port() const { return server_.serverPort(); }
    std::function<void(QTcpSocket *, const QByteArray &)> responder = [](QTcpSocket *socket, const QByteArray &) {
        socket->write(QByteArray("HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Length: 4\r\n\r\nhola"));
    };
    bool keepOpen = false;
private:
    QTcpServer server_;
    QHash<QTcpSocket *, QByteArray> pending_;
    QSet<QTcpSocket *> responded_;
};

// Envía una petición con HttpClient y devuelve la respuesta tras salir del bucle
// de eventos (con un tope de seguridad de 30 s para no colgar la suite).
reqflow::network::HttpResponse fetchResponse(const reqflow::network::HttpRequest &request,
                                             const reqflow::network::RedirectVerifier &verifier = {}) {
    reqflow::network::HttpClient client;
    reqflow::network::HttpResponse result;
    QEventLoop loop;
    QObject::connect(&client, &reqflow::network::HttpClient::finished, &loop,
                     [&](const reqflow::network::HttpResponse &response) { result = response; loop.quit(); });
    QTimer::singleShot(30000, &loop, &QEventLoop::quit);
    Q_UNUSED(client.send(request, verifier));
    loop.exec();
    return result;
}
} // namespace

class ReqFlowCoreTests final : public QObject {
    Q_OBJECT
private slots:
    void responseHeaderLookupIsCaseInsensitive() {
        reqflow::network::HttpResponse response;
        response.headers.append(qMakePair(QByteArray("Content-Type"), QByteArray("application/json")));
        QCOMPARE(response.header("content-type"), QByteArray("application/json"));
        QCOMPARE(response.header("missing"), QByteArray());
    }

    void flowValidationRejectsCyclesAndAcceptsLinearGraph() {
        reqflow::flow::FlowDefinition flow;
        flow.name = QStringLiteral("Health check");
        flow.nodes = {{QStringLiteral("start"), QStringLiteral("start"), {}},
                      {QStringLiteral("request"), QStringLiteral("request"), {{QStringLiteral("url"), QStringLiteral("https://example.test")}}},
                      {QStringLiteral("result"), QStringLiteral("result"), {}}};
        flow.edges = {{QStringLiteral("start"), QStringLiteral("request"), QStringLiteral("next")},
                      {QStringLiteral("request"), QStringLiteral("result"), QStringLiteral("next")}};
        QVERIFY2(reqflow::flow::validate(flow).isEmpty(), qPrintable(reqflow::flow::validate(flow).join('\n')));
        flow.edges.append({QStringLiteral("result"), QStringLiteral("request"), QStringLiteral("next")});
        QVERIFY(!reqflow::flow::validate(flow).isEmpty());
    }

    void flowEngineExecutesTransformAndResultNodes() {
        reqflow::flow::FlowDefinition flow;
        flow.name = QStringLiteral("Transform test");
        flow.nodes = {{QStringLiteral("start"), QStringLiteral("start"), {}},
                      {QStringLiteral("make"), QStringLiteral("transform"), {{QStringLiteral("variable"), QStringLiteral("greeting")}, {QStringLiteral("template"), QStringLiteral("hello {{name}}")}}},
                      {QStringLiteral("result"), QStringLiteral("result"), {{QStringLiteral("message"), QStringLiteral("{{greeting}}")}}}};
        flow.edges = {{QStringLiteral("start"), QStringLiteral("make"), QStringLiteral("next")},
                      {QStringLiteral("make"), QStringLiteral("result"), QStringLiteral("next")}};
        reqflow::flow::FlowEngine engine;
        const auto run = engine.run(flow, {{QStringLiteral("name"), QStringLiteral("ReqFlow")}});
        QCOMPARE(run.status, QStringLiteral("success"));
        QCOMPARE(run.variables.value(QStringLiteral("greeting")).toString(), QStringLiteral("hello ReqFlow"));
        QCOMPARE(run.logs.size(), 3);
    }

    void openApiImporterBuildsRequestsWithoutExecutingThem() {
        const QByteArray spec = "{\"openapi\":\"3.0.0\",\"servers\":[{\"url\":\"https://api.example.test\"}],\"paths\":{\"/health\":{\"get\":{\"summary\":\"Health\",\"parameters\":[{\"name\":\"verbose\",\"in\":\"query\"}]}}}}";
        const auto result = reqflow::openapi::parseOpenApiJson(spec);
        QVERIFY2(result.error.isEmpty(), qPrintable(result.error));
        QCOMPARE(result.requests.size(), 1);
        QCOMPARE(result.requests.first().method, QStringLiteral("GET"));
        QCOMPARE(result.requests.first().path, QStringLiteral("https://api.example.test/health?verbose="));
    }

    void flowRequestValidationErrorsNeverSendHttp() {
        auto runInvalidRequest = [](QJsonObject config, const QVariantMap &inputs) {
            reqflow::flow::FlowDefinition flow;
            flow.name = QStringLiteral("Must not send");
            flow.nodes = {{QStringLiteral("start"), QStringLiteral("start"), {}},
                          {QStringLiteral("request"), QStringLiteral("request"), std::move(config)},
                          {QStringLiteral("result"), QStringLiteral("result"), {}}};
            flow.edges = {{QStringLiteral("start"), QStringLiteral("request"), QStringLiteral("next")},
                          {QStringLiteral("request"), QStringLiteral("result"), QStringLiteral("next")}};
            reqflow::flow::FlowEngine engine;
            return engine.run(flow, inputs);
        };
        const auto secret = runInvalidRequest({{QStringLiteral("url"), QStringLiteral("http://127.0.0.1:1/should-not-connect")},
                                               {QStringLiteral("headers"), QStringLiteral("X-Api-Token: hidden")}}, {});
        QCOMPARE(secret.status, QStringLiteral("failed"));
        QVERIFY(secret.error.contains(QStringLiteral("credenciales")));
        QCOMPARE(secret.logs.size(), 2);
        QCOMPARE(secret.logs.last().status, QStringLiteral("failed"));

        const auto missingVariable = runInvalidRequest({{QStringLiteral("url"), QStringLiteral("http://127.0.0.1:1/{{missing}}")}}, {});
        QCOMPARE(missingVariable.status, QStringLiteral("failed"));
        QVERIFY(missingVariable.error.contains(QStringLiteral("no definida")));
        QCOMPARE(missingVariable.logs.size(), 2);

        const auto malformedUrl = runInvalidRequest({{QStringLiteral("url"), QStringLiteral("file:///etc/passwd")}}, {});
        QCOMPARE(malformedUrl.status, QStringLiteral("failed"));
        QVERIFY(malformedUrl.error.contains(QStringLiteral("URL HTTP/HTTPS")));

        const auto invalidMethod = runInvalidRequest({{QStringLiteral("url"), QStringLiteral("http://127.0.0.1:1/")},
                                                      {QStringLiteral("method"), QStringLiteral("TRACE")}}, {});
        QCOMPARE(invalidMethod.status, QStringLiteral("failed"));
        QVERIFY(invalidMethod.error.contains(QStringLiteral("Método HTTP")));

        const auto malformedHeaders = runInvalidRequest({{QStringLiteral("url"), QStringLiteral("http://127.0.0.1:1/")},
                                                         {QStringLiteral("headers"), QStringLiteral("MissingColon")}}, {});
        QCOMPARE(malformedHeaders.status, QStringLiteral("failed"));
        QVERIFY(malformedHeaders.error.contains(QStringLiteral("Cabecera HTTP")));

        const auto missingBodyVariable = runInvalidRequest({{QStringLiteral("url"), QStringLiteral("http://127.0.0.1:1/")},
                                                            {QStringLiteral("body"), QStringLiteral("{{not_defined}}")}}, {});
        QCOMPARE(missingBodyVariable.status, QStringLiteral("failed"));
        QVERIFY(missingBodyVariable.error.contains(QStringLiteral("no definida")));
    }

    void openApiImporterResolvesLocalRefsAndWarnsOnUnsupportedRefs() {
        const QByteArray spec = "{\"openapi\":\"3.0.3\",\"paths\":{\"/health\":{\"$ref\":\"#/components/pathItems/Health\"},\"/outside\":{\"get\":{\"$ref\":\"https://example.test/openapi.json#/paths/~1health/get\"}}},\"components\":{\"pathItems\":{\"Health\":{\"get\":{\"$ref\":\"#/components/operations/Health\"}}},\"operations\":{\"Health\":{\"summary\":\"Health check\"}}}}";
        const auto imported = reqflow::openapi::parseOpenApiJson(spec);
        QVERIFY2(imported.error.isEmpty(), qPrintable(imported.error));
        QCOMPARE(imported.requests.size(), 1);
        QCOMPARE(imported.requests.first().name, QStringLiteral("Health check"));
        QVERIFY(imported.warnings.join(QLatin1Char('\n')).contains(QStringLiteral("referencia $ref externa, transitiva o no resoluble")));
    }

    void openApiImporterRejectsInvalidDocumentsAndParsesYaml() {
        const auto badJson = reqflow::openapi::parseOpenApiJson(QByteArray("{not-json"));
        QVERIFY(!badJson.error.isEmpty());
        const auto unsupportedVersion = reqflow::openapi::parseOpenApiJson(QByteArray("{\"openapi\":\"2.0\",\"paths\":{}}"));
        QVERIFY(unsupportedVersion.requests.isEmpty());
        QVERIFY(!unsupportedVersion.error.isEmpty());
        // Nota: sin raw string multilínea; el preprocesador de moc (Qt 6.12) no
        // entiende raw strings que ocupan varias líneas y omite la clase entera.
        const QByteArray yaml =
            "openapi: 3.0.3\n"
            "servers:\n"
            "  - url: https://api.example.test\n"
            "paths:\n"
            "  /health:\n"
            "    get:\n"
            "      summary: Health\n";
        const auto imported = reqflow::openapi::parseOpenApiYaml(yaml);
        QVERIFY2(imported.error.isEmpty(), qPrintable(imported.error));
        QCOMPARE(imported.requests.size(), 1);
        QCOMPARE(imported.requests.first().path, QStringLiteral("https://api.example.test/health"));
        const auto invalidYaml = reqflow::openapi::parseOpenApiYaml(QByteArray("openapi: [unclosed"));
        QVERIFY(!invalidYaml.error.isEmpty());
    }


    void flowJsonRoundTrip() {
        reqflow::flow::FlowDefinition flow;
        flow.name = QStringLiteral("Tiny");
        flow.nodes.append({QStringLiteral("s"), QStringLiteral("start"), {}});
        const auto restored = reqflow::flow::fromJson(reqflow::flow::toJson(flow));
        QCOMPARE(restored.name, flow.name);
        QCOMPARE(restored.nodes.size(), 1);
        QCOMPARE(restored.nodes.first().type, QStringLiteral("start"));
    }

    void environmentTemplateExpansionIsStrictAndNonMutating() {
        const QJsonObject variables{{QStringLiteral("host"), QStringLiteral("https://api.example.test")},
                                    {QStringLiteral("version"), 2}};
        const QString input = QStringLiteral("{{host}}/v{{version}}/{{host}}");
        QString output, error;
        QVERIFY(reqflow::data::expandTemplate(input, variables, &output, &error));
        QCOMPARE(output, QStringLiteral("https://api.example.test/v2/https://api.example.test"));
        QCOMPARE(input, QStringLiteral("{{host}}/v{{version}}/{{host}}"));
        QVERIFY(!reqflow::data::expandTemplate(QStringLiteral("{{missing}}"), variables, &output, &error));
        QVERIFY(error.contains(QStringLiteral("missing")));
    }

    void responseAssertionsEvaluateTypedChecks() {
        reqflow::network::HttpResponse response;
        response.status = 201; response.elapsedMs = 12;
        response.headers.append(qMakePair(QByteArray("Content-Type"), QByteArray("application/json")));
        response.body = QByteArray("{\"data\":{\"id\":7,\"active\":true},\"items\":[\"a\",\"b\"]}");
        const QJsonArray definitions{
            QJsonObject{{QStringLiteral("name"), QStringLiteral("created")}, {QStringLiteral("type"), QStringLiteral("status")}, {QStringLiteral("expected"), QStringLiteral("201")}},
            QJsonObject{{QStringLiteral("name"), QStringLiteral("content type")}, {QStringLiteral("type"), QStringLiteral("header")}, {QStringLiteral("header"), QStringLiteral("content-type")}, {QStringLiteral("expected"), QStringLiteral("application/json")}},
            QJsonObject{{QStringLiteral("name"), QStringLiteral("id")}, {QStringLiteral("type"), QStringLiteral("json_value")}, {QStringLiteral("path"), QStringLiteral("$.data.id")}, {QStringLiteral("expected"), QStringLiteral("7")}},
            QJsonObject{{QStringLiteral("name"), QStringLiteral("active boolean")}, {QStringLiteral("type"), QStringLiteral("json_type")}, {QStringLiteral("path"), QStringLiteral("$.data.active")}, {QStringLiteral("expected"), QStringLiteral("boolean")}},
            QJsonObject{{QStringLiteral("name"), QStringLiteral("fast")}, {QStringLiteral("type"), QStringLiteral("duration_ms")}, {QStringLiteral("expected"), QStringLiteral("50")}, {QStringLiteral("operator"), QStringLiteral("less_than")}}
        };
        const auto results = reqflow::testing::evaluateAssertions(definitions, response);
        QCOMPARE(results.size(), 5);
        for (const auto &result : results) QVERIFY2(result.passed, qPrintable(result.name + QStringLiteral(": ") + result.actual));
        const auto missing = reqflow::testing::evaluateAssertions(QJsonArray{QJsonObject{{QStringLiteral("type"), QStringLiteral("json_value")}, {QStringLiteral("path"), QStringLiteral("$.missing")}, {QStringLiteral("expected"), QStringLiteral("x")}}}, response);
        QVERIFY(!missing.first().passed);
    }

    void jsonDiffReportsAddedRemovedModifiedAndLimits() {
        const QJsonObject before{{QStringLiteral("name"),QStringLiteral("Alex")},{QStringLiteral("age"),30},{QStringLiteral("active"),true},{QStringLiteral("items"),QJsonArray{1,2}}};
        const QJsonObject after{{QStringLiteral("name"),QStringLiteral("Alexandra")},{QStringLiteral("age"),30},{QStringLiteral("city"),QStringLiteral("Pánuco")},{QStringLiteral("items"),QJsonArray{1,3,4}}};
        const auto diff=reqflow::testing::compareJson(before,after);
        QStringList paths;for(const auto &change:diff.changes)paths.append(change.path);
        QVERIFY(paths.contains(QStringLiteral("$.name")));
        QVERIFY(paths.contains(QStringLiteral("$.active")));
        QVERIFY(paths.contains(QStringLiteral("$.city")));
        QVERIFY(paths.contains(QStringLiteral("$.items[1]")));
        QVERIFY(paths.contains(QStringLiteral("$.items[2]")));
        QCOMPARE(diff.changes.size(),5);
        const auto limited=reqflow::testing::compareJson(before,after,2,48);
        QCOMPARE(limited.changes.size(),2);QVERIFY(limited.truncated);
        const auto unchanged=reqflow::testing::compareJson(before,before);
        QVERIFY(unchanged.changes.isEmpty());QVERIFY(!unchanged.truncated);
    }

    void jsonDiffUsesUnambiguousPathsForSpecialObjectKeys() {
        const QJsonObject before{{QStringLiteral("user.name"),1},{QStringLiteral("quote'key"),2},{QStringLiteral("slash\\\\key"),3},
                                 {QStringLiteral("user"),QJsonObject{{QStringLiteral("name"),QStringLiteral("Alex")}}}};
        const QJsonObject after{{QStringLiteral("user.name"),4},{QStringLiteral("quote'key"),5},{QStringLiteral("slash\\\\key"),6},
                                {QStringLiteral("user"),QJsonObject{{QStringLiteral("name"),QStringLiteral("Sam")}}}};
        const auto diff=reqflow::testing::compareJson(before,after);
        QStringList paths;for(const auto &change:diff.changes)paths.append(change.path);
        QVERIFY(paths.contains(QStringLiteral("$['user.name']")));
        QVERIFY(paths.contains(QStringLiteral("$['quote\\'key']")));
        QVERIFY(paths.contains(QStringLiteral("$['slash\\\\\\\\key']")));
        QVERIFY(paths.contains(QStringLiteral("$.user.name")));
        QCOMPARE(paths.size(),4);
    }

    void localAccountUsesPersistentPbkdf2VerifierAndRejectsInvalidCredentials() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.filePath(QStringLiteral("auth.sqlite3"));
        const QString username = QStringLiteral("  Cristopher  ");
        const QString password = QStringLiteral("a-strong-local-passphrase-42");
        {
            reqflow::data::ProjectStore store(path);
            QVERIFY2(store.isOpen(), qPrintable(store.lastError()));
            QVERIFY(!store.hasLocalAccount());
            QVERIFY(store.localAccountError().isEmpty());
            QVERIFY(!store.createLocalAccount(QStringLiteral("ab"), password));
            QVERIFY(!store.createLocalAccount(QStringLiteral("Cristopher"), QStringLiteral("short")));
            QVERIFY(!store.createLocalAccount(QStringLiteral("Cristopher"), QString(1025, QLatin1Char('x'))));
            QVERIFY(!store.createLocalAccount(QStringLiteral("bad user"), password));
            QVERIFY(store.createLocalAccount(username, password));
            QVERIFY(store.hasLocalAccount());
            QVERIFY(store.verifyLocalAccount(QStringLiteral("cristopher"), password));
            QVERIFY(!store.verifyLocalAccount(QStringLiteral("someone-else"), password));
            QVERIFY(!store.verifyLocalAccount(QStringLiteral("cristopher"), QStringLiteral("wrong-password-123")));
            QVERIFY(!store.createLocalAccount(QStringLiteral("another"), password));
            QVERIFY(!store.localAccountError().isEmpty());
        }
        {
            reqflow::data::ProjectStore reopened(path);
            QVERIFY2(reopened.isOpen(), qPrintable(reopened.lastError()));
            QVERIFY(reopened.hasLocalAccount());
            QVERIFY(reopened.verifyLocalAccount(QStringLiteral("CRISTOPHER"), password));
        }
    }

    void sqliteProjectRequestEnvironmentAndSecretFiltering() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        reqflow::data::ProjectStore store(directory.filePath(QStringLiteral("nested/test.sqlite3")));
        QVERIFY2(store.isOpen(), qPrintable(store.lastError()));
        QCOMPARE(store.projects().size(), 1);
        const qint64 project = store.createProject(QStringLiteral("API local"));
        QVERIFY(project > 0);
        QVariantMap request;
        request.insert(QStringLiteral("name"), QStringLiteral("Health"));
        request.insert(QStringLiteral("method"), QStringLiteral("GET"));
        request.insert(QStringLiteral("url"), QStringLiteral("https://example.test/health"));
        request.insert(QStringLiteral("params"), QString{}); request.insert(QStringLiteral("headers"), QString{}); request.insert(QStringLiteral("body"), QString{});
        const qint64 requestId = store.saveRequest(project, -1, request);
        QVERIFY(requestId > 0);
        QCOMPARE(store.requests(project).size(), 1);
        request.insert(QStringLiteral("name"), QStringLiteral("Health v2"));
        QCOMPARE(store.saveRequest(project, requestId, request), requestId);
        QVERIFY(store.deleteRequest(project, requestId));
        QVERIFY(store.requests(project).isEmpty());
        const qint64 env = store.saveEnvironment(project, -1, QStringLiteral("Local"), QStringLiteral("{\"base_url\":\"http://localhost\"}"));
        QVERIFY(env > 0);
        QCOMPARE(store.environments(project).size(), 1);
        QVERIFY(store.saveEnvironment(project, -1, QStringLiteral("Secrets"), QStringLiteral("{\"api_token\":\"no\"}")) < 0);
        const qint64 suiteId = store.saveAssertionSuite(project, -1, QStringLiteral("Basic"), -1,
            QStringLiteral("[{\"type\":\"status\",\"expected\":\"200\"}]"));
        QVERIFY(suiteId > 0);
        QCOMPARE(store.assertionSuites(project).size(), 1);
        QVERIFY(store.addAssertionRun(suiteId, QStringLiteral("passed"), QStringLiteral("[]")) > 0);
        QVERIFY(store.saveAssertionSuite(project, -1, QStringLiteral("Bad"), -1, QStringLiteral("[{\"type\":\"unknown\"}]")) < 0);
        request.insert(QStringLiteral("headers"), QStringLiteral("Authorization: Bearer secret-value"));
        QVERIFY(store.saveRequest(project, -1, request) < 0);
        const qint64 flowId = store.saveFlow(project, -1, QStringLiteral("Smoke"), QStringLiteral("{}"));
        QVERIFY(flowId > 0);
        QVERIFY(store.addFlowRun(flowId, QStringLiteral("success"), QStringLiteral("{\"steps\":2}")) > 0);
        QVariantMap history;
        history.insert(QStringLiteral("request_name"), QStringLiteral("Health")); history.insert(QStringLiteral("method"), QStringLiteral("GET"));
        history.insert(QStringLiteral("url"), QStringLiteral("https://example.test/path?access_token=secret-value"));
        history.insert(QStringLiteral("status"), 200); history.insert(QStringLiteral("elapsed_ms"), 4); history.insert(QStringLiteral("response_size"), 40);
        history.insert(QStringLiteral("response_headers"), QStringLiteral("Set-Cookie: session=secret-value\nContent-Type: application/json"));
        history.insert(QStringLiteral("response_body"), QString::fromUtf8("{\"access_token\":\"secret-value\",\"ok\":true}"));
        history.insert(QStringLiteral("error"), QStringLiteral("some network error"));
        QVERIFY(store.addHistory(project, history) > 0);
        const auto saved = store.history(project).first();
        QVERIFY(!saved.value(QStringLiteral("url")).toString().contains(QStringLiteral("secret-value")));
        QVERIFY(!saved.value(QStringLiteral("response_headers")).toString().contains(QStringLiteral("secret-value")));
        QVERIFY(!saved.value(QStringLiteral("response_body")).toString().contains(QStringLiteral("secret-value")));
        QVERIFY(!saved.value(QStringLiteral("error")).toString().contains(QStringLiteral("some network error")));
    }

    // ---- Pruebas de la auditoría de seguridad 2026-10-26 (docs/SECURITY_AUDIT.md) ----

    void storeRejectsUnsafeUrlSchemesButAllowsTemplates() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        reqflow::data::ProjectStore store(directory.filePath(QStringLiteral("schemes.sqlite3")));
        QVERIFY2(store.isOpen(), qPrintable(store.lastError()));
        const qint64 project = store.createProject(QStringLiteral("Esquemas"));
        QVERIFY(project > 0);
        QVariantMap request;
        request.insert(QStringLiteral("name"), QStringLiteral("R"));
        request.insert(QStringLiteral("method"), QStringLiteral("GET"));
        request.insert(QStringLiteral("params"), QString{});
        request.insert(QStringLiteral("headers"), QString{});
        request.insert(QStringLiteral("body"), QString{});
        // B4: los esquemas no http(s) no se persisten.
        const QStringList unsafeUrls = {QStringLiteral("ftp://example.test/file"),
                                        QStringLiteral("file:///etc/passwd"),
                                        QStringLiteral("javascript:alert(1)"),
                                        QStringLiteral("data:text/plain,hi")};
        for (const QString &unsafeUrl : unsafeUrls) {
            request.insert(QStringLiteral("url"), unsafeUrl);
            QVERIFY2(store.saveRequest(project, -1, request) < 0, qPrintable(unsafeUrl));
            QVERIFY(store.lastError().contains(QStringLiteral("http")));
        }
        // B4: plantillas y rutas relativas siguen permitidas (importación OpenAPI sin servers).
        request.insert(QStringLiteral("url"), QStringLiteral("{{base_url}}/health"));
        QVERIFY2(store.saveRequest(project, -1, request) > 0, qPrintable(store.lastError()));
        request.insert(QStringLiteral("url"), QStringLiteral("/health"));
        QVERIFY2(store.saveRequest(project, -1, request) > 0, qPrintable(store.lastError()));
        // saveFlow aplica el mismo criterio a los nodos de petición.
        QVERIFY(store.saveFlow(project, -1, QStringLiteral("Bad"),
                               QStringLiteral("{\"nodes\":[{\"id\":\"r\",\"type\":\"request\",\"config\":{\"method\":\"GET\",\"url\":\"javascript:alert(1)\"}}]}")) < 0);
        // El importador ignora con advertencia una base de servidor no http(s).
        const auto imported = reqflow::openapi::parseOpenApiJson(
            QByteArray("{\"openapi\":\"3.0.0\",\"servers\":[{\"url\":\"ftp://files.example.test\"}],\"paths\":{\"/health\":{\"get\":{}}}}"));
        QVERIFY2(imported.error.isEmpty(), qPrintable(imported.error));
        QCOMPARE(imported.requests.size(), 1);
        QCOMPARE(imported.requests.first().path, QStringLiteral("/health"));
        QVERIFY(imported.warnings.join(QLatin1Char('\n')).contains(QStringLiteral("no usa http")));
    }

    void secretDetectionUnifiesNamesAndCatchesCredentialValues() {
        namespace security = reqflow::security;
        // M3: el patrón único reconoce nombres sensibles reales…
        QVERIFY(security::nameLooksSensitive(QStringLiteral("Authorization")));
        QVERIFY(security::nameLooksSensitive(QStringLiteral("Authentication")));
        QVERIFY(security::nameLooksSensitive(QStringLiteral("X-Api-Token")));
        QVERIFY(security::nameLooksSensitive(QStringLiteral("api_key")));
        QVERIFY(security::nameLooksSensitive(QStringLiteral("client-secret")));
        QVERIFY(security::nameLooksSensitive(QStringLiteral("Cookie")));
        QVERIFY(security::nameLooksSensitive(QStringLiteral("Session-Id")));
        // …sin rechazar cabeceras inocuas que variantes antiguas casaban mal.
        QVERIFY(!security::nameLooksSensitive(QStringLiteral("Author")));
        QVERIFY(!security::nameLooksSensitive(QStringLiteral("Content-Type")));
        QVERIFY(!security::nameLooksSensitive(QStringLiteral("X-Oauth-Trial")));
        QVERIFY(!security::nameLooksSensitive(QStringLiteral("X-Request-Id")));
        // M3: valores con forma de credencial aunque el nombre parezca inocuo.
        QVERIFY(security::valueLooksLikeCredential(QStringLiteral("Bearer abcdefghijklmn")));
        QVERIFY(security::valueLooksLikeCredential(QStringLiteral("Basic dXNlcjpwYXNzd29yZA==")));
        QVERIFY(security::valueLooksLikeCredential(QStringLiteral("sk-1234567890abcdef")));
        QVERIFY(security::valueLooksLikeCredential(QStringLiteral("ghp_abcdefghijklmnopqrstuv")));
        QVERIFY(security::valueLooksLikeCredential(QStringLiteral("AKIAAAAAAAAAAAAAAAAA")));
        QVERIFY(security::valueLooksLikeCredential(QStringLiteral("eyJhbGciOiJIUzI1NiJ9.eyJzdWIiOiIxIn0.c2lnbmF0dXJl")));
        QVERIFY(!security::valueLooksLikeCredential(QStringLiteral("plain text")));
        QVERIFY(!security::valueLooksLikeCredential(QStringLiteral("200")));
        // M3: cuerpos no JSON (form-urlencoded, etiquetas XML, atributos).
        QVERIFY(security::textBodyLooksSensitive(QStringLiteral("user=bob&password=hunter2")));
        QVERIFY(security::textBodyLooksSensitive(QStringLiteral("<token>abc</token>")));
        QVERIFY(security::textBodyLooksSensitive(QStringLiteral("<user name=\"password\">bob</user>")));
        QVERIFY(!security::textBodyLooksSensitive(QStringLiteral("user=bob&role=admin")));
        QVERIFY(!security::textBodyLooksSensitive(QStringLiteral("<name>bob</name>")));
        // M3: JSON con claves sensibles o con valores con forma de credencial.
        QVERIFY(security::jsonLooksSensitive(QJsonObject{{QStringLiteral("ok"), true}, {QStringLiteral("token"), QStringLiteral("x")}}));
        QVERIFY(security::jsonLooksSensitive(QJsonObject{{QStringLiteral("data"), QStringLiteral("Bearer abcdefghijklmn")}, {QStringLiteral("ok"), true}}));
        QVERIFY(!security::jsonLooksSensitive(QJsonObject{{QStringLiteral("name"), QStringLiteral("bob")}, {QStringLiteral("id"), 7}}));
        QVERIFY(security::headerLinesLookSensitive(QStringLiteral("X-Custom: Bearer abcdefghijklmn")));
        QVERIFY(!security::headerLinesLookSensitive(QStringLiteral("Content-Type: application/json")));
    }

    void historyRedactsByValueAndCapsBodySize() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        reqflow::data::ProjectStore store(directory.filePath(QStringLiteral("history.sqlite3")));
        QVERIFY2(store.isOpen(), qPrintable(store.lastError()));
        const qint64 project = store.createProject(QStringLiteral("Historial"));
        QVERIFY(project > 0);
        QVariantMap history;
        history.insert(QStringLiteral("request_name"), QStringLiteral("Health"));
        history.insert(QStringLiteral("method"), QStringLiteral("GET"));
        history.insert(QStringLiteral("url"),
                       QStringLiteral("https://example.test/path?sig=eyJhbGciOiJIUzI1NiJ9.eyJzdWIiOiIxIn0.c2lnbmF0dXJl"));
        history.insert(QStringLiteral("status"), 200);
        history.insert(QStringLiteral("elapsed_ms"), 5);
        history.insert(QStringLiteral("response_size"), 42);
        history.insert(QStringLiteral("response_headers"),
                       QStringLiteral("X-Custom: Bearer abcdefghijklmn\nContent-Type: application/json"));
        history.insert(QStringLiteral("response_body"),
                       QStringLiteral("{\"data\":\"sk-abcdefghijklmnop\",\"ok\":true}"));
        history.insert(QStringLiteral("error"), QString{});
        QVERIFY(store.addHistory(project, history) > 0);
        const auto saved = store.history(project).first();
        // M3: redacción por valor aunque el nombre del parámetro/cabecera sea inocuo.
        QVERIFY(!saved.value(QStringLiteral("url")).toString().contains(QStringLiteral("c2lnbmF0dXJl")));
        QVERIFY(saved.value(QStringLiteral("url")).toString().contains(QStringLiteral("[REDACTED]")));
        QVERIFY(!saved.value(QStringLiteral("response_headers")).toString().contains(QStringLiteral("abcdefghij")));
        QVERIFY(saved.value(QStringLiteral("response_headers")).toString().contains(QStringLiteral("Content-Type: application/json")));
        QVERIFY(!saved.value(QStringLiteral("response_body")).toString().contains(QStringLiteral("sk-abcdefghijklmnop")));
        QVERIFY(saved.value(QStringLiteral("response_body")).toString().contains(QStringLiteral("\"ok\":true")));
        // M1: los cuerpos por encima del tope se sustituyen por un marcador.
        history.insert(QStringLiteral("response_body"), QString(reqflow::data::kMaxHistoryBodyChars + 1, QLatin1Char('x')));
        QVERIFY(store.addHistory(project, history) > 0);
        bool foundCapped = false;
        for (const auto &entry : store.history(project))
            if (entry.value(QStringLiteral("response_body")).toString().contains(QStringLiteral("supera el límite")))
                foundCapped = true;
        QVERIFY(foundCapped);
    }

    void yamlBillionLaughsIsRejectedWithinNodeBudget() {
        // B3: «billion laughs» — un documento de menos de 1 KiB con aliases
        // anidados que al convertirse a JSON pediría decenas de millones de
        // visitas (medido en la auditoría: 2,6 s con 8 niveles sin presupuesto).
        QString bomb = QStringLiteral("a0: &a0 [\"x\",\"y\"]\n");
        for (int level = 1; level < 8; ++level) {
            bomb += QStringLiteral("a%1: &a%1 [").arg(level);
            for (int repeat = 0; repeat < 10; ++repeat) {
                if (repeat) bomb += QLatin1Char(',');
                bomb += QStringLiteral("*a%1").arg(level - 1);
            }
            bomb += QStringLiteral("]\n");
        }
        QVERIFY(bomb.size() < 1024);
        const auto imported = reqflow::openapi::parseOpenApiYaml(bomb.toUtf8());
        QVERIFY(!imported.error.isEmpty());
        QVERIFY2(imported.error.contains(QStringLiteral("presupuesto")), qPrintable(imported.error));
        QVERIFY(imported.requests.isEmpty());
    }

    void httpClientAbortsResponsesOverSizeLimit() {
        TestHttpServer server;
        QVERIFY(server.isListening());
        // El servidor anuncia 12 MiB pero solo envía 64 KiB y mantiene el
        // socket abierto: el cliente debe abortar por su cuenta (M1).
        server.keepOpen = true;
        server.responder = [](QTcpSocket *socket, const QByteArray &) {
            socket->write(QByteArray("HTTP/1.1 200 OK\r\nContent-Type: application/octet-stream\r\nConnection: close\r\nContent-Length: ")
                          + QByteArray::number(12 * 1024 * 1024) + "\r\n\r\n");
            socket->write(QByteArray(64 * 1024, 'A'));
        };
        reqflow::network::HttpRequest request;
        request.method = "GET";
        request.url = QUrl(QStringLiteral("http://127.0.0.1:%1/big").arg(server.port()));
        request.timeoutMs = 15000;
        const auto response = fetchResponse(request);
        QVERIFY(!response.error.isEmpty());
        QVERIFY2(response.error.contains(QStringLiteral("10 MiB")), qPrintable(response.error));
        QVERIFY(response.body.isEmpty());
    }

    void httpClientFollowsSameOriginRedirectsAndBlocksCrossOrigin() {
        TestHttpServer origin;
        TestHttpServer other;
        QVERIFY(origin.isListening());
        QVERIFY(other.isListening());
        origin.responder = [](QTcpSocket *socket, const QByteArray &head) {
            if (head.startsWith("GET /hop ")) {
                socket->write(QByteArray("HTTP/1.1 302 Found\r\nConnection: close\r\nLocation: /final\r\nContent-Length: 0\r\n\r\n"));
                return;
            }
            socket->write(QByteArray("HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Length: 2\r\n\r\nok"));
        };
        other.responder = [](QTcpSocket *socket, const QByteArray &) {
            socket->write(QByteArray("HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Length: 5\r\n\r\nother"));
        };
        const QUrl originBase(QStringLiteral("http://127.0.0.1:%1").arg(origin.port()));
        const QUrl otherBase(QStringLiteral("http://127.0.0.1:%1").arg(other.port()));

        // B2: mismo origen (Location relativo) se sigue sin verificador.
        reqflow::network::HttpRequest sameOrigin;
        sameOrigin.method = "GET";
        sameOrigin.url = originBase.resolved(QUrl(QStringLiteral("/hop")));
        sameOrigin.timeoutMs = 10000;
        const auto followed = fetchResponse(sameOrigin);
        QVERIFY2(followed.error.isEmpty(), qPrintable(followed.error));
        QCOMPARE(followed.body, QByteArray("ok"));

        // B2: sin verificador, saltar de origen se rechaza y no se sigue.
        origin.responder = [&other](QTcpSocket *socket, const QByteArray &) {
            socket->write(QByteArray("HTTP/1.1 302 Found\r\nConnection: close\r\nLocation: http://127.0.0.1:")
                          + QByteArray::number(other.port()) + "/final\r\nContent-Length: 0\r\n\r\n");
        };
        reqflow::network::HttpRequest crossOrigin;
        crossOrigin.method = "GET";
        crossOrigin.url = originBase.resolved(QUrl(QStringLiteral("/hop")));
        crossOrigin.timeoutMs = 10000;
        const auto declined = fetchResponse(crossOrigin);
        QVERIFY2(declined.error.contains(QStringLiteral("Redirección rechazada")), qPrintable(declined.error));
        QVERIFY(declined.body.isEmpty());

        // B2: un verificador (en la UI, diálogo del usuario) puede permitir el salto.
        QUrl askedFrom, askedTo;
        const auto accepted = fetchResponse(crossOrigin, [&](const QUrl &from, const QUrl &to) {
            askedFrom = from;
            askedTo = to;
            return to.toString().startsWith(otherBase.toString());
        });
        QVERIFY(!askedFrom.isEmpty());
        QCOMPARE(askedTo, otherBase.resolved(QUrl(QStringLiteral("/final"))));
        QVERIFY2(accepted.error.isEmpty(), qPrintable(accepted.error));
        QCOMPARE(accepted.body, QByteArray("other"));
    }

    void httpHeaderValidationFollowsRfc7230() {
        using reqflow::network::isValidHeaderName;
        using reqflow::network::isValidHeaderValue;
        // B1: nombre = tchar de la RFC 7230.
        QVERIFY(isValidHeaderName(QStringLiteral("X-Api-Token")));
        QVERIFY(isValidHeaderName(QStringLiteral("Content-Type")));
        QVERIFY(isValidHeaderName(QStringLiteral("!#$%&'*+.^_`|~09Az-")));
        QVERIFY(!isValidHeaderName(QString{}));
        QVERIFY(!isValidHeaderName(QStringLiteral("Bad Header")));
        QVERIFY(!isValidHeaderName(QStringLiteral("Bad:Name")));
        QVERIFY(!isValidHeaderName(QStringLiteral("Nombre\nInyectado")));
        // B1: valor sin CR/LF/NUL ni otros caracteres de control; obs-text admitido.
        QVERIFY(isValidHeaderValue(QStringLiteral("valor con espacios")));
        QVERIFY(isValidHeaderValue(QStringLiteral("a\tb")));
        QVERIFY(isValidHeaderValue(QString::fromUtf8("obs-\xC3\xA9-text")));
        QVERIFY(!isValidHeaderValue(QStringLiteral("a\rb")));
        QVERIFY(!isValidHeaderValue(QStringLiteral("a\nb")));
        QVERIFY(!isValidHeaderValue(QString(QChar::Null)));
        QVERIFY(!isValidHeaderValue(QStringLiteral("a\x7F")));
    }

#ifdef Q_OS_UNIX
    void databaseFilePermissionsAreOwnerOnly() {
        // M2: sin el arreglo la base nacía 0644 (umask 022); ahora es 0600.
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.filePath(QStringLiteral("perms/reqflow.sqlite3"));
        {
            reqflow::data::ProjectStore store(path);
            QVERIFY2(store.isOpen(), qPrintable(store.lastError()));
        }
        struct stat info {};
        QCOMPARE(::stat(QFile::encodeName(path).constData(), &info), 0);
        QCOMPARE(static_cast<unsigned>(info.st_mode & 0777), 0600u);
    }
#endif
};

QTEST_GUILESS_MAIN(ReqFlowCoreTests)
#include "test_core.moc"
