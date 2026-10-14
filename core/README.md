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
- `ykw2/core/resource_source.hpp`: contrato de lectura aleatoria para que
  Android/iOS suministren bytes desde sus propios bundles o archivos.
- `tests/formats_tests.cpp`: fixtures sintéticos; no necesita ROM ni assets.

Los parsers no descomprimen ni interpretan payloads. Devuelven offsets y
regiones validadas para que la siguiente capa implemente los decodificadores
Level-5 y los lectores de tablas sin acoplarse a Android o iOS.

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
el entorno de Android/iOS aporte CMake.
