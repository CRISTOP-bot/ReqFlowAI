#include "reqflow/ui/mainwindow.h"
#include "reqflow/ui/theme.h"
#include "reqflow/flow/floweditor.h"
#include "reqflow/data/environment.h"
#include "reqflow/openapi/openapiimporter.h"
#include "reqflow/security/secrets.h"
#include "reqflow/testing/jsondiff.h"
#include "reqflow/testing/assertions.h"

#include <QAbstractItemView>
#include <QAction>
#include <QColor>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFontDatabase>
#include <QIcon>
#include <QKeySequence>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonObject>
#include <QRegularExpression>
#include <QVariant>
#include <functional>
#include <algorithm>

#include <QFormLayout>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QMessageBox>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QSplitter>
#include <QStatusBar>
#include <QStyle>
#include <QTableWidgetItem>
#include <QTextCursor>
#include <QTreeWidgetItem>
#include <QJsonValue>
#include <QVBoxLayout>
#include <QWidget>
#include <QUrlQuery>
#include <QJsonArray>
#include <QJsonValue>
#include <QJsonObject>
#include <QJsonParseError>
#include <QTableWidget>
#include <QDialogButtonBox>

namespace reqflow::ui {
namespace {
namespace rs = reqflow::security;

// B3: tope para ficheros de especificación importados.
constexpr qint64 kMaxImportBytes = 20 * 1024 * 1024;

bool requestHasSensitiveFields(const QUrl &url, const QString &headers, const QString &params, const QString &body) {
    if (!url.userName().isEmpty() || !url.password().isEmpty()) return true;
    if (rs::urlLooksSensitive(url.toString())) return true;
    auto lineLooksSensitive = [](const QString &text, QChar delimiter) {
        for (const auto &line : text.split('\n', Qt::SkipEmptyParts)) {
            const int pos = line.indexOf(delimiter);
            if (pos > 0) {
                const QString name = line.left(pos).trimmed();
                const QString value = line.mid(pos + 1).trimmed();
                if (rs::nameLooksSensitive(name) || rs::valueLooksLikeCredential(value)) return true;
            }
        }
        return false;
    };
    if (lineLooksSensitive(headers, QLatin1Char(':')) || lineLooksSensitive(params, QLatin1Char('='))) return true;
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(body.toUtf8(), &error);
    if (error.error == QJsonParseError::NoError && !document.isNull())
        return rs::jsonLooksSensitive(document.isObject() ? QJsonValue(document.object()) : QJsonValue(document.array()));
    return rs::textBodyLooksSensitive(body);
}
}

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), client_(this) {
    setWindowTitle(QStringLiteral("ReqFlow AI · Cliente HTTP"));
#ifdef Q_OS_ANDROID
    resize(420, 820);
    setMinimumSize(320, 480);
#else
    resize(1180, 760);
    setMinimumSize(900, 600);
#endif
    buildUi();
    connect(&client_, &network::HttpClient::finished, this, &MainWindow::showResponse);
    refreshProjects();
    if (!store_.isOpen()) statusLabel_->setText(QStringLiteral("SQLite: %1").arg(store_.lastError()));
}

