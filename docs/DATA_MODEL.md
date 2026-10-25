# Modelo de datos y persistencia

## Base

`ProjectStore` usa SQLite mediante el driver `QSQLITE`. La ruta por defecto documentada es `~/.local/share/reqflow-ai/reqflow.sqlite3`; las pruebas deberían usar archivos temporales. El constructor crea las tablas cuando no existen y activa `PRAGMA foreign_keys=ON` en la conexión.

## Entidades presentes

| Tabla | Propósito | Relación/retención |
|---|---|---|
| `projects` | proyectos locales | entidad raíz |
| `requests` | peticiones HTTP con nombre, método, URL, parámetros, cabeceras y cuerpo | pertenece a un proyecto; cascade al borrar proyecto |
| `history` | resultado HTTP resumido y cuerpo retenido con filtros | pertenece a proyecto; el cuerpo texto no JSON se omite y JSON/cabeceras pasan por redacción heurística |
| `flows` | definición JSON de flujo y versión numérica | pertenece a proyecto; cascade |
| `flow_runs` | estado y resultado textual de ejecución | pertenece a flujo; cascade |
| `environments` | nombre y objeto JSON de variables | pertenece a proyecto; nombre único por proyecto |
| `assertion_suites` | nombre, definición JSON y referencia opcional a petición | pertenece a proyecto; referencia request con `ON DELETE SET NULL` |
| `assertion_runs` | estado y resultado JSON de la suite | pertenece a suite; cascade |

El esquema real y las declaraciones SQL son la referencia al modificar tablas. Al cambiar columnas, índices, restricciones, relaciones o estrategia de retención, actualiza este archivo y los tests de persistencia en el mismo cambio. El código actual usa `CREATE TABLE IF NOT EXISTS`; documenta expresamente cualquier migración nueva y cómo conserva las bases existentes.

## Filtros antes de persistir

La capa `ProjectStore` rechaza ciertos campos cuyos nombres parecen secretos, limpia URL y cabeceras de historial y solo persiste cuerpos JSON limpios; cuerpos no JSON se reemplazan por un mensaje de omisión. Son heurísticas conservadoras, no protección criptográfica. Las variables de entornos con nombres sospechosos se rechazan porque no hay integración de almacén seguro del sistema operativo.

## Comparación beta

La beta lee `history.response_body` de dos IDs. No persiste su comparación; solo la presenta en un diálogo. La comparación usa el cuerpo que quedó retenido y saneado, por lo cual no garantiza igualdad con el cuerpo recibido originalmente. Consulta [Seguridad y privacidad](SECURITY.md) antes de interpretar resultados.
