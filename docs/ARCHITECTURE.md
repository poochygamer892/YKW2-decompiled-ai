# Arquitectura propuesta

La arquitectura usa un núcleo C++17 pequeño y clientes nativos Android/iOS.
La aplicación móvil de Yo-kai Watch 1 solo sirve como referencia privada de
presentación y controles; el ejecutable del port no depende de su motor. El
perfil `ykw2_mobile` expresa esa referencia en datos del snapshot y mantiene
el contenido YW2 separado de la capa visual.
Los recursos y la lógica de contenido proceden del dump autorizado de Yo-kai
Watch 2 y se reimplementan por capas verificables.

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

### Perfil de presentación móvil

`mobile_yw1_smartphone_profile()` no reutiliza código ni archivos del juego
original. Solo publica una configuración común para el aspecto móvil: escena
superior, panel de diálogo, orientación preferida y controles táctiles. El
cliente sigue siendo `ykw2_mobile`; el contenido, los eventos y los recursos
se resuelven desde el dump autorizado de YW2.

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

### Compatibilidad de datos (diagnóstico)

`tools/build_yw1_yw2_overlay.py` conserva el paquete host de YW1 y escribe los
recursos de YW2 en sus rutas móviles equivalentes. Se mantiene solo para
comparar formatos y comprobar hipótesis; no es un port ni la ruta de ejecución
del cliente nativo. Eventos XQ32, tablas de IDs, modelos, shaders y reglas se
reconstruyen en el núcleo propio.

## Decisiones pendientes

- estrategia de renderizado;
- formato intermedio de recursos;
- sistema de pruebas de reproducción;
- requisitos mínimos de Android e iOS;
- licencias y procedencia de cada dependencia.

`RuntimeScene` es la frontera lógica actual: las plataformas reciben un
snapshot y envían `RuntimeInput`; `advance()` mantiene el reloj de simulación y
`runtime_save` persiste sólo el estado de la escena. Android usa JNI y un
`Handler`; iOS usa Objective-C++ y `CADisplayLink`. `xq32_reader` descomprime y
expone las filas crudas de los programas reales; `xq32_execution` selecciona
una función por símbolo y mantiene el cursor dentro de sus límites y
argumentos. `RuntimeScene` lo usa como punto de traza, sin inventar
significados para los tokens. El siguiente bloque es recuperar esos
significados con evidencia del ejecutable y de los eventos observables. El
renderizado de mapas, audio, combate y la traducción completa de recursos
siguen siendo trabajo posterior; estos cimientos aún no son un port jugable
completo.
