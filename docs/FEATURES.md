# Funciones y estado del producto

Este archivo es un inventario de capacidades basado en el código actual. Una función que aparezca aquí como beta o parcial no se debe presentar como producto estable.

## Implementación presente

- **Backend Rust parcial (integración nueva; no validada aún en build)**: expansión de plantillas de entorno delegada al crate `backend/`, con FFI C y pruebas unitarias Cargo. La migración no abarca cliente HTTP, SQLite, motor de flujos ni todas las reglas de negocio; esas piezas siguen en C++/Qt. Cargo/Rust estable es requisito obligatorio para build de esta rama.
- **Motor de flujos: validación antes de petición**: los nodos HTTP validan primero credenciales sospechosas, método permitido, plantilla completa, URL HTTP/HTTPS y sintaxis de cabeceras; ante un error retornan el flujo como fallido antes de llamar al cliente de red. Hay pruebas Qt Test de regresión escritas para estos casos; ejecución pendiente.
- **OpenAPI YAML y referencias locales**: el importador acepta YAML además de JSON y resuelve referencias locales de un nivel bajo `#/components/...` en operaciones y Path Items. Referencias externas o anidadas se omiten con advertencia; no se afirma soporte general de `$ref`.

- **Login local de aplicación (beta, sin build/ejecución verificados en esta entrega)**: alta en primer inicio y autenticación al iniciar después; cuadro modal con estilo grafito/carbón, acento terracota, password enmascarado, confirmación de registro y feedback visible. El gate termina antes de crear/mostrar la ventana principal; cancelar sale. No es autenticación web/remota y no cifra SQLite. Consulta [Seguridad](SECURITY.md) para amenazas y olvido de contraseña.
- Cliente HTTP de escritorio con métodos GET, POST, PUT, PATCH, DELETE, HEAD y OPTIONS, URL, query params, cabeceras y cuerpo.
- Proyectos, peticiones guardadas, historial y suites almacenados localmente en SQLite.
- Entornos locales por proyecto con expansión literal de `{{identificador}}` en URL, parámetros, cabeceras y cuerpo. Los nombres que parecen secretos se rechazan mientras no exista bóveda del sistema.
- Editor visual y motor de flujos con clases de nodo implementadas en el núcleo; los tipos exactos y sus límites se deben revisar en `src/flow/flowengine.cpp` y documentar al cambiar.
- Importador de un subconjunto OpenAPI 3.x JSON/YAML. Resuelve únicamente referencias locales de un nivel en operaciones y Path Items; referencias externas/anidadas y otros mecanismos completos de la especificación siguen fuera de alcance.
- Suites de aserciones con estado, cabecera, ruta JSON y duración; la interfaz exige confirmación antes de la petición asociada.
- Respuesta con JSON legible, navegación en árbol acotada, búsqueda de texto, cabeceras e indicadores básicos de API Doctor.

## Beta experimental

### Comparador de respuestas JSON del historial

Acceso desde **Comparar seleccionadas · BETA** en el panel del historial.

1. Selecciona exactamente dos filas (Ctrl/Cmd + clic).
2. Pulsa el botón de comparación.
3. La vista presenta el contexto HTTP/duración y las diferencias JSON por ruta: `Added`, `Removed`, `Modified`, valores anteriores y nuevos.

Solo compara documentos JSON; no hay comparación de texto/XML ni diff de esquemas OpenAPI. Solo compara JSON; no compara textos/XML ni esquemas OpenAPI. El orden se normaliza cronológicamente. Objetos se recorren por clave; arrays se comparan por índice (no como conjuntos semánticos, por identidad de elemento ni con alineamiento de secuencias). Cambios de tipo se muestran como modificación. Las rutas usan notación tipo JSONPath sin ambigüedad: propiedades simples se muestran como `$.user.name`; nombres con puntos u otros caracteres especiales usan corchetes y comillas, por ejemplo `$['user.name']`, escapando comillas simples y barras invertidas. Entradas no JSON o cuerpos de texto omitidos por la redacción del historial no se pueden comparar estructuralmente. El resultado se limita a 2000 cambios y 48 niveles; si se alcanza el límite se informa que el resultado está truncado. La beta no altera el historial ni envía solicitudes. Las pruebas Qt Test cubren cambios, límites y rutas con caracteres especiales; compilación/ejecución sigue pendiente en el entorno sin CMake/Qt.

## Límites conocidos

- No hay agente de IA, proveedor, conversación persistente ni generación de un flujo desde lenguaje natural en la aplicación. El motion graphic promocional es una animación, no evidencia de esa función ejecutable.
- No hay bóveda de secretos del sistema operativo.
- No hay navegador de ejecuciones históricas de suites ni comparación general de esquemas OpenAPI.
- YAML es admitido por el importador, pero no todo el esquema YAML/OpenAPI está cubierto. `$ref` solo se resuelve localmente y a un nivel para operaciones y Path Items; referencias anidadas/externas y OAuth/security schemes siguen sin soporte general.
- No hay mocks/replay, plugin API ni sincronización remota.
- Ciertas capacidades de flujo y de presentación están incompletas; comprueba el código y README antes de describirlas como disponibles.

## Reglas para introducir una beta

La beta debe servir al trabajo de APIs, declarar su estado en README y aquí, definir manejo de errores/límites, no exponer secretos, añadir pruebas relevantes y distinguir la prueba escrita de la prueba que realmente se ejecutó.
