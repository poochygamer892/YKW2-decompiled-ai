# Android

Cliente nativo Android. El núcleo común vive en `../core` y se integrará
mediante el NDK/CMake cuando se añada la aplicación. El adaptador Android debe
implementar `ykw2::core::ResourceSource` para `assets/` o almacenamiento local,
sin copiar ROMs ni recursos al repositorio.

La API de plataforma deberá quedar limitada a ciclo de vida, entrada, audio,
renderizado, permisos y lectura de archivos. Los parsers y reglas permanecen
en el núcleo C++.

El primer menú se especifica en [`docs/IMPORT_FLOW.md`](../docs/IMPORT_FLOW.md)
y tendrá un `ACTION_OPEN_DOCUMENT` con confirmación explícita de uso autorizado.
La URI se leerá mediante un descriptor de archivo; no se copiará la ROM entera
como requisito del selector ni se enviará fuera del dispositivo.

`src/main/cpp/ykw2_import_jni.cpp` conecta el descriptor con el lector NCSD/
NCCH/RomFS del núcleo y escribe los recursos en `filesDir/ykw2-cache`. El
importador actual requiere una imagen descifrada; la integración del catálogo
de recursos con el runtime del juego aún está en desarrollo.

No incluyas APKs, AABs, dumps ni recursos propietarios en Git.
