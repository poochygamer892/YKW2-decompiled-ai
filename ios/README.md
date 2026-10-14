# iOS

Cliente nativo iOS. El núcleo común vive en `../core` y se integrará desde
Xcode como biblioteca C++17. El adaptador iOS debe implementar
`ykw2::core::ResourceSource` sobre el bundle o almacenamiento de la app, sin
distribuir la ROM ni los recursos originales.

Swift/Objective-C++ se reservará para ciclo de vida, entrada, audio,
renderizado y lectura de archivos; los parsers y reglas permanecen en el
núcleo C++.

No incluyas IPA, símbolos de distribución ni recursos propietarios en Git.
