# Flujo de importación local

La aplicación no contiene una ROM ni recursos del juego. En el primer arranque
mostrará una pantalla de importación con esta secuencia:

1. El usuario confirma que tiene derecho a usar la copia seleccionada en ese
   dispositivo.
2. El selector nativo de Android/iOS abre un documento local; no se sube a un
   servidor ni se consulta un servicio de propiedad.
3. El núcleo lee los primeros `0x200` bytes y valida `NCSD`, el tamaño declarado
   y los límites de las ocho particiones.
4. El adaptador crea un caché privado de la aplicación y ejecuta la extracción
   de NCCH/RomFS con progreso, cancelación y reanudación.
5. El juego abre el catálogo generado desde el caché, nunca desde una ruta fija
   del sistema ni desde una ROM incluida en el APK/IPA.

## Menú inicial

La pantalla debe mostrar:

- `Yo-kai Watch 2`;
- `Importar una copia autorizada`;
- una casilla de confirmación de derechos;
- `Seleccionar ROM`;
- estado y progreso de importación;
- errores accionables: archivo no seleccionado, NCSD inválido, imagen truncada,
  partición fuera de límites, falta de espacio o cancelación.

La confirmación es una declaración local de uso autorizado, no una verificación
legal automática. La aplicación no debe aceptar una imagen cuyo tamaño o
particiones permitan lecturas fuera del archivo.

## Estado de implementación

`core/include/ykw2/core/import_contract.hpp` valida el encabezado NCSD y
produce un `ImportPlan` sin conservar la imagen. El extractor
`core/include/ykw2/core/romfs_extract.hpp` ya recorre una RomFS descifrada y
escribe mediante `ResourceSink`; el puente JNI de Android lo conecta al menú y
al caché privado. El puente equivalente de iOS y el catálogo/runtime que
consumirá esos archivos todavía están pendientes. Por tanto, esta fase ya
importa recursos en Android cuando se compile el módulo nativo, pero aún no es
un port jugable completo.

La versión de Yo-kai Watch 1 móvil sigue siendo solo referencia de integración;
no se copiarán sus recursos al caché de Yo-kai Watch 2.
