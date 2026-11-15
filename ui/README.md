# ZTX — CKitastam

Interface de bureau Windows en **C++17 natif**, conçue pour rester légère : Win32 pour les fenêtres et les contrôles, GDI/GDI+ pour le dessin, sans Qt, navigateur embarqué ni framework d’interface supplémentaire.

Cette version prépare l’interface du logiciel. Le bouton **Nettoyer** est volontairement sans action : aucun moteur de nettoyage n’est connecté, aucun dossier n’est analysé et aucun fichier n’est supprimé. Le journal affiche uniquement les événements de l’interface et les éventuelles erreurs rencontrées.

## Compiler avec MSYS2

Prérequis : Windows 10/11 64 bits et MSYS2. Ouvrez le terminal **MSYS2 UCRT64**, puis installez les outils :

```sh
pacman -S --needed mingw-w64-ucrt-x86_64-gcc make
```

Depuis le dossier du projet :

```sh
cd /d/programmation/CKitastam/ZTX/ui
make -j
./build/ZTX.exe
```

La compilation produit `build/ZTX.exe`. Le logo, l’icône et le manifeste Windows sont intégrés à l’exécutable. **Seul ce fichier est nécessaire pour distribuer l’application** sur une machine Windows compatible ; MSYS2 et le dossier `img` ne sont pas requis à l’exécution.

### Depuis PowerShell

Une fois le compilateur UCRT64 installé, le script fourni permet également de compiler sans `make` :

```powershell
.\build.ps1
.\build\ZTX.exe
```

Si MSYS2 est installé ailleurs que dans `C:\msys64` :

```powershell
.\build.ps1 -Msys2Root 'D:\msys64'
```

Pour une compilation de débogage :

```powershell
.\build.ps1 -Configuration Debug
```

### Avec CMake, en option

Dans le terminal UCRT64 :

```sh
pacman -S --needed mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja
cmake -S . -B build-cmake -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-cmake
./build-cmake/ZTX.exe
```

Utilisez un dossier de compilation distinct pour CMake, comme ci-dessus, afin de conserver les sorties des différents outils séparées.

## Utilisation

- **Fenêtre verticale** avec boutons de réduction et de fermeture, et adaptation à la mise à l’échelle Windows.
- **Logo ZTX en haut à gauche** : animation discrète au survol, infobulle « Logo ZTX » après une courte attente et accès aux informations du logiciel au clic.
- **Fichier** : sélection d’un dossier et fermeture de l’application.
- **Paramètres** : apparence, langue, animations et Liquid Glass.
- **Aide** : présentation de ZTX et rappel des raccourcis.
- **Configuration** : valeur initiale de **100 Mo**, saisie numérique et unités `o`, `Ko`, `Mo`, `Go`, `To`, `Po`. Les libellés deviennent `B`, `KB`, `MB`, `GB`, `TB`, `PB` en anglais.
- **Répertoire** : saisie ou collage d’un chemin ; **Parcourir** ouvre le sélecteur de dossiers Windows.
- **Nettoyage** : bouton **Nettoyer** réservé au futur moteur et zone de sortie actuellement vide.
- **Journal d’erreurs** : événements horodatés, informations de fonctionnement de l’interface et erreurs éventuelles. Il s’agit d’une zone de lecture, pas d’un interpréteur de commandes.

Les chemins et la taille saisis ne déclenchent aucun traitement. Ils ne sont pas enregistrés entre deux lancements.

### Préférences

Les réglages s’appliquent immédiatement. Ils sont enregistrés à la fermeture de la fenêtre des paramètres, y compris avec la croix ou la touche `Échap`.

| Réglage | Choix et comportement | Valeur initiale |
| --- | --- | --- |
| Apparence | Thème sombre ou clair | Sombre |
| Langue | Français ou anglais | Français |
| Animations | Transitions légères au survol et retour visuel au clic | Activées |
| Liquid Glass | Transparence et espacement de l’interface | 50 % |

**Liquid Glass** parcourt les états suivants :

- **À gauche, 0 %** : opacité d’environ 70 %, avec des espacements plus généreux.
- **Au centre, 50 %** : équilibre entre transparence et compacité.
- **À droite, 100 %** : fenêtre opaque et espacements compacts.

L’effet utilise la transparence native de Windows sur **l’ensemble de la fenêtre**, texte et contrôles compris. Ce choix limite les dépendances et le coût du rendu ; il ne simule pas une réfraction ni un flou Acrylic. La fenêtre des paramètres reste opaque pour préserver sa lisibilité.

Les préférences sont conservées dans :

```text
%LOCALAPPDATA%\CKitastam\ZTX\settings.ini
```

L’enregistrement remplace le fichier de configuration après écriture dans un fichier temporaire. Les valeurs absentes ou invalides utilisent les valeurs par défaut ; le curseur est borné entre 0 et 100. Une erreur de sauvegarde apparaît dans le journal.

