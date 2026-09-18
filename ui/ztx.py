"""
ZTX — launcher local (pywebview)
Lance l'interface HTML/CSS/JS dans une fenêtre native WebView2 (Windows).
Aucune ouverture de navigateur, aucune connexion réseau.
"""

from __future__ import annotations

import os
import subprocess
import sys
from pathlib import Path

import webview  # pip install pywebview


# ----------------------------------------------------------------------
# API JS <-> Python
# Exposée côté JS via window.pywebview.api
# ----------------------------------------------------------------------
class ZTXApi:
    """Bridge entre l'UI (JavaScript) et la fenêtre native pywebview."""

    def __init__(self) -> None:
        self._window: webview.Window | None = None
        self._maximized: bool = False

    # -- infra ---------------------------------------------------------
    def bind_window(self, window: webview.Window) -> None:
        self._window = window

    def _require(self) -> webview.Window:
        if self._window is None:
            raise RuntimeError("Window not bound yet.")
        return self._window

    # -- contrôles fenêtre --------------------------------------------
    def minimize(self) -> None:
        self._require().minimize()

    def toggle_maximize(self) -> None:
        w = self._require()
        if self._maximized:
            w.restore()
        else:
            w.maximize()
        self._maximized = not self._maximized

    def close(self) -> None:
        self._require().destroy()

    # -- menu Fichier -------------------------------------------------
    def open_local_folder(self) -> dict:
        """
        Ouvre l'explorateur de fichiers dans le dossier contenant ztx.py.
        Cross-platform : Windows / macOS / Linux.
        Retourne un petit dict pour que le JS puisse afficher un feedback.
        """
        folder = str(Path(__file__).resolve().parent)
        try:
            if sys.platform.startswith("win"):
                # Sur Windows : explorer.exe accepte un chemin direct
                os.startfile(folder)  # type: ignore[attr-defined]
            elif sys.platform == "darwin":
                subprocess.Popen(["open", folder])
            else:
                subprocess.Popen(["xdg-open", folder])
            return {"ok": True, "path": folder}
        except Exception as e:
            return {"ok": False, "error": str(e), "path": folder}

    # -- menu Aide ----------------------------------------------------
    def open_mailto(self, address: str, subject: str = "") -> dict:
        """
        Ouvre le client mail par défaut sur une adresse donnée.
        On passe par le shell OS plutôt que window.location = 'mailto:...',
        car WebView2 ne sait pas naviguer vers mailto: par lui-même.
        """
        target = f"mailto:{address}"
        if subject:
            # Simple encodage — les espaces suffisent pour un objet basique
            target += f"?subject={subject.replace(' ', '%20')}"
        try:
            if sys.platform.startswith("win"):
                os.startfile(target)  # type: ignore[attr-defined]
            elif sys.platform == "darwin":
                subprocess.Popen(["open", target])
            else:
                subprocess.Popen(["xdg-open", target])
            return {"ok": True}
        except Exception as e:
            return {"ok": False, "error": str(e)}

    # -- debug --------------------------------------------------------
    def ping(self) -> str:
        """Sanity check appelable depuis la console DevTools."""
        return "pong"


# ----------------------------------------------------------------------
# Boot
# ----------------------------------------------------------------------
def _index_path() -> str:
    """Retourne le chemin absolu de index.html à côté de ce fichier."""
    here = Path(__file__).resolve().parent
    index = here / "index.html"
    if not index.exists():
        print(f"[ZTX] index.html introuvable à : {index}", file=sys.stderr)
        sys.exit(1)
    return str(index)


def main() -> None:
    api = ZTXApi()

    window = webview.create_window(
        title="ZTX",
        url=_index_path(),
        js_api=api,
        width=1280,
        height=800,
        min_size=(880, 560),
        frameless=True,       # on utilise nos propres boutons min/max/close
        easy_drag=False,      # on gère le drag via la classe pywebview-drag-region
        background_color="#0b0d10",
        resizable=True,
        confirm_close=False,
    )

    api.bind_window(window)

    # DevTools : activées si ZTX_DEBUG=1 dans l'environnement.
    #   PowerShell : $env:ZTX_DEBUG=1 ; python ztx.py
    #   CMD        : set ZTX_DEBUG=1 && python ztx.py
    debug = os.getenv("ZTX_DEBUG", "0") == "1"
    webview.start(debug=debug)


if __name__ == "__main__":
    main()
