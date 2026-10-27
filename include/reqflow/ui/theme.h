#pragma once

#include <QPalette>
#include <QString>

// Sistema de diseño de la interfaz (tema oscuro unificado).
// Única fuente de verdad de los colores: la QSS global y el pintado
// personalizado (nodos de flujo, diff de respuestas, texto de marca)
// deben leer estos tokens, nunca literales hex sueltos.
// Ver docs/UI.md para convenciones y cómo extender el tema.
namespace reqflow::ui::theme {

// --- Tokens de color (tema oscuro) ---
inline constexpr const char kWindowBg[] = "#111827";   // fondo de ventana
inline constexpr const char kSurface[] = "#161e2e";    // tarjetas, panel de pestañas, barra de estado
inline constexpr const char kSurface2[] = "#1f2937";   // cabeceras, pestaña inactiva, superficie elevada
inline constexpr const char kButtonBg[] = "#25334a";   // botón secundario
inline constexpr const char kButtonHover[] = "#2e3e59";
inline constexpr const char kButtonPress[] = "#1a2334";
inline constexpr const char kInputBg[] = "#0e1625";    // campos de texto, tablas, lienzo
inline constexpr const char kBorder[] = "#2c3b54";
inline constexpr const char kBorderStrong[] = "#3d5170";
inline constexpr const char kText[] = "#e8edf6";
inline constexpr const char kTextDim[] = "#93a4bd";
inline constexpr const char kTextFaint[] = "#64748f";
inline constexpr const char kAccent[] = "#6558e8";      // acento primario (botones)
inline constexpr const char kAccentHover[] = "#7a6ff0";
inline constexpr const char kAccentPress[] = "#5147cf";
inline constexpr const char kAccentSoft[] = "#a99cff";  // marca, enlaces, texto seleccionado
inline constexpr const char kAccentDark[] = "#4338ca";  // selección de filas/nodos
inline constexpr const char kCanvas[] = "#0b1220";      // lienzo del editor de flujos
inline constexpr const char kSuccess[] = "#58c58a";     // añadido / estado local ok
inline constexpr const char kDanger[] = "#ef7b85";      // error / eliminado
inline constexpr const char kDangerBg[] = "#3a2022";
inline constexpr const char kDangerBorder[] = "#7a3b40";
inline constexpr const char kDangerHover[] = "#4a282b";
inline constexpr const char kWarning[] = "#e0a95a";     // modificado / aviso

// Hoja de estilo global de la aplicación. Se aplica una sola vez en main()
// con QApplication::setStyleSheet y sustituye a las hojas locales que
// tenían MainWindow y AuthDialog.
[[nodiscard]] QString appStyleSheet();

// Paleta oscura para lo que la QSS no cubre (flechas de QComboBox,
// indicadores, texto deshabilitado). Se aplica en main().
[[nodiscard]] QPalette darkPalette();

} // namespace reqflow::ui::theme
