#include "reqflow/data/projectstore.h"
#include "reqflow/data/environment.h"
#include "reqflow/flow/flowengine.h"
#include "reqflow/network/httpclient.h"
#include "reqflow/openapi/openapiimporter.h"
#include "reqflow/testing/assertions.h"
#include "reqflow/testing/jsondiff.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QtTest>

class ReqFlowCoreTests final : public QObject {
    Q_OBJECT
private slots:
    void responseHeaderLookupIsCaseInsensitive() {
        reqflow::network::HttpResponse response;
        response.headers.append(qMakePair(QByteArray("Content-Type"), QByteArray("application/json")));
        QCOMPARE(response.header("content-type"), QByteArray("application/json"));
        QCOMPARE(response.header("missing"), QByteArray{});
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
        const QByteArray spec = R"({"openapi":"3.0.0","servers":[{"url":"https://api.example.test"}],"paths":{"/health":{"get":{"summary":"Health","parameters":[{"name":"verbose","in":"query"}]}}}})";
        const auto result = reqflow::openapi::parseOpenApiJson(spec);
        QVERIFY2(result.error.isEmpty(), qPrintable(result.error));
        QCOMPARE(result.requests.size(), 1);
        QCOMPARE(result.requests.first().method, QStringLiteral("GET"));
        QCOMPARE(result.requests.first().path, QStringLiteral("https://api.example.test/health?verbose="));
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
        response.body = QByteArray(R"({"data":{"id":7,"active":true},"items":["a","b"]})");
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
        const qint64 env = store.saveEnvironment(project, -1, QStringLiteral("Local"), QStringLiteral(R"({"base_url":"http://localhost"})"));
        QVERIFY(env > 0);
        QCOMPARE(store.environments(project).size(), 1);
        QVERIFY(store.saveEnvironment(project, -1, QStringLiteral("Secrets"), QStringLiteral(R"({"api_token":"no"})")) < 0);
        const qint64 suiteId = store.saveAssertionSuite(project, -1, QStringLiteral("Basic"), -1,
            QStringLiteral(R"([{"type":"status","expected":"200"}])"));
        QVERIFY(suiteId > 0);
        QCOMPARE(store.assertionSuites(project).size(), 1);
        QVERIFY(store.addAssertionRun(suiteId, QStringLiteral("passed"), QStringLiteral("[]")) > 0);
        QVERIFY(store.saveAssertionSuite(project, -1, QStringLiteral("Bad"), -1, QStringLiteral(R"([{"type":"unknown"}])")) < 0);
        request.insert(QStringLiteral("headers"), QStringLiteral("Authorization: Bearer secret-value"));
        QVERIFY(store.saveRequest(project, -1, request) < 0);
        const qint64 flowId = store.saveFlow(project, -1, QStringLiteral("Smoke"), QStringLiteral("{}"));
        QVERIFY(flowId > 0);
        QVERIFY(store.addFlowRun(flowId, QStringLiteral("success"), QStringLiteral(R"({"steps":2})")) > 0);
        QVariantMap history;
        history.insert(QStringLiteral("request_name"), QStringLiteral("Health")); history.insert(QStringLiteral("method"), QStringLiteral("GET"));
        history.insert(QStringLiteral("url"), QStringLiteral("https://example.test/path?access_token=secret-value"));
        history.insert(QStringLiteral("status"), 200); history.insert(QStringLiteral("elapsed_ms"), 4); history.insert(QStringLiteral("response_size"), 40);
        history.insert(QStringLiteral("response_headers"), QStringLiteral("Set-Cookie: session=secret-value\nContent-Type: application/json"));
        history.insert(QStringLiteral("response_body"), QString::fromUtf8(R"({"access_token":"secret-value","ok":true})"));
        history.insert(QStringLiteral("error"), QStringLiteral("some network error"));
        QVERIFY(store.addHistory(project, history) > 0);
        const auto saved = store.history(project).first();
        QVERIFY(!saved.value(QStringLiteral("url")).toString().contains(QStringLiteral("secret-value")));
        QVERIFY(!saved.value(QStringLiteral("response_headers")).toString().contains(QStringLiteral("secret-value")));
        QVERIFY(!saved.value(QStringLiteral("response_body")).toString().contains(QStringLiteral("secret-value")));
        QVERIFY(!saved.value(QStringLiteral("error")).toString().contains(QStringLiteral("some network error")));
    }
};

QTEST_GUILESS_MAIN(ReqFlowCoreTests)
#include "test_core.moc"
