# Auditoría de seguridad — 2026-10-26

## Alcance

Revisión estática de todo el código del repositorio (C++20/Qt 6, backend Rust, web estática, manifiesto Android), workflows de GitHub Actions y CMake, con verificación empírica mediante harnesses propios temporales ejecutados sobre Qt 6.12 y yaml-cpp 0.9.0.

Los harnesses eran temporales (no se versionan) y se usaron únicamente para confirmar comportamientos de dependencias antes de diseñar los arreglos. Lo que comprobaron:

- Permiso real de ficheros derivados de SQLite con `umask 022` (salían `0644`/`0755`).
- Seguimiento de redirecciones cross-origin por parte de `QNetworkRequest` con la política por defecto (`NoLessSafeRedirectPolicy`): una petición POST redirigida con 302 a otro puerto llegaba reenviada como GET.
- Colapso por denominación de recursión ("billion laughs") con yaml-cpp: un YAML de unos 391 bytes bloqueaba la conversión a JSON más de 10 s.
- Límites re observados de recursión: yaml-cpp lanza excepción hacia ~2000 niveles y Qt limita JSON a profundidad ~1000.

## Tabla de hallazgos

| ID | Severidad | Componente / fichero | Impacto | Arreglo | Estado |
|----|-----------|----------------------|---------|---------|--------|
| M1 | Media | `src/network/httpclient.cpp`, historial (`MainWindow`, `ProjectStore`) | Cuerpos de respuesta sin límite: memoria agotable con respuestas gigantes | Límite de 10 MiB por respuesta y 2 000 000 caracteres por cuerpo en historial | arreglado en esta entrega |
| M2 | Media | `src/main.cpp`, `src/data/projectstore.cpp` | Base SQLite y derivados legibles por otros usuarios del sistema | `umask 0077`, `chmod 0600` al fichero y `0700` al directorio por defecto | arreglado en esta entrega |
| M3 | Media | `src/data/projectstore.cpp`, `src/ui/mainwindow.cpp`, `src/flow/flowengine.cpp`, `src/flow/floweditor.cpp` | Cuatro patrones de secretos distintos entre módulos: fugas por criterios dispares | Filtro único compartido en `include/reqflow/security/secrets.h` (nombres y valores) | arreglado en esta entrega |
| B1 | Baja | `src/ui/mainwindow.cpp`, `include/reqflow/network/httpclient.h` | Cabeceras HTTP inválidas: Qt 6.12 las descarta en silencio, Qt 6.4 no las validaba | Validación RFC 7230 de nombre/valor de cabeceras en UI y motor | arreglado en esta entrega |
| B2 | Media | `src/network/httpclient.cpp`, `src/ui/mainwindow.cpp`, `src/flow/floweditor.cpp` | Seguimiento de redirecciones cross-origin sin avisar | Política de verificación, máximo 5 redirects, mismo origen por defecto y diálogo al cruzar origen | arreglado en esta entrega |
| B3 | Media | `src/openapi/yaml.cpp`, `src/ui/mainwindow.cpp` | DoS con "billion laughs" al importar especificaciones | Presupuesto de 100 000 nodos YAML y tope de 20 MiB por fichero | arreglado en esta entrega |
| B4 | Media | `src/data/projectstore.cpp`, `src/openapi/openapiimporter.cpp` | Persistencia de URLs con esquemas no web (`file:`, `javascript:`, `data:`, `ftp:`) | Rechazo de esquemas no http(s) al guardar; base de servidor ignorada con advertencia | arreglado en esta entrega |
| B5 | Baja | `CMakeLists.txt`, `.github/workflows/*.yml` | Compromiso de suministro por etiqueta movible (`FetchContent`, actions por tag) | `URL_HASH SHA256` en yaml-cpp y actions fijadas por SHA completa | arreglado en esta entrega |

## Detalle de cada hallazgo

### M1 — Sin límite de tamaño de respuesta

**Qué se encontró.** `HttpClient` leía la respuesta completa con `reply->readAll()` sin ningún límite (`src/network/httpclient.cpp`, antes en la línea 58), y el historial persistía el cuerpo de respuesta sin cap. Una respuesta maliciosa o simplemente enorme podía agotar la memoria del proceso.

**Por qué importa.** El cliente trabaja contra servidores arbitrarios definidos por la persona usuaria; una respuesta de gigabytes se cargaba entera en memoria dos veces (descarga e historial).

