# ZTX — Interface v0.3

Interface locale du logiciel **ZTX** (Luminescence AI).
Fenêtre native WebView2 lancée par **pywebview** — aucune ouverture de navigateur.

## Structure

```
ui/
├── ztx.py                  ← launcher Python (à lancer)
├── requirements.txt        ← 1 dépendance : pywebview
├── index.html
├── css/
│   ├── main.css
│   ├── themes.css
│   ├── liquid-glass.css
│   ├── animations.css
│   ├── compact.css
│   └── workspace.css       ← zone de travail (chargé en dernier)
├── js/
│   ├── storage.js          ← persistance localStorage
│   ├── i18n.js             ← data-i18n / -title / -placeholder
│   ├── bridge.js           ← accès unique à window.pywebview.api
│   ├── format.js           ← tailles, heure, extensions
│   ├── ui-scale.js         ← "Taille du logiciel" (zoom global)
│   ├── theme-manager.js · liquid-glass-manager.js · premium.js
│   ├── compact-mode.js · max-button.js · window-controls.js
│   ├── menu.js · file-menu.js · help-menu.js · settings.js
│   ├── console.js          ← terminal IHM
│   ├── connector.js        ← bouton Connecteur (inerte)
│   ├── target-folder.js    ← rubrique 1 : dossier cible
│   ├── file-types.js       ← rubrique 2 : types de fichiers
│   ├── quick-delete.js     ← rubrique 3 : suppression rapide + modale
│   ├── app.js              ← point d'entrée
│   └── locales/{fr,en,es,zh,ja,vi,lo,hi}.js
└── img/
    ├── ZTX_Logo_Default.png
    └── ZTX_Logo_Primium.png
```

## Lancer

**1. Installer la dépendance** (une seule fois) :
```
pip install -r requirements.txt
```

**2. Lancer** :
```
python ztx.py
```

**Debug** : DevTools activées si la variable `ZTX_DEBUG=1` est présente.
PowerShell : `$env:ZTX_DEBUG=1 ; python ztx.py`

---

## Zone de travail

Quatre rubriques empilées dans `<main class="app-main">` :

| # | Rubrique | Contenu | Module |
|---|---|---|---|
| 1 | **Dossier cible** | Champ de saisie + bouton *Afficher* (sélecteur natif, dossiers uniquement). Le bouton **OK** n'apparaît que si la saisie diffère du chemin déjà validé : c'est lui qui confirme et met à jour la cible. | `target-folder.js` |
| 2 | **Types de fichiers** | Liste de presets `{ nom, extensions }` — livré avec `UE (.uasset .umap .pak .ubulk)`. Ajout via éditeur inline, suppression du type sélectionné, bouton *Optimiser*. | `file-types.js` |
| 3 | **Suppression rapide** | Seuil (valeur + unité Ko/Mo/Go/To, 100 Mo par défaut), interrupteur *Confirmer avant suppression*, bouton de lancement (**sans action**, cf. ci-dessous). | `quick-delete.js` |
| 4 | **Terminal** | Journal IHM repliable (info / ok / warn / err), horodaté, plafonné à 400 lignes. Ouvert par défaut, état persisté. | `console.js` |

Les rubriques 2 et 3 forment un duo collé (`.panel-duo`) : bordure partagée, rayons
plats sur la jointure. La zone de travail n'a **aucune largeur maximale** : en
fenêtre maximisée elle occupe tout l'écran.

Barre de titre : plus de titre centré (la zone reste déplaçable) ; le nom du
logiciel apparaît en infobulle au survol du logo.

### Suppression rapide — deux verrous

**1. `ENGINE_WIRED = false`** (en tête de `js/quick-delete.js`)
Le bouton *Lancer la suppression rapide* ne déclenche **aucune action** : ni scan,
ni modale, ni appel Python. Il écrit une seule ligne dans le terminal. Le moteur
sera écrit à la main ; le parcours ci-dessous est prêt et se rebranche en passant
ce drapeau à `true`.

```
Lancer  →  scan_large_files()          (Python, LECTURE SEULE)
        →  interrupteur ON  : modale listant les fichiers trouvés
           interrupteur OFF : enchaîne directement
        →  delete_files()               (Python, VERROUILLÉ)
```

Pour dérouler ce parcours sans toucher au drapeau, depuis DevTools
(`ZTX_DEBUG=1`, F12) : `ZTXQuickDelete.run()`.

**2. `DELETION_ENABLED = False`** (en tête de `ztx.py`)
Second verrou, côté Python : `delete_files()` ne supprime rien, il valide les
chemins, calcule l'espace qui *serait* libéré et renvoie le plan à l'UI.

### Aucune boîte de dialogue JS

