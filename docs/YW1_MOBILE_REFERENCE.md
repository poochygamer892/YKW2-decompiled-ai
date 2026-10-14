# Referencia de adaptación móvil

El vídeo proporcionado para fijar la referencia visual es:

- [Gameplay de Yo-kai Watch 1 para smartphone](https://www.youtube.com/watch?v=jIyUHM9TsVg)

Esta referencia define la adaptación de plataforma, no el contenido que debe
entrar en el port. El objetivo es que Android/iOS adopten una presentación y
un modelo de entrada móvil comparables, mientras que la historia, mapas,
personajes, eventos, combate, audio y recursos cargados pertenecen a Yo-kai
Watch 2.

## Criterio de implementación

- El núcleo ejecuta datos y reglas de Yo-kai Watch 2.
- Android e iOS proporcionan el ciclo de vida, renderizado, audio y entrada
  táctil adaptados a teléfono.
- El vídeo sirve para comparar navegación, composición de pantalla, respuesta
  táctil, transiciones y presentación del combate.
- Los círculos, nombres ficticios como `yokai.training` y textos inventados de
  la maqueta no son contenido final y deben desaparecer del runtime.
- No se copian vídeo, assets ni código de Yo-kai Watch 1 al repositorio.
