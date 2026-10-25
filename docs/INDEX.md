# Índice de documentación

Esta documentación describe el estado observado en el código de ReqFlow AI. Las etiquetas **implementado**, **beta**, **parcial** y **pendiente** distinguen lo disponible de lo propuesto. Si código y documentos divergen, hay que corregir la documentación como parte del mismo cambio.

## Documentos

- [Arquitectura](ARCHITECTURE.md): módulos, dependencias y flujo de una solicitud HTTP.
- [Funciones y estado](FEATURES.md): qué puede hacer el producto, beta, límites y trabajo pendiente.
- [Datos y persistencia](DATA_MODEL.md): SQLite, tablas, relaciones, entorno local e historial.
- [Seguridad y privacidad](SECURITY.md): datos retenidos, redacción, cuenta local, límites de protección y amenazas no cubiertas.
- [Desarrollo y pruebas](DEVELOPMENT.md): requisitos, build, pruebas, integración Rust parcial, FFI y evidencia de validación.
- [Reglas para agentes y colaboradores](../AGENT.md): obligación de documentar **todo** lo que se cambia y lista de cierre.

## Convención para mantener esta documentación

Cada cambio de código o producto debe revisar README y los documentos pertinentes. Una función nueva requiere una descripción de comportamiento y límites; cambios de persistencia requieren actualizar el modelo de datos; cambios de seguridad requieren actualizar la evaluación de seguridad; los cambios de pruebas deben explicar cobertura y ejecución. Ningún documento debe convertir una intención o un test no ejecutado en una afirmación de funcionalidad verificada.
