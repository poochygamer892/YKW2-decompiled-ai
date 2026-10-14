# Arquitectura propuesta

La arquitectura usa un núcleo C++17 pequeño y adaptadores nativos separados.
La versión móvil de Yo-kai Watch 1 solo sirve como referencia de integración,
formatos y comportamiento observable; los recursos del port serán los de
Yo-kai Watch 2 autorizados por el usuario.

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
ciclo de vida y APIs del sistema; no duplica las reglas de formatos del núcleo.

### Cliente Android

Aplicación nativa con integración Kotlin/Android. El cliente debe consumir el núcleo mediante una frontera estable y mantener su código de ciclo de vida separado.

### Cliente iOS

Aplicación nativa con integración Swift/iOS. El cliente debe reutilizar las mismas especificaciones de comportamiento sin asumir APIs de Android.

## Decisiones pendientes

- estrategia de renderizado;
- formato intermedio de recursos;
- sistema de pruebas de reproducción;
- requisitos mínimos de Android e iOS;
- licencias y procedencia de cada dependencia.

La ABI pública entre C++ y Kotlin/Swift aún está pendiente. Por ahora los
adaptadores pueden consumir la biblioteca estática directamente durante la
fase de investigación. No se debe fijar la semántica de IMGC o los tipos
numéricos de XPCK basándose únicamente en nombres de archivos: deben apoyarse
en pruebas diferenciales y evidencia documentada.
