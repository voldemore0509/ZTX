"""
ZTX — launcher local (pywebview)
Lance l'interface HTML/CSS/JS dans une fenêtre native WebView2 (Windows).
Aucune ouverture de navigateur, aucune connexion réseau.

Sommaire de l'API exposée à JavaScript (window.pywebview.api) :

    Fenêtre      minimize() / toggle_maximize() / close()
    Système      open_local_folder() / open_mailto()
    Dossier      pick_folder() / inspect_folder()
    Analyse      scan_large_files()
    Destructif   delete_files()          <- désactivé par DELETION_ENABLED
    Stub         optimize()
    Debug        ping()
"""

from __future__ import annotations

import os
import subprocess
import sys
import time
from pathlib import Path

import webview  # pip install pywebview


# ----------------------------------------------------------------------
# Garde-fous globaux
# ----------------------------------------------------------------------

#: Interrupteur maître de la suppression réelle.
#: Tant qu'il vaut False, delete_files() n'efface RIEN : il renvoie
#: simplement le plan de suppression à l'UI. Passer à True uniquement
#: quand le parcours complet aura été validé en conditions réelles.
DELETION_ENABLED: bool = False

#: Nombre maximum de fichiers renvoyés à l'UI après un scan.
#: Le compte réel (`matched`) reste exact, seule la liste est tronquée —
#: on évite d'envoyer 200 000 lignes au WebView.
SCAN_RESULT_LIMIT: int = 500

#: Plafond de sécurité sur le nombre de fichiers inspectés.
#: Protège contre un scan lancé par erreur sur C:\ tout entier.
SCAN_FILE_BUDGET: int = 400_000


# ----------------------------------------------------------------------
# Helpers internes (hors classe : testables isolément)
# ----------------------------------------------------------------------
def _normalize_extensions(extensions) -> set[str]:
    """
    Normalise une liste d'extensions venant du JS en set comparable.
    ('.UASSET', 'pak') -> {'.uasset', '.pak'}
    """
    normalized: set[str] = set()
    for raw in extensions or []:
        ext = str(raw).strip().lower()
        if not ext:
            continue
        if not ext.startswith("."):
            ext = "." + ext
        normalized.add(ext)
    return normalized


