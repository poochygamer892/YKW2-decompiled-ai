# YKW2-decompiled-ai

Workspace de investigación y reimplementación nativa autorizada para llevar el
contenido de **Yo-kai Watch 2** a Android e iOS con un núcleo propio. La versión
móvil de **Yo-kai Watch 1** se conserva únicamente como referencia de UX,
renderizado y controles; no se utiliza como ejecutable anfitrión del port.

> Estado: fases de importación, catálogo y runtime inicial implementadas. El repositorio no contiene ROMs, dumps, assets propietarios ni código extraído del juego.

## Objetivos

- Inventariar formatos, recursos y comportamiento observable del software autorizado.
- Registrar evidencia reproducible y separar observaciones de hipótesis.
- Reimplementar la lógica en código mantenible y testeable, empezando por los
  formatos y la máquina de eventos de YW2.
- Usar la aplicación móvil de YW1 como referencia visual y de interacción,
  reproduciendo ese perfil en un cliente YW2 independiente.
- Integrar un núcleo multiplataforma con clientes nativos de Android e iOS.
- Mantener fuera del repositorio cualquier ROM, dump, clave, recurso extraído o binario propietario.

## Estructura

- `analysis/`: notas, hashes, mapas de símbolos y resultados de investigación.
- `core/`: lógica independiente de la plataforma y formatos documentados.
- `android/`: cliente y herramientas específicas de Android.
- `ios/`: cliente y herramientas específicas de iOS.
- `tools/`: utilidades de inventario, conversión y validación.
- `docs/`: arquitectura, metodología y decisiones del proyecto.

La evaluación de `3dsrecomp` está documentada en
[`docs/3DSRECOMP.md`](docs/3DSRECOMP.md). Se usa como herramienta externa de
análisis del ejecutable 3DS, no como dependencia distribuida del APK/IPA.

## Flujo inicial

1. Trabaja con una copia del software que tengas derecho a analizar.
2. Guarda los dumps y recursos locales fuera del repositorio, por ejemplo en `.local/`.
3. Registra hashes, versión y procedencia sin subir el contenido protegido.
4. Documenta cada hallazgo con evidencia, confianza y una prueba reproducible.
5. Implementa primero formatos y comportamiento; deja la integración de plataforma para después.

## Estado actual

- El núcleo C++17 valida NCSD/NCCH, extrae la RomFS autorizada a un caché privado
  e inspecciona ARC0/XPCK/IMGC. Los mapas y personajes se indexan, pero aún no
  forman una escena jugable.
- El núcleo C++17 identifica un manifiesto de arranque real dentro de YW2:
  secuencias de título/menú, el evento inicial, configuración de combate y sus
  texturas de interfaz. También valida las cabeceras estructurales de las
  tablas de vista/IA de combate y de los catálogos de sonido de mapa/evento.
  Las rutas se resuelven exclusivamente desde el dump importado del usuario.
- Android e iOS tienen importación y un menú táctil nativo. «Nueva partida» y
  «Continuar» están bloqueados. Solo se puede inspeccionar estructuralmente el
  evento XQ32 inicial y consultar IDs de combate cuando existen esos datos.
  El núcleo decodifica las tablas XQ32 y expone sus filas para inspección.
  El snapshot común publica el perfil `ykw2_mobile` con referencia
  `yw1_smartphone`, de modo que ambas plataformas comparten la presentación
  móvil sin cargar el ejecutable de YW1. El lector XMPR/XPVB/XPVI ya extrae
  una malla acotada del primer mapa, aún sin renderizado 3D de juego.
  El runtime de combate ya selecciona comandos desde las filas reales de
  `battle_command` y los clientes táctiles los muestran como comandos
  seleccionados en el catálogo, sin iniciar combates. Todavía faltan la semántica probada de todos los opcodes,
  materiales y animaciones 3D, el decoder de audio propietario y las reglas
  completas de combate; esos límites siguen explícitos para no presentar una
  demo como el port jugable terminado.
- `tools/build_yw1_yw2_overlay.py` queda como herramienta de diagnóstico
  privada y no es la ruta de ejecución del port nativo.

Para Android se necesita JDK 21, Android SDK 35, CMake 3.22.1, NDK 27.2 y
Gradle 8.11.1; el workflow `android.yml` instala esos componentes en GitHub
Actions. El workflow de iOS usa macOS/Xcode y deja un IPA unsigned si no se
configuran secretos de firma.

## Uso responsable

Este repositorio está pensado para investigación, interoperabilidad y reimplementación autorizada. No subas ROMs, claves, assets comerciales, ejecutables extraídos ni material que no tengas derecho a redistribuir.
