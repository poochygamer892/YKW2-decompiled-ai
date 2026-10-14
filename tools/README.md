# Tools

Utilidades de inventario, hashing, validación, conversión y generación de informes.

Las herramientas deben ser reproducibles, evitar sobrescribir entradas y escribir sus resultados en rutas ignoradas cuando contengan datos derivados del material analizado.

## ARC0

`inspect_arc0.py` lee índices Level-5 `ARC0` desde un `.fa` o directamente desde la RomFS de un dump 3DS descifrado. Genera un resumen y, opcionalmente, un JSON local con rutas, offsets, tamaños y CRC32; nunca extrae el payload.

`compare_arc0_mobile.py` compara esos manifiestos con los directorios ZIP del APK/IPA usando solo rutas normalizadas y tamaños declarados. Los JSON generados deben permanecer fuera del repositorio porque contienen nombres de recursos propietarios.
