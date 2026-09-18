/* ============================================================
   ZTX — max-button.js
   Gère la visibilité du bouton "Agrandir" dans la titlebar.
   -----------------------------------------------------------
   Par défaut : caché (data-maxbtn="off" sur <html>).
   Un toggle dans les Paramètres bascule data-maxbtn="on"/"off",
   et le CSS (main.css) fait le reste : display:none si off.
   ============================================================ */

(function () {
  "use strict";

  const ATTR = "data-maxbtn";
  const KEY = "maxbtn";
  const DEFAULT = false; // par défaut : caché

  function apply(enabled) {
    const on = Boolean(enabled);
    document.documentElement.setAttribute(ATTR, on ? "on" : "off");
    window.dispatchEvent(
      new CustomEvent("ztx:maxbtn-changed", { detail: { enabled: on } })
    );
  }

  window.ZTXMaxButton = {
    set(enabled) {
      apply(enabled);
      if (window.ZTXStorage) window.ZTXStorage.set(KEY, Boolean(enabled));
    },
    get() {
      return document.documentElement.getAttribute(ATTR) === "on";
    },
    toggle() {
      this.set(!this.get());
    },
    init() {
      const saved = window.ZTXStorage
        ? window.ZTXStorage.get(KEY, DEFAULT)
        : DEFAULT;
      apply(saved);
    }
  };
})();
