# Funciones y estado del producto

Este archivo es un inventario de capacidades basado en el código actual. Una función que aparezca aquí como beta o parcial no se debe presentar como producto estable.

## Implementación presente

- Cliente HTTP de escritorio con métodos GET, POST, PUT, PATCH, DELETE, HEAD y OPTIONS, URL, query params, cabeceras y cuerpo.
- Proyectos, peticiones guardadas, historial y suites almacenados localmente en SQLite.
- Entornos locales por proyecto con expansión literal de `{{identificador}}` en URL, parámetros, cabeceras y cuerpo. Los nombres que parecen secretos se rechazan mientras no exista bóveda del sistema.
- Editor visual y motor de flujos con clases de nodo implementadas en el núcleo; los tipos exactos y sus límites se deben revisar en `src/flow/flowengine.cpp` y documentar al cambiar.
- Importador de un subconjunto OpenAPI 3.x JSON. La cobertura no equivale al soporte general de OpenAPI.
- Suites de aserciones con estado, cabecera, ruta JSON y duración; la interfaz exige confirmación antes de la petición asociada.
- Respuesta con JSON legible, navegación en árbol acotada, búsqueda de texto, cabeceras e indicadores básicos de API Doctor.

## Beta experimental

### Comparador de respuestas JSON del historial

Acceso desde **Comparar seleccionadas · BETA** en el panel del historial.

1. Selecciona exactamente dos filas (Ctrl/Cmd + clic).
2. Pulsa el botón de comparación.
3. La vista presenta el contexto HTTP/duración y las diferencias JSON por ruta: `Added`, `Removed`, `Modified`, valores anteriores y nuevos.

El orden se normaliza cronológicamente. Objetos se recorren por clave; arrays se comparan por índice (no como conjuntos semánticos, por identidad de elemento ni con alineamiento de secuencias). Cambios de tipo se muestran como modificación. Entradas no JSON o cuerpos de texto omitidos por la redacción del historial no se pueden comparar estructuralmente. El resultado se limita a 2000 cambios y 48 niveles; si se alcanza el límite se informa que el resultado está truncado. La beta no altera el historial ni envía solicitudes. El módulo puro tiene cobertura Qt Test escrita, pero la compilación/ejecución no se ha verificado en el entorno que carece de CMake/Qt.

## Límites conocidos

- No hay agente de IA, proveedor, conversación persistente ni generación de un flujo desde lenguaje natural en la aplicación. El motion graphic promocional es una animación, no evidencia de esa función ejecutable.
- No hay bóveda de secretos del sistema operativo.
- No hay navegador de ejecuciones históricas de suites ni comparación general de esquemas OpenAPI.
- No hay soporte completo de YAML, `$ref`, OAuth/security schemes ni de toda la especificación OpenAPI.
- No hay mocks/replay, plugin API ni sincronización remota.
- Ciertas capacidades de flujo y de presentación están incompletas; comprueba el código y README antes de describirlas como disponibles.

## Reglas para introducir una beta

La beta debe servir al trabajo de APIs, declarar su estado en README y aquí, definir manejo de errores/límites, no exponer secretos, añadir pruebas relevantes y distinguir la prueba escrita de la prueba que realmente se ejecutó.
