# Metodología de análisis

## Fases

1. **Inventario**: identificar contenedores, archivos, arquitecturas, compresión, cifrado y tablas de contenido.
2. **Proveniencia**: registrar versión, región, hash y fuente autorizada sin almacenar el dump.
3. **Estructuras**: documentar cabeceras, offsets, tamaños, endianess, codificaciones y relaciones.
4. **Comportamiento**: describir entradas, salidas, estados y errores observables.
5. **Pruebas diferenciales**: comparar la reimplementación con casos controlados y datos sintéticos.
6. **Núcleo**: implementar formatos y lógica independiente de Android/iOS.
7. **Clientes nativos**: añadir integración de plataforma, controles, audio, vídeo y ciclo de vida.
8. **Validación**: repetir las pruebas en dispositivos y documentar divergencias.

## Registro de evidencia

Para cada descubrimiento usa una nota con esta forma:

- **ID**:
- **Artefacto**:
- **Hash / versión**:
- **Método**:
- **Observación**:
- **Interpretación**:
- **Confianza**: baja / media / alta
- **Prueba siguiente**:

## Límites

El análisis debe mantenerse dentro de los derechos y permisos disponibles para el material de entrada. Este repositorio no debe convertirse en un canal de distribución de contenido protegido.
