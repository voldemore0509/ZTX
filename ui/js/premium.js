/* ============================================================
   ZTX — premium.js
   Bascule le mode Premium Edition :
   - attribut data-premium sur <html>
   - swap du logo (Default <-> Primium)
   ============================================================ */

(function () {
  "use strict";

  const ATTR = "data-premium";
  const KEY = "premium";
  const LOGO_DEFAULT = "img/ZTX_Logo_Default.png";
  const LOGO_PREMIUM = "img/ZTX_Logo_Primium.png";

  function apply(enabled) {
    const on = Boolean(enabled);
    document.documentElement.setAttribute(ATTR, on ? "on" : "off");

    const logo = document.getElementById("app-logo-img");
    if (logo) logo.src = on ? LOGO_PREMIUM : LOGO_DEFAULT;

    window.dispatchEvent(
      new CustomEvent("ztx:premium-changed", { detail: { enabled: on } })
    );
  }

  window.ZTXPremium = {
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
        ? window.ZTXStorage.get(KEY, false)
        : false;
      apply(saved);
    }
  };
})();
