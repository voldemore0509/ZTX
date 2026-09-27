/* ============================================================
   ZTX — target-folder.js
   Bandeau "Dossier cible" (rubrique 1).
   ------------------------------------------------------------
   - Saisie manuelle du chemin OU sélection via "Afficher"
     (dialogue natif pywebview, DOSSIERS uniquement).
   - Le bouton OK n'apparaît QUE si la saisie diffère du chemin
     déjà confirmé (ajout, correction, suppression) : c'est lui
     qui valide et met à jour la cible.
   - Le chemin confirmé est la source de vérité pour les autres
     modules : ZTXTarget.getPath().
   ============================================================ */

(function () {
  "use strict";

  const KEY = "targetPath";

  let input = null;
  let okBtn = null;
  let browseBtn = null;
  let clearBtn = null;
  let stateEl = null;

  /** Message d'interface (langue courante). */
  const ui = (key) => window.ZTXi18n.t(key);
  /** Message de terminal (toujours en anglais). */
  const tech = (key) => window.ZTXi18n.tech(key);

  /** Chemin validé (vide = aucune cible). */
  let confirmed = "";
  /** Dernier résumé affiché, réutilisé lors d'un changement de langue. */
  let lastSummary = null;

  /* ---------- Affichage d'état ---------- */
  function setState(state, text) {
    if (!stateEl) return;
    stateEl.setAttribute("data-state", state);
    stateEl.textContent = text;
    stateEl.removeAttribute("data-i18n"); // le texte devient dynamique
  }

  function renderSummary() {
    if (!lastSummary) {
      setState("idle", confirmed ? ui("ws.target.restored") : ui("ws.target.none"));
      return;
    }
    if (lastSummary.kind === "counts") {
      setState(
        "ok",
        ui("ws.target.summary")
          .replace("{files}", window.ZTXFormat.count(lastSummary.files))
          .replace("{dirs}", window.ZTXFormat.count(lastSummary.dirs))
      );
    } else if (lastSummary.kind === "unverified") {
      setState("pending", ui("ws.target.unverified"));
    } else if (lastSummary.kind === "error") {
      setState("error", ui(lastSummary.key));
    }
  }

  /* ---------- Boutons contextuels ---------- */
  function refreshButtons() {
    const value = input ? input.value.trim() : "";
    if (okBtn) okBtn.hidden = value === confirmed;
    if (clearBtn) clearBtn.hidden = value === "";
  }

  function markDirty() {
    refreshButtons();
    if (input && input.value.trim() !== confirmed) {
      lastSummary = null;
      setState("pending", ui("ws.target.dirty"));
    } else {
      renderSummary();
    }
  }

  /* ---------- Validation ---------- */
  async function confirmPath() {
    const value = input ? input.value.trim() : "";

    // Champ vidé = on détache la cible.
    if (!value) {
      confirmed = "";
      lastSummary = null;
      if (window.ZTXStorage) window.ZTXStorage.remove(KEY);
      refreshButtons();
      renderSummary();
      window.ZTXConsole.info(tech("log.target.cleared"));
      emit();
      return;
    }

    // Hors pywebview (debug navigateur) : on accepte sans vérifier.
    if (!window.ZTXBridge.available()) {
      confirmed = value;
      lastSummary = { kind: "unverified" };
      persist();
      refreshButtons();
      renderSummary();
      window.ZTXConsole.warn(tech("log.target.unverified").replace("{path}", value));
      emit();
      return;
    }

    setState("pending", ui("ws.target.checking"));
    const res = await window.ZTXBridge.call("inspect_folder", value);

    if (!res || res.ok !== true) {
      const key =
        res && res.error === "not_found"
          ? "ws.target.errNotFound"
          : res && res.error === "not_a_directory"
          ? "ws.target.errNotDir"
          : "ws.target.errGeneric";
      lastSummary = { kind: "error", key };
      renderSummary();
      window.ZTXConsole.err(tech("log.target.invalid").replace("{path}", value));
      refreshButtons();
      return;
    }

    confirmed = res.path;
    if (input) input.value = res.path;
    lastSummary = { kind: "counts", files: res.files || 0, dirs: res.dirs || 0 };
    persist();
    refreshButtons();
    renderSummary();
    window.ZTXConsole.ok(tech("log.target.set").replace("{path}", res.path));
    emit();
  }

  /* ---------- Sélecteur natif ---------- */
  async function browse() {
    if (!window.ZTXBridge.available()) {
      window.ZTXConsole.warn(tech("log.bridge.absent"));
      if (input) input.focus();
      return;
    }

    const res = await window.ZTXBridge.call("pick_folder");

    if (res && res.cancelled) {
      window.ZTXConsole.info(tech("log.target.cancelled"));
      return;
    }
    if (!res || res.ok !== true || !res.path) {
      window.ZTXConsole.err(tech("log.target.pickFailed"));
      return;
    }

    // On remplit la barre et on laisse l'utilisateur valider avec OK :
    // même parcours que la saisie manuelle, aucune surprise.
    if (input) {
      input.value = res.path;
      input.focus();
    }
    markDirty();
    window.ZTXConsole.info(tech("log.target.picked").replace("{path}", res.path));
  }

  /* ---------- Divers ---------- */
  function persist() {
    if (window.ZTXStorage) window.ZTXStorage.set(KEY, confirmed);
  }

  function emit() {
    window.dispatchEvent(
      new CustomEvent("ztx:target-changed", { detail: { path: confirmed } })
    );
  }

  window.ZTXTarget = {
    getPath() {
      return confirmed;
    },

    init() {
      input = document.getElementById("target-input");
      okBtn = document.getElementById("btn-target-ok");
      browseBtn = document.getElementById("btn-browse");
      clearBtn = document.getElementById("target-clear");
      stateEl = document.getElementById("target-state");

      // Restaure la dernière cible connue (non re-vérifiée au démarrage :
      // la vérification se fera au premier usage réel).
      confirmed = window.ZTXStorage ? window.ZTXStorage.get(KEY, "") : "";
      if (input && confirmed) input.value = confirmed;

      if (input) {
        input.addEventListener("input", markDirty);
        input.addEventListener("keydown", (e) => {
          if (e.key === "Enter") {
            e.preventDefault();
            confirmPath();
          } else if (e.key === "Escape") {
            e.preventDefault();
            input.value = confirmed;
            markDirty();
          }
        });
      }

      if (okBtn) okBtn.addEventListener("click", confirmPath);
      if (browseBtn) browseBtn.addEventListener("click", browse);
      if (clearBtn) {
        clearBtn.addEventListener("click", () => {
          if (!input) return;
          input.value = "";
          input.focus();
          markDirty();
        });
      }

      // Re-rend l'état courant quand la langue change
      window.addEventListener("ztx:language-changed", renderSummary);

      refreshButtons();
      renderSummary();
    }
  };
})();