void MainWindow::buildUi() {
    // El estilo global (tema oscuro unificado, docs/UI.md) se aplica una sola
    // vez en main(); aquí no se declara ninguna hoja local.
    auto *central = new QWidget(this);
    auto *outer = new QVBoxLayout(central);
    auto *brand = new QHBoxLayout;
    auto *title = new QLabel(QStringLiteral("<b style='font-size:20px'>ReqFlow <span style='color:%1'>AI</span></b>  <span style='color:%2'>Cliente HTTP local</span>")
                                 .arg(QString::fromLatin1(theme::kAccentSoft), QString::fromLatin1(theme::kTextDim)));
    title->setTextFormat(Qt::RichText);
    brand->addWidget(title);
    brand->addStretch();
    auto *localBadge = new QLabel(QStringLiteral("Cuenta local · Datos en este equipo"));
    localBadge->setObjectName(QStringLiteral("muted"));
    brand->addWidget(localBadge);
    outer->addLayout(brand);

    auto *projectRow = new QHBoxLayout;
    projectRow->addWidget(new QLabel(QStringLiteral("Proyecto")));
    projectBox_ = new QComboBox;
    projectRow->addWidget(projectBox_, 1);
    auto *newProject = new QPushButton(style()->standardIcon(QStyle::SP_FileDialogNewFolder),
                                       QStringLiteral("Nuevo proyecto"));
    newProject->setToolTip(QStringLiteral("Crear un proyecto nuevo"));
    projectRow->addWidget(newProject);
    environmentBox_ = new QComboBox;
#ifdef Q_OS_ANDROID
    environmentBox_->setMinimumWidth(0);
#else
    environmentBox_->setMinimumWidth(150);
#endif
    environmentBox_->setToolTip(QStringLiteral("Entorno seleccionado; variables se reemplazan con {{nombre}}"));
    auto *editEnvironments = new QPushButton(QStringLiteral("Entornos…"));
    editEnvironments->setToolTip(QStringLiteral("Crear y editar entornos con variables {{nombre}}"));
#ifdef Q_OS_ANDROID
    outer->addLayout(projectRow);
    auto *environmentRow = new QHBoxLayout;
    environmentRow->addWidget(new QLabel(QStringLiteral("Entorno")));
    environmentRow->addWidget(environmentBox_, 1);
    environmentRow->addWidget(editEnvironments);
    outer->addLayout(environmentRow);
#else
    projectRow->addWidget(environmentBox_);
    projectRow->addWidget(editEnvironments);
#endif
    connect(editEnvironments, &QPushButton::clicked, this, &MainWindow::manageEnvironments);
#ifdef Q_OS_ANDROID
    auto *actionsRow = new QHBoxLayout;
#endif
    auto *flowsButton = new QPushButton(QStringLiteral("Flujos visuales"));
    flowsButton->setToolTip(QStringLiteral("Abrir el editor visual de flujos (BETA)"));
#ifdef Q_OS_ANDROID
    actionsRow->addWidget(flowsButton);
#else
    projectRow->addWidget(flowsButton);
#endif
    connect(flowsButton, &QPushButton::clicked, this, &MainWindow::openFlowEditor);
    auto *suitesButton = new QPushButton(QStringLiteral("Suites API"));
    suitesButton->setToolTip(QStringLiteral("Definir y ejecutar suites de aserciones sobre peticiones guardadas"));
#ifdef Q_OS_ANDROID
    actionsRow->addWidget(suitesButton);
#else
    projectRow->addWidget(suitesButton);
#endif
    connect(suitesButton, &QPushButton::clicked, this, &MainWindow::manageAssertionSuites);
    auto *openApiButton = new QPushButton(style()->standardIcon(QStyle::SP_DialogOpenButton),
                                           QStringLiteral("Importar OpenAPI"));
    openApiButton->setToolTip(QStringLiteral("Importar peticiones desde una especificación OpenAPI JSON/YAML (Ctrl+O)"));
#ifdef Q_OS_ANDROID
    actionsRow->addWidget(openApiButton);
    actionsRow->addStretch();
    outer->addLayout(actionsRow);
#else
    projectRow->addWidget(openApiButton);
    projectRow->addStretch();
    auto *safetyHint = new QLabel(QStringLiteral("Verifica el destino antes de enviar."));
    safetyHint->setObjectName(QStringLiteral("muted"));
    projectRow->addWidget(safetyHint);
    outer->addLayout(projectRow);
#endif
    connect(openApiButton, &QPushButton::clicked, this, &MainWindow::importOpenApi);
    connect(projectBox_, qOverload<int>(&QComboBox::currentIndexChanged), this, &MainWindow::refreshProject);
    connect(newProject, &QPushButton::clicked, this, &MainWindow::createProject);

#ifdef Q_OS_ANDROID
    auto *split = new QSplitter(Qt::Vertical);
#else
    auto *split = new QSplitter(Qt::Horizontal);
#endif
    auto *requestPanel = new QWidget;
    auto *requestLayout = new QVBoxLayout(requestPanel);
    auto *savedRow = new QHBoxLayout;
    savedRequestBox_ = new QComboBox;
#ifndef Q_OS_ANDROID
    savedRequestBox_->setMinimumWidth(180);
#endif
    savedRow->addWidget(savedRequestBox_, 1);
    auto *saveButton = new QPushButton(style()->standardIcon(QStyle::SP_DialogSaveButton),
                                       QStringLiteral("Guardar"));
    saveButton->setToolTip(QStringLiteral("Guardar la petición actual (Ctrl+S)"));
    savedRow->addWidget(saveButton);
    auto *duplicateButton = new QPushButton(QStringLiteral("Duplicar"));
    duplicateButton->setToolTip(QStringLiteral("Guardar una copia como petición nueva"));
    savedRow->addWidget(duplicateButton);
    requestLayout->addLayout(savedRow);
    connect(savedRequestBox_, qOverload<int>(&QComboBox::currentIndexChanged), this, &MainWindow::loadSavedRequest);
    connect(saveButton, &QPushButton::clicked, this, &MainWindow::saveRequest);
    connect(duplicateButton, &QPushButton::clicked, this, [this] { selectedRequestId_ = -1; saveRequest(); });

    auto *urlRow = new QHBoxLayout;
    methodBox_ = new QComboBox;
    methodBox_->addItems({QStringLiteral("GET"), QStringLiteral("POST"), QStringLiteral("PUT"),
                          QStringLiteral("PATCH"), QStringLiteral("DELETE"), QStringLiteral("HEAD"), QStringLiteral("OPTIONS")});
    urlRow->addWidget(methodBox_);
    urlEdit_ = new QLineEdit;
    urlEdit_->setPlaceholderText(QStringLiteral("https://api.example.com/v1/resource"));
    urlRow->addWidget(urlEdit_, 1);
    sendButton_ = new QPushButton(style()->standardIcon(QStyle::SP_MediaPlay), QStringLiteral("Enviar"));
    sendButton_->setProperty("variant", "primary");
    sendButton_->setToolTip(QStringLiteral("Enviar la petición (Ctrl+Intro)"));
    urlRow->addWidget(sendButton_);
    requestLayout->addLayout(urlRow);
    connect(sendButton_, &QPushButton::clicked, this, &MainWindow::sendRequest);
    connect(urlEdit_, &QLineEdit::returnPressed, this, &MainWindow::sendRequest);

    auto *requestTabs = new QTabWidget;
    auto makeEditor = [](const QString &hint, QWidget *parent) {
        auto *page = new QWidget(parent);
        auto *layout = new QVBoxLayout(page);
        auto *label = new QLabel(hint); label->setWordWrap(true); layout->addWidget(label);
        auto *edit = new QPlainTextEdit; edit->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
        layout->addWidget(edit, 1);
        return qMakePair(page, edit);
    };
    auto params = makeEditor(QStringLiteral("Una clave=valor por línea; se añade a los parámetros existentes."), requestTabs);
    paramsEdit_ = params.second; requestTabs->addTab(params.first, QStringLiteral("Parámetros"));
    auto headers = makeEditor(QStringLiteral("Una cabecera por línea, por ejemplo Accept: application/json."), requestTabs);
    headersEdit_ = headers.second; requestTabs->addTab(headers.first, QStringLiteral("Cabeceras"));
    auto body = makeEditor(QStringLiteral("Cuerpo HTTP. Content-Type predeterminado: application/json."), requestTabs);
    bodyEdit_ = body.second; requestTabs->addTab(body.first, QStringLiteral("Cuerpo"));
    requestLayout->addWidget(requestTabs, 3);

    auto *historyHeader = new QHBoxLayout;
    historyTitle_ = new QLabel(QStringLiteral("Historial reciente (doble clic para revisar)"));
    historyTitle_->setObjectName(QStringLiteral("sectionTitle"));
    historyHeader->addWidget(historyTitle_, 1);
    auto *compareButton = new QPushButton(QStringLiteral("Comparar seleccionadas · BETA"));
    compareButton->setToolTip(QStringLiteral("Compara el JSON de dos respuestas seleccionadas del historial; no realiza solicitudes de red."));
    historyHeader->addWidget(compareButton);
    requestLayout->addLayout(historyHeader);
    connect(compareButton, &QPushButton::clicked, this, &MainWindow::compareHistoryResponses);
    historyTable_ = new QTableWidget(0, 4);
    historyTable_->setHorizontalHeaderLabels({QStringLiteral("Método"), QStringLiteral("Estado"), QStringLiteral("Duración"), QStringLiteral("Petición")});
    historyTable_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    historyTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    historyTable_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    historyTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    requestLayout->addWidget(historyTable_, 1);
    connect(historyTable_, &QTableWidget::doubleClicked, this, &MainWindow::showHistory);

    auto *responsePanel = new QWidget;
    auto *responseLayout = new QVBoxLayout(responsePanel);
    auto *responseTitle = new QLabel(QStringLiteral("Respuesta"));
    responseTitle->setObjectName(QStringLiteral("sectionTitle"));
    responseLayout->addWidget(responseTitle);
    responseMeta_ = new QLabel(QStringLiteral("Envía una petición para ver la respuesta"));
    responseMeta_->setObjectName(QStringLiteral("muted"));
    responseMeta_->setWordWrap(true);
    responseLayout->addWidget(responseMeta_);
    auto *responseTabs = new QTabWidget;
    auto *bodyPage = new QWidget;
    auto *bodyLayout = new QVBoxLayout(bodyPage);
    auto *searchRow = new QHBoxLayout;
    responseSearch_ = new QLineEdit; responseSearch_->setPlaceholderText(QStringLiteral("Buscar en cuerpo o árbol JSON… (Ctrl+F)"));
    auto *findButton = new QPushButton(style()->standardIcon(QStyle::SP_ArrowDown), QStringLiteral("Buscar siguiente"));
    findButton->setToolTip(QStringLiteral("Ir a la siguiente coincidencia (Intro en el campo de búsqueda)"));
    searchRow->addWidget(responseSearch_, 1); searchRow->addWidget(findButton); bodyLayout->addLayout(searchRow);
#ifdef Q_OS_ANDROID
    auto *bodySplit = new QSplitter(Qt::Vertical, bodyPage);
#else
    auto *bodySplit = new QSplitter(Qt::Horizontal, bodyPage);
#endif
    jsonTree_ = new QTreeWidget(bodySplit); jsonTree_->setHeaderLabels({QStringLiteral("JSON · clave"), QStringLiteral("Valor")});
    jsonTree_->setColumnWidth(0, 180);
#ifndef Q_OS_ANDROID
    jsonTree_->setMinimumWidth(220);
#endif
    responseBody_ = new QPlainTextEdit(bodySplit); responseBody_->setReadOnly(true);
    responseBody_->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    bodySplit->addWidget(jsonTree_); bodySplit->addWidget(responseBody_); bodySplit->setStretchFactor(1, 2);
    bodyLayout->addWidget(bodySplit, 1);
    connect(findButton, &QPushButton::clicked, this, &MainWindow::findInResponse);
    connect(responseSearch_, &QLineEdit::returnPressed, this, &MainWindow::findInResponse);
    connect(jsonTree_, &QTreeWidget::itemClicked, this, [this](QTreeWidgetItem *item, int) {
        const QString value = item->text(1).isEmpty() ? item->text(0) : item->text(1);
        if (value.isEmpty()) return;
        responseBody_->moveCursor(QTextCursor::Start);
        if (responseBody_->find(value)) responseBody_->setFocus();
    });
    responseHeaders_ = new QPlainTextEdit; responseHeaders_->setReadOnly(true);
    responseHeaders_->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    responseTabs->addTab(bodyPage, QStringLiteral("Cuerpo / JSON"));
    responseTabs->addTab(responseHeaders_, QStringLiteral("Cabeceras / API Doctor"));
    responseLayout->addWidget(responseTabs, 1);
    split->addWidget(requestPanel); split->addWidget(responsePanel);
#ifdef Q_OS_ANDROID
    split->setStretchFactor(0, 3); split->setStretchFactor(1, 2);
#else
    split->setStretchFactor(0, 3); split->setStretchFactor(1, 2);
#endif
    outer->addWidget(split, 1);
    // Barra de estado real (antes era una etiqueta dentro del layout central):
    // recibe los mensajes de estado y permanece visible al cambiar el layout.
    statusLabel_ = new QLabel(QStringLiteral("Listo · Los datos se guardan localmente"));
    statusBar()->addWidget(statusLabel_, 1);
    setCentralWidget(central);

    // Accesos de teclado (docs/UI.md). Como QAction de la ventana funcionan
    // con cualquier widget enfocado; los atajos no dependen del foco.
    auto addShortcut = [this](const QString &text, const QKeySequence &key, void (MainWindow::*slot)()) {
        auto *action = new QAction(text, this);
        action->setShortcut(key);
        connect(action, &QAction::triggered, this, slot);
        addAction(action);
    };
    addShortcut(QStringLiteral("Enviar petición"), QKeySequence(Qt::CTRL | Qt::Key_Return), &MainWindow::sendRequest);
    addShortcut(QStringLiteral("Guardar petición"), QKeySequence::Save, &MainWindow::saveRequest);
    addShortcut(QStringLiteral("Importar OpenAPI"), QKeySequence::Open, &MainWindow::importOpenApi);
    auto *focusUrl = new QAction(QStringLiteral("Enfocar la URL"), this);
    focusUrl->setShortcut(QKeySequence(QStringLiteral("Ctrl+L")));
    connect(focusUrl, &QAction::triggered, this, [this] { urlEdit_->setFocus(); urlEdit_->selectAll(); });
    addAction(focusUrl);
    auto *focusSearch = new QAction(QStringLiteral("Buscar en la respuesta"), this);
    focusSearch->setShortcut(QKeySequence(QStringLiteral("Ctrl+F")));
    connect(focusSearch, &QAction::triggered, this, [this] { responseSearch_->setFocus(); responseSearch_->selectAll(); });
    addAction(focusSearch);
}

