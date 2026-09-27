/* ============================================================
   ZTX — ui-scale.js
   Réglage "Taille du logiciel" (Paramètres) : − / valeur / +
   ------------------------------------------------------------
   Technique : propriété CSS `zoom` posée sur <html>.

   Pourquoi <html> et pas <body> : sous zoom, Chromium (donc WebView2)
   résout les POURCENTAGES par rapport au bloc conteneur déjà divisé par
   le zoom. Sur <html> le bloc conteneur est le viewport → `height: 100%`
   reste exact à toutes les échelles. Sur <body>, le conteneur serait
   <html> non zoomé → débordement vertical.

   Corollaire respecté dans tout le CSS du projet : AUCUNE unité vh/vw
   dans le layout. Les unités de viewport, elles, ignorent le zoom et
   feraient déborder la fenêtre.

   Raccourcis : Ctrl + "+" / Ctrl + "−" / Ctrl + 0 (remise à 100 %).
   ============================================================ */

(function () {
  "use strict";

  const KEY = "uiScale";
  const MIN = 0.7;
  const MAX = 1.6;
  const STEP = 0.1;
  const DEFAULT = 1;

  let factor = DEFAULT;
  let valueBtn = null;
  let minusBtn = null;
  let plusBtn = null;

  /** Arrondi au centième : évite les 1.0999999999999999 après plusieurs pas. */
  function clamp(value) {
    const v = Math.round((Number(value) || DEFAULT) * 100) / 100;
    return Math.min(MAX, Math.max(MIN, v));
  }

  function render() {
    if (valueBtn) valueBtn.textContent = Math.round(factor * 100) + " %";
    // Tolérance : comparaison de flottants
    if (minusBtn) minusBtn.disabled = factor <= MIN + 0.001;
    if (plusBtn) plusBtn.disabled = factor >= MAX - 0.001;
  }

  function apply(value) {
    factor = clamp(value);

    const root = document.documentElement;
    // À 100 % on retire la propriété : zéro surcoût de composition.
    root.style.zoom = factor === 1 ? "" : String(factor);
    // Exposé au CSS et aux modules qui mélangent coordonnées visuelles
    // et coordonnées de layout (menu.js, ripple d'app.js).
    root.style.setProperty("--ui-scale", String(factor));

    render();
    window.dispatchEvent(
      new CustomEvent("ztx:scale-changed", { detail: { factor } })
    );
  }

  function set(value) {
    apply(value);
    if (window.ZTXStorage) window.ZTXStorage.set(KEY, factor);
  }

  function step(direction) {
    set(factor + direction * STEP);
  }

  function onKeydown(e) {
    if (!e.ctrlKey || e.altKey || e.shiftKey) return;

    if (e.key === "+" || e.key === "=") {
      e.preventDefault();
      step(1);
    } else if (e.key === "-" || e.key === "_") {
      e.preventDefault();
      step(-1);
    } else if (e.key === "0") {
      e.preventDefault();
      set(DEFAULT);
    }
  }

  window.ZTXScale = {
    /** Facteur courant (1 = 100 %). */
    get() {
      return factor;
    },
    set,
    step,
    reset() {
      set(DEFAULT);
    },

    init() {
      valueBtn = document.getElementById("scale-value");
      minusBtn = document.getElementById("scale-minus");
      plusBtn = document.getElementById("scale-plus");

      const saved = window.ZTXStorage ? window.ZTXStorage.get(KEY, DEFAULT) : DEFAULT;
      apply(saved);

      if (minusBtn) minusBtn.addEventListener("click", () => step(-1));
      if (plusBtn) plusBtn.addEventListener("click", () => step(1));
      // Clic sur la valeur = retour à 100 %
      if (valueBtn) valueBtn.addEventListener("click", () => set(DEFAULT));

      document.addEventListener("keydown", onKeydown);
    }
  };
})();
