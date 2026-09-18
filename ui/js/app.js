/* ============================================================
   ZTX — app.js
   Point d'entrée : initialise chaque module dans le bon ordre,
   branche le ripple effect global, gère la restauration d'état.
   ============================================================ */

(function () {
  "use strict";

  /* ---------- Ripple effect générique ---------- */
  function attachRipple(selector) {
    document.body.addEventListener("click", (e) => {
      // Pas de ripple en mode compressé : la contrainte est "aucune animation"
      if (window.ZTXCompact && window.ZTXCompact.get()) return;

      const target = e.target.closest(selector);
      if (!target) return;

      const rect = target.getBoundingClientRect();
      const size = Math.max(rect.width, rect.height);
      const x = e.clientX - rect.left - size / 2;
      const y = e.clientY - rect.top - size / 2;

      const ripple = document.createElement("span");
      ripple.className = "ripple";
      ripple.style.width = ripple.style.height = size + "px";
      ripple.style.left = x + "px";
      ripple.style.top = y + "px";
      target.appendChild(ripple);

      ripple.addEventListener("animationend", () => ripple.remove(), {
        once: true
      });
    });
  }

  /* ---------- Boot ---------- */
  function boot() {
    // 1. Restaure les préférences AVANT le premier paint utile
    window.ZTXTheme.init();
    window.ZTXGlass.init();
    window.ZTXPremium.init();
    window.ZTXMaxButton.init();
    window.ZTXCompact.init();
    window.ZTXi18n.init();

    // 2. Branche les interactions UI
    window.ZTXWindow.init();
    window.ZTXMenu.init();
    window.ZTXFileMenu.init();
    window.ZTXHelpMenu.init();
    window.ZTXSettings.init();

    // 3. Effets globaux
    attachRipple(".menu-item, .win-btn, .seg-btn, .menu-action");

    // 4. Log de démarrage propre
    console.info("%cZTX", "font-weight:800;font-size:14px;color:#ffd700",
      "— Interface prête. Bridge natif :", !!(window.pywebview && window.pywebview.api));
  }

  if (document.readyState === "loading") {
    document.addEventListener("DOMContentLoaded", boot);
  } else {
    boot();
  }
})();
