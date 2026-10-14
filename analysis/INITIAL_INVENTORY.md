# Inventario inicial

Fecha de inventario: 2026-10-14

Los archivos de entrada se mantienen fuera de Git, en almacenamiento local ignorado. Este documento contiene únicamente metadatos, hashes y observaciones estáticas.

## Entradas

| Entrada | Tamaño | SHA-256 | Identificación |
|---|---:|---|---|
| `3DS1755 - Yo-Kai Watch 2 - Psychic Specters (Europe)(M7)-decrypted.3ds` | 3,906,174,976 bytes | `fdd230e69c7312ba691e36f875ea158d0931c8f2990727abd9f0b146163fcbce` | Imagen Nintendo 3DS Game Card, Card1, `CTR-P-BYSP` |
| `jp.co.level5.yws1_1.0.12_decrypted_patched.ipa` | 1,964,549,293 bytes | `e2a5e3f9f9319282e2b194946e6aef3f95d9e293022e38752aac67e555902284` | IPA ZIP, aplicación iOS ARM64 |
| `Yo-kai watch (English mod V2).apk` | 2,393,377,577 bytes | `f85948171d7cdb20c758aa995c4d937cd13d54da0e7e24e2a4e5016d5736d560` | APK ZIP, Android ARM64 |

## 3DS

- El encabezado `NCSD` aparece en el offset `0x100`.
- Se observan cabeceras `NCCH` y el identificador de producto `CTR-P-BYSP`.
- La imagen contiene referencias a componentes SDK/Nintendo y recursos de texto regionales.
- La siguiente fase debe inventariar particiones NCCH, exheader, banners y sistemas de archivos sin alterar la imagen original.

## Android

- Paquete observado: `jp.co.level5.yws4`.
- Versión observada: `1.0.05`.
- Solo contiene ABI `arm64-v8a`.
- Binario principal: `lib/arm64-v8a/libyw1_rom.so`.
- El binario nativo está compilado para Android 21 con NDK r22 y contiene nombres C++ legibles relacionados con batalla, mapas, datos y sistemas Yo-kai.
- El APK contiene 19.066 entradas y recursos bajo `assets/`, además de datos de depuración. Hay ficheros de guardado que pueden contener información personal; no deben copiarse ni publicarse.

## iOS

- Bundle ID observado: `jp.co.level5.yws1`.
- Versión mínima de iOS observada: `15.0`.
- Binario principal: `Payload/yw1_rom.app/yw1_rom`.
- El binario es Mach-O ARM64.
- El IPA contiene 19.057 entradas y recursos bajo `Payload/yw1_rom.app/data/`.
- Se observan integraciones OpenGLES, CRI Atom/ADX y servicios de Level-5 en las cadenas del binario. Estas cadenas son evidencia estática, no una conclusión sobre el comportamiento en ejecución.

## Correspondencia Android/iOS

Comparando rutas normalizadas de `assets/` en el APK con `Payload/yw1_rom.app/data/` en el IPA:

- APK: 19.033 rutas de assets.
- IPA: 19.027 rutas de datos.
- Rutas compartidas: 18.993.
- Las diferencias visibles incluyen audio/vídeo específico de plataforma, metadatos de instalación Android, normalización de nombres Unicode y datos de depuración.

Esto sugiere que la versión móvil usa un núcleo de juego común y una capa de plataforma relativamente fina. Debe verificarse con comparación de hashes por recurso y análisis de los dos binarios.

## Próximos pasos

1. Generar un manifiesto de archivos con tamaño, hash y ruta normalizada para Android e iOS.
2. Comparar recursos comunes por hash y separar diferencias de empaquetado de diferencias de contenido.
3. Extraer únicamente metadatos públicos de `Info.plist` y del manifiesto Android.
4. Analizar el binario 3DS y los binarios ARM64 de referencia con símbolos, cadenas y fronteras nativas.
5. Definir formatos intermedios y pruebas con datos sintéticos antes de implementar el núcleo.
