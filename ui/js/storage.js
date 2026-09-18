/* ============================================================
   ZTX — storage.js
   Persistance des préférences dans localStorage
   avec fallback en mémoire si non disponible (mode file://)
   ============================================================ */

(function () {
  "use strict";

  const KEY = "ztx.prefs.v1";
  const memoryFallback = { data: {} };
  let usingLocalStorage = true;

  // Test disponibilité de localStorage
  try {
    const test = "__ztx_test__";
    localStorage.setItem(test, "1");
    localStorage.removeItem(test);
  } catch (e) {
    usingLocalStorage = false;
  }

  function readAll() {
    if (!usingLocalStorage) return { ...memoryFallback.data };
    try {
      const raw = localStorage.getItem(KEY);
      return raw ? JSON.parse(raw) : {};
    } catch (e) {
      return {};
    }
  }

  function writeAll(obj) {
    if (!usingLocalStorage) {
      memoryFallback.data = { ...obj };
      return;
    }
    try {
      localStorage.setItem(KEY, JSON.stringify(obj));
    } catch (e) {
      /* silencieux — quota, mode privé, etc. */
    }
  }

  window.ZTXStorage = {
    get(key, fallback) {
      const all = readAll();
      return key in all ? all[key] : fallback;
    },
    set(key, value) {
      const all = readAll();
      all[key] = value;
      writeAll(all);
    },
    remove(key) {
      const all = readAll();
      delete all[key];
      writeAll(all);
    }
  };
})();
