#include "reqflow/ui/theme.h"

#include <QLatin1String>
#include <QStringView>

namespace reqflow::ui::theme {
namespace {

// La hoja usa marcadores {{token}} que se sustituyen por los tokens del
// header: así los literales hex viven en un solo sitio (theme.h).
QString interpolate(QString sheet) {
    const struct { const char *token; const char *value; } replacements[] = {
        {"{{windowBg}}", kWindowBg},   {"{{surface}}", kSurface},
        {"{{surface2}}", kSurface2},   {"{{buttonBg}}", kButtonBg},
        {"{{buttonHover}}", kButtonHover}, {"{{buttonPress}}", kButtonPress},
        {"{{inputBg}}", kInputBg},     {"{{border}}", kBorder},
        {"{{borderStrong}}", kBorderStrong}, {"{{text}}", kText},
        {"{{textDim}}", kTextDim},     {"{{textFaint}}", kTextFaint},
        {"{{accent}}", kAccent},       {"{{accentHover}}", kAccentHover},
        {"{{accentPress}}", kAccentPress}, {"{{accentSoft}}", kAccentSoft},
        {"{{accentDark}}", kAccentDark}, {"{{canvas}}", kCanvas},
        {"{{success}}", kSuccess},     {"{{danger}}", kDanger},
        {"{{dangerBg}}", kDangerBg},   {"{{dangerBorder}}", kDangerBorder},
        {"{{dangerHover}}", kDangerHover}, {"{{warning}}", kWarning},
    };
    for (const auto &entry : replacements)
        sheet.replace(QLatin1String(entry.token), QLatin1String(entry.value));
    return sheet;
}

} // namespace

QString appStyleSheet() {
    return interpolate(QStringLiteral(R"reqflow(
/* ===== Base ===== */
QMainWindow, QDialog { background: {{windowBg}}; color: {{text}}; }
QWidget { background: {{windowBg}}; color: {{text}}; }
QLabel { background: transparent; color: {{text}}; }
QLabel#muted, QLabel#subtitle, QLabel#hint { color: {{textDim}}; }
QLabel#eyebrow { color: {{accentSoft}}; font-size: 11px; font-weight: 700; letter-spacing: 2px; }
QLabel#localnote { color: {{success}}; font-size: 11px; }
QLabel#sectionTitle { color: {{accentSoft}}; font-weight: 600; }
QLabel#error { color: #ffd7d2; background: {{dangerBg}}; border: 1px solid {{dangerBorder}}; border-radius: 6px; padding: 8px; }
QWidget:disabled { color: {{textFaint}}; }

/* ===== Campos de texto ===== */
QLineEdit, QPlainTextEdit, QTextEdit, QSpinBox {
    background: {{inputBg}}; color: {{text}};
    border: 1px solid {{border}}; border-radius: 6px; padding: 6px 8px;
    selection-background-color: {{accentDark}}; selection-color: #ffffff;
}
QLineEdit:focus, QPlainTextEdit:focus, QTextEdit:focus { border: 1px solid {{accent}}; }
QLineEdit:disabled, QPlainTextEdit:disabled { color: {{textFaint}}; }

/* ===== ComboBox ===== */
QComboBox {
    background: {{inputBg}}; color: {{text}};
    border: 1px solid {{border}}; border-radius: 6px; padding: 6px 26px 6px 8px;
    min-height: 20px;
}
QComboBox:hover { border: 1px solid {{borderStrong}}; }
QComboBox:focus { border: 1px solid {{accent}}; }
QComboBox::drop-down { subcontrol-origin: padding; subcontrol-position: center right; width: 24px; border: 0; }
QComboBox QAbstractItemView {
    background: {{surface}}; color: {{text}};
    border: 1px solid {{borderStrong}}; selection-background-color: {{accent}};
    selection-color: #ffffff; outline: 0;
}

/* ===== Botones ===== */
QPushButton {
    background: {{buttonBg}}; color: {{text}};
    border: 1px solid transparent; border-radius: 6px;
    padding: 7px 14px; font-weight: 600;
}
QPushButton:hover { background: {{buttonHover}}; }
QPushButton:pressed { background: {{buttonPress}}; }
QPushButton:disabled { background: {{surface2}}; color: {{textFaint}}; }
QPushButton[variant="primary"] { background: {{accent}}; color: #ffffff; }
QPushButton[variant="primary"]:hover { background: {{accentHover}}; }
QPushButton[variant="primary"]:pressed { background: {{accentPress}}; }
QPushButton[variant="primary"]:disabled { background: {{accentDark}}; color: #c9c6f5; }
QPushButton[variant="danger"] { background: {{dangerBg}}; color: #ffd7d2; }
QPushButton[variant="danger"]:hover { background: {{dangerHover}}; }
QPushButton[variant="danger"]:disabled { background: {{surface2}}; color: {{textFaint}}; }
QPushButton#cancel { background: transparent; border: 1px solid {{border}}; color: {{textDim}}; }
QPushButton#cancel:hover { background: {{surface2}}; color: {{text}}; }

/* ===== Pestañas ===== */
QTabWidget::pane { border: 1px solid {{border}}; border-radius: 8px; background: {{surface}}; top: -1px; }
QTabBar::tab {
    background: {{surface2}}; color: {{textDim}};
    padding: 7px 14px; margin-right: 2px;
    border-top-left-radius: 6px; border-top-right-radius: 6px;
}
QTabBar::tab:selected { background: {{surface}}; color: {{accentSoft}}; }
QTabBar::tab:hover:!selected { background: {{buttonHover}}; color: {{text}}; }

/* ===== Tablas y árboles ===== */
QTableWidget, QTreeView, QTreeWidget {
    background: {{inputBg}}; alternate-background-color: {{surface}};
    color: {{text}}; border: 1px solid {{border}}; border-radius: 8px;
    gridline-color: {{border}}; selection-background-color: {{accentDark}};
    selection-color: #ffffff; outline: 0;
}
QHeaderView::section {
    background: {{surface2}}; color: {{textDim}};
    padding: 6px 8px; border: 0;
    border-right: 1px solid {{border}}; border-bottom: 1px solid {{border}};
    font-weight: 600;
}
QTableCornerButton::section { background: {{surface2}}; border: 0; }

/* ===== Barra de estado ===== */
QStatusBar { background: {{surface}}; border-top: 1px solid {{border}}; }
QStatusBar::item { border: 0; }
QStatusBar QLabel { background: transparent; color: {{textDim}}; padding: 2px 8px; }

/* ===== Barras de desplazamiento ===== */
QScrollBar:vertical { background: {{inputBg}}; width: 12px; margin: 0; border-radius: 6px; }
QScrollBar::handle:vertical { background: {{border}}; border-radius: 6px; min-height: 32px; }
QScrollBar::handle:vertical:hover { background: {{borderStrong}}; }
QScrollBar:horizontal { background: {{inputBg}}; height: 12px; margin: 0; border-radius: 6px; }
QScrollBar::handle:horizontal { background: {{border}}; border-radius: 6px; min-width: 32px; }
QScrollBar::handle:horizontal:hover { background: {{borderStrong}}; }
QScrollBar::add-line, QScrollBar::sub-line { width: 0; height: 0; background: transparent; border: 0; }
QScrollBar::add-page, QScrollBar::sub-page { background: transparent; border: 0; }
QScrollBar::corner { background: {{inputBg}}; }

/* ===== Splitter ===== */
QSplitter::handle { background: {{border}}; }
QSplitter::handle:horizontal { width: 2px; }
QSplitter::handle:vertical { height: 2px; }

/* ===== Lienzo del editor de flujos ===== */
QGraphicsView { background: {{canvas}}; border: 1px solid {{border}}; border-radius: 8px; }

/* ===== Tarjeta del login ===== */
QFrame#card { background: {{surface}}; border: 1px solid {{border}}; border-radius: 14px; }

/* ===== Diálogos auxiliares ===== */
QDialogButtonBox QPushButton { min-width: 92px; }
QToolTip { background: {{surface2}}; color: {{text}}; border: 1px solid {{borderStrong}}; padding: 6px 8px; }
QMenu { background: {{surface}}; color: {{text}}; border: 1px solid {{border}}; padding: 4px; }
QMenu::item { padding: 6px 24px 6px 12px; border-radius: 4px; }
QMenu::item:selected { background: {{accent}}; color: #ffffff; }
QMenuBar { background: {{surface}}; color: {{text}}; }
QMenuBar::item:selected { background: {{buttonHover}}; }
)reqflow"));
}

QPalette darkPalette() {
    QPalette palette;
    const auto color = [](const char *value) { return QColor(QString::fromLatin1(value)); };
    palette.setColor(QPalette::Window, color(kWindowBg));
    palette.setColor(QPalette::WindowText, color(kText));
    palette.setColor(QPalette::Base, color(kInputBg));
    palette.setColor(QPalette::AlternateBase, color(kSurface));
    palette.setColor(QPalette::ToolTipBase, color(kSurface2));
    palette.setColor(QPalette::ToolTipText, color(kText));
    palette.setColor(QPalette::Text, color(kText));
    palette.setColor(QPalette::Button, color(kButtonBg));
    palette.setColor(QPalette::ButtonText, color(kText));
    palette.setColor(QPalette::BrightText, color(kDanger));
    palette.setColor(QPalette::Link, color(kAccentSoft));
    palette.setColor(QPalette::LinkVisited, color(kAccentSoft));
    palette.setColor(QPalette::Highlight, color(kAccent));
    palette.setColor(QPalette::HighlightedText, color("#ffffff"));
    palette.setColor(QPalette::PlaceholderText, color(kTextFaint));
    palette.setColor(QPalette::Disabled, QPalette::Text, color(kTextFaint));
    palette.setColor(QPalette::Disabled, QPalette::WindowText, color(kTextFaint));
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, color(kTextFaint));
    palette.setColor(QPalette::Disabled, QPalette::Highlight, color(kSurface2));
    return palette;
}

} // namespace reqflow::ui::theme
