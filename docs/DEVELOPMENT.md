# Desarrollo y pruebas

## Requisitos

- C++20.
- Rust estable y Cargo (requeridos para construir `backend/` como biblioteca estática integrada en `reqflow-ai`).
- Dependencia Cargo `serde_json`; Cargo obtiene las dependencias desde crates.io durante el primer build.
- yaml-cpp (requerida para importar especificaciones OpenAPI YAML). En Android, CMake construye yaml-cpp 0.8.0 dentro del cross-build para el ABI seleccionado; no se enlaza una biblioteca del host.
- CMake 3.21+.
- Qt 6.4+ con Core, Widgets, Network y Sql.
- Driver `QSQLITE` disponible.
- Qt Test para el target de pruebas.

## Port Android (primera compilación, pendiente de CI)

El proyecto incluye una ruta de empaquetado Android con Qt for Android y CMake, no una reescritura Kotlin. El workflow `.github/workflows/android-build.yml` apunta exclusivamente a `dev`, instala Qt 6.8.3 `android_arm64_v8a`, JDK 17, Android SDK Platform API 34 y NDK 26.1.10909125 (esa combinación de NDK admite como máximo API 34), instala la target Rust `aarch64-linux-android`, y pide a CMake producir el target `apk`. La acción sube el APK como artefacto de 14 días solo después de crear y verificar el archivo ZIP, manifest y bibliotecas nativas `arm64-v8a`. Esto no está probado hasta que ese workflow pase.

La app conserva módulos C++/Qt, SQLite, importación JSON/YAML y backend Rust completo. `yaml-cpp` 0.8.0 se construye con CMake para Android; Cargo usa target Rust Android y el linker Clang del NDK (API 28, compatible con el mínimo ABI), en vez de utilizar bibliotecas host. Los errores de cross-build son bloqueantes deliberadamente; no se reemplaza el backend por stubs ni se descartan funciones. Android solicita `INTERNET`; el tráfico HTTP cleartext queda deshabilitado por manifiesto.

En Android, la base de datos predeterminada usa `QStandardPaths::AppDataLocation`, el directorio privado de la app; las ubicaciones de escritorio existentes se conservan. La interfaz usa un splitter vertical y distribuye controles de proyecto/entorno en filas para pantallas estrechas, con un tamaño mínimo reducido. Es una adaptación inicial de widgets de escritorio, no una UI Android nativa ni una validación visual. El importador de OpenAPI usa `QFileDialog`; la selección de documentos mediante Storage Access Framework no está implementada ni verificada. Los flujos visuales y diálogos complejos tampoco han recibido QA táctil.

Para compilar localmente se requiere Qt for Android 6.8.3 para `arm64-v8a`, JDK 17, Android SDK Platform 35, NDK 26.1.10909125, CMake/Ninja, Cargo/Rust con `aarch64-linux-android` y acceso de red para FetchContent. Configure con el `qt-cmake` de `android_arm64_v8a` y Android SDK/NDK, luego `cmake --build <build-dir> --target apk`. CI valida estructura del APK/ABI, no instala ni ejecuta la app en emulador/dispositivo. API/NDK fijadas son las de este workflow; no significan que API 34 sea la versión mínima soportada por la app (target del ABI Rust/Ndk: 28). Artefacto no firmado para pruebas directas; Google Play y firma de release quedan fuera.

## Backend Rust parcial

`backend/Cargo.toml` define la biblioteca Rust. CMake invoca `cargo build --release` y enlaza la biblioteca con el ejecutable y los tests. La función delegada es la expansión estricta de plantillas `{{identificador}}`; Qt conserva su API mediante `src/data/rustbackend.cpp`. Para probar el crate por separado:

```sh
cargo test --manifest-path backend/Cargo.toml
```

La FFI usa punteros y longitudes UTF-8 explícitas; los resultados se liberan con `reqflow_string_free`. Esta primera migración no mueve a Rust red HTTP, persistencia, motor de flujos ni la interfaz.


## Validación del motor de flujos

`flowRequestValidationErrorsNeverSendHttp` cubre cabeceras sospechosas/mal formadas, variables faltantes en URL/cuerpo, esquemas no HTTP y métodos fuera de lista. Usa direcciones loopback y exige error del nodo de petición para verificar el retorno previo a `HttpClient::send`; las pruebas están escritas pero todavía deben ejecutarse en CI. El detector es una heurística ampliada (nombres de campos sensibles y algunos patrones comunes de tokens); tiene falsos positivos/negativos posibles y no cifra SQLite. La regresión usa endpoints loopback no disponibles para confirmar que las fallas de validación no llegan a intentar conectar.

## Importador OpenAPI

El importador admite JSON y YAML, y resuelve referencias locales bajo `#/components/...` únicamente cuando el objeto Path Item o la operación contiene directamente `$ref`. La referencia se expande en una sola pasada: referencias externas o transitivas/anidadas no se resuelven y se omiten con advertencia. El alcance no equivale a soporte OpenAPI general de `$ref`. Los tests cubren YAML válido/inválido, referencias locales directas y aviso de referencia externa. `yaml-cpp` es dependencia de compilación y debe estar instalada.

