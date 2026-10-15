# Clasificación de formatos internos

Fecha: 2026-10-14

Este análisis lee cabeceras de recursos, tablas de nombres `XPCK` y tablas comprimidas de metadatos `XQ32`. No extrae ni publica payloads.

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

## IMGC

Los 2.050 archivos `.xi` inspeccionados tienen la firma de ocho bytes `IMGC00\0\0`.
Los campos observados en `0x14`, `0x18` (16 bits), `0x1a` (16 bits), `0x1c` y
`0x20` son invariantes en todo el conjunto: `0x30`, `0x30`, `1`, `0x48` y `3`,
respectivamente. Los pares de 16 bits en `0x10/0x12` varían con el recurso; los
dejamos como huella estructural, no como una afirmación definitiva sobre el
formato de píxel. Los más frecuentes son `64x64` (890), `512x256` (346),
`128x128` (264) y `1024x1024` (211).

`tools/inspect_imgc.py` registra estos campos y comprueba la firma sin decodificar
texturas. La siguiente etapa deberá validar formato de píxel, paleta y orden
de bloques contra la implementación móvil de referencia.

## XPCK anidado

Se inspeccionaron los 11.163 archivos de nivel superior cuya extensión y firma indican `XPCK`.

- 493.042 entradas internas.
- 9.790 tablas de nombres usan `LZ10`; 1.373 no están comprimidas.
- No se produjeron errores de lectura.
- El campo de tipo XPCK observado toma los valores `0`–`11`; todavía no se ha asignado semántica definitiva a cada tipo.

La distribución por extensión del contenedor padre ayuda a separar familias,
sin convertir todavía los valores numéricos en nombres semánticos:

| Contenedor padre | Tipos más frecuentes |
|---|---|
| `.xc` | `6`: 2.201; `3`: 1.763; `5`: 1.380; `7`: 1.069 |
| `.xa` | `5`: 512; `6`: 95; `2`: 86; `8`: 61 |
| `.pck` | `0`: 309; `1`: 257; `3`: 190; `4`: 179 |
| `.xr` | `3`: 419; `1`: 48 |
| `.xv` | `1`: 861 |
| `.xk` | `2`: 115 |

Los tipos `6`–`8` concentran recursos de modelo/animación (`.mbn`, `.prm`,
`.atr`, `.mtr`, `.txp`, `.mtninf`), mientras que el tipo `3` concentra más
`.cmn`, `.mtn2`, `.xi` y datos auxiliares. Es una correlación útil para el
runtime nativo, no una especificación suficiente para interpretar cada
payload.

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

## XQ32

Se validaron los 2.789 archivos `.xq`: no hubo errores. Las tablas usan los
offsets en unidades de cuatro bytes del encabezado XQ32 y sus tamaños
descomprimidos coinciden con los layouts conocidos (`function` 0x18/0x20,
`jump` e `instruction` 0x0c/0x10, `argument` 0x08/0x10) cuando la tabla tiene
entradas.

| Tabla | Métodos observados | Entradas con layout identificado |
|---|---|---:|
| `function` | RLE 1.904; LZ10 451; Huffman4 187; none 246; Huffman8 1 | 2.788 |
| `jump` | none 1.952; LZ10 819; Huffman4 18 | 1.537 |
| `instruction` | LZ10 2.533; none 256 | 2.778 |
| `argument` | LZ10 2.537; none 248; Huffman8 4 | 2.778 |
| `strings` | LZ10 1.910; none 878; Huffman8 1 | no aplica |

`none` aquí identifica el método Level-5 sin compresión, no una extracción de
la tabla. El lector [`tools/inspect_xq32.py`](../tools/inspect_xq32.py) valida
cabeceras, regiones y tamaños, pero no publica nombres, instrucciones ni
bytecode.

El lector nativo de `core/` comparte esa validación, descomprime las cinco
regiones y conserva las filas decodificadas como palabras de 32 bits. El
runtime usa esas filas como puntos de traza del evento inicial; todavía no
asigna semántica a ningún opcode. Esta separación permite recuperar el ABI con
pruebas reproducibles sin convertir una hipótesis sobre el bytecode en lógica
de juego.

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

/workspace/.local/pyctr-venv/bin/python tools/inspect_xq32.py \
  --rom /ruta/al/dump.3ds \
  --arc0-manifest /ruta/local/yw2_a.json \
  --json-out /ruta/local/xq32.json

/workspace/.local/pyctr-venv/bin/python tools/inspect_imgc.py \
  --rom /ruta/al/dump.3ds \
  --arc0-manifest /ruta/local/yw2_a.json \
  --json-out /ruta/local/imgc.json
```

Los JSON completos contienen nombres de recursos y deben permanecer fuera de GitHub.
