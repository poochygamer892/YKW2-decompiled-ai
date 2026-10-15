# Evaluación de herramientas de ingeniería inversa (2026-10-15)

Objetivo inmediato: recuperar con evidencia la semántica de eventos XQ32 y
recursos de Yo-kai Watch 2 (3DS) para clientes nativos Android/iOS. No se
incluyen binarios ni recursos del juego en el repositorio.

| Herramienta | Evidencia publicada | Decisión actual |
| --- | --- | --- |
| [decomp.wtf](https://github.com/i12bp8/decomp.wtf) | Su README enumera importación inicial de N64, PS1/PS2, GameCube, Wii, GBA y DS; no 3DS. La ruta de decompilación pública depende de un proyecto ya configurado. | No instalar ni alimentar con el dump 3DS: su importador actual no cubre esta plataforma. Reconsiderar si aparece soporte 3DS o una ruta pública de proyecto comprobada. |
| [re-skill](https://github.com/vgrichina/re-skill) | Su README describe una skill de Claude Code para proyectos retro, con registro de hallazgos, verificaciones y port web. | No copiar su bucle de auto-commits ni su port web al runtime nativo. Usar el principio de documentar evidencia, incertidumbre y pruebas reproducibles. |

Siguiente punto de validación: con el dump autorizado disponible localmente,
identificar el recurso de título correcto y contrastar las operaciones XQ32
con comportamiento observado antes de implementar handlers. Hasta entonces,
el menú muestra solo las funciones de inspección/catalogación realmente
disponibles y mantiene la partida bloqueada.
