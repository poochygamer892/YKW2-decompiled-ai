# Contribuir

## Regla principal

No subas ROMs, dumps, claves, recursos extraídos, ejecutables propietarios ni otro material cuya redistribución no esté autorizada. Usa rutas locales ignoradas por Git y documenta únicamente metadatos, hashes y resultados que puedan compartirse.

## Investigación

Cada hallazgo debe indicar:

- fuente o artefacto de entrada;
- versión y hash, cuando sea posible;
- método utilizado;
- evidencia observada;
- nivel de confianza;
- siguiente prueba propuesta.

Distingue claramente entre hechos observados, inferencias y decisiones de implementación.

## Cambios de código

- Mantén el código específico de plataforma separado del núcleo.
- Añade pruebas para formatos y reglas de juego que se puedan verificar sin recursos propietarios.
- Evita mezclar refactors grandes con cambios de comportamiento.
- Describe las limitaciones de cada implementación.

## Pull requests

Incluye un resumen, las pruebas ejecutadas y cualquier dependencia local necesaria para reproducirlas.