### Raccourcis

| Raccourci | Action |
| --- | --- |
| `Ctrl+O` | Choisir un dossier |
| `Ctrl+,` | Ouvrir les paramètres |
| `F1` | Afficher l’aide / les informations |
| `Alt+F4` | Fermer la fenêtre active |
| `Tab` / `Maj+Tab` | Parcourir les contrôles |
| `Espace` | Activer le bouton sélectionné |
| Flèches, `Début`, `Fin` | Ajuster le curseur lorsqu’il a le focus |
| `Échap` | Fermer les paramètres |

## Conception et ressources

- **Boucle événementielle** : l’application attend les messages Windows ; aucune boucle de rendu permanente.
- **Animations temporaires** : les minuteurs servent uniquement aux transitions et s’arrêtent une fois celles-ci terminées.
- **Ressources maîtrisées** : objets graphiques, images et interfaces COM libérés par leurs propriétaires ; tampons de dessin réutilisés pour limiter les allocations et le scintillement.
- **Journal borné** : capacité de 32 768 caractères. Le contenu ancien est effacé lorsque la prochaine entrée dépasserait cette limite.
- **Compilation Release** : optimisation de taille, optimisation à l’édition des liens et suppression des sections inutilisées. Les bibliothèques d’exécution GCC/C++ sont liées statiquement ; les bibliothèques système restent fournies par Windows.
- **Logo intégré et optimisé** : les ressources de petite taille sont dérivées du logo fourni. L’original `img/logo ZTX.png` est conservé intact.
- **Aucun privilège administrateur demandé** : l’application s’exécute avec les droits de l’utilisateur courant.

Le poids final et la mémoire consommée dépendent du compilateur, de la configuration et de l’environnement Windows ; aucune valeur arbitraire n’est annoncée ici.

## Organisation du projet

| Fichier | Rôle |
| --- | --- |
| `src/main.cpp` | Fenêtre principale, dessin, contrôles, sélection de dossier, traduction et paramètres |
| `src/preferences.h` | Contrat des préférences et valeurs par défaut |
| `src/preferences.cpp` | Lecture, validation et sauvegarde des préférences |
| `resources/ztx.rc` | Intégration des ressources dans l’exécutable |
| `resources/resource.h` | Identifiants des ressources |
| `resources/ztx.manifest` | Compatibilité Windows, comportement DPI et droits d’exécution |
| `resources/logo.png`, `resources/ztx.ico` | Logo d’interface et icône Windows optimisés |
| `img/logo ZTX.png` | Logo original fourni |
| `Makefile` | Compilation MSYS2 UCRT64 |
| `build.ps1` | Compilation depuis PowerShell |
| `CMakeLists.txt` | Configuration CMake facultative |
| `tests/preferences_test.cpp`, `tests/run.ps1` | Tests isolés de la persistance des préférences |

## Tests automatisés

Avec le compilateur MSYS2 UCRT64 installé, lancez depuis PowerShell :

```powershell
.\tests\run.ps1
```

Le script accepte également `-Msys2Root 'D:\msys64'` et produit `build/preferences_test.exe`. Il compile avec les avertissements traités comme des erreurs, puis vérifie les valeurs par défaut, toutes les combinaisons de préférences, les bornes du curseur, les données invalides, les chemins Unicode et les échecs de sauvegarde.

Les tests redirigent `LOCALAPPDATA` uniquement dans leur propre processus, vers un dossier temporaire créé pour eux. Ils ne lisent ni ne modifient les préférences réelles de l’utilisateur. Les fichiers de test connus sont nettoyés individuellement à la fin.

## Vérification manuelle

Après compilation, ce parcours permet de vérifier les principaux comportements :

1. Lancez ZTX et contrôlez la disposition verticale, la valeur **100 Mo**, le logo et les menus.
2. Saisissez un autre nombre, changez d’unité et collez un chemin contenant des espaces ou des caractères accentués.
3. Ouvrez **Parcourir**, choisissez un dossier, puis rouvrez le sélecteur et annulez. Vérifiez que l’annulation conserve le chemin précédent.
4. Cliquez sur **Nettoyer** : aucun traitement ne démarre et la sortie reste vide.
5. Testez les deux thèmes et les deux langues, puis désactivez et réactivez les animations. Survolez aussi le logo jusqu’à l’apparition de l’infobulle.
6. Déplacez Liquid Glass sur **0**, **50** et **100 %** ; vérifiez la transparence, les espacements et la lisibilité de la fenêtre des paramètres.
7. Fermez les paramètres, relancez ZTX et vérifiez la conservation des préférences.
8. Parcourez les contrôles au clavier et testez les raccourcis, la réduction et la fermeture. Vérifiez également le rendu avec la mise à l’échelle Windows utilisée sur votre écran.

Cette liste décrit les vérifications à effectuer ; elle ne constitue pas un relevé automatique de tests réalisés.
