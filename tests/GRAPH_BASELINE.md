# Etat de reference du graphe

## Visibilite

- Minimal : graphe masque.
- Compact : graphe masque.
- Complet avec `show_graph = false` : graphe masque.
- Complet avec `show_graph = true` : graphe et footer visibles.

## Geometrie actuelle

- Hauteur de fenetre Complete avec graphe : 286 px logiques avant DPI.
- Hauteur minimale du graphe : 44 DIPs.
- Marge superieure du graphe : 14 DIPs.
- Pied de graphe : texte de consommation recente sous le rectangle principal.
- Nombre maximal de samples de quota charges : 512.

## Interactions actuelles

- Le graphe ne possede aucun hit-test propre.
- La zone cliente sert au deplacement de la fenetre lorsque la position n'est
  pas verrouillee.
- En click-through, la zone du graphe laisse passer la souris.
- Aucun survol, onglet ou tooltip n'est actuellement gere.

Ce document sert de reference pour verifier que l'extraction du graphe de quotas
ne change pas son rendu avant l'ajout du graphe de tokens.
