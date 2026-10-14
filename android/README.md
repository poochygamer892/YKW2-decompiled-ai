# Android

Cliente nativo Android. El núcleo común vive en `../core` y se integrará
mediante el NDK/CMake cuando se añada la aplicación. El adaptador Android debe
implementar `ykw2::core::ResourceSource` para `assets/` o almacenamiento local,
sin copiar ROMs ni recursos al repositorio.

El proyecto Gradle está en este directorio (`settings.gradle.kts` y
`build.gradle.kts`) y usa `src/main/cpp/CMakeLists.txt` para compilar el núcleo
común y el puente JNI. Requiere Android Studio/SDK con API 35, NDK y CMake
3.22.1. El target nativo se puede validar directamente con CMake/NDK; el
ensamblado APK además necesita que Gradle tenga disponibles sus plugins Maven.

La API de plataforma deberá quedar limitada a ciclo de vida, entrada, audio,
renderizado, permisos y lectura de archivos. Los parsers y reglas permanecen
en el núcleo C++.

El primer menú se especifica en [`docs/IMPORT_FLOW.md`](../docs/IMPORT_FLOW.md)
y tendrá un `ACTION_OPEN_DOCUMENT` con confirmación explícita de uso autorizado.
La URI se leerá mediante un descriptor de archivo; no se copiará la ROM entera
como requisito del selector ni se enviará fuera del dispositivo.

`src/main/cpp/ykw2_import_jni.cpp` conecta el descriptor con el lector NCSD/
NCCH/RomFS del núcleo y escribe los recursos en `filesDir/ykw2-cache`. Tras
extraerlos genera `.catalog` y después `.import-complete`; una extracción
parcial no se considera arrancable. El importador actual requiere una imagen
descifrada. Al abrirse, el menú vuelve a validar ambos archivos antes de
mostrar que la caché está lista; todavía no contiene el runtime jugable.

`RuntimeMenuActivity.kt` hospeda la primera escena portable: recibe snapshots
del núcleo mediante JNI y envía navegación/confirmación al mismo estado C++.
Al arrancar construye un `ResourceStore` sobre el caché, valida el catálogo y
parsea el índice ARC0 de `yw2_a.fa`; Nueva partida permanece bloqueada hasta
que ese contenido sea legible.
Las acciones de nueva partida y opciones aún son puntos de extensión, no una
implementación de juego.

No incluyas APKs, AABs, dumps ni recursos propietarios en Git.