void MainWindow::refreshProjects() {
    const auto rows = store_.projects();
    projectBox_->blockSignals(true); projectBox_->clear();
    for (const auto &row : rows) projectBox_->addItem(row.value(QStringLiteral("name")).toString(), row.value(QStringLiteral("id")));
    projectBox_->blockSignals(false);
    if (!rows.isEmpty()) { projectBox_->setCurrentIndex(0); refreshProject(); }
}

void MainWindow::refreshProject() {
    const qint64 previousProjectId = projectId_;
    const qint64 previousEnvironmentId = environmentBox_->currentData().toLongLong();
    projectId_ = projectBox_->currentData().toLongLong();
    selectedRequestId_ = -1;
    refreshRequests();
    environmentBox_->blockSignals(true);
    environmentBox_->clear();
    environmentBox_->addItem(QStringLiteral("Sin entorno"), -1);
    for (const auto &environment : store_.environments(projectId_))
        environmentBox_->addItem(environment.value(QStringLiteral("name")).toString(), environment.value(QStringLiteral("id")));
    if (previousProjectId == projectId_) {
        const int savedIndex = environmentBox_->findData(previousEnvironmentId);
        if (savedIndex >= 0) environmentBox_->setCurrentIndex(savedIndex);
    }
    environmentBox_->blockSignals(false);
    historyTable_->setRowCount(0);
    const auto items = store_.history(projectId_, 50);
    historyTitle_->setText(items.isEmpty()
                               ? QStringLiteral("Historial reciente · aún no hay peticiones")
                               : QStringLiteral("Historial reciente (doble clic para revisar)"));
    for (const auto &item : items) {
        const int row = historyTable_->rowCount(); historyTable_->insertRow(row);
        historyTable_->setItem(row, 0, new QTableWidgetItem(item.value(QStringLiteral("method")).toString()));
        historyTable_->setItem(row, 1, new QTableWidgetItem(item.value(QStringLiteral("status")).toString()));
        historyTable_->setItem(row, 2, new QTableWidgetItem(QStringLiteral("%1 ms").arg(item.value(QStringLiteral("elapsed_ms")).toLongLong())));
        auto *name = new QTableWidgetItem(item.value(QStringLiteral("request_name")).toString());
        name->setData(Qt::UserRole, item.value(QStringLiteral("id"))); historyTable_->setItem(row, 3, name);
    }
}

void MainWindow::manageEnvironments() {
    if (projectId_ <= 0) return;
    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("Entornos y variables locales"));
    dialog.setWindowFlag(Qt::WindowContextHelpButtonHint, false);
    dialog.resize(560, 480);
    auto *layout = new QVBoxLayout(&dialog);
    auto *list = new QComboBox(&dialog);
    auto refresh = [this, list] {
        list->clear();
        for (const auto &item : store_.environments(projectId_))
            list->addItem(item.value(QStringLiteral("name")).toString(), item.value(QStringLiteral("id")));
    };
    refresh();
    layout->addWidget(new QLabel(QStringLiteral("Selecciona para editar, o crea un entorno nuevo. No guardes claves ni tokens: no hay bóveda segura."), &dialog));
    layout->addWidget(list);
    auto *envName = new QLineEdit(&dialog); envName->setPlaceholderText(QStringLiteral("Nombre: desarrollo, pruebas…")); layout->addWidget(envName);
    auto *variables = new QPlainTextEdit(&dialog); variables->setPlaceholderText(QStringLiteral("JSON de variables, por ejemplo {\"base_url\":\"http://localhost:8080\"}")); layout->addWidget(variables, 1);
    auto *buttons = new QHBoxLayout;
    auto *newButton = new QPushButton(QStringLiteral("Nuevo"), &dialog);
    auto *saveButton = new QPushButton(QStringLiteral("Guardar"), &dialog);
    saveButton->setProperty("variant", "primary");
    saveButton->setDefault(true);
    auto *deleteButton = new QPushButton(QStringLiteral("Eliminar"), &dialog);
    deleteButton->setProperty("variant", "danger");
    deleteButton->setToolTip(QStringLiteral("Eliminar el entorno seleccionado"));
    auto *closeButton = new QPushButton(QStringLiteral("Cerrar"), &dialog);
    closeButton->setObjectName(QStringLiteral("cancel"));
    buttons->addWidget(newButton); buttons->addStretch();
    buttons->addWidget(closeButton); buttons->addWidget(deleteButton); buttons->addWidget(saveButton); layout->addLayout(buttons);
    connect(closeButton, &QPushButton::clicked, &dialog, &QDialog::reject);
    connect(list, qOverload<int>(&QComboBox::currentIndexChanged), &dialog, [this,list,envName,variables](int index) {
        if (index < 0) return;
        const qint64 id = list->itemData(index).toLongLong();
        for (const auto &item : store_.environments(projectId_)) if (item.value(QStringLiteral("id")).toLongLong() == id) {
            envName->setText(item.value(QStringLiteral("name")).toString());
            variables->setPlainText(item.value(QStringLiteral("variables_json")).toString()); return;
        }
    });
    connect(newButton, &QPushButton::clicked, &dialog, [envName,variables,list] { list->setCurrentIndex(-1); envName->clear(); variables->setPlainText(QStringLiteral("{}")); });
    connect(saveButton, &QPushButton::clicked, &dialog, [this,&dialog,list,envName,variables,refresh] {
        QJsonParseError error; const auto doc = QJsonDocument::fromJson(variables->toPlainText().toUtf8(), &error);
        if (error.error != QJsonParseError::NoError || !doc.isObject()) { QMessageBox::warning(&dialog, QStringLiteral("JSON inválido"), QStringLiteral("Las variables deben ser un objeto JSON.")); return; }
        const qint64 id = store_.saveEnvironment(projectId_, list->currentData().toLongLong(), envName->text(), variables->toPlainText());
        if (id < 0) { QMessageBox::warning(&dialog, QStringLiteral("No se pudo guardar"), store_.lastError()); return; }
        refresh(); list->setCurrentIndex(list->findData(id)); refreshProject();
        const int globalIndex = environmentBox_->findData(id);
        if (globalIndex >= 0) environmentBox_->setCurrentIndex(globalIndex);
    });
    connect(deleteButton, &QPushButton::clicked, &dialog, [this,&dialog,list,refresh] {
        const qint64 id=list->currentData().toLongLong(); if(id<0)return;
        if(QMessageBox::question(&dialog,QStringLiteral("Eliminar entorno"),QStringLiteral("¿Eliminar este entorno?"))!=QMessageBox::Yes)return;
        store_.deleteEnvironment(projectId_,id); refresh(); refreshProject();
    });
    dialog.exec();
}