`alert()`, `confirm()` et `prompt()` figent WebView2 : l'extension ne reçoit plus
aucun événement. L'ajout d'un type passe donc par un éditeur inline et la
confirmation de suppression par une modale maison (`#delete-modal`).

---

## API Python exposée à JavaScript

`ZTXApi` est accessible côté JS via `window.pywebview.api`, toujours à travers
`ZTXBridge.call(...)` qui normalise les retours en `{ ok, ... }`.

| Méthode | Rôle |
|---|---|
| `minimize()` · `toggle_maximize()` · `close()` | Contrôles de fenêtre |
| `open_local_folder()` | Ouvre l'explorateur sur le dossier de `ztx.py` |
| `open_mailto(address, subject)` | Client mail par défaut |
| `pick_folder()` | Sélecteur natif, **dossiers uniquement** |
| `inspect_folder(path)` | Valide un chemin saisi à la main (+ compte superficiel) |
| `scan_large_files(path, min_bytes, extensions, limit)` | Parcours récursif, lecture seule |
| `delete_files(paths)` | Suppression — verrouillée par `DELETION_ENABLED` |
| `optimize(path, presets)` | Stub réservé au futur moteur |
| `ping()` | Sanity check DevTools |

Garde-fous du scan : `SCAN_RESULT_LIMIT` (500 fichiers renvoyés à l'UI, le compte
réel reste exact) et `SCAN_FILE_BUDGET` (400 000 fichiers inspectés maximum —
protège d'un scan lancé par erreur sur `C:\` entier).

---

## Taille du logiciel (zoom global)

Réglage `− 100 % +` dans **Paramètres**, de 70 % à 160 % par pas de 10, persisté.
Raccourcis : `Ctrl +`, `Ctrl −`, `Ctrl 0`. Un clic sur la valeur remet à 100 %.

Implémentation : propriété CSS `zoom` posée sur `<html>` (`js/ui-scale.js`).

> **Règle à respecter dans tout nouveau CSS : aucune unité `vh`/`vw` dans le
> layout.** Sous `zoom`, Chromium recalcule les *pourcentages* par rapport au
> viewport zoomé, mais **pas** les unités de viewport : un `height: 100vh`
> déborderait de la fenêtre dès 110 %. Le layout est donc en flex + `%`.

Corollaire côté JS : `getBoundingClientRect()` renvoie des pixels *visuels*
(multipliés par le zoom) alors que `style.left` attend des pixels de *layout*.
Les deux endroits concernés (`menu.js`, ripple d'`app.js`) divisent par
`ZTXScale.get()`.

## Internationalisation

8 langues (`fr, en, es, zh, ja, vi, lo, hi`). Trois attributs sont traduits
automatiquement : `data-i18n` (texte), `data-i18n-title` (tooltip) et
`data-i18n-placeholder` (champ de saisie). Les textes dynamiques utilisent des
jetons `{path}`, `{count}`, `{size}`… remplacés côté module.

**Le terminal parle anglais**, quelle que soit la langue de l'interface :
convention du métier pour un journal d'exécution. Les clés `log.*` ne vivent donc
que dans `locales/en.js` et passent par `ZTXi18n.tech(key)` — jamais `t(key)`.
Les tailles y suivent la même règle (`ZTXFormat.techBytes/techCount` → `MB`,
`1,234`), alors que l'interface utilise `o/Ko/Mo/Go/To` en français et
`B/KB/MB/GB/TB` ailleurs. La clé interne (`data-unit`) ne change jamais.

## Détection du bridge

`window.pywebview` **n'existe pas** au `DOMContentLoaded` : pywebview l'injecte
puis émet `pywebviewready`. Tester le bridge au démarrage le déclarait absent à
tort (le terminal affichait « Bridge Python absent » en pleine fenêtre native).
`ZTXBridge.ready()` attend l'événement — avec un filet par scrutation — avant de
conclure.

## Fallback navigateur

Ouvert hors pywebview (Live Server, double-clic), l'UI reste utilisable pour
itérer sur le design : après expiration, le terminal l'annonce en WARN et les
actions natives (sélecteur de dossier, scan) sont neutralisées proprement au lieu
de planter.

## Intégration future avec le backend C

| Option | Idée |
|---|---|
| **Python garde le shell** | `ztx.py` reste le launcher. Un module Python appelle le binaire C via `ctypes` ou `subprocess` et route les résultats vers l'UI. |
| **Migration C pur** | Remplacer pywebview par [libwebview](https://github.com/webview/webview) (header-only C). L'UI HTML reste identique, on réécrit le launcher en C. |

Les points d'entrée à brancher côté C sont déjà isolés : `scan_large_files`,
`delete_files` et `optimize`.
