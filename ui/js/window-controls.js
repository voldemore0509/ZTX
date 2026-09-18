/* ============================================================
   ZTX — window-controls.js
   Minimize / Maximize / Close via pywebview.api
   -----------------------------------------------------------
   L'API Python (ZTXApi) est exposée par pywebview sur :
       window.pywebview.api.minimize()
       window.pywebview.api.toggle_maximize()
       window.pywebview.api.close()

   window.pywebview n'est pas garanti dispo au tout premier paint :
   il est injecté puis l'événement "pywebviewready" est émis.
   On tolère les deux cas.

   Fallback : si on ouvre index.html hors pywebview (ex : debug
   navigateur), on retombe sur la Fullscreen API + window.close().
   ============================================================ */

(function () {
  "use strict";

  function api() {
    return window.pywebview && window.pywebview.api ? window.pywebview.api : null;
  }

  async function callPy(method, fallback) {
    const a = api();
    if (a && typeof a[method] === "function") {
      try {
        await a[method]();
        return true;
      } catch (e) {
        console.warn("[ZTX] pywebview.api." + method + " a levé :", e);
      }
    }
    if (typeof fallback === "function") fallback();
    return false;
  }

  function fallbackFullscreen() {
    const el = document.documentElement;
    if (!document.fullscreenElement) {
      (el.requestFullscreen || el.webkitRequestFullscreen)?.call(el);
    } else {
      (document.exitFullscreen || document.webkitExitFullscreen)?.call(document);
    }
  }

  function onMinimize() {
    callPy("minimize", () => console.info("[ZTX] minimize (bridge absent)"));
  }

  function onMaximize() {
    callPy("toggle_maximize", fallbackFullscreen);
  }

  function onClose() {
    callPy("close", () => window.close());
  }

  window.ZTXWindow = {
    init() {
      document.getElementById("btn-minimize")?.addEventListener("click", onMinimize);
      document.getElementById("btn-maximize")?.addEventListener("click", onMaximize);
      document.getElementById("btn-close")?.addEventListener("click", onClose);
    },
    // Exposé pour d'autres modules (ex : raccourci clavier futur)
    minimize: onMinimize,
    toggleMaximize: onMaximize,
    close: onClose
  };
})();
