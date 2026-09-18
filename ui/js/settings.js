/* ============================================================
   ZTX — settings.js
   Connecte les contrôles du panneau Paramètres aux gestionnaires :
   thème, liquid glass, langue, bouton agrandir, mode compressé, premium.
   ============================================================ */

(function () {
  "use strict";

  function activateSegmented(container, activeBtn) {
    container.querySelectorAll(".seg-btn").forEach((b) => {
      const isActive = b === activeBtn;
      b.classList.toggle("active", isActive);
      b.setAttribute("aria-checked", isActive ? "true" : "false");
    });
  }

  function initTheme() {
    const buttons = document.querySelectorAll("[data-theme-choice]");
    const current = window.ZTXTheme.get();

    buttons.forEach((btn) => {
      const choice = btn.getAttribute("data-theme-choice");
      const isActive = choice === current;
      btn.classList.toggle("active", isActive);
      btn.setAttribute("aria-checked", isActive ? "true" : "false");

      btn.addEventListener("click", () => {
        window.ZTXTheme.set(choice);
        activateSegmented(btn.parentElement, btn);
      });
    });
  }

  function initGlassSlider() {
    const slider = document.getElementById("glass-slider");
    if (!slider) return;
    slider.value = window.ZTXGlass.get();
    // input = pendant le drag, change = à la fin. On prend "input" pour un feedback live.
    slider.addEventListener("input", (e) => {
      window.ZTXGlass.set(e.target.value);
    });
  }

  function initLanguage() {
    const buttons = document.querySelectorAll("[data-lang-choice]");
    const current = window.ZTXi18n.getLang();

    buttons.forEach((btn) => {
      const choice = btn.getAttribute("data-lang-choice");
      btn.classList.toggle("active", choice === current);
      btn.setAttribute("aria-checked", choice === current ? "true" : "false");

      btn.addEventListener("click", () => {
        window.ZTXi18n.setLang(choice);
        activateSegmented(btn.parentElement, btn);
      });
    });
  }

  function initPremiumToggle() {
    const toggle = document.getElementById("premium-toggle");
    if (!toggle) return;
    toggle.checked = window.ZTXPremium.get();
    toggle.addEventListener("change", (e) => {
      window.ZTXPremium.set(e.target.checked);
    });
  }

  function initMaxButtonToggle() {
    const toggle = document.getElementById("maxbtn-toggle");
    if (!toggle || !window.ZTXMaxButton) return;
    toggle.checked = window.ZTXMaxButton.get();
    toggle.addEventListener("change", (e) => {
      window.ZTXMaxButton.set(e.target.checked);
    });
  }

  function initCompactToggle() {
    const toggle = document.getElementById("compact-toggle");
    if (!toggle || !window.ZTXCompact) return;
    toggle.checked = window.ZTXCompact.get();
    toggle.addEventListener("change", (e) => {
      window.ZTXCompact.set(e.target.checked);
    });
  }

  window.ZTXSettings = {
    init() {
      initTheme();
      initGlassSlider();
      initLanguage();
      initMaxButtonToggle();
      initCompactToggle();
      initPremiumToggle();
    }
  };
})();
