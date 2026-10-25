# Instrucciones para agentes y colaboradores de ReqFlow AI

## Regla obligatoria: documentar TODO

**Todo cambio debe documentarse. Sin excepciones.** Cada funcionalidad, corrección, refactor, cambio de interfaz, modificación de datos, regla de seguridad, dependencia, prueba o limitación que se agregue, quite o altere debe quedar explicado en la documentación correspondiente y enlazado desde el índice de `docs/` cuando sea una materia nueva o sustancial.

Antes de terminar cualquier tarea:

1. Actualiza `README.md` si cambia la instalación, el uso, el estado del producto, una función visible o una limitación relevante.
2. Actualiza uno o más documentos de `docs/` para describir con detalle el comportamiento, arquitectura, modelo de datos, seguridad, pruebas o decisiones afectadas.
3. Añade o actualiza pruebas para el comportamiento modificable; registra qué cubren y qué no.
4. Mantén este archivo y `docs/INDEX.md` coherentes si cambian las reglas de contribución o la estructura de documentación.
5. Registra todos los archivos afectados y el motivo de cada cambio en el resumen de entrega o en la descripción del commit/PR.
6. Anota los comandos de validación realmente ejecutados y sus resultados exactos. Si algo no se pudo ejecutar, indícalo como pendiente; nunca lo describas como aprobado.

No se permite cerrar un cambio diciendo «documentación pendiente», dejar un TODO de documentación para después ni asumir que el código se explica por sí solo. Si el alcance no permite terminar la documentación, el cambio no está terminado.

## Reglas de implementación

- ReqFlow AI es una aplicación de escritorio nativa C++20 y Qt 6 para trabajar con APIs HTTP, proyectos y flujos de API. Las mejoras beta deben permanecer dentro de ese propósito.
- Revisa el código y la documentación existentes antes de cambiar el diseño. Conserva las responsabilidades separadas: UI Qt, red, persistencia, motor de flujos, importación OpenAPI y utilidades de pruebas.
- No afirmes que una función está implementada, que una prueba pasa o que un binario funciona sin evidencia de la ejecución correspondiente.
- Mantén claros los estados **implementado**, **beta/experimental**, **parcial** y **pendiente** en `README.md` y documentos pertinentes.
- Evita exponer secretos en historial, logs, capturas, ejemplos, pruebas, documentación o diffs. La redacción heurística actual no equivale a una bóveda segura.
- Toda operación que envíe solicitudes a un servidor debe hacer visibles el destino y el efecto, e incluir una confirmación cuando el flujo automatizado pueda ejecutar una acción de red sin una interacción inmediata del usuario.
- No agregues telemetría, servicios remotos, cuentas, dependencias pesadas, funciones no relacionadas ni cambios destructivos sin necesidad y justificación documentadas.
- Las mejoras beta deben tener límites documentados, comportamiento ante entradas inválidas y pruebas deterministas cuando resulte posible.

## Reglas para pruebas y reportes

- Ejecuta primero las pruebas focalizadas del cambio y, si las herramientas están disponibles, también la compilación y suite completa.
- Reporta por separado pruebas escritas, pruebas ejecutadas y pruebas que no se pudieron ejecutar.
- Registra plataforma, herramientas y dependencias relevantes cuando afecten la reproducibilidad.
- Si CMake, Qt, el compilador, QSQLITE u otra dependencia faltan, di exactamente cuál falta. No sustituyas compilación real por un análisis estático.
- No declares terminada una función cuyo recorrido principal no haya sido probado o cuyo estado no pueda verificarse.

## Lista de cierre obligatoria

- [ ] README actualizado si cambió uso, estado, requisitos o limitaciones.
- [ ] Documentación de `docs/` actualizada y enlazada desde `docs/INDEX.md`.
- [ ] Cada archivo tocado queda explicado en la entrega.
- [ ] Prueba de regresión o justificación documentada de por qué no aplica.
- [ ] Comandos/resultados reales consignados; lo no ejecutado figura como pendiente.
- [ ] Secretos, datos personales y acciones de red revisados.
- [ ] Función clasificada correctamente: implementada, beta, parcial o pendiente.
