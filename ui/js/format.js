/* ============================================================
   ZTX — format.js
   Conversions et formatage partagés (tailles, heure, extensions).
   Base 1024. Les libellés suivent la langue de l'interface :
   o/Ko/Mo/Go/To en français, B/KB/MB/GB/TB ailleurs.
   Les variantes tech* servent au terminal, qui reste anglophone.
   ============================================================ */

(function () {
  "use strict";

  /* Clés internes des unités (jamais affichées telles quelles) */
  const UNITS = { o: 1, Ko: 1024, Mo: 1048576, Go: 1073741824, To: 1099511627776 };
  const KEYS = ["o", "Ko", "Mo", "Go", "To"];

  /* Libellés affichés : français d'un côté, notation internationale de l'autre */
  const LABELS = {
    fr: ["o", "Ko", "Mo", "Go", "To"],
    intl: ["B", "KB", "MB", "GB", "TB"]
  };

  /* Etiquettes BCP-47 pour la séparation des milliers */
  const TECH_TAG = "en-US";
  const LOCALE_TAGS = {
    fr: "fr-FR", en: "en-US", es: "es-ES", zh: "zh-CN",
    ja: "ja-JP", vi: "vi-VN", lo: "lo-LA", hi: "hi-IN"
  };

  function lang() {
    return window.ZTXi18n ? window.ZTXi18n.getLang() : "fr";
  }

  function ladder() {
    return lang() === "fr" ? LABELS.fr : LABELS.intl;
  }

  function localeTag() {
    return LOCALE_TAGS[lang()] || "fr-FR";
  }

  function pad2(n) {
    return n < 10 ? "0" + n : String(n);
  }

  /** Cœur du formatage d'une taille : base 1024, une décimale sous 100. */
  function formatBytes(n, labels, tag) {
    let size = Number(n);
    if (!isFinite(size) || size <= 0) return "0 " + labels[0];

    let i = 0;
    while (size >= 1024 && i < labels.length - 1) {
      size /= 1024;
      i++;
    }
    const rounded = size >= 100 || i === 0 ? Math.round(size) : Math.round(size * 10) / 10;
    return rounded.toLocaleString(tag) + " " + labels[i];
  }

  window.ZTXFormat = {
    UNITS,

    /** Clés d'unité manipulées par l'UI (data-unit) */
    units() {
      return KEYS.slice();
    },

    /** Libellé affichable d'une unité dans la langue courante : "Mo" / "MB" */
    unitLabel(key) {
      const index = KEYS.indexOf(key);
      return index === -1 ? key : ladder()[index];
    },

    /** (100, "Mo") -> 104857600 */
    toBytes(value, unit) {
      const v = Number(value);
      const factor = UNITS[unit] || UNITS.Mo;
      if (!isFinite(v) || v <= 0) return 0;
      return Math.round(v * factor);
    },

    /** Interface : 104857600 -> "100 Mo" (ou "100 MB" hors français) */
    bytes(n) {
      return formatBytes(n, ladder(), localeTag());
    },

    /** Terminal : toujours "100 MB", quelle que soit la langue de l'UI */
    techBytes(n) {
      return formatBytes(n, LABELS.intl, TECH_TAG);
    },

    /** Interface : 1234 -> "1 234" (séparateur selon la langue) */
    count(n) {
      return Number(n || 0).toLocaleString(localeTag());
    },

    /** Terminal : 1234 -> "1,234" */
    techCount(n) {
      return Number(n || 0).toLocaleString(TECH_TAG);
    },

    /** Horodatage court pour le terminal : "14:07:32" */
    time(date) {
      const d = date || new Date();
      return pad2(d.getHours()) + ":" + pad2(d.getMinutes()) + ":" + pad2(d.getSeconds());
    },

    /**
     * Normalise une saisie d'extensions.
     * ".uasset, UMAP ; pak" -> [".uasset", ".umap", ".pak"]
     */
    parseExtensions(raw) {
      return String(raw || "")
        .split(/[,;\s]+/)
        .map((token) => token.trim().toLowerCase())
        .filter(Boolean)
        .map((token) => (token.startsWith(".") ? token : "." + token))
        .filter((token, index, list) => list.indexOf(token) === index);
    }
  };
})();
