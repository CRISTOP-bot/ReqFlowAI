# Seguridad y privacidad

## Principios actuales

- La aplicación es local-first; ahora incluye una cuenta local de aplicación, pero no identidad de usuario ni sincronización remota implementada.
- Enviar solicitudes HTTP puede cambiar sistemas externos; la persona debe verificar destino, método, cuerpo y autorización.
- No debe incluirse información sensible en ejemplos, resultados de pruebas, capturas o reportes.

## Cuenta local de ReqFlow AI

El login es una **barrera local de la interfaz**, no una identidad web, SSO, autenticación remota ni protección del sistema operativo. No hay servidor ni sincronización de credenciales. En primer inicio se registra una sola cuenta; el nombre se normaliza trim/NFKC/casefold, la contraseña tiene un mínimo de 12 y máximo 1024 caracteres. En cada inicio se verifica el secreto mediante PBKDF2-HMAC-SHA-256 (600 000 iteraciones, 32 bytes) con sal aleatoria por cuenta de 16 bytes; el verificador se compara en tiempo constante. La contraseña no se guarda. Los fallos no registran ni imprimen la contraseña y la UI la vacía después del intento. No hay límite de intentos ni defensa frente a fuerza bruta offline sobre una copia de la base: PBKDF2 encarece cada prueba, pero no sustituye controles de acceso del sistema operativo.

**La autenticación no cifra la base SQLite ni protege frente al acceso directo al archivo, copias, modificaciones o malware/usuario con acceso al sistema operativo.** Quien controle el equipo/archivo puede saltarse la pantalla de login y leer/modificar los datos. El bloqueo tampoco borra ni cifra los proyectos. Olvidar la contraseña no tiene recuperación remota ni botón de reinicio destructivo; la aplicación no ofrece forma de recuperar la cuenta y sus datos. No borrar el archivo como supuesto restablecimiento: eso también borraría proyectos e historial. Proteger el dispositivo y respaldar el archivo de manera segura.

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
