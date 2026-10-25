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

## Estado real

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
- Qt 6.4 o posterior: Core, Widgets, Network, Sql; Qt Test requerido por el workflow CI y opcional para builds locales.
- Driver QSQLITE habilitado en el paquete de Qt.

## Compilar y ejecutar

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
./build/reqflow-ai
```

### Compilación automática para Linux

Cada push a `main` o `dev` compila la aplicación y los tests Qt en Ubuntu 24.04, y ejecuta CTest. Los pushes exitosos a `main` publican una pre-release automatizada con identificador de commit; las etiquetas `v*` publican una release normal. Ambas adjuntan un `.deb` para Debian/Ubuntu, un `.rpm` para Fedora y un `.pkg.tar.zst` para Arch, además de `reqflow-ai-linux.tar.gz`. Los paquetes instalan la app y añaden un acceso al menú. Las pre-releases CI no son versiones estables. El tarball no incluye Qt y requiere las bibliotecas Qt 6 del sistema.

Pruebas:

```sh
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

La batería de pruebas incluye una regresión que comprueba que un nodo de flujo con una cabecera que parece contener credenciales falla antes de enviar la petición. La prueba confirma que no se producen variables de respuesta. El filtro es heurístico y no cifra SQLite ni sustituye una bóveda segura.

Datos locales en `~/.local/share/reqflow-ai/reqflow.sqlite3`.

## Verificación de esta entrega

La disponibilidad de herramientas debe comprobarse en el entorno de cada build. En la última revisión del proyecto se intentó configurar con `cmake -S . -B build -DBUILD_TESTING=ON`; falló con `cmake: command not found` (exit code 127), así que no se compiló, no se ejecutó la GUI ni se probaron solicitudes de red. También pasó un escaneo estático con Python de rutas de fuentes/headers citadas por CMake y delimitadores C++; esto no sustituye al compilador. Los tests Qt Test escritos no se ejecutaron en aquella revisión local. Más tarde, el workflow de GitHub Actions para `main` compiló la aplicación y los tests, ejecutó CTest y subió el artefacto correctamente en la ejecución [40235712301](https://github.com/CRISTOP-bot/ReqFlowAI/actions/runs/40235712301), commit `2e7c0ffdb9116cd4b3ebe51d03b4a75207402479`. Este resultado valida ese SHA concreto en Ubuntu 24.04, no una ejecución interactiva de la GUI ni otros sistemas. Requiere toolchain C++20, CMake, Qt 6.4+, Qt Test y QSQLITE.

## Uso seguro

Enviar una petición puede modificar un servidor. Verifica destino, método y contenido, y usa solo APIs que tengas autorización para probar. Sin bóveda del sistema operativo disponible, la interfaz bloquea guardar/enviar peticiones con nombres de campos que parecen secretos; esto es un filtro conservador, no un gestor de credenciales ni una garantía de detección. No guardes tokens en texto libre. La aplicación es offline salvo las peticiones HTTP que el usuario envíe.


## Seguridad y credenciales

Consulta [docs/SECURITY.md](docs/SECURITY.md) para conocer el límite de protección local, el filtro heurístico de credenciales y la regresión de flujo HTTP. La base SQLite no está cifrada y el filtro no sustituye una bóveda segura.


## Releases y artefactos Linux

GitHub Actions compila la aplicación y ejecuta Qt Test/CTest antes de empaquetar. Si falla build o test, no se publican los paquetes. Un push exitoso a `main` publica una **pre-release CI** identificada por commit; los tags `v*` publican releases de versión. La pre-release CI no es una versión estable. Los paquetes x86_64 se construyen en Ubuntu 24.04, Fedora y Arch Linux y no se declaran universales para otras arquitecturas o distribuciones.


### Paquetes para más distribuciones Linux

Las releases incluyen paquetes construidos en CI para:

- Ubuntu 24.04/Debian amd64: `.deb`; instalar con `sudo apt install ./reqflow-ai_*.deb`.
- Fedora x86_64: `.rpm`; instalar con `sudo dnf install ./reqflow-ai-*.rpm`.
- Arch Linux x86_64 y derivados: `.pkg.tar.zst`; instalar con `sudo pacman -U ./reqflow-ai-*.pkg.tar.zst`.

La instalación añade la aplicación al menú y usa las bibliotecas Qt del sistema. El tarball `.tar.gz` también está disponible, pero no incluye Qt: requiere instalar las bibliotecas Qt 6 compatibles con tu distribución. Los paquetes no son universales entre arquitecturas; esta publicación de CI construye paquetes amd64/x86_64.


### Instalar en Fedora o Arch

En Fedora, descarga el archivo `.rpm` de Assets y ejecuta:

```sh
sudo dnf install ./reqflow-ai-*.rpm
```

En Arch Linux o derivados, instala el paquete local `.pkg.tar.zst` así:

```sh
sudo pacman -U ./reqflow-ai-*.pkg.tar.zst
```

Ambos paquetes incluyen un lanzador para el menú de aplicaciones y dependen de Qt 6 del sistema.
