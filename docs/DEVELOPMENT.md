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

## Port Android (ARM64, APK construido por CI)

El proyecto se empaqueta con Qt for Android y CMake, conservando la app C++/Qt, SQLite, la importación JSON/YAML y el backend Rust. No es una reescritura Kotlin. El workflow `.github/workflows/android-build.yml` corre en `dev` y valida el archivo APK, el manifiesto y las bibliotecas nativas `arm64-v8a`. La compilación exitosa más reciente es [run 40263775013](https://github.com/CRISTOP-bot/ReqFlowAI/actions/runs/40263775013), commit `011bf983ec06cb6178387c66eec98003f7725c35`; subió el artefacto `reqflow-ai-android-arm64-v8a` (APK ARM64, retención temporal). Otro run completo anterior, [40263741785](https://github.com/CRISTOP-bot/ReqFlowAI/actions/runs/40263741785), también construyó y verificó un APK. La CI no instala ni abre la aplicación en un emulador ni teléfono.

La configuración usa Qt 6.8.3 `android_arm64_v8a`, JDK 17, Android SDK Platform API 34, NDK 26.1.10909125 y el target Rust `aarch64-linux-android`. `yaml-cpp` 0.8.0 se compila con CMake para Android; Cargo enlaza el backend Rust para el ABI ARM64 con el linker del NDK. Los fallos del cross-build son bloqueantes; no se eliminan componentes para obtener un APK.

En Android la base SQLite vive en `QStandardPaths::AppDataLocation` (espacio privado de la aplicación). La interfaz Qt Widgets usa un layout adaptado a pantallas estrechas y un splitter vertical; es una adaptación inicial, no una UI nativa ni una evaluación visual táctil. La importación de archivos usa `QFileDialog`; el flujo Android Storage Access Framework no está verificado. Tampoco se han probado táctilmente el editor de flujos y los diálogos complejos. El manifiesto solicita `INTERNET` y mantiene deshabilitado el tráfico HTTP cleartext.

El APK es arm64-v8a y no está firmado para distribución. No se probó instalación/ejecución en dispositivo ni emulador, el uso de los selectores de documentos Android, ni Google Play; la firma de release y otros ABI quedan pendientes. Android se trabajó únicamente en `dev`; no se fusionó con `main`, que sigue divergida. La rama `dev` conserva otros fallos CI Linux en pruebas Qt; el run más reciente al registrar esta evidencia, [40263774958](https://github.com/CRISTOP-bot/ReqFlowAI/actions/runs/40263774958), falló en MOC de `tests/cpp/test_core.cpp:254` por uso de macro en el test, no en el cross-build Android.

Para repetir localmente: Qt for Android 6.8.3 ARM64, JDK 17, Android SDK API 34, NDK 26.1.10909125, CMake/Ninja, Rust/Cargo con `aarch64-linux-android` y acceso de red para descargar dependencias. Configure con el `qt-cmake` del kit Android y SDK/NDK; genere el instalador con `cmake --build <build-dir> --target apk`.

## Dependencias inmutables en CI y build (B5)

Los tres workflows (`.github/workflows/linux-build.yml`, `android-build.yml` y `pages.yml`) fijan cada action por SHA completa de 40 hexadecimales con comentario de tag (checkout v4.4.0, setup-java v4.9.1, install-qt v4.5.0, upload-artifact v4.6.2, configure-pages v5.0.0, upload-pages-artifact v3.0.1, deploy-pages v4.0.5), sin subir de versión mayor respecto de lo que ya usaban. Los tags elegidos son ligeros, verificados con `git ls-remote <repo> 'refs/tags/<TAG>^{}'` (vacío) y con control positivo sobre un tag anotado.

Excepción razonada: `dtolnay/rust-toolchain`. La rama `stable` de esa acción se regenera con cada push a `master`, y su SHA no vive en la historia de `master` (la documentación de la action advierte que sería basura y rompería la CI). Se fija el tip de `master` (`683b588c487a69d01f6c6ddf40f036ff682cc2c8`) con `with: toolchain: stable` explícito, el patrón que indica el README de la action para selección de toolchain por input.

En Android, el FetchContent de yaml-cpp 0.8.0 lleva `URL_HASH SHA256=fbe74bbdcee21d656715688706da3c8becfd946d92cd44705cc6098bb23b3a16` (hash del tarball calculado con `sha256sum`; gzip verificado con `gzip -t`). Limitación conocida: GitHub no garantiza los tarballs a nivel byte para siempre; si el hash deja de servir, hay que actualizar el hash junto con la versión.

Además, `backend/Cargo.lock` queda versionado (se generó en la primera ejecución de `cargo build`/`cargo test` en local): fija la versión exacta y el checksum de cada crate de crates.io, de modo que el backend Rust se reconstruye con las mismas dependencias en cada máquina. Es el equivalente Rust del pin por SHA de las actions.

## Backend Rust parcial

`backend/Cargo.toml` define la biblioteca Rust. CMake invoca `cargo build --release` y enlaza la biblioteca con el ejecutable y los tests. La función delegada es la expansión estricta de plantillas `{{identificador}}`; Qt conserva su API mediante `src/data/rustbackend.cpp`. Para probar el crate por separado:

```sh
cargo test --manifest-path backend/Cargo.toml
```

La FFI usa punteros y longitudes UTF-8 explícitas; los resultados se liberan con `reqflow_string_free`. Esta primera migración no mueve a Rust red HTTP, persistencia, motor de flujos ni la interfaz.


## Validación del motor de flujos

`flowRequestValidationErrorsNeverSendHttp` cubre cabeceras sospechosas/mal formadas, variables faltantes en URL/cuerpo, esquemas no HTTP y métodos fuera de lista. Usa direcciones loopback y exige error del nodo de petición para verificar el retorno previo a `HttpClient::send`; la prueba se ejecutó en local (Qt 6.12) y pasó dentro de la suite de 24. El detector es una heurística ampliada (nombres de campos sensibles y patrones comunes de credenciales, ahora unificados en `security/secrets.h`); tiene falsos positivos/negativos posibles y no cifra SQLite. La regresión usa endpoints loopback no disponibles para confirmar que las fallas de validación no llegan a intentar conectar.

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

### Pruebas de la auditoría de seguridad (2026-10-26)

La entrega de la auditoría (ver [SECURITY_AUDIT.md](SECURITY_AUDIT.md)) añade ocho pruebas en `tests/cpp/test_core.cpp`:

- `storeRejectsUnsafeUrlSchemesButAllowsTemplates`: `saveRequest`/`saveFlow` rechazan esquemas `ftp`/`file`/`javascript`/`data` y admiten plantillas `{{var}}` y rutas relativas; el importador ignora con advertencia una base de servidor no http(s).
- `secretDetectionUnifiesNamesAndCatchesCredentialValues`: el patrón único (`secrets.h`) detecta nombres sensibles sin rechazar `Author`/`Content-Type`/`X-Oauth-Trial`, reconoce valores Bearer/Basic/sk-/gh_/AKIA/JWT y cuerpos form-urlencoded/XML.
- `historyRedactsByValueAndCapsBodySize`: el historial redacta parámetros, cabeceras y campos JSON por valor con forma de credencial, y sustituye cuerpos de más de 2 000 000 de caracteres por un marcador.
- `yamlBillionLaughsIsRejectedWithinNodeBudget`: una bomba de aliases YAML de <1 KiB (8 niveles) se rechaza con el error de presupuesto de 100 000 nodos en lugar de expandirse (medido en la auditoría: 2,6 s sin presupuesto).
- `httpClientAbortsResponsesOverSizeLimit`: contra un `QTcpServer` local que anuncia `Content-Length` de 12 MiB y envía solo 64 KiB con el socket abierto, el cliente aborta por su cuenta y el error indica el límite de 10 MiB.
- `httpClientFollowsSameOriginRedirectsAndBlocksCrossOrigin`: dos `QTcpServer` locales; un 302 al mismo origen se sigue, un 302 a otro origen se rechaza sin verificador y se sigue con un verificador que lo permite.
- `httpHeaderValidationFollowsRfc7230`: validadores de cabecera (tchar, sin CR/LF/NUL/CTL/DEL, obs-text admitido).
- `databaseFilePermissionsAreOwnerOnly` (solo Unix): la base creada por `ProjectStore` queda en modo 0600.

También se corrigió una incompatibilidad del preprocesador de `moc` (Qt 6.12) con los raw strings `R"(...)"` en el propio fichero de tests: moc no los entiende, y según el contexto (raw string multilínea, o raw string dentro de una llamada partida en varias líneas) provoca desde errores de macro hasta la omisión silenciosa de la clase entera («No relevant classes found», vtable sin definir en el enlazado). Los seis raw strings restantes se sustituyeron por strings escapados equivalentes. Ese mismo problema de fondo ya rompía el MOC en CI (run 40263774958, `test_core.cpp:254`).

## Evidencia y reportes

### Validación local del 2026-10-26 (auditoría de seguridad)

En esta entrega sí hay entorno de build: **Fedora, Qt 6.12.0, gcc 16.2.1, CMake, yaml-cpp 0.9.0, rustc/cargo 1.99.1** (rustup en `~/.cargo`; `~/.cargo/bin` debe estar en el PATH para que `find_program(CARGO_EXECUTABLE cargo)` lo encuentre). Comandos ejecutados y resultados reales:

```sh
cmake -S . -B build -DBUILD_TESTING=ON        # → Configuring done / Generating done
cmake --build build --parallel                # → [100%] Built target reqflow-core-tests y reqflow-ai
ctest --test-dir build --output-on-failure    # → 1/1 Test #1: reqflow-core-tests ... Passed  17.21 sec
./build/reqflow-core-tests                    # → Totals: 24 passed, 0 failed, 0 skipped, 17638ms
cargo test --manifest-path backend/Cargo.toml # → test result: ok. 6 passed; 0 failed
```

Durante el desarrollo de esta entrega aparecieron y se corrigieron sobre la marcha (documentados arriba): la incompatibilidad del `moc` de Qt 6.12 con los raw strings del fichero de tests, la inexistencia de `QNetworkAccessManager::setReadBufferSize` en Qt 6 (el límite de 10 MiB se apoya en el abort diferido de `downloadProgress`), un `#include <QJsonObject>` faltante en `secrets.h` y el bindeo de `QString` nulo como `SQL NULL` en columnas `NOT NULL` de `requests`/`history` (normalizado con `textOrEmpty` en `ProjectStore`).

Pendiente real: que los workflows de GitHub Actions terminen en verde tras el commit (incluido el cross-build Android, no ejecutable en local), y el QA manual de la interfaz (diálogos de redirección, ejecución de flujos, login interactivo).

### Evidencias anteriores

En la validación del 2026-10-25 no había toolchain en el entorno de trabajo (`git`, `cmake`, `g++`, `cargo`, `rustc`, `qmake6` y `pkg-config` no se encontraban en PATH; `cmake -S . -B build -DBUILD_TESTING=ON` devolvía «command not found», exit code 127). Esa entrega dejó el workflow Linux configurado con `BUILD_TESTING=ON` para compilar/ejecutar Qt Test y `cargo test` en CI. El run [40263774958](https://github.com/CRISTOP-bot/ReqFlowAI/actions/runs/40263774958) falló en MOC de `tests/cpp/test_core.cpp:254` (raw strings vs. preprocesador de moc), no en el cross-build Android; ese fallo de fondo queda corregido en esta entrega.

Un escaneo estático de archivos o delimitadores puede servir de revisión auxiliar, nunca sustituye a compilación. Al informar resultados, separar con claridad: pruebas añadidas, ejecutadas y no ejecutables por falta de dependencia.

## Contribuciones

Seguir obligatoriamente [AGENT.md](../AGENT.md). **Documentar todo**: README si cambia el uso/estado; documentos temáticos e índice para nuevas áreas; archivos afectados, motivo, limitaciones, pruebas y resultados reales en la entrega. Mantener docs sin afirmaciones basadas solo en intención.


## Sitio web en GitHub Pages

El sitio estático se mantiene en `website/`: `index.html`, `funciones.html`, `descargas.html`, `proyecto.html`, `documentacion.html`, `validacion.html` y `multimedia.html` comparten navegación y cargan `assets/site.css` y `assets/reveal.js`. `.github/workflows/pages.yml` despliega desde la rama `dev` con GitHub Pages Actions cuando cambia el contenido web (`website/**`) o el workflow; también admite ejecución manual. El repositorio público usa GitHub Pages con el plan gratuito y GitHub Actions como source. La URL solo se considera activa tras un deploy exitoso y debe leerse del deployment de GitHub Actions. Las descargas disponibles son las que existan realmente como releases; hoy el sitio ofrece el ZIP de código fuente y avisa que el binario Linux no está publicado/validado.


El sitio tiene páginas informativas separadas para inicio, funciones, descargas, estado del proyecto, documentación, checklist de validación y multimedia. La página multimedia reutiliza los GIF de vista previa y MP4 de `videos/` mediante URLs públicas de GitHub; si se mueven los archivos, hay que actualizar los enlaces. El efecto progresivo respeta `prefers-reduced-motion`; el contenido queda visible si no hay soporte para IntersectionObserver. En cada cambio web, validar enlaces internos y externos y comprobar el despliegue de Pages; no afirmar que el build de la aplicación funciona por el hecho de que la web publique.
