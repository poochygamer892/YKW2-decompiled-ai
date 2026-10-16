# Herramientas externas de ingeniería inversa

Evaluación para el dump 3DS autorizado de Yo-kai Watch 2. Ningún ejecutable,
función, ROM, clave o recurso del juego se subió a servicios externos.

## Resultado

| Herramienta | Uso posible | Resultado en este proyecto |
| --- | --- | --- |
| [decomp.me](https://decomp.me) | Igualar una función C con su ensamblador, con el compilador y ABI correctos. | No reemplaza el análisis de la ROM ni descubre automáticamente funciones. Las páginas/API consultadas no permitieron verificar desde aquí un toolchain exacto para el ejecutable de 3DS. Reservarlo para funciones pequeñas cuando tengamos función, ensamblador y toolchain identificados; no publicar fragmentos propietarios. |
| [decomp.wtf](https://decomp.wtf) | Flujo local y colaborativo para proyectos de decompilación. | Su matriz de plataformas deja Nintendo 3DS como “coming” para decompilación desde cero. No es una vía lista para decompilar YW2 ahora. |
| [ninfs](https://github.com/ihaveamac/ninfs) | Montar imágenes/particiones 3DS como sistema de archivos navegable. | Instalado localmente en `/workspace/.local/tools/ninfs`. Con el archivo de boot ROM aportado en este chat, montó el CCI y su RomFS en modo de solo lectura. No se guardó el boot ROM en el repositorio ni se añadió ninguna dependencia al APK/IPA. |
| [3dsrecomp](https://github.com/fearkov/3dsrecomp) | Analizar funciones ARM de 3DS y generar stubs/código para overrides. | Es la herramienta aplicable ahora: ya reconoció el ejecutable de YW2 y contó funciones/instrucciones. Ver [`3DSRECOMP.md`](3DSRECOMP.md). |

## Flujo recomendado

1. Seguir el análisis local con `3dsrecomp` para obtener límites de funciones,
   referencias y candidatos de comportamiento.
2. Recuperar prototipos y semántica con desensamblado y pruebas diferenciales
   locales; implementar comportamiento portable en `core/`.
3. Cuando una función concreta necesite coincidencia de compilación, comprobar
   primero que `decomp.me` soporta el compilador/ABI de esa función. Usar solo
   un fragmento mínimo que se tenga derecho a compartir; por defecto, mantener
   el código y las muestras en local.
4. Volver a intentar `ninfs` solo si se dispone de los requisitos de claves de
   3DS necesarios. Para el inventario de RomFS ya extraído, continuar usando
   las herramientas locales del repositorio en vez de bloquear el trabajo en
   un montaje FUSE.

## Nota sobre ninfs

Se instaló en el entorno de trabajo, no dentro de la aplicación. La primera
prueba con `--dec` no encontró un boot ROM. Después se proporcionó un archivo
de 65.536 bytes que ninfs aceptó mediante `--boot9`: el montaje CCI mostró
`content0.game/decrypted.cxi`, `exefs.bin` y `romfs.bin`; el montaje RomFS
anidado expuso los archivos y directorios esperados, incluidos los paquetes
`yw2_*.fa`, `mov/` y `snd/`. Ambos montajes se hicieron de solo lectura y se
desmontaron al terminar la inspección. No se copiaron sus contenidos al repo.
