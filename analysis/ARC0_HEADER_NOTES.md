# Observaciones de contenedores ARC0 y audio

Fecha: 2026-10-14

Esta nota registra solo cabeceras y tamaños observados directamente desde la RomFS; no contiene payloads ni archivos extraídos.

## Contenedores FA

Los ocho archivos `.fa` empiezan con:

- Magic ASCII: `ARC0`
- Cabecera de 72 bytes (`0x48`); el valor `0x48` es el primer campo de offset, no parte del magic.
- Los campos de índice cambian entre `yw2_a.fa` y los siete archivos de idioma, por lo que el archivo principal y la localización comparten formato pero no necesariamente el mismo número de entradas.

El índice usa cuatro regiones comprimidas: entradas de directorio, hashes de directorio, entradas de archivo y tabla de nombres Shift-JIS. En esta ROM se observó la misma combinación en los ocho contenedores:

| Región | Compresión |
|---|---|
| Entradas de directorio | Level-5 Huffman 8-bit |
| Hashes de directorio | Sin compresión |
| Entradas de archivo | Level-5 Huffman 8-bit |
| Nombres | Level-5 LZ10 |

La herramienta reproducible es [`tools/inspect_arc0.py`](../tools/inspect_arc0.py). Su estructura sigue el lector ARC0 público de [Kuriimu2](https://github.com/FanTranslatorsInternational/Kuriimu2/blob/imgui/plugins/Level5/plugin_level5/N3DS/Archive/Arc0.cs), pero solo implementa lectura de metadatos para este proyecto.

Tamaños lógicos:

| Archivo | Tamaño |
|---|---:|
| `yw2_a.fa` | 967.055.688 bytes |
| `yw2_lg_engb.fa` | 133.166.756 bytes |
| `yw2_lg_de.fa` | 134.495.136 bytes |
| `yw2_lg_es.fa` | 133.920.492 bytes |
| `yw2_lg_fr.fa` | 134.710.088 bytes |
| `yw2_lg_it.fa` | 133.524.532 bytes |
| `yw2_lg_nl.fa` | 133.238.340 bytes |
| `yw2_lg_ru.fa` | 136.836.304 bytes |

## Audio

Las cabeceras son reconocibles sin decodificar el contenido:

- 7 bancos `.bcsar`: magic `CSAR`.
- 7.011 streams `.bcstm`: magic `CSTM`, con secciones `INFO`, `SEEK ` y `DATA`.

Esto permite separar desde el principio el trabajo del port en tres lectores independientes: archivo `ARC0`, banco de audio `CSAR` y stream `CSTM`.

## Resultado del lector

El lector emite:

1. ruta lógica,
2. offset,
3. tamaño,
4. flags de compresión,
5. CRC32 almacenado para el nombre.

Los manifiestos completos se escriben fuera del repositorio y mantienen los datos propietarios fuera de GitHub. El resumen de comparación está en [`analysis/ARC0_INVENTORY.md`](ARC0_INVENTORY.md).
