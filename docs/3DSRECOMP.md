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

`3dsrecomp build` y `port` pueden generar C estático para las funciones ARM y
permitir overrides manuales. Esto puede acelerar la recuperación del motor de
YW2 y servir como oráculo de comportamiento frente al intérprete de Zakuro.

## Límite importante

El resultado de `port` sigue dependiendo de Zakuro para kernel, servicios, GPU,
sonido, entrada y lectura de archivos. Por tanto, no genera directamente un
APK o IPA ni reemplaza el runtime nativo del proyecto. Para usarlo en móvil
habría que portar también Zakuro a Android/iOS y conectar su backend gráfico,
de audio y de archivos con nuestro `ResourceStore`.

La decisión actual es usar 3dsrecomp como pista de análisis y posible fuente de
casos de prueba/overrides, mientras el núcleo de `core/` mantiene la
reimplementación portable. No se incorporan al repositorio las bibliotecas,
fuentes C generadas ni binarios derivados del dump.

## Reproducción local

```bash
cargo build --release
target/release/3dsrecomp analyze /ruta/a/tu/copia.3ds
```

El análisis es seguro para el repositorio: lee la copia local y emite métricas.
Los comandos `build` y `port` generan código derivado que debe permanecer en
almacenamiento privado.
