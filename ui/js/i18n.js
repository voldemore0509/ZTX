/* ============================================================
   ZTX — i18n.js
   Petit moteur de traduction : lit data-i18n dans le DOM
   et remplace le texte selon la langue courante.
   Supporte aussi data-i18n-title pour les tooltips (title="...").
   ============================================================ */

(function () {
  "use strict";

  const DEFAULT_LANG = "fr";
  const SUPPORTED = ["fr", "en", "es", "zh", "ja", "vi", "lo", "hi"];
  let currentLang = DEFAULT_LANG;

  function translate(key, lang) {
    const dict = (window.ZTX_LOCALES && window.ZTX_LOCALES[lang]) || {};
    if (key in dict) return dict[key];
    // Fallback : si la clé manque dans la langue courante, tente le français,
    // sinon retourne la clé brute (utile pour debug).
    const fallback = (window.ZTX_LOCALES && window.ZTX_LOCALES[DEFAULT_LANG]) || {};
    return key in fallback ? fallback[key] : key;
  }

  function applyAll(lang) {
    if (!SUPPORTED.includes(lang)) lang = DEFAULT_LANG;
    currentLang = lang;
    document.documentElement.setAttribute("lang", lang);

    // Texte
    document.querySelectorAll("[data-i18n]").forEach((el) => {
      const key = el.getAttribute("data-i18n");
      el.textContent = translate(key, lang);
    });

    // Tooltips (attribut title)
    document.querySelectorAll("[data-i18n-title]").forEach((el) => {
      const key = el.getAttribute("data-i18n-title");
      el.setAttribute("title", translate(key, lang));
    });

    // Notifie les autres modules (settings, etc.)
    window.dispatchEvent(
      new CustomEvent("ztx:language-changed", { detail: { lang } })
    );
  }

  window.ZTXi18n = {
    setLang(lang) {
      applyAll(lang);
      if (window.ZTXStorage) window.ZTXStorage.set("lang", lang);
    },
    getLang() {
      return currentLang;
    },
    t(key) {
      return translate(key, currentLang);
    },
    supported() {
      return SUPPORTED.slice();
    },
    init() {
      const saved = window.ZTXStorage
        ? window.ZTXStorage.get("lang", DEFAULT_LANG)
        : DEFAULT_LANG;
      applyAll(saved);
    }
  };
})();
