# Evaluación de 3dsrecomp

Se revisó [`fearkov/3dsrecomp`](https://github.com/fearkov/3dsrecomp), commit
`c3f21f94ba50d688025febc8974ff348d471b587`, cuya licencia declarada por el
proyecto es MIT.

## Resultado con el dump autorizado

La orden `3dsrecomp analyze` reconoce YW2 como `yw2`, sin módulos CRO, y
encuentra:

- 33.909 funciones;
- 95,8 % del código ejecutable localizado;
- 2.405.117 instrucciones decodificadas;
- 570 switches, 1.008 llamadas SVC y 207 funciones sugeridas por hints.

Eso lo convierte en una herramienta útil para recuperar llamadas, límites de
funciones y comportamiento ARM del ejecutable 3DS. La prueba se ejecutó con
la copia local autorizada y no se escribió código generado en el repositorio.

## Qué aporta al port

`3dsrecomp build` y `port` generan código nativo para las funciones ARM. Con
Zakuro, esto no se limita a ser un oráculo: Zakuro aporta la máquina 3DS
(intérprete de respaldo, memoria, kernel, servicios, GPU, audio y archivos)
que ejecuta ese código. Zakuro v0.2.27 lista YW2: Psychic Specters EUR como
jugable con recompilación.

## Límite importante

La recompilación no produce por sí sola un APK/IPA: hace falta Zakuro y un
frontend móvil. La dirección es adaptar Zakuro como backend de ejecución y
conservar `core/` como herramientas de análisis de formatos, no tratar su
prototipo de escena como el motor del juego. La interfaz tomará como referencia
YW1 Smartphone, pero ejecutará YW2. Los binarios y fuentes generados desde la
ROM permanecen fuera del repositorio.

## Reproducción local

```bash
cargo build --release
target/release/3dsrecomp analyze /ruta/a/tu/copia.3ds
```

El análisis es seguro para el repositorio: lee la copia local y emite métricas.
Los comandos `build` y `port` generan código derivado que debe permanecer en
almacenamiento privado.

## Prueba de recompilación local

Se compiló `3dsrecomp` en el entorno privado y se repitió `analyze` sobre el
CCI descifrado local: confirmó 33.909 funciones y 2.405.117 instrucciones. Se
ejecutó también `build` con salida en `/workspace/.local/derived`, fuera del
repositorio. La generación terminó (451 archivos C, ~498 MiB); se interrumpió
una primera compilación optimizada al quedar menos de 500 MiB libres en el
disco de trabajo. Se movió el directorio generado al scratch temporal y se
completó la compilación de validación con optimización desactivada. El resultado
es `/tmp/yw2-recomp-native/00040000001B2900.so` (549 MiB), un ELF x86-64; no se
creó un APK con ese código.

Esta salida generada no debe añadirse a Git: deriva del ejecutable del juego y
el build de `3dsrecomp` la enlaza contra la ABI de Zakuro. La generación C y
la biblioteca anfitriona por sí solas no son un juego ejecutable.

Se probó además la compilación cruzada con NDK r27c y
`aarch64-linux-android24-clang`. El resultado
`/workspace/.local/derived/yw2-recomp-android-arm64.so` se verificó como ELF64
AArch64 para Android API 24 (523 MiB). No se añade al repositorio ni al APK
automáticamente; el host móvil acepta una ruta opcional a esa biblioteca, que
debe instalarse privadamente para activar la ruta recompilada.

## Prueba de arranque del runtime

Se ejecutó localmente Zakuro v0.2.27 (licencia MIT) con la copia descifrada
autorizada y la biblioteca recompilada x86-64. La prueba headless de 120
fotogramas terminó sin error; después se mantuvo 1.800 fotogramas:

```text
Completed after 1800 frames in 41.10s
196.2 MIPS, frame 1800 | 15 threads | 0 modules | 4878 draws | 3159 transfers | 6318 fills | gpu 25.6s | interpreted 0.44%
```

Esto confirma carga del título y ejecución sostenida con actividad gráfica y
un porcentaje pequeño de código interpretado, sin fallo en el runtime de
escritorio. `mobile-host/` ya expone carga, `run_frame`, HID, ambas pantallas
RGBA y audio PCM; el adaptador JNI Android conecta esas funciones y CMake
compila host y JNI para ARM64. El APK debug se generó y verificó con ambos
binarios nativos; no había un móvil conectado para probar el arranque. iOS
necesita aún su adaptador; la biblioteca de 523 MiB requiere un mecanismo
privado de instalación y pruebas en dispositivo.
