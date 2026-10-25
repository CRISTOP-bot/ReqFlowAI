# Desarrollo y pruebas

## Requisitos

- C++20.
- Rust estable y Cargo (requeridos para construir `backend/` como biblioteca estática integrada en `reqflow-ai`).
- CMake 3.21 o posterior.
- Qt 6.4 o posterior con Core, Widgets, Network y Sql; controlador QSQLITE habilitado.
- Qt Test para compilar y ejecutar el target de pruebas C++.
- Dependencia Cargo `serde_json`; Cargo obtiene las dependencias desde crates.io durante el primer build.

## Integración Rust parcial

Solo la expansión estricta de plantillas `{{identificador}}` se ejecuta en Rust; la aplicación, persistencia SQLite, HTTP, motor de flujos y UI siguen en C++/Qt. La FFI usa punteros y longitudes UTF-8 explícitas para entrada/salida/error, incluidos bytes NUL. Los buffers propiedad de Rust se liberan llamando `reqflow_string_free(buffer, length)`. La función de expansión no devuelve una salida parcial cuando falla.

La rama aún no contiene `backend/Cargo.lock`: no se pudo generar sin Cargo en el entorno disponible, por lo que no se ha inventado. Cuando haya Cargo, genera el lockfile real y activa `--locked` en el build/CI.


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

El primer run de CI con la integración Rust (`c49e514`, run 40222792370) instaló y configuró correctamente Ubuntu 24.04/Qt, y las seis pruebas Cargo pasaron. La compilación del target de pruebas C++ falló en Qt MOC con `missing ')' in macro usage` en `tests/cpp/test_core.cpp`; se ha aislado una edición correctiva para el re-run. Por tanto, ese run no valida el build completo y no se afirma que Qt Test pase. El resultado del run posterior se añadirá una vez termine.

La evidencia previa de autenticación/login corresponde a otra edición: `git`, `cmake`, `g++`, `qmake6` y `pkg-config` no estaban disponibles entonces; no debe confundirse con el nuevo run de GitHub Actions.

La revisión previa registró `cmake -S . -B build -DBUILD_TESTING=ON` con `cmake: command not found` (exit code 127). No se configuró ni compiló el proyecto en ese entorno.

Un escaneo estático de archivos o delimitadores puede servir de revisión auxiliar, nunca sustituye a compilación. Al informar resultados, separar con claridad: pruebas añadidas, ejecutadas y no ejecutables por falta de dependencia.

## Contribuciones

Seguir obligatoriamente [AGENT.md](../AGENT.md). **Documentar todo**: README si cambia el uso/estado; documentos temáticos e índice para nuevas áreas; archivos afectados, motivo, limitaciones, pruebas y resultados reales en la entrega. Mantener docs sin afirmaciones basadas solo en intención.