void MainWindow::manageAssertionSuites() {
    if (projectId_ <= 0) return;
    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("Suites de aserciones API"));
    dialog.setWindowFlag(Qt::WindowContextHelpButtonHint, false);
    dialog.resize(680, 500);
    auto *layout = new QVBoxLayout(&dialog);
    layout->addWidget(new QLabel(QStringLiteral("Define validaciones JSON para una petición guardada. Ejecutar envía la petición: siempre se pide confirmación."), &dialog));
    auto *suiteList = new QComboBox(&dialog);
    auto refresh = [this, suiteList] {
        suiteList->clear();
        suiteList->addItem(QStringLiteral("— Suite nueva —"), -1);
        for (const auto &suite : store_.assertionSuites(projectId_))
            suiteList->addItem(suite.value(QStringLiteral("name")).toString(), suite.value(QStringLiteral("id")));
    };
    refresh(); layout->addWidget(suiteList);
    auto *suiteName = new QLineEdit(&dialog); suiteName->setPlaceholderText(QStringLiteral("Nombre de la suite")); layout->addWidget(suiteName);
    auto *requestList = new QComboBox(&dialog);
    requestList->addItem(QStringLiteral("Selecciona una petición guardada"), -1);
    for (const auto &request : store_.requests(projectId_))
        requestList->addItem(QStringLiteral("%1 · %2").arg(request.value(QStringLiteral("name")).toString(), request.value(QStringLiteral("method")).toString()), request.value(QStringLiteral("id")));
    layout->addWidget(requestList);
    auto *definition = new QPlainTextEdit(&dialog);
    definition->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    definition->setPlaceholderText(QStringLiteral("JSON array: [{\"name\":\"HTTP 200\",\"type\":\"status\",\"expected\":\"200\"}]"));
    layout->addWidget(definition, 1);
    auto *result = new QPlainTextEdit(&dialog); result->setReadOnly(true); result->setMaximumHeight(100); layout->addWidget(result);
    auto *buttons = new QHBoxLayout;
    auto *save = new QPushButton(QStringLiteral("Guardar suite"), &dialog);
    auto *run = new QPushButton(QStringLiteral("Ejecutar suite…"), &dialog);
    run->setProperty("variant", "primary");
    run->setToolTip(QStringLiteral("Ejecuta la suite: siempre pide confirmación antes de enviar la petición"));
    auto *closeSuite = new QPushButton(QStringLiteral("Cerrar"), &dialog);
    closeSuite->setObjectName(QStringLiteral("cancel"));
    buttons->addStretch(); buttons->addWidget(closeSuite); buttons->addWidget(save); buttons->addWidget(run); layout->addLayout(buttons);
    connect(closeSuite, &QPushButton::clicked, &dialog, &QDialog::reject);
    connect(suiteList, qOverload<int>(&QComboBox::currentIndexChanged), &dialog, [this, suiteList, suiteName, requestList, definition](int index) {
        const qint64 id = suiteList->itemData(index).toLongLong();
        if (id < 1) { suiteName->clear(); requestList->setCurrentIndex(0); definition->setPlainText(QStringLiteral("[]")); return; }
        for (const auto &suite : store_.assertionSuites(projectId_)) if (suite.value(QStringLiteral("id")).toLongLong() == id) {
            suiteName->setText(suite.value(QStringLiteral("name")).toString());
            requestList->setCurrentIndex(qMax(0, requestList->findData(suite.value(QStringLiteral("request_id")))));
            definition->setPlainText(suite.value(QStringLiteral("definition")).toString()); return;
        }
    });
    connect(save, &QPushButton::clicked, &dialog, [this, &dialog, suiteList, suiteName, requestList, definition, refresh] {
        const qint64 id = store_.saveAssertionSuite(projectId_, suiteList->currentData().toLongLong(), suiteName->text(), requestList->currentData().toLongLong(), definition->toPlainText());
        if (id < 0) { QMessageBox::warning(&dialog, QStringLiteral("Suite inválida"), store_.lastError()); return; }
        refresh(); suiteList->setCurrentIndex(suiteList->findData(id));
    });
    connect(run, &QPushButton::clicked, &dialog, [this, &dialog, suiteList, requestList, definition, result] {
        const qint64 suiteId = suiteList->currentData().toLongLong();
        const qint64 requestId = requestList->currentData().toLongLong();
        QJsonParseError error;
        const auto parsed = QJsonDocument::fromJson(definition->toPlainText().toUtf8(), &error);
        if (suiteId < 1 || requestId < 1 || error.error != QJsonParseError::NoError || !parsed.isArray()) {
            QMessageBox::warning(&dialog, QStringLiteral("Faltan datos"), QStringLiteral("Guarda una suite válida y asígnale una petición guardada.")); return;
        }
        bool found = false;
        for (const auto &saved : store_.requests(projectId_)) if (saved.value(QStringLiteral("id")).toLongLong() == requestId) {
            found = true;
            selectedRequestId_ = requestId;
            methodBox_->setCurrentText(saved.value(QStringLiteral("method")).toString());
            urlEdit_->setText(saved.value(QStringLiteral("url")).toString());
            paramsEdit_->setPlainText(saved.value(QStringLiteral("params")).toString());
            headersEdit_->setPlainText(saved.value(QStringLiteral("headers")).toString());
            bodyEdit_->setPlainText(saved.value(QStringLiteral("body")).toString());
            break;
        }
        if (!found) { QMessageBox::warning(&dialog, QStringLiteral("Petición eliminada"), QStringLiteral("La petición guardada ya no existe.")); return; }
        queuedAssertionSuiteId_ = suiteId;
        queuedAssertions_ = parsed.array();
        statusLabel_->setText(QStringLiteral("Confirma el envío para ejecutar la suite."));
        sendRequest();
        if (sendButton_->isEnabled()) { pendingAssertionSuiteId_ = -1; pendingAssertions_ = {}; return; }
        result->setPlainText(QStringLiteral("Petición enviada; resultados aparecerán al recibir respuesta."));
    });
    dialog.exec();
}

