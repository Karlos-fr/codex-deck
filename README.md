<p align="center">
  <a href="README.md"><img src="doc/flag-fr.svg" alt="" width="18" height="12" /> Français</a>
  /
  <a href="doc/README.en.md"><img src="doc/flag-gb.svg" alt="" width="18" height="12" /> English</a>
</p>

# Codex Deck

## Description

Codex Deck est un client Windows natif, rapide et léger pour piloter plusieurs sessions, projets et agents Codex depuis une interface unique. Écrit en C++23 avec Win32, Direct2D, DirectWrite et DirectComposition, il organise les sessions dans un arbre compact et les ouvre dans un Workbench pensé pour suivre conversations, commandes, outils, approbations et modifications de fichiers sans dépendre d’une interface web lourde.

Le projet est actuellement en pré-alpha et part du socle éprouvé de [Codex Glass](https://github.com/Karlos-fr/codex-glass).

## Fonctions disponibles

- création, renommage et suppression de projets logiques locaux sans modifier les dossiers ou dépôts Git ;
- création rapide de sessions dans le projet courant, depuis un dossier ou via le formulaire global ;
- favoris locaux, consultation et restauration des sessions archivées ;
- détection périodique des sessions créées ou renommées depuis un autre client Codex ;
- thème système, clair ou sombre mémorisé localement.

## Cible

- Windows 11 x64
- C++23
- CMake, Ninja, MSVC Build Tools

## Build

```powershell
cmake --preset debug
cmake --build --preset debug
ctest --preset debug --output-on-failure

cmake --preset release
cmake --build --preset release
ctest --preset release --output-on-failure
```
