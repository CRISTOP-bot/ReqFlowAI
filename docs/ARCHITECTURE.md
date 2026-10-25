# Arquitectura

## Estado

La estructura en el repositorio está separada por responsabilidades, aunque la aplicación continúa en desarrollo. Este documento describe el diseño y los componentes presentes; no certifica compilación ni ejecución de la versión actual.

## Componentes

- `src/main.cpp`: punto de entrada Qt; abre el almacén, ejecuta el gate de autenticación antes de construir/mostrar la ventana principal y termina sin mostrarla si se cancela.
- `include/reqflow/ui/authdialog.h`, `src/ui/authdialog.cpp`: diálogo modal nativo de alta/login, consistente con carbón/grafito y terracota, con feedback accesible, contraseña oculta y confirmación solo en el primer registro.
- `ProjectStore` implementa cuenta local sobre SQLite: normalización de usuario, PBKDF2-HMAC-SHA-256 y persistencia de la sal/verificador. El login no implica backend, sincronización o cifrado de base.
- `include/reqflow/ui/`, `src/ui/`: interfaz Qt Widgets para proyectos, editor HTTP, historial, entornos, suites API, importación OpenAPI y editor de flujos.
- `include/reqflow/network/`, `src/network/`: cliente HTTP basado en `QNetworkAccessManager`, con señales para resultados asíncronos.
- `include/reqflow/data/`, `src/data/`: `ProjectStore`, SQLite vía Qt SQL, migración inicial con `CREATE TABLE IF NOT EXISTS` y API Qt para expansión de plantillas.
- `backend/`: crate Rust `reqflow_backend`, compilada como biblioteca estática e integrada con C++ mediante una FFI C pequeña. En este primer paso implementa la expansión estricta de `{{identificador}}`; el resto del backend sigue en C++/Qt.
- `src/data/rustbackend.cpp`, `include/reqflow/data/rustbackend.h`: adaptador que serializa el entorno JSON y gestiona propiedad/liberación de strings Rust antes de devolver los resultados a Qt.
- `CMakeLists.txt`: invoca Cargo durante el build y enlaza la biblioteca Rust con el ejecutable y las pruebas.
- `.github/workflows/linux-build.yml`: instala Rust, ejecuta pruebas Cargo y después construye y publica como artefacto el ejecutable Linux en `dev` (en `main` se conserva la construcción existente hasta integrar el crate allí).

La frontera FFI entrega buffers UTF-8 propiedad de Rust y sus longitudes exactas; esto incluye valores que contengan bytes NUL. Qt no debe liberarlos con `free`, sino mediante `reqflow_string_free(buffer, length)`. Los errores y salidas también tienen longitudes explícitas; una expansión fallida no expone contenido parcial. El crate depende de `serde_json`; el manejo de red, SQLite y flujos no se ha migrado todavía.

En este primer incremento no se pudo generar `Cargo.lock` porque Cargo no está disponible en el entorno de edición. La crate y su integración siguen pendientes de validación de compilación; véase [Desarrollo y pruebas](DEVELOPMENT.md).

### Flujo de expansión de plantillas

1. `expandTemplate` conserva la API Qt usada por UI y motor de flujos.
2. El adaptador C++ serializa el `QJsonObject` y llama a `reqflow_expand_template`.
3. Rust valida JSON, resuelve tokens, convierte valores JSON a texto compacto y devuelve salida o mensaje de error.
4. El adaptador convierte el texto a `QString` y libera ambos buffers con la función Rust de liberación.
5. El flujo aborta ante identificador inválido o variable faltante; no retorna expansión parcial.

Esta migración es parcial y no convierte aún en Rust el backend entero de la aplicación.

La integración Rust/FFI no se ha compilado ni ejecutado en este entorno. En la revisión del 2026-10-25 se comprobó que `cargo`, `rustc`, `cmake` y `git` no están disponibles en PATH; no se pudo ejecutar Cargo, generar `Cargo.lock`, compilar CMake ni correr Qt Test. Cargo y Rust estable son ahora requisitos obligatorios del build en la rama que contiene esta integración; la CI añadida está pendiente de ejecutarse.

`CMakeLists.txt` registra las fuentes del ejecutable y, cuando Qt Test está disponible y `BUILD_TESTING` se activa, define el ejecutable de pruebas. Consulta [Desarrollo y pruebas](DEVELOPMENT.md); no se afirma que el build pase sin ejecutarlo en una toolchain compatible.
- `include/reqflow/flow/`, `src/flow/`: estructuras, motor de ejecución y editor/canvas visual de flujos.
- `include/reqflow/openapi/`, `src/openapi/`: parser del subconjunto de OpenAPI JSON que soporta la implementación actual.
- `include/reqflow/testing/`, `src/testing/`: evaluación de aserciones y comparación de valores JSON.
- `tests/cpp/test_core.cpp`: pruebas Qt Test de componentes de núcleo y de persistencia/autenticación; no incluye una prueba interactiva de toda la GUI.

## Flujo de una solicitud

1. El usuario selecciona o edita el método, URL, parámetros, cabeceras y cuerpo.
2. La interfaz expande `{{variables}}` usando el entorno local activo; si una variable falta o hay un marcador mal formado, aborta antes de enviar.
3. La capa de interfaz aplica el filtro heurístico de campos sensibles que no pueden almacenarse o enviarse sin una bóveda real.
4. `HttpClient` configura y envía la petición con `QNetworkAccessManager` y emite el resultado; la GUI refleja estado, cuerpo y cabeceras.
5. La respuesta puede registrarse en SQLite con redacción/omisión conservadora del historial. La UI puede ejecutar una suite guardada y/o mostrar navegación JSON.

La comparación beta lee dos registros existentes de historial; no llama a la red ni altera las respuestas guardadas.

## Almacenamiento local

La base predeterminada es `~/.local/share/reqflow-ai/reqflow.sqlite3`. No existe servicio de sincronización en la nube. La configuración CMake requiere C++20, CMake 3.21+ y Qt 6.4+ (Core, Widgets, Network, Sql); el controlador QSQLITE debe estar disponible.

## Construcción

`CMakeLists.txt` registra las fuentes de producto y, cuando Qt Test está disponible y `BUILD_TESTING` se activa, define el ejecutable de pruebas. Consulta [Desarrollo y pruebas](DEVELOPMENT.md); no hay afirmación de build exitoso si no se ejecutó en una toolchain compatible.