void MainWindow::refreshRequests() {
    savedRequestBox_->blockSignals(true); savedRequestBox_->clear();
    const auto rows = store_.requests(projectId_);
    savedRequestBox_->addItem(QStringLiteral("— Peticiones guardadas —"), -1);
    for (const auto &row : rows)
        savedRequestBox_->addItem(QStringLiteral("%1 · %2").arg(row.value(QStringLiteral("name")).toString(), row.value(QStringLiteral("method")).toString()), row.value(QStringLiteral("id")));
    savedRequestBox_->blockSignals(false);
}

void MainWindow::openFlowEditor() {
    if (projectId_ <= 0) { QMessageBox::warning(this, QStringLiteral("Proyecto requerido"), QStringLiteral("Crea o selecciona un proyecto antes de editar flujos.")); return; }
    auto *dialog = new QDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowTitle(QStringLiteral("Editor visual de flujos"));
    dialog->setWindowFlag(Qt::WindowContextHelpButtonHint, false);
    dialog->resize(1080, 700);
    auto *layout = new QVBoxLayout(dialog);
    layout->addWidget(new flow::FlowEditor(&store_, projectId_, dialog));
    auto *closeFlow = new QPushButton(QStringLiteral("Cerrar"), dialog);
    closeFlow->setObjectName(QStringLiteral("cancel"));
    closeFlow->setToolTip(QStringLiteral("Cerrar el editor (también con Esc o la X de la ventana)"));
    auto *closeFlowRow = new QHBoxLayout;
    closeFlowRow->addStretch();
    closeFlowRow->addWidget(closeFlow);
    layout->addLayout(closeFlowRow);
    connect(closeFlow, &QPushButton::clicked, dialog, &QDialog::close);
    dialog->show();
}

void MainWindow::importOpenApi() {
    if (projectId_ <= 0) { QMessageBox::warning(this, QStringLiteral("Proyecto requerido"), QStringLiteral("Crea o selecciona un proyecto primero.")); return; }
    const QString fileName = QFileDialog::getOpenFileName(this, QStringLiteral("Importar especificación OpenAPI"), {}, QStringLiteral("OpenAPI JSON/YAML (*.json *.yaml *.yml);;JSON (*.json);;YAML (*.yaml *.yml);;Todos los archivos (*)"));
    if (fileName.isEmpty()) return;
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) { QMessageBox::warning(this, QStringLiteral("No se pudo leer"), file.errorString()); return; }
    if (file.size() > kMaxImportBytes) {
        QMessageBox::warning(this, QStringLiteral("Especificación demasiado grande"),
                             QStringLiteral("El fichero supera el límite de 20 MiB; divídelo o exporta solo las rutas necesarias."));
        return;
    }
    const QByteArray specification = file.readAll();
    const QString suffix = QFileInfo(fileName).suffix().toLower();
    const auto imported = (suffix == QStringLiteral("yaml") || suffix == QStringLiteral("yml"))
        ? openapi::parseOpenApiYaml(specification)
        : openapi::parseOpenApiJson(specification);
    if (!imported.error.isEmpty()) { QMessageBox::warning(this, QStringLiteral("OpenAPI no compatible"), imported.error); return; }
    if (imported.requests.isEmpty()) { QMessageBox::information(this, QStringLiteral("Sin operaciones"), imported.warnings.join(QLatin1Char('\n'))); return; }
    const QString detail = QStringLiteral("Se crearán %1 peticiones guardadas (no se enviarán).\n\nAdvertencias:\n%2").arg(imported.requests.size()).arg(imported.warnings.isEmpty() ? QStringLiteral("Ninguna") : imported.warnings.join(QLatin1Char('\n')));
    if (QMessageBox::question(this, QStringLiteral("Confirmar importación"), detail) != QMessageBox::Yes) return;
    int saved = 0;
    for (const auto &item : imported.requests) {
        QVariantMap request;
        request.insert(QStringLiteral("name"), item.name);
        request.insert(QStringLiteral("method"), item.method);
        request.insert(QStringLiteral("url"), item.path);
        request.insert(QStringLiteral("params"), QString{});
        request.insert(QStringLiteral("headers"), item.headers.trimmed());
        request.insert(QStringLiteral("body"), item.body);
        if (store_.saveRequest(projectId_, -1, request) < 0) break;
        ++saved;
    }
    refreshRequests();
    QMessageBox::information(this, QStringLiteral("Importación finalizada"), QStringLiteral("Se guardaron %1 de %2 operaciones. Revisa cada URL y variable antes de enviar.").arg(saved).arg(imported.requests.size()));
}

void MainWindow::createProject() {
    bool ok = false;
    const QString name = QInputDialog::getText(this, QStringLiteral("Nuevo proyecto"), QStringLiteral("Nombre del proyecto:"), QLineEdit::Normal, {}, &ok);
    if (!ok) return;
    if (store_.createProject(name) < 0) { QMessageBox::warning(this, QStringLiteral("No se pudo crear"), store_.lastError()); return; }
    refreshProjects();
}

QString MainWindow::redactUrl(const QUrl &url) const {
    QUrl safe(url);
    safe.setUserInfo(QString{});
    QUrlQuery query(safe);
    const auto items = query.queryItems(QUrl::FullyDecoded);
    query.clear();
    for (const auto &item : items)
        query.addQueryItem(item.first,
                           (rs::nameLooksSensitive(item.first) || rs::valueLooksLikeCredential(item.second))
                               ? QStringLiteral("[REDACTED]") : item.second);
    safe.setQuery(query);
    return safe.toString(QUrl::FullyEncoded);
}

QString MainWindow::redactResponseBody(const QByteArray &body) const {
    // M1: tope de tamaño antes de parsear; el cuerpo enorme se omite entero.
    if (body.size() > data::kMaxHistoryBodyChars)
        return QStringLiteral("[cuerpo omitido del historial: supera el límite de %1 caracteres]").arg(data::kMaxHistoryBodyChars);
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(body, &error);
    if (error.error != QJsonParseError::NoError)
        return body.isEmpty() ? QString{} : QStringLiteral("[Respuesta de texto omitida del historial para evitar retener posibles secretos]");
    std::function<QJsonValue(const QJsonValue &)> clean = [&](const QJsonValue &value) -> QJsonValue {
        if (value.isObject()) {
            QJsonObject output;
            const auto object = value.toObject();
            for (auto it = object.begin(); it != object.end(); ++it)
                output.insert(it.key(), rs::nameLooksSensitive(it.key()) ? QJsonValue(QStringLiteral("[REDACTED]")) : clean(it.value()));
            return output;
        }
        if (value.isArray()) {
            QJsonArray output; for (const auto &item : value.toArray()) output.append(clean(item)); return output;
        }
        // M3: redacción también por valor (claves que no parecen sensibles).
        if (value.isString() && rs::valueLooksLikeCredential(value.toString()))
            return QJsonValue(QStringLiteral("[REDACTED]"));
        return value;
    };
    const QJsonValue root = document.isObject() ? QJsonValue(document.object()) : QJsonValue(document.array());
    const QJsonValue safe = clean(root);
    if (safe.isArray()) return QString::fromUtf8(QJsonDocument(safe.toArray()).toJson(QJsonDocument::Compact));
    return QString::fromUtf8(QJsonDocument(safe.toObject()).toJson(QJsonDocument::Compact));
}

