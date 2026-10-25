#pragma once

#include <QSqlDatabase>
#include <QString>
#include <QVariantMap>
#include <QVector>

namespace reqflow::data {

class ProjectStore final {
public:
    explicit ProjectStore(const QString &filePath = {});
    ~ProjectStore();
    ProjectStore(const ProjectStore &) = delete;
    ProjectStore &operator=(const ProjectStore &) = delete;

    [[nodiscard]] bool isOpen() const;
    [[nodiscard]] QString lastError() const;
    [[nodiscard]] QVector<QVariantMap> projects() const;
    qint64 createProject(const QString &name);
    [[nodiscard]] QVector<QVariantMap> requests(qint64 projectId) const;
    qint64 saveRequest(qint64 projectId, qint64 requestId, const QVariantMap &request);
    bool deleteRequest(qint64 projectId, qint64 requestId);
    qint64 addHistory(qint64 projectId, const QVariantMap &item);
    [[nodiscard]] QVector<QVariantMap> history(qint64 projectId, int limit = 100) const;
    qint64 saveFlow(qint64 projectId, qint64 flowId, const QString &name, const QString &definition);
    [[nodiscard]] QVector<QVariantMap> flows(qint64 projectId) const;
    qint64 addFlowRun(qint64 flowId, const QString &status, const QString &result);
    qint64 saveEnvironment(qint64 projectId, qint64 environmentId, const QString &name, const QString &variablesJson);
    [[nodiscard]] QVector<QVariantMap> environments(qint64 projectId) const;
    bool deleteEnvironment(qint64 projectId, qint64 environmentId);
    qint64 saveAssertionSuite(qint64 projectId, qint64 suiteId, const QString &name, qint64 requestId, const QString &definition);
    [[nodiscard]] QVector<QVariantMap> assertionSuites(qint64 projectId) const;
    qint64 addAssertionRun(qint64 suiteId, const QString &status, const QString &result);

private:
    bool initialize();
    QSqlDatabase database_;
    QString connectionName_;
    QString lastError_;
};

} // namespace reqflow::data
