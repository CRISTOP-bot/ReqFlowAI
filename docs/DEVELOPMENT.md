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

El target `reqflow-core-tests` cubre contratos seleccionados del cliente HTTP, persistencia, entornos, motor de flujos, importador OpenAPI, aserciones, filtros heurísticos y comparador JSON. Es una prueba de núcleo, no prueba visual/GUI de toda la aplicación, ni valida contra un servidor real. Cada nueva función debe ampliar tests adecuados y debe explicarse qué escenarios no cubre.

## Evidencia y reportes

En el estado documentado de esta entrega se intentó `cmake -S . -B build -DBUILD_TESTING=ON` y falló con `cmake: command not found` (exit code 127). Por tanto, en ese entorno no se configuró ni compiló el proyecto y los tests Qt Test no se ejecutaron. Esta información no debe presentarse como un resultado nuevo si se ejecutan pruebas futuras: actualizarla con fecha, plataforma, comandos y códigos/resultados exactos.

Un escaneo estático de archivos o delimitadores puede servir de revisión auxiliar, nunca sustituye a compilación. Al informar resultados, separar con claridad: pruebas añadidas, ejecutadas y no ejecutables por falta de dependencia.

## Contribuciones

Seguir obligatoriamente [AGENT.md](../AGENT.md). **Documentar todo**: README si cambia el uso/estado; documentos temáticos e índice para nuevas áreas; archivos afectados, motivo, limitaciones, pruebas y resultados reales en la entrega. Mantener docs sin afirmaciones basadas solo en intención.
