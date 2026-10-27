#include "reqflow/flow/floweditor.h"
#include "reqflow/flow/flowgraphics.h"
#include "reqflow/security/secrets.h"
#include "reqflow/ui/theme.h"

#include <QHBoxLayout>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QSplitter>
#include <QPen>
#include <QLineF>
#include <QGraphicsLineItem>
#include <QGraphicsTextItem>
#include <algorithm>
#include <QVBoxLayout>
#include <QUuid>

namespace reqflow::flow {
namespace {
QString nodeId() { return QUuid::createUuid().toString(QUuid::WithoutBraces).left(8); }
QJsonObject storedSafe(const QJsonObject &input) {
    QJsonObject out;
    for (auto it=input.begin(); it!=input.end(); ++it) {
        if (security::nameLooksSensitive(it.key())) out.insert(it.key(), QStringLiteral("[REDACTED]"));
        else if (it.value().isString() && security::valueLooksLikeCredential(it.value().toString())) out.insert(it.key(), QStringLiteral("[REDACTED]"));
        else if (it.value().isObject()) out.insert(it.key(), storedSafe(it.value().toObject()));
        else out.insert(it.key(), it.value());
    }
    return out;
}
}

FlowEditor::FlowEditor(data::ProjectStore *store, qint64 projectId, QWidget *parent)
    : QWidget(parent), store_(store), projectId_(projectId) {
    auto *root = new QVBoxLayout(this);
    auto *top = new QHBoxLayout;
    savedFlows_ = new QComboBox(this); savedFlows_->setMinimumWidth(220);
    name_ = new QLineEdit(this); name_->setPlaceholderText(QStringLiteral("Nombre del flujo"));
    type_ = new QComboBox(this); type_->addItems({"start", "request", "extract", "transform", "condition", "result"});
    auto *add = new QPushButton(QStringLiteral("Añadir nodo"), this);
    add->setToolTip(QStringLiteral("Añadir un nodo del tipo seleccionado al lienzo"));
    auto *connectButton = new QPushButton(QStringLiteral("Conectar nodos"), this);
    connectButton->setToolTip(QStringLiteral("Crear una arista entre dos nodos (sin bucles directos)"));
    auto *validateButton = new QPushButton(QStringLiteral("Validar"), this);
    validateButton->setToolTip(QStringLiteral("Comprobar que el grafo es acíclico y alcanzable; no envía red"));
    auto *runButton = new QPushButton(QStringLiteral("Ejecutar"), this);
    runButton->setProperty("variant", "primary");
    runButton->setToolTip(QStringLiteral("Ejecutar el flujo: pide confirmación antes de enviar cualquier petición"));
    auto *saveButton = new QPushButton(QStringLiteral("Guardar flujo"), this);
    saveButton->setToolTip(QStringLiteral("Guardar el flujo en la base local (sin credenciales)"));
    top->addWidget(savedFlows_); top->addWidget(name_, 1); top->addWidget(type_); top->addWidget(add); top->addWidget(connectButton); top->addWidget(validateButton); top->addWidget(runButton); top->addWidget(saveButton);
    root->addLayout(top);
    auto *split = new QSplitter(Qt::Horizontal, this);
    scene_ = new QGraphicsScene(this); view_ = new QGraphicsView(scene_, this); view_->setRenderHint(QPainter::Antialiasing); view_->setDragMode(QGraphicsView::RubberBandDrag); view_->setMinimumHeight(260);
#ifndef Q_OS_ANDROID
    // Con la escena vacía el splitter encoge el lienzo a casi nada; en
    // escritorio se reserva ancho para que el editor siempre sea usable.
    view_->setMinimumWidth(360);
#endif
    split->addWidget(view_);
    auto *right = new QWidget(this); auto *rightLayout = new QVBoxLayout(right);
    auto *configTitle = new QLabel(QStringLiteral("Configuración JSON del nodo seleccionado"), right);
    configTitle->setObjectName(QStringLiteral("sectionTitle"));
    rightLayout->addWidget(configTitle);
    config_ = new QPlainTextEdit(right); config_->setPlaceholderText(QStringLiteral("{\n  \"url\": \"https://…\"\n}")); rightLayout->addWidget(config_, 1);
    auto *apply = new QPushButton(QStringLiteral("Aplicar configuración"), right);
    apply->setToolTip(QStringLiteral("Validar el JSON y aplicarlo al nodo seleccionado"));
    rightLayout->addWidget(apply);
    auto *logTitle = new QLabel(QStringLiteral("Validación y ejecución"), right);
    logTitle->setObjectName(QStringLiteral("sectionTitle"));
    rightLayout->addWidget(logTitle);
    log_ = new QPlainTextEdit(right); log_->setReadOnly(true);
    log_->setPlaceholderText(QStringLiteral("Aquí aparecerán los resultados de validar o ejecutar el flujo."));
    rightLayout->addWidget(log_, 1);
    split->addWidget(right); split->setStretchFactor(0, 3); split->setStretchFactor(1, 2); root->addWidget(split, 1);
    connect(add, &QPushButton::clicked, this, &FlowEditor::addNode);
    connect(connectButton, &QPushButton::clicked, this, [this] {
        if (flow_.nodes.size() < 2) { appendLog(QStringLiteral("Agrega al menos dos nodos.")); return; }
        QStringList ids; for (const auto &node : flow_.nodes) ids << node.id;
        bool ok=false; const QString from=QInputDialog::getItem(this, QStringLiteral("Conectar nodos"), QStringLiteral("Desde:"), ids, 0, false, &ok); if(!ok) return;
        const QString to=QInputDialog::getItem(this, QStringLiteral("Conectar nodos"), QStringLiteral("Hacia:"), ids, 0, false, &ok); if(!ok) return;
        if(from==to) { appendLog(QStringLiteral("No se permiten bucles directos.")); return; }
        QString label;
        auto it=std::find_if(flow_.nodes.cbegin(),flow_.nodes.cend(),[&](const FlowNode &n){return n.id==from;});
        if(it!=flow_.nodes.cend() && it->type==QStringLiteral("condition")) label=QInputDialog::getItem(this,QStringLiteral("Salida condicional"),QStringLiteral("Rama:"),QStringList{"true","false"},0,false,&ok); else label=QStringLiteral("next");
        if(it!=flow_.nodes.cend() && it->type==QStringLiteral("condition") && !ok) return;
        for(const auto &edge:flow_.edges) if(edge.source==from && edge.label==label){appendLog(QStringLiteral("Esa salida ya está conectada."));return;}
        flow_.edges.append({from,to,label}); rebuildCanvas();
    });
    connect(validateButton, &QPushButton::clicked, this, &FlowEditor::validateFlow);
    connect(runButton, &QPushButton::clicked, this, &FlowEditor::runFlow);
    connect(saveButton, &QPushButton::clicked, this, &FlowEditor::saveFlow);
    connect(apply, &QPushButton::clicked, this, &FlowEditor::applyConfig);
    connect(savedFlows_, qOverload<int>(&QComboBox::currentIndexChanged), this, &FlowEditor::loadSelectedFlow);
    for (const auto &row: store_->flows(projectId_)) savedFlows_->addItem(row.value(QStringLiteral("name")).toString(),row.value(QStringLiteral("id")));
    if(savedFlows_->count()) loadSelectedFlow(0); else { flow_.name=QStringLiteral("Nuevo flujo"); name_->setText(flow_.name); }
}

void FlowEditor::appendLog(const QString &text) { log_->appendPlainText(text); }
QJsonObject FlowEditor::currentJson() const { FlowDefinition copy=flow_; copy.name=name_->text().trimmed(); for(auto &node:copy.nodes) node.config=storedSafe(node.config); return toJson(copy); }
bool FlowEditor::safeToPersist() const {
    for(const auto &node:flow_.nodes){
        // M3: claves y valores sensibles (incluidos cuerpos JSON en texto plano).
        if(security::jsonLooksSensitive(node.config)) return false;
        const auto data=QString::fromUtf8(QJsonDocument(node.config).toJson(QJsonDocument::Compact));
        // También el texto serializado, para pillar nombres de cabecera dentro de cadenas.
        if(security::nameLooksSensitive(data)) return false;
    }
    return true;
}
void FlowEditor::addNode() {
    const QString type=type_->currentText(); FlowNode node{nodeId(),type,{}};
    if(type=="request") node.config={{"method","GET"},{"url","https://example.test/"}};
    if(type=="extract") node.config={{"variable","value"},{"source","response.json"},{"path","$.data"}};
    if(type=="transform") node.config={{"variable","value"},{"template","{{response.status}}"}};
    if(type=="condition") node.config={{"variable","response.status"},{"operator","equals"},{"value",200}};
    if(type=="result") node.config={{"message","Flujo terminado"}};
    if(type=="start" && std::any_of(flow_.nodes.cbegin(),flow_.nodes.cend(),[](const FlowNode &n){return n.type=="start";})){ appendLog(QStringLiteral("Ya existe un nodo start.")); return; }
    flow_.nodes.append(node); positions_[node.id]=QPointF(30+(flow_.nodes.size()%3)*230,30+((flow_.nodes.size()-1)/3)*120); rebuildCanvas();
}
void FlowEditor::applyConfig() {
    auto it=std::find_if(flow_.nodes.begin(),flow_.nodes.end(),[this](const FlowNode &n){return n.id==selectedNodeId_;}); if(it==flow_.nodes.end()) return;
    QJsonParseError error; const auto doc=QJsonDocument::fromJson(config_->toPlainText().toUtf8(),&error); if(error.error!=QJsonParseError::NoError||!doc.isObject()){QMessageBox::warning(this,QStringLiteral("JSON inválido"),error.errorString());return;}
    it->config=doc.object(); rebuildCanvas(); appendLog(QStringLiteral("Configuración aplicada a ")+it->id);
}
void FlowEditor::saveFlow() {
    flow_.name=name_->text().trimmed(); if(flow_.name.isEmpty()){QMessageBox::warning(this,QStringLiteral("Nombre requerido"),QStringLiteral("Escribe un nombre para el flujo."));return;}
    if(!safeToPersist()){QMessageBox::warning(this,QStringLiteral("Posible secreto"),QStringLiteral("No se guardó: quita credenciales del flujo. La redacción es heurística, no una bóveda segura."));return;}
    const auto definition=QString::fromUtf8(QJsonDocument(currentJson()).toJson(QJsonDocument::Compact));
    flowId_=store_->saveFlow(projectId_,flowId_,flow_.name,definition); if(flowId_<0){QMessageBox::warning(this,QStringLiteral("Error SQLite"),store_->lastError());return;}
    int index=savedFlows_->findData(flowId_); if(index<0){savedFlows_->addItem(flow_.name,flowId_);index=savedFlows_->count()-1;} savedFlows_->setCurrentIndex(index); appendLog(QStringLiteral("Flujo guardado localmente."));
}
void FlowEditor::validateFlow(){flow_.name=name_->text().trimmed();const auto errors=validate(flow_); if(errors.isEmpty()) appendLog(QStringLiteral("Validación correcta: grafo acíclico y alcanzable.")); else appendLog(errors.join(QStringLiteral("\n")));}
void FlowEditor::runFlow(){
    flow_.name=name_->text().trimmed();
    // B2/AGENT.md: confirmar antes de toda ejecución de flujo que enviará red, listando destinos.
    QStringList destinations;
    for(const auto &node:flow_.nodes)
        if(node.type==QStringLiteral("request"))
            destinations.append(QStringLiteral("%1 %2").arg(node.config.value(QStringLiteral("method")).toString(QStringLiteral("GET")), node.config.value(QStringLiteral("url")).toString()));
    if(destinations.isEmpty()) destinations.append(QStringLiteral("(el flujo no tiene nodos de petición)"));
    if(QMessageBox::question(this, QStringLiteral("Confirmar ejecución de red"),
        QStringLiteral("El flujo enviará peticiones de red a estos destinos:\n\n%1\n\nVerifica método y URL. ¿Ejecutar ahora?").arg(destinations.join(QLatin1Char('\n'))))!=QMessageBox::Yes) return;
    FlowEngine engine;
    const auto result=engine.run(flow_, {}, nullptr, 500,
        [this](const QUrl &from, const QUrl &to) {
            if(network::isSameOrigin(from, to)) return true;
            return QMessageBox::question(this, QStringLiteral("Redirección cruzada"),
                QStringLiteral("El servidor pide redirigir de %1 a %2.\n\n¿Permitir este salto?").arg(from.toString(), to.toString()))==QMessageBox::Yes;
        });
    QJsonObject saved{{"status",result.status},{"error",result.error},{"steps",static_cast<int>(result.logs.size())}}; if(flowId_>0) store_->addFlowRun(flowId_,result.status,QString::fromUtf8(QJsonDocument(saved).toJson(QJsonDocument::Compact))); appendLog(QStringLiteral("Ejecución: ")+result.status+(result.error.isEmpty()?QString{}:QStringLiteral(" · ")+result.error)); for(const auto &entry:result.logs) appendLog(QStringLiteral("%1 · %2 · %3 ms · %4").arg(entry.nodeId,entry.status).arg(entry.elapsedMs).arg(entry.message));}
void FlowEditor::loadSelectedFlow(int index){if(index<0)return;const qint64 id=savedFlows_->itemData(index).toLongLong();for(const auto &row:store_->flows(projectId_))if(row.value("id").toLongLong()==id){QJsonParseError error;const auto doc=QJsonDocument::fromJson(row.value("definition").toString().toUtf8(),&error);if(error.error!=QJsonParseError::NoError||!doc.isObject()){appendLog(QStringLiteral("No se pudo leer el flujo guardado."));return;}flow_=fromJson(doc.object());flowId_=id;name_->setText(flow_.name);positions_.clear();for(int i=0;i<flow_.nodes.size();++i)positions_[flow_.nodes[i].id]=QPointF(30+(i%3)*230,30+(i/3)*120);rebuildCanvas();return;}}
void FlowEditor::selectedNodeChanged(){auto it=std::find_if(flow_.nodes.cbegin(),flow_.nodes.cend(),[this](const FlowNode &n){return n.id==selectedNodeId_;});if(it!=flow_.nodes.cend())config_->setPlainText(QString::fromUtf8(QJsonDocument(it->config).toJson(QJsonDocument::Indented)));}
void FlowEditor::nodeMoved(const QString &id,const QPointF &position){positions_[id]=position;}
void FlowEditor::rebuildCanvas(){scene_->clear();QMap<QString,FlowNodeItem*> items;for(const auto &node:flow_.nodes){auto *item=new FlowNodeItem(node.id,node.type);scene_->addItem(item);item->setPos(positions_.value(node.id));items[node.id]=item;connect(item,&FlowNodeItem::selected,this,[this](const QString &id){selectedNodeId_=id;selectedNodeChanged();});connect(item,&FlowNodeItem::moved,this,&FlowEditor::nodeMoved);}for(const auto &edge:flow_.edges)if(items.contains(edge.source)&&items.contains(edge.target)){auto *line=scene_->addLine(QLineF(items[edge.source]->pos()+QPointF(95,74),items[edge.target]->pos()),QPen(QColor(QString::fromLatin1(reqflow::ui::theme::kAccentSoft)),2));line->setZValue(-1);auto *label=scene_->addText(edge.label);label->setDefaultTextColor(QColor(QString::fromLatin1(reqflow::ui::theme::kText)));label->setPos((items[edge.source]->pos()+items[edge.target]->pos())/2);}scene_->setSceneRect(scene_->itemsBoundingRect().adjusted(-30,-30,30,30));}

} // namespace reqflow::flow
