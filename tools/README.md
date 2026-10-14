# Tools

Utilidades de inventario, hashing, validación, conversión y generación de informes.

Las herramientas deben ser reproducibles, evitar sobrescribir entradas y escribir sus resultados en rutas ignoradas cuando contengan datos derivados del material analizado.

## ARC0

`inspect_arc0.py` lee índices Level-5 `ARC0` desde un `.fa` o directamente desde la RomFS de un dump 3DS descifrado. Genera un resumen y, opcionalmente, un JSON local con rutas, offsets, tamaños y CRC32; nunca extrae el payload.

`compare_arc0_mobile.py` compara esos manifiestos con los directorios ZIP del APK/IPA usando solo rutas normalizadas y tamaños declarados. Los JSON generados deben permanecer fuera del repositorio porque contienen nombres de recursos propietarios.

`compare_format_headers.py` clasifica firmas de recursos y compara los primeros 4 bytes/tamaños de rutas compartidas. `inspect_xpck.py` enumera las tablas internas de `XPCK` sin extraer `RES.bin` ni otros payloads. `inspect_xq32.py` valida las tablas y métodos de compresión de scripts `XQ32`; `inspect_imgc.py` registra la huella de cabecera de texturas `IMGC` sin decodificarlas.