void MainWindow::saveRequest() {
    const QString urlTemplate = urlEdit_->text().trimmed();
    QString validationText = urlTemplate;
    static const QRegularExpression variable(QStringLiteral(R"(\{\{[A-Za-z_][A-Za-z0-9_.-]*\}\})"));
    if (validationText.startsWith(QStringLiteral("{{"))) {
        const auto firstEnd = validationText.indexOf(QStringLiteral("}}"));
        if (firstEnd >= 0) validationText.replace(0, firstEnd + 2, QStringLiteral("https://placeholder"));
    }
    validationText.replace(variable, QStringLiteral("x"));
    const QUrl url(validationText);
    if (!url.isValid() || (url.scheme() != QStringLiteral("http") && url.scheme() != QStringLiteral("https"))) {
        QMessageBox::warning(this, QStringLiteral("URL inválida"), QStringLiteral("Usa una URL completa http:// o https://.")); return;
    }
    if (requestHasSensitiveFields(url, headersEdit_->toPlainText(), paramsEdit_->toPlainText(), bodyEdit_->toPlainText())) {
        QMessageBox::warning(this, QStringLiteral("Credenciales no protegidas"), QStringLiteral("ReqFlow aún no tiene una bóveda segura. Quita las credenciales antes de guardar la petición.")); return;
    }
    const QString suggested = url.host() + url.path();
    bool ok = false;
    const QString name = QInputDialog::getText(this, QStringLiteral("Guardar petición"), QStringLiteral("Nombre:"), QLineEdit::Normal, suggested, &ok);
    if (!ok) return;
    QVariantMap item;
    item.insert(QStringLiteral("name"), name.trimmed().isEmpty() ? suggested : name.trimmed());
    item.insert(QStringLiteral("method"), methodBox_->currentText()); item.insert(QStringLiteral("url"), urlTemplate);
    item.insert(QStringLiteral("params"), paramsEdit_->toPlainText()); item.insert(QStringLiteral("headers"), headersEdit_->toPlainText()); item.insert(QStringLiteral("body"), bodyEdit_->toPlainText());
    selectedRequestId_ = store_.saveRequest(projectId_, selectedRequestId_, item);
    if (selectedRequestId_ < 0) { QMessageBox::warning(this, QStringLiteral("No se pudo guardar"), store_.lastError()); return; }
    refreshRequests(); statusLabel_->setText(QStringLiteral("Petición guardada localmente."));
}

void MainWindow::loadSavedRequest(int index) {
    if (index <= 0) { selectedRequestId_ = -1; return; }
    const qint64 id = savedRequestBox_->itemData(index).toLongLong();
    for (const auto &item : store_.requests(projectId_)) {
        if (item.value(QStringLiteral("id")).toLongLong() != id) continue;
        selectedRequestId_ = id; methodBox_->setCurrentText(item.value(QStringLiteral("method")).toString());
        urlEdit_->setText(item.value(QStringLiteral("url")).toString()); paramsEdit_->setPlainText(item.value(QStringLiteral("params")).toString());
        headersEdit_->setPlainText(item.value(QStringLiteral("headers")).toString()); bodyEdit_->setPlainText(item.value(QStringLiteral("body")).toString());
        return;
    }
}

void MainWindow::sendRequest() {
    qint64 suiteToRun = -1;
    QJsonArray assertionsToRun;
    if (queuedAssertionSuiteId_ > 0 && sendButton_->isEnabled()) {
        if (QMessageBox::question(this, QStringLiteral("Confirmar suite API"), QStringLiteral("La suite ejecutará una petición al destino que aparece en el editor. Verifica la URL y el método. ¿Enviar ahora?")) != QMessageBox::Yes) {
            queuedAssertionSuiteId_ = -1;
            queuedAssertions_ = {};
            return;
        }
        suiteToRun = queuedAssertionSuiteId_;
        assertionsToRun = queuedAssertions_;
        queuedAssertionSuiteId_ = -1;
        queuedAssertions_ = {};
    }
    QJsonObject variables;
    const qint64 environmentId = environmentBox_->currentData().toLongLong();
    if (environmentId > 0) {
        for (const auto &environment : store_.environments(projectId_)) {
            if (environment.value(QStringLiteral("id")).toLongLong() != environmentId) continue;
            QJsonParseError parseError;
            const auto document = QJsonDocument::fromJson(environment.value(QStringLiteral("variables_json")).toString().toUtf8(), &parseError);
            if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
                QMessageBox::warning(this, QStringLiteral("Entorno inválido"), QStringLiteral("No se pudo leer el JSON del entorno seleccionado.")); return;
            }
            variables = document.object();
            break;
        }
    }

    QString expandedUrl, expandedParams, expandedHeaders, expandedBody, expansionError;
    const auto expand = [&](const QString &source, QString *destination) {
        return data::expandTemplate(source, variables, destination, &expansionError);
    };
    if (!expand(urlEdit_->text().trimmed(), &expandedUrl) ||
        !expand(paramsEdit_->toPlainText(), &expandedParams) ||
        !expand(headersEdit_->toPlainText(), &expandedHeaders) ||
        !expand(bodyEdit_->toPlainText(), &expandedBody)) {
        QMessageBox::warning(this, QStringLiteral("Variable desconocida"), expansionError); return;
    }
    const QUrl base(expandedUrl.trimmed());
    if (!base.isValid() || (base.scheme() != QStringLiteral("http") && base.scheme() != QStringLiteral("https"))) {
        QMessageBox::warning(this, QStringLiteral("URL inválida"), QStringLiteral("Escribe una URL completa http:// o https://.")); return;
    }
    if (requestHasSensitiveFields(base, expandedHeaders, expandedParams, expandedBody)) {
        QMessageBox::warning(this, QStringLiteral("Posibles credenciales"), QStringLiteral("No enviaré credenciales detectadas sin bóveda segura. Elimina los valores sensibles antes de continuar.")); return;
    }
    QUrl url(base); QUrlQuery query(url);
    for (const QString &line : expandedParams.split('\n', Qt::SkipEmptyParts)) {
        const int equals = line.indexOf('=');
        if (equals <= 0) { QMessageBox::warning(this, QStringLiteral("Parámetro inválido"), QStringLiteral("Usa una clave=valor por línea.")); return; }
        query.addQueryItem(line.left(equals).trimmed(), line.mid(equals+1).trimmed());
    }
    url.setQuery(query);
    network::HttpRequest request; request.method = methodBox_->currentText().toLatin1(); request.url = url;
    for (const QString &line : expandedHeaders.split('\n', Qt::SkipEmptyParts)) {
        const int colon = line.indexOf(':');
        if (colon <= 0) { QMessageBox::warning(this, QStringLiteral("Cabecera inválida"), QStringLiteral("Usa Nombre: valor en cada línea.")); return; }
        // B1: Qt 6.12 descarta en silencio las cabeceras inválidas y Qt 6.4 no las
        // validaba; aquí se rechazan de forma explícita con un error claro.
        const QString headerName = line.left(colon).trimmed();
        const QString headerValue = line.mid(colon + 1).trimmed();
        if (!network::isValidHeaderName(headerName) || !network::isValidHeaderValue(headerValue)) {
            QMessageBox::warning(this, QStringLiteral("Cabecera inválida"),
                                 QStringLiteral("La cabecera «%1» no es válida (RFC 7230): revisa el nombre y evita saltos de línea o caracteres de control en el valor.").arg(headerName));
            return;
        }
        request.headers.insert(headerName.toLatin1(), headerValue.toLatin1());
    }
    request.body = expandedBody.toUtf8();
    if (!request.body.isEmpty() && !request.headers.contains("Content-Type")) request.headers.insert("Content-Type", "application/json");
    if (suiteToRun > 0) {
        pendingAssertionSuiteId_ = suiteToRun;
        pendingAssertions_ = assertionsToRun;
    }
    sendButton_->setEnabled(false); statusLabel_->setText(QStringLiteral("Enviando…")); responseMeta_->setText(QStringLiteral("Esperando respuesta…"));
    // B2: mismo origen sin preguntar; cruzar origen requiere confirmación explícita.
    client_.send(request, [this](const QUrl &from, const QUrl &to) {
        if (network::isSameOrigin(from, to)) return true;
        return QMessageBox::question(this, QStringLiteral("Redirección cruzada"),
                                     QStringLiteral("El servidor pide redirigir de %1 a %2.\n\n¿Permitir este salto?")
                                         .arg(from.toString(), to.toString())) == QMessageBox::Yes;
    });
}

