#include "reqflow/ui/theme.h"

#include <QColor>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QStatusBar>
#include <QStyleFactory>
#include <QTabWidget>
#include <QTableWidget>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QWidget>
#include <QtTest/QTest>

// Prueba del sistema de tema (docs/UI.md). Cubre:
//  - la hoja global no deja marcadores {{token}} sin sustituir,
//  - la hoja tiene las convenciones documentadas (variantes de botón y #cancel),
//  - la hoja se puede aplicar y parsear por Qt sin avisos "stylesheet",
//  - todos los tokens de color son colores válidos,
//  - la paleta auxiliar es oscura y usa el acento del tema.
// No cubre el aspecto renderizado (eso se revisa con tools/ui_preview.cpp).

namespace {

QtMessageHandler previousHandler = nullptr;
QStringList capturedWarnings;

void collectWarnings(QtMsgType type, const QMessageLogContext &context, const QString &message) {
    if (type == QtWarningMsg) capturedWarnings << message;
    if (previousHandler) previousHandler(type, context, message);
}

} // namespace

class TestUiTheme final : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() {
        // Mismo orden que main(): estilo Fusion, paleta y hoja global.
        qApp->setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
        qApp->setPalette(reqflow::ui::theme::darkPalette());
        qApp->setStyleSheet(reqflow::ui::theme::appStyleSheet());
    }

    void sheetHasNoUnresolvedTokens() {
        const QString sheet = reqflow::ui::theme::appStyleSheet();
        QVERIFY2(!sheet.contains(QStringLiteral("{{")),
                 "Quedan marcadores {{token}} sin sustituir en la hoja global");
        const int open = sheet.count(QLatin1Char('{'));
        const int close = sheet.count(QLatin1Char('}'));
        QCOMPARE(open, close);
        QVERIFY(open > 20);
    }

    void sheetKeepsButtonAndDialogConventions() {
        const QString sheet = reqflow::ui::theme::appStyleSheet();
        // Convenciones documentadas en docs/UI.md que la app depende:
        QVERIFY(sheet.contains(QStringLiteral("QPushButton[variant=\"primary\"]")));
        QVERIFY(sheet.contains(QStringLiteral("QPushButton[variant=\"danger\"]")));
        QVERIFY(sheet.contains(QStringLiteral("QPushButton#cancel")));
        QVERIFY(sheet.contains(QStringLiteral("QStatusBar")));
        QVERIFY(sheet.contains(QStringLiteral("QFrame#card")));
        QVERIFY(sheet.contains(QStringLiteral("QLabel#error")));
    }

    void sheetParsesWithoutWarnings() {
        capturedWarnings.clear();
        previousHandler = qInstallMessageHandler(collectWarnings);

        // Widgets representativos de la app con las propiedades que usa el código.
        QWidget window;
        auto *layout = new QVBoxLayout(&window);
        auto *send = new QPushButton(QStringLiteral("Enviar"), &window);
        send->setProperty("variant", "primary");
        auto *remove = new QPushButton(QStringLiteral("Eliminar"), &window);
        remove->setProperty("variant", "danger");
        auto *cancel = new QPushButton(QStringLiteral("Cerrar"), &window);
        cancel->setObjectName(QStringLiteral("cancel"));
        auto *field = new QLineEdit(&window);
        field->setPlaceholderText(QStringLiteral("https://api.example.com"));
        auto *combo = new QComboBox(&window);
        combo->addItems({QStringLiteral("GET"), QStringLiteral("POST")});
        auto *editor = new QPlainTextEdit(&window);
        auto *tabs = new QTabWidget(&window);
        tabs->addTab(new QLabel(QStringLiteral("Parámetros")), QStringLiteral("Parámetros"));
        auto *table = new QTableWidget(1, 2, &window);
        table->setHorizontalHeaderLabels({QStringLiteral("Método"), QStringLiteral("Estado")});
        auto *note = new QLabel(QStringLiteral("Listo"), &window);
        note->setObjectName(QStringLiteral("muted"));
        auto *error = new QLabel(QStringLiteral("Fallo"), &window);
        error->setObjectName(QStringLiteral("error"));
        layout->addWidget(send);
        layout->addWidget(remove);
        layout->addWidget(cancel);
        layout->addWidget(field);
        layout->addWidget(combo);
        layout->addWidget(editor);
        layout->addWidget(tabs);
        layout->addWidget(table);
        layout->addWidget(note);
        layout->addWidget(error);
        window.show();
        QApplication::processEvents();

        qInstallMessageHandler(previousHandler);
        previousHandler = nullptr;

        for (const QString &warning : capturedWarnings)
            QVERIFY2(!warning.contains(QStringLiteral("stylesheet"), Qt::CaseInsensitive),
                     qPrintable(QStringLiteral("Qt rechazó parte de la hoja: %1").arg(warning)));
        // El resto de avisos se ignoran deliberadamente: pueden venir de
        // plugins o del entorno sin display, no del tema.
    }

    void tokenColorsAreValid() {
        const char *const tokens[] = {
            reqflow::ui::theme::kWindowBg,   reqflow::ui::theme::kSurface,
            reqflow::ui::theme::kSurface2,   reqflow::ui::theme::kButtonBg,
            reqflow::ui::theme::kButtonHover, reqflow::ui::theme::kButtonPress,
            reqflow::ui::theme::kInputBg,    reqflow::ui::theme::kBorder,
            reqflow::ui::theme::kBorderStrong, reqflow::ui::theme::kText,
            reqflow::ui::theme::kTextDim,    reqflow::ui::theme::kTextFaint,
            reqflow::ui::theme::kAccent,     reqflow::ui::theme::kAccentHover,
            reqflow::ui::theme::kAccentPress, reqflow::ui::theme::kAccentSoft,
            reqflow::ui::theme::kAccentDark, reqflow::ui::theme::kCanvas,
            reqflow::ui::theme::kSuccess,    reqflow::ui::theme::kDanger,
            reqflow::ui::theme::kDangerBg,   reqflow::ui::theme::kDangerBorder,
            reqflow::ui::theme::kDangerHover, reqflow::ui::theme::kWarning,
        };
        for (const char *token : tokens) {
            const QColor color(QString::fromLatin1(token));
            QVERIFY2(color.isValid(), token);
            QCOMPARE(color.alpha(), 255);
        }
    }

    void paletteIsDarkAndUsesAccent() {
        const QPalette palette = reqflow::ui::theme::darkPalette();
        QVERIFY2(palette.color(QPalette::Window).lightness() < 100,
                 "La paleta no es oscura: la ventana clara rompería el tema");
        QCOMPARE(palette.color(QPalette::Highlight),
                 QColor(QString::fromLatin1(reqflow::ui::theme::kAccent)));
        QCOMPARE(palette.color(QPalette::PlaceholderText),
                 QColor(QString::fromLatin1(reqflow::ui::theme::kTextFaint)));
    }
};

QTEST_MAIN(TestUiTheme)
#include "test_ui_theme.moc"