**Cómo se arregló.** En `include/reqflow/network/httpclient.h`: `kMaxResponseBytes = 10 * 1024 * 1024` (10 MiB) y `kMaxRedirects = 5`. `HttpClient` vigila `downloadProgress` (Qt 6 no expone un búfer de lectura configurable en `QNetworkAccessManager`; el abort diferido corta la descarga aunque el servidor siga enviando); si `bytesTotal` o `bytesReceived` superan 10 MiB se marca la propiedad `reqflowTooLarge`, se aborta la petición (deferido, para no abortar dentro de la propia señal de progreso) y la respuesta se entrega con cuerpo vacío y error «La respuesta supera el límite de 10 MiB...». Historial: máximo 2 000 000 caracteres por cuerpo de respuesta (`reqflow::data::kMaxHistoryBodyChars`); por encima se guarda un marcador de omisión en lugar del cuerpo. Aplica en `MainWindow::redactResponseBody` y `ProjectStore::safeHistoryBody`, lo que además evita parsear cuerpos gigantes como JSON.

**Prueba que lo cubre.** `httpClientAbortsResponsesOverSizeLimit` (aborto y error al superar el límite) y `historyRedactsByValueAndCapsBodySize` (tope de cuerpo y omisión) en `tests/cpp/test_core.cpp`.

### M2 — Permisos de la base local

**Qué se encontró.** Con el `umask 022` habitual del sistema, los ficheros creados por SQLite salían con permisos `0644` y los directorios `0755` (verificado empíricamente con harness), de modo que cualquier cuenta local del equipo podía leer la base, sus ficheros derivados (`-journal`, `-wal`, `-shm`), el historial y los verificadores de contraseña.

**Por qué importa.** El login es una barrera de UI, no control de acceso; un permiso abierto en el archivo anula esa barrera para otros usuarios locales.

**Cómo se arregló.** `main.cpp`: `umask(S_IRWXG | S_IRWXO)` (0077) al arrancar en Unix, antes de crear la base, para que los ficheros derivados de SQLite salgan `0600`. `ProjectStore`: tras abrir la base hace `chmod 0600` (solo propietario) al fichero SQLite; si se usa la ruta por defecto `~/.local/share/reqflow-ai` (o AppData en Android) hace `chmod 0700` al directorio base. Las rutas personalizadas (tests) no se repermisionan salvo el fichero. Esto **no cifra** la base: el límite sigue siendo el control de acceso del sistema operativo.

**Prueba que lo cubre.** `databaseFilePermissionsAreOwnerOnly` en `tests/cpp/test_core.cpp`.

### M3 — Filtro de secretos desunificado

**Qué se encontró.** Existían **cuatro variantes distintas** del patrón de nombres sensibles, una por módulo: `ProjectStore`, `MainWindow`, `FlowEngine` y `FlowEditor` mantenían copias propias de la expresión regular (por ejemplo, las de `projectstore.cpp` y `mainwindow.cpp` no cubrían `credential` ni `private_key`, mientras la de `flowengine.cpp` sí añadía `auth`). Además, la redacción era solo por nombre: un valor con forma clara de credencial en un campo de nombre inocuo pasaba sin redactar.

**Por qué importa.** Cada módulo con su propio patrón garantiza fugas: lo que un sitio redacta, otro lo persiste o lo muestra.

**Cómo se arregló.** Nuevo cabecero compartido `include/reqflow/security/secrets.h` (namespace `reqflow::security`) con `sensitiveNamePattern()` (patrón único: pass(word)?, passwd, secret, token, api_key, access_key, credential, private_key, cookie, session, client_secret, y auth/orization con límites de palabra `(?:^|[-_.])auth(?:orization)?(?=$|[-_.])` — ya **no** matchea «author» ni «oauth» sueltos), `credentialValuePattern()` (Bearer/Basic + 8 chars, sk-+12, gh[pousr]_+16, AKIA+16 mayúsculas/dígitos, JWT eyJ….…), y funciones `nameLooksSensitive`, `valueLooksLikeCredential`, `jsonLooksSensitive` (claves **y** valores), `textBodyLooksSensitive` (claves de form-urlencoded, etiquetas XML, atributos `name="…"`, valores con patrón de credencial) y `urlLooksSensitive`. Lo usan ahora con el mismo criterio `ProjectStore` (saveRequest, saveFlow, historial), `MainWindow` (guardar/enviar, redacción de historial), `FlowEngine` (detección y redacción) y `FlowEditor` (persistencia). La redacción del historial ahora también es por **valor**: cabeceras, parámetros de URL y valores de campos JSON con forma de credencial se sustituyen por `[REDACTED]` aunque el nombre no parezca sensible.

