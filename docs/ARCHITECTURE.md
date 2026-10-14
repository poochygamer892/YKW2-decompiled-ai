# Arquitectura propuesta

La arquitectura queda deliberadamente abierta hasta terminar el inventario inicial.

## Capas

### Núcleo

Lógica de formatos, estado del juego, reglas, simulación y pruebas. No debe depender de APIs de Android o iOS.

### Adaptadores

Cargadores de recursos, persistencia, entrada, audio, gráficos y reloj. Cada adaptador debe tener una interfaz verificable y una implementación por plataforma cuando sea necesario.

### Cliente Android

Aplicación nativa con integración Kotlin/Android. El cliente debe consumir el núcleo mediante una frontera estable y mantener su código de ciclo de vida separado.

### Cliente iOS

Aplicación nativa con integración Swift/iOS. El cliente debe reutilizar las mismas especificaciones de comportamiento sin asumir APIs de Android.

## Decisiones pendientes

- lenguaje y ABI del núcleo;
- estrategia de renderizado;
- formato intermedio de recursos;
- sistema de pruebas de reproducción;
- requisitos mínimos de Android e iOS;
- licencias y procedencia de cada dependencia.

No se debe fijar ninguna de estas decisiones basándose únicamente en nombres de archivos: deben apoyarse en evidencia documentada.
