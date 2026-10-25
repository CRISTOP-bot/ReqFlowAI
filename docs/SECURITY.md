# Seguridad y validación de credenciales

## Protección local actual

ReqFlow AI no incluye una bóveda de credenciales del sistema operativo y la base SQLite no está cifrada. Los filtros de campos potencialmente sensibles son heurísticos: pueden dar falsos positivos y no detectar todos los secretos. No se deben describir como garantía ni como sustituto de Keychain, Secret Service, Credential Manager o controles del sistema operativo.

## Nodos HTTP de flujos

Antes de preparar o enviar una solicitud, el motor comprueba el objeto de configuración del nodo HTTP con el filtro de nombres sensibles. Si el filtro detecta un campo potencialmente secreto, el nodo termina como fallido y no llama al cliente HTTP. Una prueba Qt de regresión, `flowRequestWithSecretIsRejectedBeforeNetwork`, usa una cabecera Authorization sintética y comprueba que el flujo falla, registra el nodo como fallido y no produce variables de respuesta.

La prueba está añadida al target `reqflow-core-tests`. Su resultado solo debe declararse verificado después de que el build y CTest de GitHub Actions terminen correctamente; la inspección del código no sustituye su ejecución.

## Recomendaciones

- No guardar tokens, contraseñas, cookies ni credenciales reales en requests o flujos.
- Usar datos sintéticos en fixtures y pruebas.
- Verificar destino, método y autorización antes de enviar cualquier petición: las solicitudes HTTP pueden modificar sistemas externos.
- Revisar la cobertura y posibles falsos positivos del filtro cuando cambien los formatos de configuración.
