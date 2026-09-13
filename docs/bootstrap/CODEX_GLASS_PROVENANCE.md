# Provenance du bootstrap Codex Deck

Codex Deck a ete bootstrape depuis `Karlos-fr/codex-glass` au commit :

`d66821aa28c8c5293604949dad48ad9e9e99b434`

Le snapshot initial comprend `src/`, `tests/`, `.gitignore` et `CMakeLists.txt`.
La logique specifique quotas/tokens n'est conservee que pour etablir une baseline compilable avant son retrait controle.

## Synchronisation du moteur Glass

Le moteur Glass a ete recupere depuis la copie locale
`D:\VibeCoding\codex-glass` au commit :

`60fec98`

La synchronisation comprend tous les fichiers de `src/glass/`, le shader
`src/shaders/GlassEffect.hlsl`, le pont `WidgetRenderGlassEffect`, les types de
rendu, les constantes et la construction des lentilles locales. Elle couvre
l'apparence Glass, la capture DXGI, la deformation CPU, le traitement GPU et
les effets cumulables Calm Water, Liquid et Rain. Ces sources restent
volontairement hors de la cible CMake tant que leur adaptation au Workbench
Codex Deck et le retrait de leurs dependances `Widget` ne sont pas realises.
