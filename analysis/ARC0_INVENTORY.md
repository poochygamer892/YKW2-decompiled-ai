# Inventario de índices ARC0

Fecha: 2026-10-14

El lector [`tools/inspect_arc0.py`](../tools/inspect_arc0.py) procesó las ocho tablas ARC0 desde la RomFS, sin extraer ni subir archivos propietarios. Los conteos se basan en las entradas declaradas por cada índice; los manifiestos completos se conservaron solo en el entorno local.

## Contenedores 3DS

Todos usan `Huffman8` para las tablas de directorio y archivo, ninguna compresión para hashes y `LZ10` para nombres.

| Contenedor | Directorios | Archivos | Tamaño del contenedor | Payload declarado |
|---|---:|---:|---:|---:|
| `yw2_a.fa` | 4.503 | 28.587 | 967.055.688 | 966.361.295 |
| `yw2_lg_engb.fa` | 1.118 | 5.977 | 133.166.756 | 133.013.648 |
| `yw2_lg_de.fa` | 1.118 | 5.979 | 134.495.136 | 134.344.084 |
| `yw2_lg_es.fa` | 1.118 | 5.979 | 133.920.492 | 133.769.456 |
| `yw2_lg_fr.fa` | 1.118 | 5.979 | 134.710.088 | 134.558.980 |
| `yw2_lg_it.fa` | 1.118 | 5.978 | 133.524.532 | 133.373.484 |
| `yw2_lg_nl.fa` | 1.118 | 5.978 | 133.238.340 | 133.087.260 |
| `yw2_lg_ru.fa` | 1.118 | 5.980 | 136.836.304 | 136.684.732 |

Extensiones más frecuentes del archivo principal:

| Extensión | Entradas |
|---|---:|
| `.xc` | 7.796 |
| `.bin` | 6.805 |
| `.npcbin` | 5.243 |
| `.xq` | 2.789 |
| `.xi` | 2.050 |
| `.pck` | 1.113 |
| `.xa` | 788 |

Las tablas de idioma contienen principalmente `.bin`, `.xa`, `.xc`, `.xi` y `.xr`, con pequeñas diferencias de conteo entre idiomas.

## Comparación con APK e IPA de referencia

La comparación usa únicamente nombres normalizados y tamaños declarados en el directorio ZIP; no descomprime ni calcula hashes de los paquetes.

| Fuente | Rutas normalizadas | Coinciden APK/IPA |
|---|---:|---:|
| APK Android | 19.033 | 18.993 |
| IPA iOS | 19.027 | 18.993 |

Comparación de rutas exactas con `yw2_a.fa`:

- 2.495 rutas aparecen tanto en el índice 3DS como en el APK.
- Las mismas 2.495 aparecen también en el IPA.
- Los siete índices de idioma no tienen coincidencias exactas con los paquetes móviles de Yo-kai Watch 1, como cabe esperar de recursos localizados y versiones de juego distintas.

La coincidencia de rutas del archivo principal muestra una interfaz de recursos reutilizable entre generaciones, pero no demuestra que los payloads sean binariamente compatibles. La clasificación de formatos y la lectura de los índices `XPCK` están documentadas en [`analysis/FORMAT_HEADER_INVENTORY.md`](FORMAT_HEADER_INVENTORY.md).

## Uso

```bash
# Usa el Python del entorno donde se instaló tools/requirements-3ds.txt.
/workspace/.local/pyctr-venv/bin/python tools/inspect_arc0.py \
  --rom /ruta/al/dump.3ds \
  --romfs-path /yw2_a.fa \
  --json-out /ruta/local/yw2_a.json

python3 tools/compare_arc0_mobile.py \
  --arc0 main=/ruta/local/yw2_a.json \
  --apk /ruta/al/referencia.apk \
  --ipa /ruta/al/referencia.ipa \
  --json-out /ruta/local/comparison.json
```

Los JSON completos contienen nombres de recursos propietarios y deben permanecer fuera de GitHub.