def _open_with_os(target: str) -> dict:
    """Délègue l'ouverture d'un chemin ou d'une URI au shell du système."""
    try:
        if sys.platform.startswith("win"):
            os.startfile(target)  # type: ignore[attr-defined]
        elif sys.platform == "darwin":
            subprocess.Popen(["open", target])
        else:
            subprocess.Popen(["xdg-open", target])
        return {"ok": True, "target": target}
    except Exception as e:
        return {"ok": False, "error": str(e), "target": target}


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
        """
        folder = str(Path(__file__).resolve().parent)
        res = _open_with_os(folder)
        res["path"] = folder
        return res

    # -- menu Aide ----------------------------------------------------
    def open_mailto(self, address: str, subject: str = "") -> dict:
        """
        Ouvre le client mail par défaut sur une adresse donnée.
        On passe par le shell OS plutôt que window.location = 'mailto:...',
        car WebView2 ne sait pas naviguer vers mailto: par lui-même.
        """
        target = f"mailto:{address}"
        if subject:
            target += f"?subject={subject.replace(' ', '%20')}"
        return _open_with_os(target)

    # ==================================================================
    # ZONE DE TRAVAIL — dossier cible
    # ==================================================================
    def pick_folder(self) -> dict:
        """
        Ouvre le sélecteur de DOSSIER natif (aucun fichier sélectionnable).
        Retourne {"ok": True, "path": ...} ou {"ok": False, "cancelled": True}.
        """
        try:
            result = self._require().create_file_dialog(webview.FOLDER_DIALOG)
        except Exception as e:
            return {"ok": False, "error": str(e)}

        # pywebview renvoie un tuple/liste de chemins, ou None si annulation.
        if not result:
            return {"ok": False, "cancelled": True}

        path = result[0] if isinstance(result, (list, tuple)) else str(result)
        return {"ok": True, "path": os.path.normpath(str(path))}

    def inspect_folder(self, path: str) -> dict:
        """
        Valide un chemin saisi à la main dans la barre d'adresse de l'UI.
        Ne lit aucun contenu en profondeur : juste de quoi afficher un état.
        """
        raw = (path or "").strip().strip('"')
        if not raw:
            return {"ok": False, "error": "empty", "path": ""}

        try:
            resolved = Path(raw).expanduser()
            normalized = os.path.normpath(str(resolved))

            if not resolved.exists():
                return {"ok": False, "error": "not_found", "path": normalized}
            if not resolved.is_dir():
                return {"ok": False, "error": "not_a_directory", "path": normalized}

            # Compte superficiel (profondeur 1) — instantané même sur un gros dossier.
            files = dirs = 0
            with os.scandir(resolved) as it:
                for entry in it:
                    try:
                        if entry.is_dir(follow_symlinks=False):
                            dirs += 1
                        else:
                            files += 1
                    except OSError:
                        continue

            return {
                "ok": True,
                "path": normalized,
                "name": resolved.name or normalized,
                "files": files,
                "dirs": dirs,
            }
        except Exception as e:
            return {"ok": False, "error": str(e), "path": raw}

    # ==================================================================
    # ZONE DE TRAVAIL — analyse
    # ==================================================================
    def scan_large_files(
        self,
        path: str,
        min_bytes: int = 100 * 1024 * 1024,
        extensions=None,
        limit: int = SCAN_RESULT_LIMIT,
    ) -> dict:
        """
        Parcourt récursivement `path` et retourne les fichiers dont la taille
        est >= min_bytes (filtrés par extension si `extensions` est fourni).

        Opération strictement EN LECTURE SEULE.

        Retour :
            ok, root, min_bytes, scanned, matched, total_bytes,
            files[{path, name, dir, size}], truncated, budget_reached,
            errors, elapsed_ms
        """
        started = time.perf_counter()

        target = Path((path or "").strip().strip('"')).expanduser()
        if not target.exists() or not target.is_dir():
            return {"ok": False, "error": "invalid_folder", "root": str(target)}

        try:
            threshold = max(0, int(min_bytes))
        except (TypeError, ValueError):
            return {"ok": False, "error": "invalid_min_bytes"}

        wanted = _normalize_extensions(extensions)

        matches: list[dict] = []
        scanned = 0
        errors = 0
        total_bytes = 0
        budget_reached = False

        for root, _sub, names in os.walk(target, onerror=lambda _e: None):
            for name in names:
                scanned += 1
                if scanned > SCAN_FILE_BUDGET:
                    budget_reached = True
                    break

                if wanted and os.path.splitext(name)[1].lower() not in wanted:
                    continue

                full = os.path.join(root, name)
                try:
                    size = os.stat(full, follow_symlinks=False).st_size
                except OSError:
                    errors += 1
                    continue

                if size >= threshold:
                    total_bytes += size
                    matches.append(
                        {"path": full, "name": name, "dir": root, "size": size}
                    )

            if budget_reached:
                break

        matches.sort(key=lambda item: item["size"], reverse=True)
        capped = max(1, int(limit or SCAN_RESULT_LIMIT))

        return {
            "ok": True,
            "root": os.path.normpath(str(target)),
            "min_bytes": threshold,
            "scanned": scanned,
            "matched": len(matches),
            "total_bytes": total_bytes,
            "files": matches[:capped],
            "truncated": len(matches) > capped,
            "budget_reached": budget_reached,
            "errors": errors,
            "elapsed_ms": int((time.perf_counter() - started) * 1000),
        }

    # ==================================================================
    # ZONE DE TRAVAIL — destructif (verrouillé)
    # ==================================================================
    def delete_files(self, paths) -> dict:
        """
        Supprime les fichiers listés — UNIQUEMENT si DELETION_ENABLED est True.

        Tant que le drapeau est à False, la méthode se comporte comme une
        simulation : elle valide les chemins, calcule l'espace qui SERAIT
        libéré, et ne touche à rien. L'UI affiche ce plan tel quel.
        """
        requested = [str(p) for p in (paths or []) if str(p).strip()]
        if not requested:
            return {"ok": False, "error": "empty_selection"}

        planned: list[str] = []
        skipped: list[dict] = []
        freed = 0

        for raw in requested:
            candidate = Path(raw)
            if not candidate.exists():
                skipped.append({"path": raw, "reason": "not_found"})
                continue
            if candidate.is_dir():
                skipped.append({"path": raw, "reason": "is_directory"})
                continue
            try:
                freed += candidate.stat().st_size
            except OSError:
                skipped.append({"path": raw, "reason": "unreadable"})
                continue
            planned.append(str(candidate))

        if not DELETION_ENABLED:
            return {
                "ok": True,
                "simulated": True,
                "deleted": 0,
                "planned": len(planned),
                "freed_bytes": freed,
                "skipped": skipped,
                "note": "DELETION_ENABLED=False — aucun fichier touché.",
            }

        # --- Chemin réellement destructif ------------------------------
        # Activé seulement si DELETION_ENABLED a été basculé à True.
        deleted = 0
        failures: list[dict] = []
        for item in planned:
            try:
                os.remove(item)
                deleted += 1
            except OSError as e:
                failures.append({"path": item, "reason": str(e)})

        return {
            "ok": True,
            "simulated": False,
            "deleted": deleted,
            "planned": len(planned),
            "freed_bytes": freed,
            "skipped": skipped + failures,
        }

    # ==================================================================
    # ZONE DE TRAVAIL — optimisation (stub)
    # ==================================================================
    def optimize(self, path: str, presets=None) -> dict:
        """
        Point d'entrée réservé au futur moteur d'optimisation (backend C).
        Ne fait rien d'autre que confirmer ce qu'il recevrait.
        """
        return {
            "ok": True,
            "implemented": False,
            "path": (path or "").strip(),
            "presets": presets or [],
        }

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