Sigue sin haber bóveda: la detección es heurística y puede dar falsos positivos y negativos; no es una garantía.

**Prueba que lo cubre.** `secretDetectionUnifiesNamesAndCatchesCredentialValues` y `historyRedactsByValueAndCapsBodySize` en `tests/cpp/test_core.cpp`.

### B1 — Validación de cabeceras HTTP (RFC 7230)

**Qué se encontró.** `MainWindow::sendRequest` construía las cabeceras dividiendo cada línea en el primer `:` sin validar nombre ni valor (antes de `src/ui/mainwindow.cpp`, en la línea 650); solo se requería que hubiera dos puntos. `FlowEngine` ya validaba. Qt 6.12 descarta en silencio cabeceras inválidas y Qt 6.4 no las validaba, así que el comportamiento era inconsistente y silencioso.

**Por qué importa.** Cabeceras con CR/LF en el valor permiten inyección de cabeceras (request smuggling a nivel de texto) y, en el mejor caso, se pierden sin aviso.

**Cómo se arregló.** `reqflow::network::isValidHeaderName` / `isValidHeaderValue` en `httpclient.h/cpp`: nombre solo tchar (`[!#$%&'*+.^_\`|~0-9A-Za-z-]`), valor sin CR, LF, NUL ni otros caracteres de control (<0x20 salvo HTAB) ni DEL (0x7F). Las usa `MainWindow::sendRequest` además de `FlowEngine`, con error claro.

**Prueba que lo cubre.** `httpHeaderValidationFollowsRfc7230` en `tests/cpp/test_core.cpp`.

### B2 — Redirecciones

**Qué se encontró.** `HttpClient` seguía redirecciones con la política por defecto de Qt (`NoLessSafeRedirectPolicy`) sin límite estricto de origen. Verificado con harness: una petición POST redirigida con 302 a otro puerto se reenviaba al destino cross-origin como GET, sin aviso a la persona usuaria.

**Por qué importa.** Un servidor (o un intermediario) puede redirigir la petición —y sus cabeceras, incluida la autorización— a otro origen sin que el usuario lo vea.

**Cómo se arregló.** `HttpClient::send(request, verifier)` usa `QNetworkRequest::UserVerifiedRedirectPolicy` + `setMaximumRedirectsAllowed(5)` (coincide con `kMaxRedirects = 5`). Sin verificador (por defecto, p. ej. flujos en pruebas/CLI): solo se permiten redirecciones al mismo origen (esquema + host + puerto normalizados) respecto de la URL original; si no, se rechaza con error claro «…redirección… rechazada…». `MainWindow` y `FlowEditor` pasan un verificador: mismo origen se permite sin preguntar; distinto origen muestra diálogo de confirmación con origen y destino (`QMessageBox`). `FlowEditor::runFlow` ahora pide confirmación **antes** de ejecutar el flujo listando método y URL de cada nodo de petición (regla de AGENT.md sobre acciones de red automatizadas). Nota técnica Qt: en 301/302 Qt reenvía con GET por compatibilidad; el aviso al usuario muestra el destino real.

**Prueba que lo cubre.** `httpClientFollowsSameOriginRedirectsAndBlocksCrossOrigin` en `tests/cpp/test_core.cpp`.

### B3 — YAML (billion laughs / DoS)

**Qué se encontró.** La conversión YAML→JSON de `src/openapi/yaml.cpp` expandía anidamientos sin presupuesto. Verificado con harness: un fichero de ~391 bytes bloqueaba más de 10 s.

**Por qué importa.** Un archivo de especificación aparentemente inofensivo podía dejar la aplicación inutilizable (o agotar memoria) al importarlo.

**Cómo se arregló.** Presupuesto de 100 000 nodos visitados durante la conversión a JSON; al superarlo se devuelve error «supera el presupuesto de nodos» en lugar de expandir. `MainWindow::importOpenApi`: tope de 20 MiB por fichero de especificación. El parser yaml-cpp ya lanzaba excepción de recursión (~2000 niveles, manejada) y Qt limita JSON a profundidad ~1000: verificado, sin fix necesario.

