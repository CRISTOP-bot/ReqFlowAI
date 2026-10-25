#include "reqflow/ui/mainwindow.h"

#include <QApplication>
#include <QCoreApplication>
#include <QIcon>

int main(int argc, char *argv[]) {
    QApplication application(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("ReqFlow"));
    QCoreApplication::setApplicationName(QStringLiteral("ReqFlow AI"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.2.0"));
    reqflow::ui::MainWindow window;
    window.show();
    return application.exec();
}