## Compilación

Desde la raíz:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
./build/reqflow-ai
```

## Pruebas

```sh
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

El target `reqflow-core-tests` cubre contratos seleccionados del cliente HTTP, persistencia, entornos, motor de flujos, importador OpenAPI, aserciones, filtros heurísticos, comparador JSON y autenticación local. El test de autenticación usa `QTemporaryDir` y cubre ausencia de cuenta, alta con nombre normalizado, validación/política (usuario inválido, contraseña corta), login correcto, contraseña/usuario incorrectos, alta duplicada y persistencia después de reabrir SQLite. El comparador tiene regresiones para diferencias, límites y claves JSON con puntos, comillas o barras invertidas. Son pruebas de núcleo, no prueba visual/GUI del flujo de login, no verifican tiempos/entropía en un entorno de producción ni validan contra un servidor real. Cada nueva función debe ampliar tests adecuados y debe explicarse qué escenarios no cubre.

## Evidencia y reportes

En la inspección Android (2026-10-25/26), el sandbox tampoco dispone de `git`, `cmake`, `java` ni `adb`; por eso no se pudo ejecutar localmente el cross-build, producir/inspeccionar un APK, instalar ni lanzar la app. El workflow Android añadido debe dar la evidencia de compilación y empaquetado. Hasta que termine exitosamente, el port es una ruta de build implementada pero no compilada ni verificada, y no hay enlace de APK disponible.

En la validación Linux del 2026-10-25 se comprobó la disponibilidad real de herramientas: `git`, `cmake`, `g++`, `cargo`, `rustc`, `qmake6` y `pkg-config` no se encontraron en PATH. Se actualizó el workflow Linux para instalar Rust/yaml-cpp, configurar `BUILD_TESTING=ON`, compilar aplicación/tests y ejecutar Cargo/CTest; los resultados de GitHub Actions deben inspeccionarse tras el commit. Por tanto, configuración, build y tests Qt Test no se ejecutaron. La firma y el prototipo `QPasswordDigestor::deriveKeyPbkdf2(QCryptographicHash::Sha256, data, salt, iterations, dkLen)` se revisaron en la documentación oficial de Qt (Qt Network), pero esto no demuestra que una instalación local Qt 6.4 compile el código ni su comportamiento runtime. Las pruebas se escribieron, pero no están ejecutadas. Comandos de build/test no ejecutados porque falta CMake y la toolchain Qt/C++ no está disponible; no declarar build aprobado. Actualiza esta evidencia si se dispone de entorno nuevo.

La revisión previa registró `cmake -S . -B build -DBUILD_TESTING=ON` con `cmake: command not found` (exit code 127). No se configuró ni compiló el proyecto en ese entorno.

Un escaneo estático de archivos o delimitadores puede servir de revisión auxiliar, nunca sustituye a compilación. Al informar resultados, separar con claridad: pruebas añadidas, ejecutadas y no ejecutables por falta de dependencia.

## Contribuciones

Seguir obligatoriamente [AGENT.md](../AGENT.md). **Documentar todo**: README si cambia el uso/estado; documentos temáticos e índice para nuevas áreas; archivos afectados, motivo, limitaciones, pruebas y resultados reales en la entrega. Mantener docs sin afirmaciones basadas solo en intención.


## Sitio web en GitHub Pages

El sitio estático se mantiene en `website/`: `index.html`, `funciones.html`, `descargas.html`, `proyecto.html`, `documentacion.html`, `validacion.html` y `multimedia.html` comparten navegación y cargan `assets/site.css` y `assets/reveal.js`. `.github/workflows/pages.yml` despliega desde la rama `dev` con GitHub Pages Actions cuando cambia el contenido web (`website/**`) o el workflow; también admite ejecución manual. El repositorio público usa GitHub Pages con el plan gratuito y GitHub Actions como source. La URL solo se considera activa tras un deploy exitoso y debe leerse del deployment de GitHub Actions. Las descargas disponibles son las que existan realmente como releases; hoy el sitio ofrece el ZIP de código fuente y avisa que el binario Linux no está publicado/validado.


El sitio tiene páginas informativas separadas para inicio, funciones, descargas, estado del proyecto, documentación, checklist de validación y multimedia. La página multimedia reutiliza los GIF de vista previa y MP4 de `videos/` mediante URLs públicas de GitHub; si se mueven los archivos, hay que actualizar los enlaces. El efecto progresivo respeta `prefers-reduced-motion`; el contenido queda visible si no hay soporte para IntersectionObserver. En cada cambio web, validar enlaces internos y externos y comprobar el despliegue de Pages; no afirmar que el build de la aplicación funciona por el hecho de que la web publique.
