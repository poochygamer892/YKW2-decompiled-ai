# Overlay privado YW1 Mobile → YW2

El nuevo objetivo es conservar el ejecutable, la presentación y la entrada
táctil de Yo-kai Watch 1 Mobile, sustituyendo su contenido por recursos de
Yo-kai Watch 2. El repositorio no incluye ninguno de los paquetes ni recursos;
el proceso trabaja con copias locales autorizadas.

La primera herramienta es
[`tools/build_yw1_yw2_overlay.py`](../tools/build_yw1_yw2_overlay.py). Opera
sobre el `yw2_a.fa` extraído del dump y un manifiesto generado por
`tools/inspect_arc0.py`. Mantiene el binario y la envoltura móvil de YW1,
reemplaza las rutas de datos que coinciden y añade las rutas nuevas de YW2.

Ejemplo de inspección sin escribir un paquete:

```sh
python3 tools/build_yw1_yw2_overlay.py \
  --yw1 /ruta/propia/yw1-mobile.apk \
  --yw2-arc0 /ruta/propia/yw2_a.fa \
  --yw2-manifest /ruta/propia/yw2_arc0.json \
  --output /tmp/yw2-on-yw1.apk \
  --dry-run
```

Para una primera prueba de arranque se pueden seleccionar solo las secuencias
y recursos de UI:

```sh
python3 tools/build_yw1_yw2_overlay.py \
  --yw1 /ruta/propia/yw1-mobile.apk \
  --yw2-arc0 /ruta/propia/yw2_a.fa \
  --yw2-manifest /ruta/propia/yw2_arc0.json \
  --output /tmp/yw2-on-yw1.apk \
  --include 'seq/**' \
  --include 'data/menu/**'
```

El resultado pierde la firma del APK/IPA y debe firmarlo el propietario. El
overlay no afirma compatibilidad automática: los `.xq`, bases de datos,
modelos, shaders y referencias internas pueden necesitar conversores. El
siguiente paso técnico es comparar los recursos compartidos, comprobar el
formato de los eventos y adaptar IDs de YW2 a las tablas que el ejecutable de
YW1 Mobile espera.

Para que la copia móvil no se instale como una actualización de YW1, el
generador cambia la identidad del paquete a `jp.co.level5.yws2` en Android y
en iOS. En Android también conserva un ZIP clásico compatible con el
instalador y aplica una firma nueva; en iOS el resultado queda sin firmar para
que el propietario lo firme con su certificado. El nombre visible del IPA es
`Yo-kai Watch 2 Smartphone`.
