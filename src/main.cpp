#include "reqflow/ui/mainwindow.h"
#include "reqflow/ui/authdialog.h"

#include <QApplication>
#include <QCoreApplication>
#include <QMessageBox>

int main(int argc, char *argv[]) {
    QApplication application(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("ReqFlow"));
    QCoreApplication::setApplicationName(QStringLiteral("ReqFlow AI"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.2.0"));

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
