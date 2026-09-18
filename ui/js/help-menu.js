/* ============================================================
   ZTX — help-menu.js
   Actions du menu "Aide".
   -----------------------------------------------------------
   - Contacter le développeur : ouvre le client mail par défaut
     (via l'API Python — WebView2 ne sait pas naviguer vers mailto:).
   - FAQ         : no-op (à câbler plus tard).
   - Page d'aide : no-op (à câbler plus tard).
   ============================================================ */

(function () {
  "use strict";

  // TODO Monsieur Chanudet : remplacer par la vraie adresse contact
  const DEV_EMAIL = "contact@luminescence-ai.local";
  const DEV_MAIL_SUBJECT = "ZTX — Contact développeur";

  function api() {
    return window.pywebview && window.pywebview.api ? window.pywebview.api : null;
  }

  async function contactDev() {
    const a = api();
    if (a && typeof a.open_mailto === "function") {
      try {
        const res = await a.open_mailto(DEV_EMAIL, DEV_MAIL_SUBJECT);
        if (res && res.ok === false) {
          console.warn("[ZTX] open_mailto a échoué :", res.error);
        }
        return;
      } catch (e) {
        console.warn("[ZTX] open_mailto a levé :", e);
      }
    }
    // Fallback navigateur (hors pywebview) : la balise mailto marche.
    window.location.href =
      "mailto:" + DEV_EMAIL + "?subject=" + encodeURIComponent(DEV_MAIL_SUBJECT);
  }

  function faq() {
    // Placeholder : à câbler quand la FAQ existera.
    console.info("[ZTX] FAQ — non câblée");
  }

  function helpPage() {
    // Placeholder : à câbler quand la page d'aide existera.
    console.info("[ZTX] Page d'aide — non câblée");
  }

  function bindActions() {
    const dropdown = document.getElementById("dropdown-help");
    if (!dropdown) return;

    dropdown.addEventListener("click", (e) => {
      const btn = e.target.closest("[data-action]");
      if (!btn) return;
      const action = btn.getAttribute("data-action");

      switch (action) {
        case "contact-dev":
          contactDev();
          if (window.ZTXMenu) window.ZTXMenu.close();
          break;
        case "faq":
          faq();
          // Menu volontairement laissé ouvert : rien ne se passe côté UI,
          // le user comprend visuellement que ce n'est pas encore branché.
          break;
        case "help-page":
          helpPage();
          break;
        default:
          console.info("[ZTX] Aide — action inconnue :", action);
      }
    });
  }

  window.ZTXHelpMenu = {
    init: bindActions,
    contactDev,
    faq,
    helpPage
  };
})();
