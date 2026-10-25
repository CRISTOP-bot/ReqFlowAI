#pragma once

#include "reqflow/data/projectstore.h"
#include "reqflow/network/httpclient.h"

#include <QComboBox>
#include <QJsonArray>
#include <QMainWindow>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSqlDatabase>
#include <QTableWidget>
#include <QTabWidget>
#include <QLineEdit>
#include <QLabel>
#include <QTreeWidget>
#include <QTextDocument>

namespace reqflow::ui {

class MainWindow final : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);

private slots:
    void sendRequest();
    void saveRequest();
    void loadSavedRequest(int index);
    void createProject();
    void openFlowEditor();
    void importOpenApi();
    void showResponse(const reqflow::network::HttpResponse &response);
    void refreshProject();
    void manageEnvironments();
    void manageAssertionSuites();

private:
    void buildUi();
    void refreshProjects();
    void refreshRequests();
    void showHistory(const QModelIndex &index);
    void populateJsonTree(const QString &jsonText);
    void findInResponse();
    [[nodiscard]] QString redactResponseBody(const QByteArray &body) const;
    [[nodiscard]] QString redactUrl(const QUrl &url) const;

    data::ProjectStore store_;
    network::HttpClient client_;
    qint64 projectId_ = -1;
    qint64 selectedRequestId_ = -1;
    qint64 pendingAssertionSuiteId_ = -1;
    QJsonArray pendingAssertions_;
    qint64 queuedAssertionSuiteId_ = -1;
    QJsonArray queuedAssertions_;
    QComboBox *projectBox_ = nullptr;
    QComboBox *savedRequestBox_ = nullptr;
    QComboBox *methodBox_ = nullptr;
    QComboBox *environmentBox_ = nullptr;
    QLineEdit *urlEdit_ = nullptr;
    QPlainTextEdit *paramsEdit_ = nullptr;
    QPlainTextEdit *headersEdit_ = nullptr;
    QPlainTextEdit *bodyEdit_ = nullptr;
    QPlainTextEdit *responseBody_ = nullptr;
    QPlainTextEdit *responseHeaders_ = nullptr;
    QTableWidget *historyTable_ = nullptr;
    QLabel *responseMeta_ = nullptr;
    QLineEdit *responseSearch_ = nullptr;
    QTreeWidget *jsonTree_ = nullptr;
    QLabel *statusLabel_ = nullptr;
    QPushButton *sendButton_ = nullptr;
};

} // namespace reqflow::ui
