# Arquitectura propuesta

La arquitectura usa un núcleo C++17 pequeño y una capa de overlay sobre el
host móvil de Yo-kai Watch 1. El ejecutable y la presentación móvil de YW1
son el destino privado; los recursos y la lógica de contenido que se intentan
adaptar son los de Yo-kai Watch 2 autorizados por el usuario.

## Capas

### Núcleo

Lógica de formatos, estado del juego, reglas, simulación y pruebas. No depende
de APIs de Android o iOS. La entrada de recursos se hace mediante
`core/include/ykw2/core/resource_source.hpp`; la resolución desde el catálogo,
mediante `core/include/ykw2/core/resource_store.hpp`; y la validación de
cabeceras, mediante `core/include/ykw2/core/formats.hpp`.

El flujo previsto para un recurso es:

```text
ResourceStore -> catálogo -> firma -> ResourceSource/archivo -> ByteView ->
parser de cabecera -> decoder Level-5 -> lector de recurso
```

Cada etapa debe validar límites antes de leer y devolver errores deterministas.
Los decodificadores no deben escribir en el repositorio ni asumir que el
recurso completo está en memoria.

La importación inicial está descrita en
[`docs/IMPORT_FLOW.md`](IMPORT_FLOW.md). El usuario selecciona su documento en
la plataforma, el núcleo valida el NCSD y el extractor futuro escribe el
catálogo en el almacenamiento privado de la app.

### Adaptadores

Cargadores de recursos, persistencia, entrada, audio, gráficos y reloj. Cada
adaptador debe tener una interfaz verificable y una implementación por
plataforma cuando sea necesario. El adaptador solo implementa almacenamiento,
ciclo de vida, dibujo de `RuntimeSnapshot` y APIs del sistema; no duplica las
reglas de formatos del núcleo.

### Cliente Android

Aplicación nativa con integración Kotlin/Android. El cliente debe consumir el núcleo mediante una frontera estable y mantener su código de ciclo de vida separado.

### Cliente iOS

Aplicación nativa con integración Swift/iOS. El cliente debe reutilizar las mismas especificaciones de comportamiento sin asumir APIs de Android.

### Overlay de contenido

`tools/build_yw1_yw2_overlay.py` conserva el paquete host de YW1 y escribe los
recursos de YW2 en sus rutas móviles equivalentes. El overlay es una prueba de
compatibilidad de datos, no un parche mágico del ejecutable: eventos XQ32,
tablas de IDs, modelos, shaders y reglas deben validarse antes de afirmar que
el contenido de YW2 funciona dentro del host.

## Decisiones pendientes

- estrategia de renderizado;
- formato intermedio de recursos;
- sistema de pruebas de reproducción;
- requisitos mínimos de Android e iOS;
- licencias y procedencia de cada dependencia.

`RuntimeScene` es la frontera lógica actual: las plataformas reciben un
snapshot y envían `RuntimeInput`; `advance()` mantiene el reloj de simulación y
`runtime_save` persiste sólo el estado de la escena. Android usa JNI y un
`Handler`; iOS usa Objective-C++ y `CADisplayLink`. La capa de presentación no
contiene la lógica de formatos. `xq32_reader` ya recupera metadatos y símbolos de los programas de evento
verificados, pero no ejecuta sus instrucciones: antes de habilitar historia
deben recuperarse y probarse sus opcodes. El renderizado de mapas, audio, combate,
eventos y la
traducción completa de recursos siguen siendo trabajo posterior; no se deben
confundir estos cimientos con un port jugable completo.
