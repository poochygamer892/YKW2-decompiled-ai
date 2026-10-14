# Clasificación de formatos internos

Fecha: 2026-10-14

Este análisis lee como máximo 64 bytes por recurso y las tablas de nombres de los contenedores `XPCK`. No extrae ni publica payloads.

## Cabeceras del ARC0 principal

| Extensión | Cantidad | Tamaño lógico | Cabecera dominante | Clasificación inicial |
|---|---:|---:|---|---|
| `.xc` | 7.796 | 781.640.956 | `XPCK` | contenedor de recursos |
| `.xa` | 788 | 65.183.112 | `XPCK` | contenedor de recursos/audio |
| `.pck` | 1.113 | 7.722.340 | `XPCK` | contenedor de recursos |
| `.xr` | 481 | 34.984.992 | `XPCK` | contenedor de recursos |
| `.xv` | 866 | 2.970.920 | `XPCK` | contenedor de recursos |
| `.xi` | 2.050 | 51.809.508 | `IMGC` | imagen/textura |
| `.xq` | 2.789 | 5.973.992 | `XQ32` | script/bytecode |
| `.xs` | 89 | 25.444 | `XSCR` | script/configuración |
| `.cfg` | 382 | 136.075 | `CAME` | configuración de cámara |
| `.npcbin` | 5.243 | 2.436.576 | entero little-endian variable | datos NPC |
| `.bin` | 6.805 | 10.980.480 | entero little-endian variable | datos binarios diversos |

Otros formatos menos frecuentes (`.actbin`, `.astarbin`, `.chartbin`, `.ctpk`, `.efp3ds`, `.icn`, `.mapenv`, `.rt`, `.xf`, `.xk`) requieren parsers específicos; se mantienen clasificados por extensión y firma en el manifiesto local.

## XPCK anidado

Se inspeccionaron los 11.163 archivos de nivel superior cuya extensión y firma indican `XPCK`.

- 493.042 entradas internas.
- 9.790 tablas de nombres usan `LZ10`; 1.373 no están comprimidas.
- No se produjeron errores de lectura.
- El campo de tipo XPCK observado toma los valores `0`–`11`; todavía no se ha asignado semántica definitiva a cada tipo.

Extensiones internas dominantes:

| Extensión interna | Entradas |
|---|---:|
| `.mbn` | 105.836 |
| `.prm` | 72.879 |
| `.cmn` | 57.840 |
| `.atr` | 43.905 |
| `.mtr` | 43.884 |
| `.txp` | 44.625 |
| `.mtninf` | 34.930 |
| `.pbi` | 26.961 |
| `.xi` | 20.730 |
| `.mtn2` | 6.515 |
| `.npcbin` | 5.242 |

El lector [`tools/inspect_xpck.py`](../tools/inspect_xpck.py) solo enumera índices, nombres y tamaños; no descomprime `RES.bin` ni escribe recursos.

## Comparación con las versiones móviles de referencia

Se compararon las 2.495 rutas que existen simultáneamente en `yw2_a.fa`, el APK y el IPA. Para cada ruta se compararon los primeros 4 bytes y el tamaño declarado en el ZIP.

| Extensión | Rutas | Tamaño igual 3DS/APK | Tamaño igual 3DS/IPA | Cabecera igual 3DS/APK | Cabecera igual 3DS/IPA |
|---|---:|---:|---:|---:|---:|
| `.xc` | 1.044 | 7 | 7 | 1.044 | 1.044 |
| `.xi` | 639 | 6 | 6 | 9 | 9 |
| `.xq` | 323 | 91 | 91 | 323 | 323 |
| `.npcbin` | 198 | 102 | 102 | 169 | 169 |
| `.bin` | 69 | 34 | 34 | 35 | 35 |
| `.xa` | 125 | 0 | 0 | 125 | 125 |
| `.xs` | 45 | 37 | 37 | 45 | 45 |
| `.xk` | 30 | 4 | 4 | 30 | 30 |
| `.mapenv` | 2 | 2 | 2 | 2 | 2 |

La igualdad de firma en `XPCK`, `XQ32` y `XSCR` confirma una familia de formatos compartida, pero la baja igualdad de tamaños demuestra que el APK/IPA de Yo-kai Watch 1 es una referencia de formato y runtime, no un sustituto directo de los datos de Yo-kai Watch 2.

## Herramienta

```bash
/workspace/.local/pyctr-venv/bin/python tools/compare_format_headers.py \
  --rom /ruta/al/dump.3ds \
  --arc0-manifest /ruta/local/yw2_a.json \
  --apk /ruta/al/referencia.apk \
  --ipa /ruta/al/referencia.ipa \
  --json-out /ruta/local/format-comparison.json

/workspace/.local/pyctr-venv/bin/python tools/inspect_xpck.py \
  --rom /ruta/al/dump.3ds \
  --arc0-manifest /ruta/local/yw2_a.json \
  --json-out /ruta/local/xpck.json
```

Los JSON completos contienen nombres de recursos y deben permanecer fuera de GitHub.
