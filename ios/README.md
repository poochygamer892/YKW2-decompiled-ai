# iOS

Cliente nativo iOS. El núcleo común vive en `../core` y se integrará desde
Xcode como biblioteca C++17. El adaptador iOS debe implementar
`ykw2::core::ResourceSource` sobre el bundle o almacenamiento de la app, sin
distribuir la ROM ni los recursos originales.

Swift/Objective-C++ se reservará para ciclo de vida, entrada, audio,
renderizado y lectura de archivos; los parsers y reglas permanecen en el
núcleo C++.

El primer menú se especifica en [`docs/IMPORT_FLOW.md`](../docs/IMPORT_FLOW.md)
y usará `UIDocumentPickerViewController` con acceso seguro al documento. La
URI se entregará al extractor nativo y la salida quedará en el contenedor
privado de la aplicación.

El lector RomFS ya está en el núcleo C++ y `YKW2ImportBridge.mm` conecta el
documento seleccionado con `ResourceSource` y un `ResourceSink` sobre el
contenedor privado. Tras extraerlos genera `.catalog` y después
`.import-complete`. La primera versión también requerirá una imagen
descifrada. Al abrirse, el menú vuelve a validar ambos archivos antes de
mostrar que la caché está lista; todavía no contiene el runtime jugable.

No incluyas IPA, símbolos de distribución ni recursos propietarios en Git.
