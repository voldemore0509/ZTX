# ZTX — Interface v0.1

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
│   └── animations.css
├── js/
│   ├── app.js
│   ├── storage.js
│   ├── i18n.js
│   ├── theme-manager.js
│   ├── liquid-glass-manager.js
│   ├── premium.js
│   ├── window-controls.js  ← utilise window.pywebview.api
│   ├── menu.js
│   ├── settings.js
│   └── locales/{fr,en,es}.js
└── img/
    ├── ZTX_Logo_Default.png
    └── ZTX_Logo_Primium.png
```

## Lancer

**1. Installer la dépendance** (une seule fois) :
```
pip install -r requirements.txt
```

Sur Windows, `pywebview` embarque le runtime WebView2 automatiquement (déjà présent sur Windows 11 et la plupart des Windows 10 récents).

**2. Lancer** :
```
python ztx.py
```

Une fenêtre native s'ouvre — pas de navigateur, pas de barre d'adresse, pas de connexion réseau. Vos préférences (thème, langue, Liquid Glass, Premium) sont persistées via localStorage embarqué dans le WebView2.

**Debug** : le launcher active DevTools par défaut (F12 dans la fenêtre) pour inspecter le DOM et voir la console JS. Passez `debug=False` dans `ztx.py` pour la release.

## Comment ça marche

`ztx.py` crée une fenêtre pywebview sans décorations (`frameless=True`) et expose une classe `ZTXApi` à JavaScript. Les boutons Réduire / Agrandir / Fermer appellent :

```js
await window.pywebview.api.minimize()
await window.pywebview.api.toggle_maximize()
await window.pywebview.api.close()
```

La zone déplaçable de la titlebar utilise la classe CSS officielle `pywebview-drag-region`.

## Fallback navigateur (optionnel)

Si vous ouvrez `index.html` directement dans un navigateur (Live Server, double-clic…), l'UI reste fonctionnelle : les boutons min/max/close détectent l'absence de `window.pywebview` et retombent sur la Fullscreen API + `window.close()`. Utile pour itérer sur le design sans relancer Python.

## Intégration future avec le backend C

Deux voies possibles quand le backend C sera prêt :

| Option | Idée |
|---|---|
| **Python garde le shell** | `ztx.py` reste le launcher. Un module Python appelle votre binaire C via `ctypes` ou `subprocess` et route les résultats vers l'UI via `window.evaluate_js()`. |
| **Migration C pur** | Remplacer pywebview par [libwebview de Zserge](https://github.com/webview/webview) (header-only C). L'UI HTML reste identique, on réécrit juste le launcher en C. |

La deuxième option colle plus à la philosophie ZTX ; la première est plus rapide pour prototyper.
