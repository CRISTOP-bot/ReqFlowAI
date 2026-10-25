# Arquitectura

## Estado

La estructura en el repositorio está separada por responsabilidades, aunque la aplicación continúa en desarrollo. Este documento describe el diseño y los componentes presentes; no certifica compilación ni ejecución de la versión actual.

## Componentes

- `src/main.cpp`: punto de entrada de Qt y creación de la ventana principal.
- `include/reqflow/ui/`, `src/ui/`: interfaz Qt Widgets para proyectos, editor HTTP, historial, entornos, suites API, importación OpenAPI y editor de flujos.
- `include/reqflow/network/`, `src/network/`: cliente HTTP basado en `QNetworkAccessManager`, con señales para resultados asíncronos.
- `include/reqflow/data/`, `src/data/`: `ProjectStore`, SQLite vía Qt SQL, migración inicial con `CREATE TABLE IF NOT EXISTS` y expansión estricta de plantillas en `environment.cpp`.
- `include/reqflow/flow/`, `src/flow/`: estructuras, motor de ejecución y editor/canvas visual de flujos.
- `include/reqflow/openapi/`, `src/openapi/`: parser del subconjunto de OpenAPI JSON que soporta la implementación actual.
- `include/reqflow/testing/`, `src/testing/`: evaluación de aserciones y comparación de valores JSON.
- `tests/cpp/test_core.cpp`: pruebas Qt Test de componentes de núcleo; no incluye una prueba interactiva de toda la GUI.

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
