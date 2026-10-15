# Core

El núcleo común está escrito en C++17 para poder compilarse como biblioteca
estática desde el NDK de Android y desde Xcode/iOS sin que el código de juego
dependa de APIs de plataforma.

Prioridades:

1. formatos documentados;
2. pruebas con datos sintéticos;
3. lógica determinista;
4. interfaces pequeñas para los adaptadores de plataforma.

## API actual

- `ykw2/core/byte_view.hpp`: lectura little-endian acotada y no propietaria.
- `ykw2/core/formats.hpp`: metadatos de Level-5 y parsers de cabeceras
  `ARC0`, `XPCK`, `XQ32` e `IMGC`.
- `ykw2/core/xq32_reader.hpp`: lector acotado de programas XQ32 que descomprime
  las tablas de funciones, saltos, instrucciones y argumentos y conserva sus
  filas como palabras crudas para la recuperación verificable del ABI.
- `ykw2/core/xq32_execution.hpp`: cursor seguro que selecciona una función por
  símbolo, respeta sus límites y expone el token de operación y su ventana de
  argumentos; también ofrece saltos intra-función con comprobación de límites
  para los handlers que recuperen la semántica de cada operación.
- `ykw2/core/xq32_dispatch.hpp`: dispatcher extensible de tokens; los tokens
  aún desconocidos avanzan de forma segura y los handlers recuperados pueden
  controlar el cursor.
- `ykw2/core/level5_decode.hpp`: decodificación acotada de Level-5 `none`,
  `LZ10`, `Huffman4/8` y `RLE`; `Zlib` devuelve un error explícito hasta tener
  un backend portable.
- `ykw2/core/arc0_reader.hpp` y `ykw2/core/xpck_reader.hpp`: lectores de
  índices que usan `ResourceStore`, descomprimen sus tablas y validan offsets
  de archivos anidados sin copiar el archivo completo; ARC0 también ofrece
  lectura por rango de un recurso anidado para el renderer.
- `ykw2/core/runtime_scene.hpp`: primera escena portable; expone estado de
  importación, menú inicial y comandos para que cada plataforma dibuje su UI.
- `ykw2/core/mobile_presentation.hpp`: perfil de presentación móvil
  independiente, inspirado en la UX de YW1 pero aplicado al contenido de YW2.
- `ykw2/core/initial_content.hpp`: localiza y valida el índice ARC0 del
  archivo inicial antes de habilitar Nueva partida y selecciona metadatos de
  mapa, personaje y fondo para la primera escena; durante el arranque valida
  los índices XPCK de mapa/personaje y la cabecera IMGC del fondo.
- `ykw2/core/import_contract.hpp`: validación acotada de NCSD y plan de
  importación que exige confirmación local de derechos.
- `ykw2/core/romfs_extract.hpp`: lector NCCH/RomFS para fuentes descifradas y
  escritura por streaming mediante `ResourceSink`.
- `ykw2/core/resource_source.hpp`: contrato de lectura aleatoria para que
  Android/iOS suministren bytes desde sus propios bundles o archivos.
- `ykw2/core/resource_store.hpp`: frontera del runtime para resolver rutas del
  catálogo y comprobar firmas antes de servir lecturas acotadas; también
  expone ventanas de un recurso ARC0 para reutilizar los lectores XPCK/IMGC.
- `tests/formats_tests.cpp`: fixtures sintéticos; no necesita ROM ni assets.

Los parsers de cabecera no interpretan payloads. El lector XQ32 sí descomprime
sus tablas con el decodificador Level-5 y entrega filas crudas, pero todavía no
ejecuta opcodes ni les asigna significados no demostrados. El extractor
RomFS ya tiene una prueba sintética de recorrido y escritura; no implementa
descifrado ni extrae ExeFS.

## Compilación local

Con un compilador C++17:

```bash
c++ -std=c++17 -Wall -Wextra -Wpedantic \
  -Icore/include \
  core/src/byte_view.cpp core/src/formats.cpp \
  core/tests/formats_tests.cpp \
  -o /tmp/ykw2_core_tests
/tmp/ykw2_core_tests
```

También hay un `CMakeLists.txt` preparado para `YKW2_BUILD_TESTS=ON` cuando
el entorno de Android/iOS aporte CMake. Los lectores ARC0/XPCK operan sobre
rangos del `ResourceStore`, así que no necesitan mapear en memoria el `.fa` o
el contenedor anidado completo.

El decodificador Level-5 no incorpora todavía una dependencia de Zlib: los
streams `none`, `LZ10`, `Huffman4/8` y `RLE` quedan disponibles en el núcleo,
que devuelve `level5_zlib_backend_unavailable` para el método 5. Los índices
observados de YKW2 usan los cuatro primeros métodos en sus tablas documentadas.
