# iOS

Cliente nativo iOS. El núcleo común vive en `../core` y se integra desde
Xcode como fuentes C++17 dentro de `YKW2Port.xcodeproj`. El adaptador iOS debe implementar
`ykw2::core::ResourceSource` sobre el bundle o almacenamiento de la app, sin
distribuir la ROM ni los recursos originales.

`CMakeLists.txt` enumera las mismas fuentes del núcleo para validación host y
para integraciones CMake. El proyecto `YKW2Port.xcodeproj` ya incluye
`AppDelegate.swift`, los dos controladores, `YKW2ImportBridge.mm`, el puente
Objective-C y todas las fuentes del núcleo. Configura `C++ Language Dialect =
C++17` y enlaza Foundation, UIKit y UniformTypeIdentifiers. Este entorno Linux
no puede compilar un target UIKit porque Xcode requiere macOS.

En macOS, abre `YKW2Port.xcodeproj`, selecciona un equipo de desarrollo y
ejecuta `Product > Archive`. Desde terminal, el equivalente es:

```sh
xcodebuild -project ios/YKW2Port.xcodeproj -scheme YKW2Port \
  -configuration Release -sdk iphoneos \
  -archivePath build/YKW2Port.xcarchive archive
```

El `.ipa` debe exportarse después desde el Organizer de Xcode o con
`xcodebuild -exportArchive`, usando un perfil de firma propio. No se incluye
ningún certificado, perfil ni IPA en el repositorio.

También se puede lanzar desde GitHub Actions con `Run workflow`. Para que el
artefacto sea instalable en un iPhone, añade estos secretos del repositorio:
`IOS_CERTIFICATE_BASE64` (certificado `.p12`), `IOS_CERTIFICATE_PASSWORD`,
`IOS_PROVISIONING_PROFILE_BASE64` y `IOS_TEAM_ID`. Sin ellos, el workflow
publica `YKW2Port-unsigned-validation-ipa`, útil para verificar la compilación
pero no instalable por iOS debido a la ausencia de firma.

El perfil debe corresponder exactamente al identificador `com.ykw2.port.ios`
y el certificado debe ser compatible con ese perfil. Cuando esos secretos están
presentes, Actions importa el `.p12`, instala el perfil, archiva y exporta el
`.ipa` firmado automáticamente.

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

`RuntimeViewController.swift` ya hospeda la primera escena portable: dibuja el
fondo IMGC corregido, mundo, diálogo y combate, y envía navegación,
confirmación y acciones de combate al puente Objective-C++. Los controles están
dentro de un `UIScrollView` para que sean táctiles también en pantallas
pequeñas; los eventos producen señales de audio del sistema. Al arrancar el
puente valida el catálogo y parsea el índice ARC0 de `yw2_a.fa`; Nueva partida
permanece bloqueada hasta que ese contenido sea legible.
La historia y el combate incluidos son una vertical slice de prueba, no la
reimplementación completa del juego.

No incluyas IPA, símbolos de distribución ni recursos propietarios en Git.
