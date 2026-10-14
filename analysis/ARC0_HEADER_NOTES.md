# Observaciones de contenedores ARC0 y audio

Fecha: 2026-10-14

Esta nota registra solo cabeceras y tamaños observados directamente desde la RomFS; no contiene payloads ni archivos extraídos.

## Contenedores FA

Los ocho archivos `.fa` empiezan con:

- Magic ASCII: `ARC0`
- Variante de cabecera observada: `ARC0H`
- Tamaño de cabecera little-endian: `0x48`
- Los campos de índice cambian entre `yw2_a.fa` y los siete archivos de idioma, por lo que el archivo principal y la localización comparten formato pero no necesariamente el mismo número de entradas.

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

## Siguiente tarea técnica

Implementar un lector de índice `ARC0` que emita únicamente:

1. ruta lógica,
2. offset,
3. tamaño,
4. flags de compresión,
5. hash local opcional.

El lector debe escribir un manifiesto fuera del repositorio y mantener los datos propietarios fuera de GitHub. Después se podrá comparar la estructura con los contenedores de la versión móvil de referencia.
