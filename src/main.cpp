#include "reqflow/ui/mainwindow.h"
#include "reqflow/ui/authdialog.h"
#include "reqflow/ui/theme.h"

#include <QApplication>
#include <QCoreApplication>
#include <QMessageBox>
#include <QStyleFactory>

#ifdef Q_OS_UNIX
#include <sys/stat.h>
#endif

int main(int argc, char *argv[]) {
#ifdef Q_OS_UNIX
    // M2: con umask 0022 los ficheros nacen 0644 y legibles por el grupo. Se
    // restringe a solo propietario desde el arranque para que la base SQLite y
    // sus derivados (-journal/-wal/-shm) salgan 0600. Ver docs/SECURITY_AUDIT.md.
    ::umask(S_IRWXG | S_IRWXO);
#endif
    QApplication application(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("ReqFlow"));
    QCoreApplication::setApplicationName(QStringLiteral("ReqFlow AI"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.2.0"));

    // Tema oscuro unificado (docs/UI.md): Fusion da un aspecto consistente
    // entre distribuciones; la paleta cubre lo que la QSS no gestiona y la
    // hoja global sustituye a las hojas locales que tenían MainWindow y
    // AuthDialog. Se aplica antes de crear cualquier widget.
    application.setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
    application.setPalette(reqflow::ui::theme::darkPalette());
    application.setStyleSheet(reqflow::ui::theme::appStyleSheet());

    reqflow::data::ProjectStore authStore;
    if (!authStore.isOpen()) {
        QMessageBox::critical(nullptr, QStringLiteral("ReqFlow AI"),
                              QStringLiteral("No se pudo abrir la base local: %1").arg(authStore.lastError()));
        return 1;
    }
    const bool firstRun = !authStore.hasLocalAccount();
    if (!authStore.localAccountError().isEmpty()) {
        QMessageBox::critical(nullptr, QStringLiteral("ReqFlow AI"), authStore.localAccountError());
        return 1;
    }
    reqflow::ui::AuthDialog authentication(authStore, firstRun);
    if (authentication.exec() != QDialog::Accepted)
        return 0;

    reqflow::ui::MainWindow window;
    window.show();
    return application.exec();
}
