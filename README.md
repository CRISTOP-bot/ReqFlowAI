# ReqFlow AI

Aplicación nativa de escritorio **C++20 + Qt 6**, local-first, para crear y enviar peticiones HTTP sin escribir código. Esta es una base de producto en desarrollo, no la plataforma completa del prompt maestro.

## Motion graphics

### Parte 1 — Presentación de ReqFlow AI

![ReqFlow AI — Motion Graphic, Parte 1](videos/reqflow-ai-motion-part-1-preview.gif)

[Ver o descargar MP4 — Parte 1](https://github.com/CRISTOP-bot/ReqFlowAI/blob/main/videos/reqflow-ai-motion-part-1.mp4) · 12 s · 1280 × 720 · H.264 · 24 FPS.

### Parte 2 — De una instrucción a un flujo API

![ReqFlow AI — Motion Graphic, Parte 2](videos/reqflow-ai-motion-part-2-preview.gif)

[Ver o descargar MP4 — Parte 2](https://github.com/CRISTOP-bot/ReqFlowAI/blob/main/videos/reqflow-ai-motion-part-2.mp4) · 10 s · 1280 × 720 · H.264 · 24 FPS · sin audio.

Los GIF son vistas previas animadas para que se reproduzcan directamente en GitHub; los MP4 enlazados son las versiones completas.

---

## Documentación para desarrolladores

Consulta el [índice de documentación](docs/INDEX.md), la [arquitectura](docs/ARCHITECTURE.md), las [funciones y su estado](docs/FEATURES.md), el [modelo de datos](docs/DATA_MODEL.md), la [guía de seguridad](docs/SECURITY.md) y las [instrucciones de desarrollo y pruebas](docs/DEVELOPMENT.md). Las reglas de contribución están en [AGENT.md](AGENT.md) y exigen documentar **todo** cambio antes de darlo por terminado.

## Estado real

### Beta experimental (en desarrollo)

- **Comparador de respuestas JSON del historial**: selecciona dos ejecuciones guardadas y compara los cuerpos JSON localmente, mostrando rutas con cambios añadidos, eliminados o modificados; también enseña estado HTTP y duración de cada ejecución. No hace solicitudes de red. La comparación está limitada a 2000 cambios y 48 niveles, y depende de los cuerpos que el historial haya conservado tras su redacción segura. Integrado en el historial como «Comparar seleccionadas · BETA»; requiere seleccionar exactamente dos filas. Añadida prueba Qt Test. Pendiente de compilar y ejecutar en entorno con CMake/Qt.

### Implementado en C++/Qt

- Ventana Qt Widgets con editor de peticiones, proyectos y respuesta.
- Cliente de red basado en `QNetworkAccessManager`: GET, POST, PUT, PATCH, DELETE, HEAD y OPTIONS; cabeceras, parámetros, cuerpo, timeout y cancelación de respuestas activas en la capa de red.
- SQLite mediante Qt SQL: proyectos, peticiones guardadas, historial; añadir, editar y eliminar petición.
- Entornos locales por proyecto con variables JSON; sustitución estricta `{{nombre}}` en URL, parámetros, cabeceras y cuerpo antes de enviar. Variables desconocidas o marcadores mal formados bloquean el envío; las plantillas guardadas no se modifican. Por ausencia de bóveda, se rechazan nombres de variable que parezcan secretos.
- Response Studio en progreso: cuerpo con formato JSON legible, árbol navegable para respuestas JSON, búsqueda siguiente en el texto del cuerpo y selección de un nodo para localizar su valor en el texto. El árbol limita la vista a 5000 nodos y 64 niveles; no edita la respuesta ni sustituye una vista segura especializada para HTML/XML. El cuerpo se limita a 2 millones de caracteres.
- API Doctor básico en respuesta: hallazgos observables y heurística explícitamente etiquetada.
- Protección conservadora: evita persistir peticiones cuyos campos parecen incluir credenciales; redacción de claves sensibles en respuestas JSON y cabeceras al registrar historial.
- Editor visual Qt de flujos conectado a la aplicación: canvas con nodos movibles, selección, conexiones por origen/destino y rama true/false, configuración JSON, validación, ejecución y persistencia local. El motor permite inicio, petición HTTP, extracción JSON por ruta, transformación, condición y resultado; limita pasos y registra ejecuciones. Aún no hay repetición/esperas, minimapa, control de cancelación desde la UI ni editor de propiedades basado en formularios.
- Importador local de OpenAPI 3.x en JSON para operaciones HTTP, query params, encabezados y ejemplo sencillo de cuerpo application/json; muestra advertencias y pide confirmación antes de guardar las peticiones. No expande `$ref`, no importa YAML, autenticación, servidores múltiples ni todos los esquemas.
- SQLite filtra en la propia capa de almacenamiento URL con userinfo/parámetros sensibles, cabeceras/campos JSON con nombre sensible, URLs del historial, cabeceras/respuestas JSON y mensajes de error. Las respuestas de texto no JSON se omiten del historial. Es redacción heurística, no una bóveda.
- Suites de aserciones API locales: `status`, `header`, `json_value`, `json_type` por ruta y `duration_ms`; las comparaciones aceptan `equals`, `not_equals`, `contains` y límites de duración. Definiciones y ejecuciones se guardan en SQLite. La UI exige confirmación explícita antes de enviar la petición guardada asociada. Aún no hay explorador de resultados históricos de suites ni editor visual de aserciones: definición por JSON. Ejemplo: `[ {"name":"OK","type":"status","expected":"200"}, {"type":"json_value","path":"$.data.id","expected":"7"} ]`.
- Un ejecutable de pruebas Qt Test está registrado en CMake con casos para cabeceras, flujos, OpenAPI, aserciones, persistencia y rechazo heurístico de variables secretas. No está ejecutado.
- Timeout HTTP ahora marca explícitamente las solicitudes abortadas por límite y el motor de flujos reporta el caso de terminal sin resultado en vez de considerarlo éxito.

### Todavía pendiente

Quedan fuera: nodos de espera/reintento/loop, exportación/versionado formal de flujos, extracción visual, árbol JSON/búsqueda/resaltado, diff de respuestas y especificaciones, OpenAPI YAML/$ref/security schemes, agente IA con conversaciones persistentes y proveedores, API Doctor completo, mock server/replay, plugin API, sincronización real, Keychain/Secret Service/Credential Manager y Security Lab completo con auditoría de alcance. La sustitución de entornos es literal, no una bóveda ni secret manager. La UI de suites usa JSON, no un constructor visual, y aún no expone el historial de ejecuciones. No se afirma que estas funciones estén implementadas.

## Requisitos

- C++20.
- CMake 3.21 o posterior.
- Qt 6.4 o posterior: Core, Widgets, Network, Sql; Qt Test opcional para pruebas.
- Driver QSQLITE habilitado en el paquete de Qt.

## Compilar y ejecutar

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
./build/reqflow-ai
```

Pruebas:

```sh
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Datos locales en `~/.local/share/reqflow-ai/reqflow.sqlite3`.

## Verificación de esta entrega

La disponibilidad de herramientas debe comprobarse en el entorno de cada build. En la última revisión del proyecto se intentó configurar con `cmake -S . -B build -DBUILD_TESTING=ON`; falló con `cmake: command not found` (exit code 127), así que no se compiló, no se ejecutó la GUI ni se probaron solicitudes de red. También pasó un escaneo estático con Python de rutas de fuentes/headers citadas por CMake y delimitadores C++; esto no sustituye al compilador. Los tests Qt Test escritos no se ejecutaron. Requiere toolchain C++20, CMake, Qt 6.4+, Qt Test y QSQLITE.

## Uso seguro

Enviar una petición puede modificar un servidor. Verifica destino, método y contenido, y usa solo APIs que tengas autorización para probar. Sin bóveda del sistema operativo disponible, la interfaz bloquea guardar/enviar peticiones con nombres de campos que parecen secretos; esto es un filtro conservador, no un gestor de credenciales ni una garantía de detección. No guardes tokens en texto libre. La aplicación es offline salvo las peticiones HTTP que el usuario envíe.
