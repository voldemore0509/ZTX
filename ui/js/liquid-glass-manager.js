/* ============================================================
   ZTX — liquid-glass-manager.js
   Traduit une valeur 0..100 en --glass-blur (0..30px)
   0    -> data-glass="off" (blur désactivé pour perf)
   >0   -> data-glass="on"  + variable CSS mise à jour
   ============================================================ */

(function () {
  "use strict";

  const KEY = "glass";
  const DEFAULT = 50; // curseur au milieu par défaut
  const MAX_BLUR_PX = 30;

  function apply(value) {
    const v = Math.max(0, Math.min(100, Number(value) || 0));
    const blur = (v / 100) * MAX_BLUR_PX;
    const root = document.documentElement;

    root.style.setProperty("--glass-blur", blur.toFixed(1) + "px");

    // Rythme de saturation pour rendre le verre plus vivant à plein régime
    const sat = 100 + (v / 100) * 60; // 100% -> 160%
    root.style.setProperty("--glass-saturate", sat.toFixed(0) + "%");

    // Etat off / on pour économiser le GPU quand le slider est à 0
    root.setAttribute("data-glass", v === 0 ? "off" : "on");

    window.dispatchEvent(
      new CustomEvent("ztx:glass-changed", { detail: { value: v, blur } })
    );
  }

  window.ZTXGlass = {
    set(value) {
      apply(value);
      if (window.ZTXStorage) window.ZTXStorage.set(KEY, value);
    },
    get() {
      const raw = document.documentElement.style.getPropertyValue("--glass-blur");
      const px = parseFloat(raw) || 0;
      return Math.round((px / MAX_BLUR_PX) * 100);
    },
    init() {
      const saved = window.ZTXStorage
        ? window.ZTXStorage.get(KEY, DEFAULT)
        : DEFAULT;
      apply(saved);
    }
  };
})();
