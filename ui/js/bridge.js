/* ============================================================
   ZTX — bridge.js
   Point d'accès unique à l'API Python (window.pywebview.api).
   ------------------------------------------------------------
   Objectif : plus aucun module de la zone de travail n'appelle
   window.pywebview directement. On centralise ici :
     - la détection du bridge (absent en mode navigateur)
     - la gestion des exceptions
     - un format de retour homogène { ok, ... }
   ============================================================ */

(function () {
  "use strict";

  function api() {
    return window.pywebview && window.pywebview.api ? window.pywebview.api : null;
  }

  window.ZTXBridge = {
    /** Le bridge natif est-il disponible ? */
    available() {
      return api() !== null;
    },

    /**
     * Attend l'injection de l'API par pywebview.
     * window.pywebview n'existe PAS au DOMContentLoaded : il est injecté
     * puis l'événement "pywebviewready" est émis. Tester le bridge trop tôt
     * le déclare absent à tort.
     * Résout true dès que l'API est là, false après expiration.
     */
    ready(timeout) {
      if (api()) return Promise.resolve(true);

      const limit = typeof timeout === "number" ? timeout : 4000;
      return new Promise((resolve) => {
        let settled = false;
        let poll = null;
        const finish = (value) => {
          if (settled) return;
          settled = true;
          if (poll !== null) clearInterval(poll);
          resolve(value);
        };

        window.addEventListener("pywebviewready", () => finish(Boolean(api())), { once: true });

        // Filet de sécurité : certains backends injectent l'API sans émettre
        // l'événement (ou l'émettent avant l'écoute).
        const started = Date.now();
        poll = setInterval(() => {
          if (api()) finish(true);
          else if (Date.now() - started > limit) finish(false);
        }, 120);
      });
    },

    /** La méthode existe-t-elle côté Python ? */
    has(method) {
      const a = api();
      return Boolean(a && typeof a[method] === "function");
    },

    /**
     * Appelle une méthode Python et retourne TOUJOURS un objet.
     * Codes d'erreur maison :
     *   bridge_absent   -> on tourne hors pywebview (navigateur)
     *   method_missing  -> l'API Python n'expose pas cette méthode
     *   call_failed     -> l'appel a levé côté Python
     */
    async call(method, ...args) {
      const a = api();
      if (!a) return { ok: false, error: "bridge_absent" };
      if (typeof a[method] !== "function") return { ok: false, error: "method_missing", method };

      try {
        const result = await a[method](...args);
        // Certaines méthodes ne renvoient rien (minimize, close…)
        return result === undefined || result === null ? { ok: true } : result;
      } catch (e) {
        console.warn("[ZTX] pywebview.api." + method + " a levé :", e);
        return { ok: false, error: "call_failed", detail: String(e) };
      }
    }
  };
})();
