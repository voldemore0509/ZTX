/* ============================================================
   ZTX — connector.js
   Bouton "Connecteur" de la barre de titre.
   ------------------------------------------------------------
   Volontairement inerte pour l'instant : aucune fenêtre, aucun
   dropdown. Il écrit simplement une ligne dans le terminal pour
   que l'utilisateur sache que le clic a été reçu.

   Quand le module de connexion existera, il suffira de remplacer
   le corps de onClick() — le câblage UI est déjà en place.
   ============================================================ */

(function () {
  "use strict";

  function onClick() {
    if (window.ZTXConsole) {
      window.ZTXConsole.warn(window.ZTXi18n.tech("log.connector.notWired"));
    }
  }

  window.ZTXConnector = {
    init() {
      const btn = document.getElementById("btn-connector");
      if (!btn) return;
      btn.addEventListener("click", () => {
        // Referme un éventuel menu ouvert : le Connecteur n'en a pas.
        if (window.ZTXMenu) window.ZTXMenu.close();
        onClick();
      });
    }
  };
})();
