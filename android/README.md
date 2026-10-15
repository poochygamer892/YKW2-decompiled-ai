# Android

Cliente nativo Android. El núcleo común se integra mediante NDK/CMake. El
adaptador lee el caché privado mediante `ResourceSource`, sin copiar ROMs ni
recursos al repositorio.

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
mostrar que la caché está lista; contiene el runtime inicial, pero todavía no
es el juego jugable completo.

`RuntimeMenuActivity.kt` hospeda el menú táctil nativo: recibe snapshots del
núcleo mediante JNI y selecciona directamente las opciones disponibles.
`RuntimeSceneView.kt` puede dibujar una imagen IMGC relacionada con el título.
Los controles del inspector XQ32 y del catálogo de combate son contextuales.
Al arrancar construye un `ResourceStore` sobre el caché, valida el catálogo y
parsea el índice ARC0 de `yw2_a.fa`; «Nueva partida» y «Continuar» permanecen
bloqueadas incluso si los recursos son legibles.
El núcleo ya identifica eventos, reglas, vista/IA de combate y catálogos de
sonido reales de YW2. El evento de apertura avanza mediante el cursor XQ32
acotado y la interfaz muestra su estado estructural sobre el fondo extraído;
los tokens sin semántica probada no se convierten en diálogo ficticio. El
runtime de combate selecciona comandos desde las filas reales de
`battle_command` y los botones táctiles muestran el ID elegido.
Todavía no simula el bytecode, el audio ni las reglas de daño/encuentro; la
interfaz no debe sustituirlos por enemigos o estadísticas inventadas.

No incluyas APKs, AABs, dumps ni recursos propietarios en Git.
