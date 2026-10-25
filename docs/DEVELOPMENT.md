# Desarrollo y pruebas

## Requisitos

- C++20.
- CMake 3.21+.
- Qt 6.4+ con Core, Widgets, Network y Sql.
- Driver `QSQLITE` disponible.
- Qt Test para el target de pruebas (componente opcional que CMake busca cuando `BUILD_TESTING` está activo).

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

En esta tarea de login local (2026-10-25) se comprobó la disponibilidad real de herramientas: `git`, `cmake`, `g++`, `qmake6` y `pkg-config` no se encontraron en PATH. Por tanto, configuración, build y tests Qt Test no se ejecutaron. La firma y el prototipo `QPasswordDigestor::deriveKeyPbkdf2(QCryptographicHash::Sha256, data, salt, iterations, dkLen)` se revisaron en la documentación oficial de Qt (Qt Network), pero esto no demuestra que una instalación local Qt 6.4 compile el código ni su comportamiento runtime. Las pruebas se escribieron, pero no están ejecutadas. Comandos de build/test no ejecutados porque falta CMake y la toolchain Qt/C++ no está disponible; no declarar build aprobado. Actualiza esta evidencia si se dispone de entorno nuevo.

La revisión previa registró `cmake -S . -B build -DBUILD_TESTING=ON` con `cmake: command not found` (exit code 127). No se configuró ni compiló el proyecto en ese entorno.

Un escaneo estático de archivos o delimitadores puede servir de revisión auxiliar, nunca sustituye a compilación. Al informar resultados, separar con claridad: pruebas añadidas, ejecutadas y no ejecutables por falta de dependencia.

## Contribuciones

Seguir obligatoriamente [AGENT.md](../AGENT.md). **Documentar todo**: README si cambia el uso/estado; documentos temáticos e índice para nuevas áreas; archivos afectados, motivo, limitaciones, pruebas y resultados reales en la entrega. Mantener docs sin afirmaciones basadas solo en intención.
