# Arquitectura del port

El objetivo es ejecutar Yo-kai Watch 2 en móviles con una presentación y unos
controles inspirados en Yo-kai Watch 1 Smartphone. El juego que se ejecuta es
YW2; no se intercambia contenido dentro del APK/binario privado de YW1. La
base de ejecución elegida es el núcleo HLE de Zakuro junto con el código ARM
recompilado por 3dsrecomp. Zakuro declara YW2: Psychic Specters jugable en
escritorio y una prueba local ejecutó el dump durante 1.800 fotogramas. Se ha
añadido `mobile-host/`, una frontera C estable sobre `zakuro-core`, y Android
ya conecta carga, fotogramas, entrada táctil, pantallas RGBA y audio PCM por
JNI. El host/JNI compilan para Android ARM64 y el APK debug se generó y firmó
con la clave debug local. iOS aún requiere su adaptador/build propios.

El núcleo C++17 del repositorio conserva lectores, validadores y prototipos de
recursos para investigar formatos de Level-5. `RuntimeScene` no es el motor
3DS ni debe presentarse como un port jugable. Las pantallas de maqueta no
sustituyen la ejecución de Zakuro.

## Capas

### Runtime del juego

`3dsrecomp` recompila funciones ARM del ejecutable de YW2. Zakuro aporta la
máquina 3DS: CPU/interprete de respaldo, memoria, kernel/servicios HLE, carga
de títulos, GPU, audio y sistema de archivos. La biblioteca recompilada es
específica de arquitectura y permanece fuera de Git.

`mobile-host/` carga la ROM elegida, avanza hasta ocho fotogramas, acepta HID,
devuelve ambas pantallas RGBA y drena audio PCM. Android lo enlaza con JNI y
`ZakuroRuntimeActivity`; la biblioteca recompilada `3dsrecomp` es opcional y
requiere instalarse aparte. iOS aún necesita un enlace estático compatible
con sus restricciones. La UI no depende de detalles internos de Zakuro.

### Núcleo de análisis de recursos

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

`mobile_yw1_smartphone_profile()` solo describe la referencia de interfaz:
orientación y controles táctiles. El runtime de YW2 produce las imágenes,
eventos, historia, mapas y combate; no se convierten archivos de YW2 al
formato de datos de YW1.

El usuario selecciona su documento con el selector del sistema y la app
conserva el permiso SAF; Zakuro abre la imagen elegida directamente. La
extracción RomFS experimental queda fuera del camino de arranque, para que sus
limitaciones NCSD no impidan abrir un título compatible.

### Adaptadores

Cargadores de recursos, persistencia, entrada, audio, gráficos y reloj. Cada
adaptador debe tener una interfaz verificable y una implementación por
plataforma cuando sea necesario. El adaptador solo implementa almacenamiento,
ciclo de vida, dibujo de `RuntimeSnapshot` y APIs del sistema; no duplica las
reglas de formatos del núcleo.

### Cliente Android

Aplicación Kotlin/Android con JNI hacia el host móvil de Zakuro. El cliente
mantiene el selector SAF, ciclo de vida, presentación, audio y controles fuera
del emulador.

### Cliente iOS

Aplicación Swift/iOS con una frontera C/Objective-C++ equivalente y las mismas
especificaciones de comportamiento, sin asumir APIs de Android.

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

`RuntimeScene` es un prototipo de lectura de recursos, no la frontera de
ejecución del juego. Android ya usa JNI para el host Zakuro; iOS Objective-C++
sigue pendiente. `xq32_reader` descomprime y
expone las filas crudas de los programas reales; `xq32_execution` selecciona
una función por símbolo y mantiene el cursor dentro de sus límites y
argumentos, y ofrece saltos acotados para los handlers futuros. `RuntimeScene`
lo usa mediante `xq32_dispatch` para registrar si cada token fue manejado,
desconocido o inválido. Los tokens desconocidos avanzan de forma segura y se
exponen al cliente móvil para no ocultar qué parte falta. `cfg_reader` valida
ahora el prefijo de 16 bytes de cada CFG de combate y sonido seleccionado y
publica sus palabras crudas, sin afirmar semánticas no demostradas.
`initial_content` conserva además los registros identificables de
`battle_config`, `battle_command` y `map_sound`; el runtime usa los IDs reales
de `battle_command` para responder a Atacar/Habilidad sin inventar daño. El
trabajo futuro es ampliar la cobertura de eventos, historia y sistemas del
juego con evidencia del ejecutable. La pantalla inicial ya resuelve `TitleMenu.xq` a su
paquete real `title_u00_*.xa` y decodifica la imagen base `000.xi`; no usa la
ilustración de transferencia de datos como fondo. Zakuro aporta el renderizado
3D y audio del runtime. El APK se verificó como ARM64, con ambos `.so` y firma
debug válida, pero no había un dispositivo conectado para probar instalación
ni ejecución; por tanto, aún no hay confirmación de una partida jugable.
