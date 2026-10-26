# Modelo de datos y persistencia

## Ubicación de SQLite

Cuando no se entrega una ruta explícita, el escritorio mantiene `~/.local/share/reqflow-ai/reqflow.sqlite3`. En Android usa `QStandardPaths::AppDataLocation/reqflow.sqlite3`, dentro del almacenamiento privado de la aplicación; Android elimina esos datos al desinstalarla y no hay migración automática desde bases externas. Las pruebas de almacenamiento deben pasar rutas explícitas cuando necesiten directorios temporales.

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
| `local_account` | cuenta local singleton: usuario normalizado, sal, verificador derivado e iteraciones | id=1; inicialmente vacía; no contiene contraseñas |

El esquema real y las declaraciones SQL son la referencia al modificar tablas. Al cambiar columnas, índices, restricciones, relaciones o estrategia de retención, actualiza este archivo y los tests de persistencia en el mismo cambio. El código actual usa `CREATE TABLE IF NOT EXISTS`; documenta expresamente cualquier migración nueva y cómo conserva las bases existentes.

## Cuenta local

Al abrir la base se crea la tabla singleton `local_account` sin fila hasta completar el alta. La fila contiene el usuario trim/NFKC/casefold, la sal individual codificada en Base64, el verificador PBKDF2-HMAC-SHA-256 y las iteraciones (600 000). No se persiste contraseña. El alta se realiza en una transacción y la clave primaria/check limita la tabla a una cuenta; al crear segunda cuenta, se rechaza sin cambiar la existente. La API de autenticación vive en `ProjectStore`; la base de datos es la misma que contiene el resto del trabajo y **no está cifrada**.

El conteo de iteraciones se verifica exactamente en su valor actual: un dato dañado/no reconocido bloquea la autenticación en vez de reinterpretarse. No hay mecanismo de recuperación; olvidar la contraseña no autoriza borrar/restablecer la tabla ni toca datos del usuario.

## Filtros antes de persistir

La capa `ProjectStore` rechaza ciertos campos cuyos nombres parecen secretos, limpia URL y cabeceras de historial y solo persiste cuerpos JSON limpios; cuerpos no JSON se reemplazan por un mensaje de omisión. Son heurísticas conservadoras, no protección criptográfica. Las variables de entornos con nombres sospechosos se rechazan porque no hay integración de almacén seguro del sistema operativo.

## Comparación beta

La beta lee `history.response_body` de dos IDs. No persiste su comparación; solo la presenta en un diálogo. La comparación usa el cuerpo que quedó retenido y saneado, por lo cual no garantiza igualdad con el cuerpo recibido originalmente. Consulta [Seguridad y privacidad](SECURITY.md) antes de interpretar resultados.
