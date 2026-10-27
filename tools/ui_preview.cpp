// Previsualizador de UI (docs/UI.md).
//
// Construye los widgets principales de ReqFlow AI con el tema oscuro unificado
// y vuelca capturas PNG para poder revisar la interfaz sin ejecutar la app
// completa (y sin tocar la base real: QStandardPaths va en modo de prueba).
//
// No forma parte de la app ni de la CI (EXCLUDE_FROM_ALL). Uso:
//   cmake --build <build> --target reqflow-ui-preview
//   <build>/reqflow-ui-preview [directorio_de_salida]

#include "reqflow/data/projectstore.h"
#include "reqflow/flow/floweditor.h"
#include "reqflow/ui/authdialog.h"
#include "reqflow/ui/mainwindow.h"
#include "reqflow/ui/theme.h"

#include <QApplication>
#include <QDir>
#include <QLabel>
#include <QStandardPaths>
#include <QStyleFactory>
#include <QVBoxLayout>
#include <QWidget>

#include <cstdio>

namespace {

// Muestra el widget, deja que Qt lo repinte (offscreen también repinta) y
// guarda la captura. Devuelve false si el fichero no se pudo escribir.
bool grab(QWidget *widget, const QString &path) {
    widget->show();
    QApplication::processEvents();
    QApplication::processEvents();
    const bool saved = widget->grab().save(path);
    std::fprintf(saved ? stdout : stderr, "%s: %s\n", qPrintable(path),
                 saved ? "captura guardada" : "ERROR al guardar la captura");
    widget->hide();
    QApplication::processEvents();
    return saved;
}

} // namespace

int main(int argc, char *argv[]) {
    // Perfil de prueba: la base y las capturas no tocan los datos reales.
    QStandardPaths::setTestModeEnabled(true);

    QApplication application(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("ReqFlow"));
    QCoreApplication::setApplicationName(QStringLiteral("ReqFlow AI · ui-preview"));

    // Mismo orden de aplicación del tema que en main().
    application.setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
    application.setPalette(reqflow::ui::theme::darkPalette());
    application.setStyleSheet(reqflow::ui::theme::appStyleSheet());

    const QString outputDir = argc > 1 ? QString::fromLocal8Bit(argv[1])
                                       : QStringLiteral("ui-preview-shots");
    QDir dir(outputDir);
    if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
        std::fprintf(stderr, "No se pudo crear el directorio de salida: %s\n",
                     qPrintable(outputDir));
        return 1;
    }

    reqflow::data::ProjectStore store;
    if (!store.isOpen()) {
        std::fprintf(stderr, "No se pudo abrir la base local de prueba: %s\n",
                     qPrintable(store.lastError()));
        return 1;
    }

    bool ok = true;

    // 1. Login (pantalla de entrada real, sin credenciales).
    {
        reqflow::ui::AuthDialog dialog(store, false);
        ok = grab(&dialog, dir.filePath(QStringLiteral("01_login.png"))) && ok;
    }
    // 2. Primer arranque / creación de cuenta local.
    {
        reqflow::ui::AuthDialog dialog(store, true);
        ok = grab(&dialog, dir.filePath(QStringLiteral("02_alta_cuenta.png"))) && ok;
    }
    // 3. Ventana principal con el proyecto recién creado.
    {
        reqflow::ui::MainWindow window;
        window.show();
        QApplication::processEvents();
        ok = grab(&window, dir.filePath(QStringLiteral("03_ventana_principal.png"))) && ok;
        window.close();
        QApplication::processEvents();
    }
    // 4. Editor visual de flujos (lienzo y panel lateral).
    {
        const qint64 projectId = store.createProject(QStringLiteral("Preview"));
        QWidget container;
        auto *layout = new QVBoxLayout(&container);
        auto *editor = new reqflow::flow::FlowEditor(&store, projectId, &container);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->addWidget(editor);
        container.resize(1100, 700);
        ok = grab(&container, dir.filePath(QStringLiteral("04_editor_flujos.png"))) && ok;
    }

    std::fprintf(stdout, "Capturas en %s\n", qPrintable(dir.absolutePath()));
    return ok ? 0 : 1;
}
