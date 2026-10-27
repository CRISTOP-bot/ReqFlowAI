# Interfaz de usuario: sistema de diseño y UX

Estado: **implementado** (tema oscuro unificado + mejoras de UX aplicadas el 26 de octubre de 2026). La interfaz es 100 % programática en Qt 6 Widgets (no hay ficheros `.ui`, `.qml`, `.qrc` ni hojas `.qss` por fichero): todo el estilo vive en un único módulo de código.

## Sistema de tema

| Pieza | Fichero | Papel |
|---|---|---|
| Tokens de color | `include/reqflow/ui/theme.h` | Única fuente de verdad: 24 constantes `k*` (fondos, superficies, bordes, texto, acento, estados) |
| Hoja global (QSS) | `src/ui/theme.cpp` → `appStyleSheet()` | QSS con marcadores `{{token}}` que se interpolan con los valores del header; define el estilo de todos los controles |
| Paleta auxiliar | `src/ui/theme.cpp` → `darkPalette()` | Cubre lo que la QSS no gestiona (flechas de `QComboBox`, texto deshabilitado, placeholder, selección) |
| Aplicación | `src/main.cpp` | Una sola vez, antes de crear widgets: `setStyle("Fusion")` + `setPalette` + `setStyleSheet` |

Convicciones de la arquitectura:

- **Un solo punto de aplicación.** `MainWindow` y `AuthDialog` ya no declaran hojas locales (antes convivían tres paletas sin relación: azul en la ventana principal, marrón cálido en el login y violetas sueltos).
- **Sin literales hex sueltos en el código de widgets.** El pintado personalizado (nodos del editor de flujos, colores del diff de respuestas, texto de marca) lee los tokens de `theme.h`. Si hace falta un color nuevo, se añade un token; si hace falta un estilo nuevo, se añade una regla en `appStyleSheet()` usando `{{token}}`.
- **Marca seleccionable:** el estilo usa la QSS, así que el texto seleccionado, el foco y los estados deshabilitados son consistentes en todos los controles.

### Variantes de botón

Los botones son secundarios por defecto. Las propiedades dinámicas marcan las excepciones:

| Propiedad | Uso | Ejemplos |
|---|---|---|
| `setProperty("variant", "primary")` | Acción principal de la vista | Enviar, Entrar/Crear cuenta, Ejecutar suite, Ejecutar flujo |
| `setProperty("variant", "danger")` | Acción destructiva | Eliminar entorno |
| `setObjectName("cancel")` | Cerrar/cancelar, siempre a la izquierda del primario | Cancelar, Cerrar |

Regla de orden de botones: `[secundarios a la izquierda] … [Cerrar/Cancelar] [secundario] [primario a la derecha]`.

## Accesos de teclado

Declarados como `QAction` de la ventana principal (`MainWindow::buildUi`), por lo que funcionan con cualquier widget enfocado:

| Atajo | Acción |
|---|---|
| `Ctrl+Intro` | Enviar la petición |
| `Ctrl+S` | Guardar la petición actual |
| `Ctrl+O` | Importar una especificación OpenAPI |
| `Ctrl+L` | Enfocar y seleccionar la URL |
| `Ctrl+F` | Enfocar el buscador de la respuesta |

En el login, `Intro` avanza del usuario a la contraseña y envía; `Esc` cierra (botón Cancelar). Los diálogos auxiliares cierran con `Esc` o con su botón Cerrar.

## Barra de estado y estados vacíos

- `statusLabel_` vive ahora en la **barra de estado real** de `QMainWindow` (`QStatusBar`), no como una etiqueta más del layout central: los mensajes («Enviando…», resultados de suites, errores de red) permanecen visibles al cambiar el tamaño de la ventana.
- Historial vacío: el título cambia a «Historial reciente · aún no hay peticiones» (`refreshProject`).
- Respuesta vacía: `responseMeta_` muestra «Envía una petición para ver la respuesta».
- Editor de flujos: el panel de log tiene placeholder propio («Aquí aparecerán los resultados de validar o ejecutar el flujo»).

## Iconos y tooltips

- Iconos estándar de Qt (`QStyle::standardIcon`): nuevo proyecto, importar, guardar, enviar, buscar siguiente. No se introducen ficheros de icono ni recursos `.qrc`.
- Todos los botones de acción llevan tooltip con su efecto y su atajo («Guardar la petición actual (Ctrl+S)»); los nodos del lienzo explican interacción («Arrastra para moverlo; clic para editarlo»).

## Diálogos

- Los cuatro diálogos de `MainWindow` (Entornos, Suites, Editor de flujos, Comparar respuestas) desactivan el botón de ayuda de contexto (`WindowContextHelpButtonHint`) y exponen un cierre explícito (**Cerrar**); antes solo se cerraban con la X de la ventana.
- `AuthDialog` conserva sus `objectName` (`card`, `eyebrow`, `subtitle`, `hint`, `localnote`, `error`, `cancel`) porque la QSS global los referencia.
- **Los ramales `#ifdef Q_OS_ANDROID` se conservan intactos** (tamaños, filas alternas, márgenes) en `mainwindow.cpp` y `authdialog.cpp`.

## Validación

| Qué | Cómo | Cubre |
|---|---|---|
| `tests/cpp/test_ui_theme.cpp` (target `reqflow-ui-theme-tests`, corre en `ctest`) | `QT_QPA_PLATFORM=offscreen` | QSS sin marcadores `{{` residuales, reglas de variantes/`#cancel` presentes, parseo sin avisos «stylesheet» de Qt, 24 tokens con `QColor` válido, paleta oscura y con el acento |
| `tools/ui_preview.cpp` (target `reqflow-ui-preview`, `EXCLUDE_FROM_ALL`) | `cmake --build <build> --target reqflow-ui-preview && <build>/reqflow-ui-preview <salida>` | Construye `AuthDialog`, `MainWindow` y `FlowEditor` en modo `QStandardPaths` de prueba y vuelca 4 PNG para revisión visual manual |

No cubierto por pruebas automáticas: el aspecto renderido (colores finos, espaciados) y el comportamiento en pantalla real de Android; eso se revisa con las capturas del previsualizador y con la app instalada.

La CI Linux necesita `qt6-qpa-plugins` (plugin `offscreen`) para poder ejecutar el test de tema sin display; está añadido al `apt-get` de `.github/workflows/linux-build.yml`.
