# YKW2-decompiled-ai

Workspace de investigación y reimplementación para estudiar **Yo-kai Watch 2** y construir, cuando sea legalmente posible, clientes nativos para Android e iOS tomando como referencia técnica la versión móvil de **Yo-kai Watch 1**.

> Estado: fases de importación, catálogo y runtime inicial implementadas. El repositorio no contiene ROMs, dumps, assets propietarios ni código extraído del juego.

## Objetivos

- Inventariar formatos, recursos y comportamiento observable del software autorizado.
- Registrar evidencia reproducible y separar observaciones de hipótesis.
- Reimplementar la lógica en código mantenible y testeable.
- Integrar un núcleo multiplataforma con clientes nativos de Android e iOS.
- Mantener fuera del repositorio cualquier ROM, dump, clave, recurso extraído o binario propietario.

## Estructura

- `analysis/`: notas, hashes, mapas de símbolos y resultados de investigación.
- `core/`: lógica independiente de la plataforma y formatos documentados.
- `android/`: cliente y herramientas específicas de Android.
- `ios/`: cliente y herramientas específicas de iOS.
- `tools/`: utilidades de inventario, conversión y validación.
- `docs/`: arquitectura, metodología y decisiones del proyecto.

## Flujo inicial

1. Trabaja con una copia del software que tengas derecho a analizar.
2. Guarda los dumps y recursos locales fuera del repositorio, por ejemplo en `.local/`.
3. Registra hashes, versión y procedencia sin subir el contenido protegido.
4. Documenta cada hallazgo con evidencia, confianza y una prueba reproducible.
5. Implementa primero formatos y comportamiento; deja la integración de plataforma para después.

## Estado actual

- El núcleo C++17 valida NCSD/NCCH, extrae la RomFS autorizada a un caché privado,
  carga `yw2_a.fa`, inspecciona ARC0/XPCK/IMGC y selecciona el primer mapa,
  personaje y fondo localizado.
- `RuntimeScene` ya tiene menú inicial, tres eventos de diálogo, entrada de
  mundo, movimiento, combate de prueba con tres acciones, señales de audio y
  guardado binario versionado con restauración validada contra el catálogo
  importado.
- Android e iOS tienen renderizador nativo de la escena, controles táctiles,
  ciclo de actualización y workflows reproducibles de compilación. Esto es una
  vertical slice funcional; todavía falta conectar el guion completo, los mapas
  jugables, las reglas reales de combate, el audio original y los eventos del
  juego completo.

Para Android se necesita JDK 21, Android SDK 35, CMake 3.22.1, NDK 27.2 y
Gradle 8.11.1; el workflow `android.yml` instala esos componentes en GitHub
Actions. El workflow de iOS usa macOS/Xcode y deja un IPA unsigned si no se
configuran secretos de firma.

## Uso responsable

Este repositorio está pensado para investigación, interoperabilidad y reimplementación autorizada. No subas ROMs, claves, assets comerciales, ejecutables extraídos ni material que no tengas derecho a redistribuir.