void MainWindow::showResponse(const network::HttpResponse &response) {
    sendButton_->setEnabled(true);
    const QString status = response.status ? QStringLiteral("%1 %2").arg(response.status).arg(response.reason) : QStringLiteral("Error de red");
    responseMeta_->setText(QStringLiteral("%1 · %2 ms · %3 bytes").arg(status).arg(response.elapsedMs).arg(response.sizeBytes()));
    QString text = QString::fromUtf8(response.body);
    const QJsonDocument document = QJsonDocument::fromJson(response.body);
    if (!document.isNull()) text = QString::fromUtf8(document.toJson(QJsonDocument::Indented));
    constexpr qsizetype maxChars = 2'000'000;
    if (text.size() > maxChars) text = text.left(maxChars) + QStringLiteral("\n… vista truncada a 2 millones de caracteres.");
    responseBody_->setPlainText(text);
    populateJsonTree(text);
    QStringList headerLines;
    for (const auto &header : response.headers) {
        const QString name = QString::fromLatin1(header.first);
        const QString rawValue = QString::fromLatin1(header.second);
        // M3: se redacta por nombre O por valor con forma de credencial.
        const QString value = (rs::nameLooksSensitive(name) || rs::valueLooksLikeCredential(rawValue))
                                  ? QStringLiteral("[REDACTED]") : rawValue;
        headerLines.append(name + QStringLiteral(": ") + value);
    }
    const QString contentType = QString::fromLatin1(response.header("Content-Type")).toLower();
    if (response.url.scheme() == QStringLiteral("https") && !response.header("Strict-Transport-Security").size())
        headerLines.append(QStringLiteral("\n[heurística] No se observó Strict-Transport-Security en esta respuesta HTTPS."));
    if (!response.header("Content-Type").size()) headerLines.append(QStringLiteral("[evidencia] La respuesta no incluyó Content-Type."));
    if (contentType.contains(QStringLiteral("json")) && document.isNull()) headerLines.append(QStringLiteral("[evidencia] Content-Type indica JSON, pero el cuerpo no se pudo parsear."));
    responseHeaders_->setPlainText(headerLines.join('\n'));
    const bool suiteWasRun = pendingAssertionSuiteId_ > 0;
    if (suiteWasRun) {
        const auto outcomes = testing::evaluateAssertions(pendingAssertions_, response);
        const QJsonArray outcomeJson = testing::assertionResultsToJson(outcomes);
        int passed = 0;
        for (const auto &outcome : outcomes) if (outcome.passed) ++passed;
        const QString runStatus = passed == outcomes.size() ? QStringLiteral("passed") : QStringLiteral("failed");
        store_.addAssertionRun(pendingAssertionSuiteId_, runStatus,
                               QString::fromUtf8(QJsonDocument(outcomeJson).toJson(QJsonDocument::Compact)));
        QStringList lines{QStringLiteral("\n--- Suite API: %1/%2 aprobadas (%3) ---").arg(passed).arg(outcomes.size()).arg(runStatus)};
        for (const auto &outcome : outcomes)
            lines.append(QStringLiteral("%1 · %2 · actual=%3").arg(outcome.passed ? QStringLiteral("PASS") : QStringLiteral("FAIL"), outcome.name, outcome.actual));
        responseHeaders_->appendPlainText(lines.join(QLatin1Char('\n')));
        statusLabel_->setText(QStringLiteral("Suite %1: %2/%3 aserciones aprobadas.").arg(runStatus).arg(passed).arg(outcomes.size()));
        pendingAssertionSuiteId_ = -1;
        pendingAssertions_ = {};
    }
    if (projectId_ > 0) {
        QVariantMap item;
        item.insert(QStringLiteral("request_name"), selectedRequestId_ > 0 ? savedRequestBox_->currentText() : QStringLiteral("Petición"));
        item.insert(QStringLiteral("method"), methodBox_->currentText()); item.insert(QStringLiteral("url"), redactUrl(response.url));
        item.insert(QStringLiteral("status"), response.status); item.insert(QStringLiteral("elapsed_ms"), response.elapsedMs);
        QStringList safeHeaders;
        for (const auto &header : response.headers) {
            const QString headerName = QString::fromLatin1(header.first);
            const QString rawHeaderValue = QString::fromLatin1(header.second);
            // M3: el historial también redacta por valor.
            const bool isSensitive = rs::nameLooksSensitive(headerName) || rs::valueLooksLikeCredential(rawHeaderValue);
            safeHeaders.append(headerName + QStringLiteral(": ") + (isSensitive ? QStringLiteral("[REDACTED]") : rawHeaderValue));
        }
        item.insert(QStringLiteral("response_size"), response.sizeBytes()); item.insert(QStringLiteral("response_headers"), safeHeaders.join('\n'));
        item.insert(QStringLiteral("response_body"), redactResponseBody(response.body));
        item.insert(QStringLiteral("error"), response.error.isEmpty() ? QString{} : QStringLiteral("[network error omitted from history]"));
        store_.addHistory(projectId_, item); refreshProject();
    }
    if (!suiteWasRun)
        statusLabel_->setText(response.error.isEmpty() ? QStringLiteral("Respuesta recibida y añadida al historial local.") : response.error);
}

void MainWindow::populateJsonTree(const QString &jsonText) {
    jsonTree_->clear();
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(jsonText.toUtf8(), &error);
    if (error.error != QJsonParseError::NoError || document.isNull()) {
        jsonTree_->setEnabled(false);
        return;
    }
    jsonTree_->setEnabled(true);
    int count = 0;
    constexpr int maxNodes = 5000;
    constexpr int maxDepth = 64;
    std::function<void(QTreeWidgetItem *, const QString &, const QJsonValue &, int)> append;
    append = [&](QTreeWidgetItem *parent, const QString &key, const QJsonValue &value, int depth) {
        if (++count > maxNodes) return;
        QString display;
        if (value.isObject()) display = QStringLiteral("{…}");
        else if (value.isArray()) display = QStringLiteral("[…]");
        else if (value.isString()) display = value.toString();
        else if (value.isNull()) display = QStringLiteral("null");
        else display = value.toVariant().toString();
        auto *item = new QTreeWidgetItem(parent, {key, display});
        if (depth >= maxDepth || count >= maxNodes) return;
        if (value.isObject()) {
            const auto object = value.toObject();
            for (auto it = object.begin(); it != object.end() && count < maxNodes; ++it)
                append(item, it.key(), it.value(), depth + 1);
        } else if (value.isArray()) {
            const auto array = value.toArray();
            for (qsizetype i = 0; i < array.size() && count < maxNodes; ++i)
                append(item, QStringLiteral("[%1]").arg(i), array.at(i), depth + 1);
        }
    };
    const QJsonValue root = document.isObject() ? QJsonValue(document.object()) : QJsonValue(document.array());
    append(jsonTree_->invisibleRootItem(), document.isArray() ? QStringLiteral("[root]") : QStringLiteral("{root}"), root, 0);
    if (count >= maxNodes) {
        auto *notice = new QTreeWidgetItem(jsonTree_->invisibleRootItem(), {QStringLiteral("…"), QStringLiteral("Árbol limitado a 5000 elementos")});
        Q_UNUSED(notice);
    }
    jsonTree_->expandToDepth(1);
}