**Prueba que lo cubre.** `yamlBillionLaughsIsRejectedWithinNodeBudget` en `tests/cpp/test_core.cpp`.

### B4 — Esquema de URL al persistir

**Qué se encontró.** `ProjectStore::saveRequest` y `saveFlow` no validaban el esquema de la URL: se podían guardar referencias `ftp:`, `file:`, `javascript:` o `data:`. `openapiimporter.cpp` tomaba `servers[0].url` sin comprobar el esquema (antes, línea 48-49).

**Por qué importa.** Una especificación importada o un proyecto manipulado podían introducir esquemas que luego se abren o se ejecutan con otras expectativas de seguridad.

**Cómo se arregló.** En `ProjectStore::saveRequest` y `saveFlow`: si la URL tiene esquema y no es http/https se rechaza (ftp:, file:, javascript:, data:…). Se permite URL relativa (`/health`) y plantillas `{{var}}/path` (esquema vacío) para no romper la importación OpenAPI sin `servers`. En `openapiimporter.cpp`: si `servers[0].url` tiene esquema no http(s), se ignora la base con advertencia en `warnings`.

**Prueba que lo cubre.** `storeRejectsUnsafeUrlSchemesButAllowsTemplates` en `tests/cpp/test_core.cpp`.

### B5 — Dependencias inmutables

**Qué se encontró.** `CMakeLists.txt` declaraba yaml-cpp con `FetchContent` solo por URL de tag sin `URL_HASH`, y los workflows de GitHub Actions fijaban actions por etiqueta movible (`actions/checkout@v4`, `actions/setup-java@v4`, `jurplel/install-qt-action@v4`, `actions/upload-artifact@v4`, `actions/configure-pages@v5`, `actions/upload-pages-artifact@v3`, `actions/deploy-pages@v4`, `dtolnay/rust-toolchain@stable`).

**Por qué importa.** Una etiqueta puede moverse (compromiso de la action o del tarball): el build deja de ser reproducible y puede ejecutar código de terceros no revisado.

**Cómo se arregló.** `CMakeLists.txt` (solo rama ANDROID): FetchContent yaml-cpp 0.8.0 con `URL_HASH SHA256=<hash real>` del tarball. `.github/workflows/*.yml`: todas las actions fijadas por SHA completa con comentario de tag (checkout@v4.x, setup-java@v4.x, install-qt-action@v4.x, upload-artifact@v4.x, configure-pages@v5.x, upload-pages-artifact@v3.x, deploy-pages@v4.x, dtolnay/rust-toolchain fijado o con toolchain explícita).

**Prueba que lo cubre.** Sin prueba unitaria; la inmutabilidad se verifica por inspección del manifiesto y de los workflows.

## Verificado sin explotar

| Punto | Resultado de la revisión |
|-------|--------------------------|
| Límite de profundidad de JSON de Qt (~1000 niveles) | Ya existente; suficiente, sin fix |
| Excepción de recursión de yaml-cpp (~500-2000 niveles) | Ya manejada; sin fix |
| Inyección SQL | Sin inyección: todas las consultas usan parámetros (`QSqlQuery` con placeholders) |
| Web estática | Sin XSS identificado |
| Workflows | Permisos acotados en los workflows |
| FFI Rust | FFI seguro |
| Login PBKDF2-HMAC-SHA-256 600 000 iteraciones | Correcto, con sus límites documentados en [SECURITY.md](SECURITY.md) (cuenta local, no bóveda) |

## Limitaciones conocidas

- La detección de secretos sigue siendo **heurística, no una bóveda**: puede dar falsos positivos (p. ej. rechazar cuerpos que contienen ejemplos tipo JWT de documentación) y falsos negativos.
- La aplicación sigue **sin cifrar** la base SQLite; el límite de protección sigue siendo el control de acceso del sistema operativo (ahora reforzado con permisos, M2).
- PBKDF2 encarece la fuerza bruta, pero **no protege ante una copia offline maliciosa** con tiempo suficiente.
- **No hay límite de intentos de login** ni bloqueo por fallos consecutivos.
- Fijar actions por SHA requiere **mantenimiento manual** para actualizarse; los comentarios con el tag ayudan, pero la actualización es consciente.

La validación real de las pruebas se registra en `README.md` y [DEVELOPMENT.md](DEVELOPMENT.md); en este documento se describe qué cubre cada prueba, no resultados de ejecución.

El contexto original de privacidad y protección de datos está en [SECURITY.md](SECURITY.md).
