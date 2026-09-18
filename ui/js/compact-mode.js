/* ============================================================
   ZTX — compact-mode.js
   Bascule le mode "Compressé" :
   - attribut data-compact="on|off" sur <html>
   - le CSS (compact.css) coupe animations, force le glass en noir,
     verrouille les réglages non-applicables (thème, slider, premium)
   -----------------------------------------------------------
   Persisté dans localStorage. En cohérence avec les autres modules
   du projet (ZTXTheme, ZTXGlass, ZTXPremium).
   ============================================================ */

(function () {
  "use strict";

  const ATTR = "data-compact";
  const KEY = "compact";
  const DEFAULT = false;

  function apply(enabled) {
    const on = Boolean(enabled);
    document.documentElement.setAttribute(ATTR, on ? "on" : "off");
    window.dispatchEvent(
      new CustomEvent("ztx:compact-changed", { detail: { enabled: on } })
    );
  }

  window.ZTXCompact = {
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
