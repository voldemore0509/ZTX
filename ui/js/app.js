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

      // Coordonnées visuelles -> coordonnées de layout (cf. ui-scale.js)
      const scale = window.ZTXScale ? window.ZTXScale.get() : 1;
      const rect = target.getBoundingClientRect();
      const size = Math.max(rect.width, rect.height) / scale;
      const x = (e.clientX - rect.left) / scale - size / 2;
      const y = (e.clientY - rect.top) / scale - size / 2;

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
    window.ZTXScale.init();
    window.ZTXi18n.init();

    // 2. Branche les interactions de la barre de titre
    window.ZTXWindow.init();
    window.ZTXMenu.init();
    window.ZTXFileMenu.init();
    window.ZTXHelpMenu.init();
    window.ZTXSettings.init();

    // 3. Zone de travail — le terminal d'abord : les autres modules y écrivent
    window.ZTXConsole.init();
    window.ZTXConnector.init();
    window.ZTXTarget.init();
    window.ZTXFileTypes.init();
    window.ZTXQuickDelete.init();

    // 4. Effets globaux
    attachRipple(".menu-item, .win-btn, .seg-btn, .menu-action, .btn");

    // 5. Log de démarrage
    const tech = (key) => window.ZTXi18n.tech(key);
    window.ZTXConsole.ok(tech("log.boot.ready"));

    // window.pywebview est injecté APRÈS le DOMContentLoaded : on attend
    // l'événement pywebviewready avant de conclure quoi que ce soit.
    window.ZTXBridge.ready().then((connected) => {
      console.info("%cZTX", "font-weight:800;font-size:14px;color:#ffd700",
        "— Interface prête. Bridge natif :", connected);
      window.ZTXConsole[connected ? "info" : "warn"](
        tech(connected ? "log.boot.bridge" : "log.boot.noBridge")
      );
    });
  }

  if (document.readyState === "loading") {
    document.addEventListener("DOMContentLoaded", boot);
  } else {
    boot();
  }
})();