void MainWindow::findInResponse() {
    const QString query = responseSearch_->text();
    if (query.isEmpty()) return;
    if (!responseBody_->find(query)) {
        QTextCursor cursor = responseBody_->textCursor();
        cursor.movePosition(QTextCursor::Start);
        responseBody_->setTextCursor(cursor);
        if (!responseBody_->find(query)) {
            statusLabel_->setText(QStringLiteral("No se encontró «%1» en la respuesta.").arg(query));
            return;
        }
    }
    responseBody_->setFocus();
    statusLabel_->setText(QStringLiteral("Coincidencia encontrada en el cuerpo de la respuesta."));
}

void MainWindow::compareHistoryResponses() {
    const auto selectedRows = historyTable_->selectionModel()->selectedRows();
    if (selectedRows.size() != 2) {
        QMessageBox::information(this, QStringLiteral("Selecciona dos respuestas"), QStringLiteral("Selecciona exactamente dos filas del historial (Ctrl/Cmd + clic) y vuelve a comparar."));
        return;
    }
    qint64 firstId=-1, secondId=-1;
    for (const auto &item : {historyTable_->item(selectedRows.at(0).row(),3), historyTable_->item(selectedRows.at(1).row(),3)}) {
        if (!item) continue;
        if (firstId < 0) firstId=item->data(Qt::UserRole).toLongLong(); else secondId=item->data(Qt::UserRole).toLongLong();
    }
    if(firstId<0||secondId<0||firstId==secondId){QMessageBox::warning(this,QStringLiteral("No se pudieron identificar"),QStringLiteral("No pude cargar las dos entradas seleccionadas."));return;}
    QVariantMap first,second;
    for(const auto &item:store_.history(projectId_,1000)){
        const qint64 id=item.value(QStringLiteral("id")).toLongLong();
        if(id==firstId)first=item;else if(id==secondId)second=item;
    }
    if(first.isEmpty()||second.isEmpty()){QMessageBox::warning(this,QStringLiteral("Historial no disponible"),QStringLiteral("Una de las respuestas ya no está disponible en el historial reciente."));return;}
    // Keep visual order chronological regardless of click order.
    if(first.value(QStringLiteral("id")).toLongLong()>second.value(QStringLiteral("id")).toLongLong())std::swap(first,second);
    QDialog dialog(this);dialog.setWindowTitle(QStringLiteral("Comparar respuestas · BETA"));dialog.setWindowFlag(Qt::WindowContextHelpButtonHint, false);dialog.resize(860,580);
    auto *layout=new QVBoxLayout(&dialog);
    auto *description=new QLabel(QStringLiteral("Comparación local del historial. No se envían nuevas solicitudes."));description->setWordWrap(true);layout->addWidget(description);
    auto *meta=new QLabel;meta->setWordWrap(true);
    meta->setText(QStringLiteral("Anterior: %1 %2 · HTTP %3 · %4 ms\nPosterior: %5 %6 · HTTP %7 · %8 ms")
        .arg(first.value(QStringLiteral("method")).toString(),first.value(QStringLiteral("url")).toString(),first.value(QStringLiteral("status")).toString(),QString::number(first.value(QStringLiteral("elapsed_ms")).toLongLong()),
             second.value(QStringLiteral("method")).toString(),second.value(QStringLiteral("url")).toString(),second.value(QStringLiteral("status")).toString(),QString::number(second.value(QStringLiteral("elapsed_ms")).toLongLong())));
    layout->addWidget(meta);
    QJsonParseError beforeError,afterError;
    const auto beforeDoc=QJsonDocument::fromJson(first.value(QStringLiteral("response_body")).toString().toUtf8(),&beforeError);
    const auto afterDoc=QJsonDocument::fromJson(second.value(QStringLiteral("response_body")).toString().toUtf8(),&afterError);
    auto *table=new QTableWidget(0,4,&dialog);table->setHorizontalHeaderLabels({QStringLiteral("Tipo"),QStringLiteral("Ruta JSON"),QStringLiteral("Anterior"),QStringLiteral("Posterior")});
    table->horizontalHeader()->setSectionResizeMode(1,QHeaderView::ResizeToContents);table->horizontalHeader()->setSectionResizeMode(2,QHeaderView::Stretch);table->horizontalHeader()->setSectionResizeMode(3,QHeaderView::Stretch);table->setWordWrap(true);table->setTextElideMode(Qt::ElideMiddle);
    QString limitation;
    if(!beforeDoc.isNull()&&!afterDoc.isNull()){
        const QJsonValue before=beforeDoc.isObject()?QJsonValue(beforeDoc.object()):QJsonValue(beforeDoc.array());
        const QJsonValue after=afterDoc.isObject()?QJsonValue(afterDoc.object()):QJsonValue(afterDoc.array());
        const auto diff=testing::compareJson(before,after,2000,48);
        for(const auto &change:diff.changes){const int row=table->rowCount();table->insertRow(row);
            const QString kind=testing::jsonChangeKindName(change.kind);auto *type=new QTableWidgetItem(kind);
            // Colores del diff desde los tokens del tema (theme.h), no literales.
            type->setForeground(change.kind==testing::JsonChangeKind::Added?QColor(QString::fromLatin1(theme::kSuccess))
                :change.kind==testing::JsonChangeKind::Removed?QColor(QString::fromLatin1(theme::kDanger))
                :QColor(QString::fromLatin1(theme::kWarning)));
            table->setItem(row,0,type);table->setItem(row,1,new QTableWidgetItem(change.path));table->setItem(row,2,new QTableWidgetItem(testing::jsonValueForDiff(change.before)));table->setItem(row,3,new QTableWidgetItem(testing::jsonValueForDiff(change.after)));
        }
        if(diff.truncated)limitation=QStringLiteral("La comparación se limitó a 2000 cambios y 48 niveles por seguridad.");
        else if(diff.changes.isEmpty())limitation=QStringLiteral("No hay diferencias en los cuerpos JSON guardados.");
    } else {
        limitation=QStringLiteral("No se pudo comparar el cuerpo como JSON (puede ser texto omitido por la redacción segura del historial). La comparación de estado HTTP y duración sigue disponible arriba.");
    }
    if(table->rowCount()==0)table->setRowCount(1),table->setItem(0,0,new QTableWidgetItem(QStringLiteral("Sin diferencias"))),table->setItem(0,1,new QTableWidgetItem(limitation));
    layout->addWidget(table,1);
    if(!limitation.isEmpty()&&table->rowCount()>0&&table->item(0,0)->text()!=QStringLiteral("Sin diferencias")){auto *note=new QLabel(limitation);note->setWordWrap(true);layout->addWidget(note);}
    auto *close=new QDialogButtonBox(QDialogButtonBox::Close,&dialog);connect(close,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);connect(close,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);layout->addWidget(close);
    dialog.exec();
}

void MainWindow::showHistory(const QModelIndex &index) {
    if (!index.isValid()) return;
    const auto *nameItem = historyTable_->item(index.row(), 3);
    if (!nameItem) return;
    const qint64 id = nameItem->data(Qt::UserRole).toLongLong();
    for (const auto &item : store_.history(projectId_, 100)) {
        if (item.value(QStringLiteral("id")).toLongLong() != id) continue;
        responseMeta_->setText(QStringLiteral("Historial · %1 · %2 ms · %3 bytes").arg(item.value(QStringLiteral("status")).toString()).arg(item.value(QStringLiteral("elapsed_ms")).toLongLong()).arg(item.value(QStringLiteral("response_size")).toLongLong()));
        responseBody_->setPlainText(item.value(QStringLiteral("response_body")).toString());
        populateJsonTree(responseBody_->toPlainText());
        responseHeaders_->setPlainText(item.value(QStringLiteral("response_headers")).toString());
        break;
    }
}

} // namespace reqflow::ui
