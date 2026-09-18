/* ============================================================
   ZTX — theme-manager.js
   Gère l'attribut data-theme sur <html> ("dark" | "light")
   ============================================================ */

(function () {
  "use strict";

  const ATTR = "data-theme";
  const KEY = "theme";
  const DEFAULT = "dark";

  function apply(theme) {
    if (theme !== "dark" && theme !== "light") theme = DEFAULT;
    document.documentElement.setAttribute(ATTR, theme);
    window.dispatchEvent(
      new CustomEvent("ztx:theme-changed", { detail: { theme } })
    );
  }

  window.ZTXTheme = {
    set(theme) {
      apply(theme);
      if (window.ZTXStorage) window.ZTXStorage.set(KEY, theme);
    },
    get() {
      return document.documentElement.getAttribute(ATTR) || DEFAULT;
    },
    init() {
      const saved = window.ZTXStorage
        ? window.ZTXStorage.get(KEY, DEFAULT)
        : DEFAULT;
      apply(saved);
    }
  };
})();
