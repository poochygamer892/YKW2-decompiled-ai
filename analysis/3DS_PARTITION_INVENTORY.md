# Inventario técnico de la ROM 3DS

Fecha del análisis: 2026-10-14

Este documento contiene únicamente metadatos y estructura. No incluye la ROM, ejecutables, assets extraídos ni claves. Los archivos originales permanecen en el entorno local autorizado.

## Herramienta reproducible

- Python 3.12 en un entorno virtual aislado.
- `pyctr==0.7.6`.
- Lectura en modo solo lectura con `CCIReader`, `NCCHReader` y `RomFSReader`.
- La imagen está marcada como dump descifrado; no se necesitó un `boot9.bin` para inspeccionar los contenedores ya descifrados.

RomFS sigue el contenedor IVFC/Level 3 descrito en la [documentación de RomFS](https://docs.mikage.app/RomFS/). Para un extractor alternativo reproducible puede usarse [3dstool](https://github.com/dnasdw/3dstool).

## CCI/NCSD

| Campo | Valor |
|---|---:|
| Tamaño del archivo | 3.906.174.976 bytes |
| Tamaño lógico NCSD | 4.294.967.296 bytes |
| Media units | 8.388.608 |
| Media ID | `00040000001b2900` |
| Formato | NCSD/CCI |
| Producto de la aplicación | `CTR-P-BYSP` |

Particiones presentes:

| Índice | Offset | Tamaño | Producto | RomFS |
|---:|---:|---:|---|---:|
| 0 | `0x4000` | 3.849.998.336 | `CTR-P-BYSP` | `0x4bc000`, 3.845.033.984 bytes |
| 1 | `0xe57a9000` | 5.402.624 | `CTR-P-CTAP` | `0x1000`, 5.398.528 bytes |
| 6 | `0xe5cd0000` | 9.998.336 | `CTR-P-CTAP` | `0x1000`, 9.994.240 bytes |
| 7 | `0xe6659000` | 40.759.296 | `CTR-P-CTAP` | `0x1000`, 40.755.200 bytes |

La partición 0 contiene:

- Exheader de 1.024 bytes.
- ExeFS de 4.950.528 bytes.
- `.code`: 4.633.388 bytes.
- `banner`: 301.808 bytes.
- `icon`: 14.016 bytes.
- El código nativo detectado es ARM32, coherente con 3DS; no debe reutilizarse directamente como binario Android/iOS.

## RomFS de la aplicación

Total: 7.100 archivos, 3.814.363.417 bytes lógicos, en cinco directorios contenedores.

| Ruta | Archivos | Tamaño lógico |
|---|---:|---:|
| `/snd/product/stream` | 7.011 | 1.325.508.160 |
| `/yw2_a.fa` | 1 | 967.055.688 |
| `/mov` | 74 | 434.588.873 |
| `/snd/product` (BSCAR) | 7 | 147.319.048 |
| Archivos de idioma `yw2_lg_*.fa` | 7 | 939.891.648 |

Formatos identificados por cabecera:

| Extensión | Cantidad | Cabecera observada | Función probable |
|---|---:|---|---|
| `.fa` | 8 | `ARC0` | contenedores de recursos y localización |
| `.bcstm` | 7.011 | `CSTM` | streams de audio Nintendo |
| `.bcsar` | 7 | `CSAR` | bancos/índices de audio |
| `.moflex` | 74 | `L2AA` | vídeo comprimido |

La distribución confirmó que el primer objetivo técnico es el lector de índices `FA/ARC0`, implementado en [`tools/inspect_arc0.py`](../tools/inspect_arc0.py). El resumen de sus resultados está en [`analysis/ARC0_INVENTORY.md`](ARC0_INVENTORY.md); no se extrajo todo el RomFS.

## Relación con las versiones móviles

La comparación de inventarios entre el APK y el IPA de Yo-kai Watch 1 encontró:

- APK: 19.033 rutas bajo `assets/`.
- IPA: 19.027 rutas bajo `Payload/yw1_rom.app/data/`.
- 18.993 rutas compartidas tras normalizar el prefijo.
- Las diferencias principales son bibliotecas nativas, vídeo/audio específico de plataforma, metadatos de instalación y nombres Unicode.

Las dos versiones móviles contienen una biblioteca nativa común conceptualmente, con una capa de plataforma fina. El APK observado usa únicamente `arm64-v8a` y expone `libyw1_rom.so`; el IPA contiene un ejecutable Mach-O arm64. Ambos conservan símbolos y cadenas de subsistemas `yw` relacionados con combate, mapas, datos y entidades. Esto respalda la arquitectura inicial del port:

1. Implementar un runtime de datos común.
2. Crear adaptadores Android/iOS para ciclo de vida, entrada, audio, renderizado y guardado.
3. Usar la versión móvil como referencia de integración y formato, sin copiar sus binarios.
4. Mantener la compatibilidad de recursos mediante lectores propios y pruebas con hashes locales.

## Límites y próximos pasos

- No se han subido binarios ni assets propietarios a GitHub.
- Los nombres y tamaños anteriores son un inventario, no una redistribución de contenido.
- El índice `ARC0` ya está identificado y documentado; falta clasificar las cabeceras de sus formatos internos.
- Después conviene comparar hashes de archivos homólogos 3DS/móvil localmente y documentar solo resultados agregados.
