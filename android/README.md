# Android

Cliente nativo Android. El núcleo común vive en `../core` y se integrará
mediante el NDK/CMake cuando se añada la aplicación. El adaptador Android debe
implementar `ykw2::core::ResourceSource` para `assets/` o almacenamiento local,
sin copiar ROMs ni recursos al repositorio.

La API de plataforma deberá quedar limitada a ciclo de vida, entrada, audio,
renderizado, permisos y lectura de archivos. Los parsers y reglas permanecen
en el núcleo C++.

No incluyas APKs, AABs, dumps ni recursos propietarios en Git.
