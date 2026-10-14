# Referencia móvil de Yo-kai Watch 1

Fecha: 2026-10-14

Este documento registra únicamente metadatos e hipótesis de ingeniería
obtenidos de copias autorizadas del APK y del IPA de referencia. Los binarios,
recursos extraídos y bases de datos de Ghidra permanecen fuera del repositorio
en `/workspace/.local/reference/` y `/tmp/`.

## Artefactos analizados

| Artefacto | Tamaño | SHA-256 | Binario nativo |
|---|---:|---|---|
| APK de Android | 2.393.377.577 bytes | `f85948171d7cdb20c758aa995c4d937cd13d54da0e7e24e2a4e5016d5736d560` | `lib/arm64-v8a/libyw1_rom.so`, ELF AArch64 |
| IPA de iOS | 1.931.701.143 bytes | `97fce1fdd01ec0f2d329ff0f69397acbcc6e6c55e2bc6168f9487597754a4e10` | `Payload/yw1_rom.app/yw1_rom`, Mach-O arm64 |

El APK procede del enlace de Google Drive proporcionado por el usuario y el
IPA del enlace de Archive.org proporcionado por el usuario. El APK contiene
`libyw1_rom.so` (12.669.568 bytes sin comprimir); el IPA contiene `yw1_rom`
(6.760.400 bytes sin comprimir).

## Inventario de paquetes

| Métrica | APK | IPA |
|---|---:|---:|
| Entradas ZIP | 19.066 | 19.056 |
| Tamaño sin comprimir | 2.476.692.947 | 2.007.494.224 |
| Archivos de recursos del juego | 19.033 bajo `assets/` | 19.056 bajo el bundle, incluyendo metadatos de iOS |
| `.bin` | 5.942 | 5.942 |
| `.npcbin` | 5.712 | 5.712 |
| `.xc` | 3.295 | 3.295 |
| `.xq` | 1.361 | 1.361 |
| `.xi` | 1.300 | 1.300 |
| `.xv` | 495 | 495 |
| `.xa` | 390 | 390 |
| `.cfg` | 223 | 223 |
| `.mp4` | 38 | 38 |

Las diferencias de rutas son de empaquetado, no una razón para duplicar el
runtime: los recursos principales aparecen como `assets/data/...` en Android y
`data/data/...` en iOS. El audio CRI aparece como `assets/android/snd/cri/...`
frente a `data/ios/snd/cri/...`. El APK también conserva copias en
`assets/mov/`, mientras que la ruta compartida normalizada es `data/mov/`.

## Comparación de recursos entre plataformas

Para la comparación se eliminaron los prefijos de plataforma y se unificaron
las rutas equivalentes anteriores. No se publicaron los nombres completos ni
el contenido de los recursos.

| Resultado | Cantidad |
|---|---:|
| Claves normalizadas en IPA | 19.056 |
| Claves normalizadas en APK | 19.029 |
| Rutas presentes en ambos | 17.352 |
| Coincidencias exactas de tamaño y CRC ZIP | 15.693 |
| Rutas solo en IPA | 1.704 |
| Rutas solo en APK | 1.677 |

Las coincidencias exactas incluyen 5.688 `.npcbin`, 4.446 `.bin`, 3.295
`.xc`, 1.022 `.xi`, 495 `.xv`, 322 `.xa`, 222 `.cfg`, 45 `.xs`, 36 vídeos
`.mp4`, 29 `.xk`, 27 `.ralcam`, todos los 16 `.acb` y los 15 `.awb` que se
alinearon por ruta. Los recursos que no coinciden byte a byte se concentran en
menús, imágenes y datos localizados; por tanto, el APK/IPA es una buena
referencia de empaquetado y runtime, no una fuente para sustituir los datos de
Yo-kai Watch 2.

## Hallazgos del binario nativo

Se importaron copias temporales en Ghidra 12.1.4 con el script
[`tools/ghidra/DumpYwFunctions.java`](../tools/ghidra/DumpYwFunctions.java).
El script solo emite arquitectura, direcciones y nombres de funciones; no
copia código ni recursos al repositorio.

| Binario | Lenguaje Ghidra | Funciones detectadas | Seleccionadas por nombre |
|---|---|---:|---:|
| `libyw1_rom.so` | `AARCH64:LE:64:v8A` | 24.627 | 18.144 |
| `yw1_rom` | `AARCH64:LE:64:AppleSilicon` | 435 | 9 |

En el binario Android aparecen símbolos C++ que delimitan subsistemas que el
port de YKW2 deberá modelar detrás de interfaces propias: `CCharaResourceManager`,
`CCharaResourceUtil`, `MOTEV_RESOURCE_INFO`, `StageEffectState_Sound`, además
de ciclos `ywCEnv` (`Loop_Update`, `Loop_Draw`, `Reset_Game`) y adaptadores
`lxpAndroid...` para entrada, sensores, WebView y compras. El Mach-O está más
despojado para este inventario, pero conserva las dependencias de plataforma
Apple y el mismo patrón de recursos en el bundle.

El análisis completo de C++ despojado genera una cantidad alta de avisos de
excepciones/landing pads en Ghidra; por eso se conserva el inventario rápido y
reproducible. No se debe interpretar el número de funciones como cobertura de
decompilación.

## Decisiones para el port de YKW2

1. Mantener un `ResourceStore` común y resolver el backend de plataforma solo
   para rutas, audio y ciclo de vida.
2. Usar los lectores existentes de `XPCK`, `XQ32`, `IMGC` y `ARC0` como base
   portable; validar cada formato con el dump autorizado de YKW2.
3. Implementar primero un bucle de escena, mapa, personaje y audio con datos de
   YKW2; usar YW1 móvil solo para contrastar nombres de subsistemas, rutas y
   contratos de carga.
4. No enlazar ni redistribuir `libyw1_rom.so`, `yw1_rom`, código descompilado,
   imágenes, audio, vídeos o bases de datos de los paquetes de referencia.

La siguiente entrada de trabajo es conectar el inventario de `yw2_a.fa` con
los lectores de recursos del runtime y añadir una prueba de carga de mapa y
personaje usando únicamente el dump de YKW2 del usuario.
