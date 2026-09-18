/* ============================================================
   ZTX — file-menu.js
   Actions du menu "Fichier".
   -----------------------------------------------------------
   Pour l'instant : "Voir l'emplacement local" → appelle
   window.pywebview.api.open_local_folder() qui ouvre l'explorateur
   sur le dossier contenant ztx.py.
   ============================================================ */

(function () {
  "use strict";

  function api() {
    return window.pywebview && window.pywebview.api ? window.pywebview.api : null;
  }

  async function openLocalFolder() {
    const a = api();
    if (!a || typeof a.open_local_folder !== "function") {
      console.warn("[ZTX] open_local_folder indisponible (bridge absent)");
      return;
    }
    try {
      const res = await a.open_local_folder();
      if (res && res.ok === false) {
        console.warn("[ZTX] Ouverture explorateur échouée :", res.error);
      }
    } catch (e) {
      console.warn("[ZTX] open_local_folder a levé :", e);
    }
  }

  function bindActions() {
    const dropdown = document.getElementById("dropdown-file");
    if (!dropdown) return;

    dropdown.addEventListener("click", (e) => {
      const btn = e.target.closest("[data-action]");
      if (!btn) return;
      const action = btn.getAttribute("data-action");

      switch (action) {
        case "open-local-folder":
          openLocalFolder();
          if (window.ZTXMenu) window.ZTXMenu.close();
          break;
        default:
          console.info("[ZTX] Fichier — action inconnue :", action);
      }
    });
  }

  window.ZTXFileMenu = {
    init: bindActions,
    openLocalFolder
  };
})();
