# Seguridad y privacidad

## Principios actuales

- La aplicación es local-first y no tiene cuenta de usuario ni sincronización remota implementada.
- Enviar solicitudes HTTP puede cambiar sistemas externos; la persona debe verificar destino, método, cuerpo y autorización.
- No debe incluirse información sensible en ejemplos, resultados de pruebas, capturas o reportes.

## Credenciales y variables

Aún no hay bóveda de Keychain, Secret Service, Credential Manager ni otro almacén cifrado del sistema operativo. La UI y `ProjectStore` aplican filtros heurísticos según nombres de campo/variable para impedir algunos casos sensibles. La detección no es completa, no cifra valores y no equivale a gestión segura de credenciales. No describas el filtro como una garantía ni recomiendes almacenar secretos en texto claro.

## Historial

El historial omite mensajes de error de red, sanea URL y cabeceras según nombres potencialmente sensibles, redacta campos identificables en JSON y sustituye cuerpos de texto no JSON por una notificación de omisión. La redacción no detecta todos los secretos, PII ni formas de codificación. Los resultados de comparación beta se limitan al contenido que haya sobrevivido a esta redacción.

## Comparador beta

Compara solo datos locales ya guardados; no envía peticiones ni exporta datos. Presenta valores anteriores y nuevos, así que esos valores pueden contener información privada que no fue detectada por los filtros. El usuario debe proteger la base local y revisar el resultado antes de compartir capturas.

## Datos que no se deben revelar

No subas bases SQLite personales, respuestas reales con información sensible, tokens, claves, cookies, URLs con credenciales ni capturas privadas. En commits de código usa fixtures sintéticos. Documenta cualquier nueva fuente de datos, campo persistido, registro, telemetría o transferencia externa antes de implementarla.

## Al reportar vulnerabilidades

Describe el impacto, el componente afectado y una reproducción segura con datos sintéticos. No publiques secretos ni datos de terceros. Distingue hallazgos reproducidos de hipótesis y cubre la corrección y regresión en la documentación.
