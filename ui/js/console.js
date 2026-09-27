/* ============================================================
   ZTX — console.js
   Terminal IHM : retour d'exécution vers l'utilisateur.
   ------------------------------------------------------------
   - Repliable (chevron), état persisté ; ouvert par défaut.
   - 4 niveaux : info / ok / warn / err
   - Journal plafonné (MAX_LINES) pour ne pas gonfler le DOM.
   - Tous les modules de la zone de travail écrivent ici plutôt
     que dans console.log : c'est l'utilisateur qui doit voir.
   ============================================================ */

(function () {
  "use strict";

  const MAX_LINES = 400;
  const KEY_OPEN = "console.open";

  const LEVELS = { info: "INFO", ok: "OK", warn: "WARN", err: "ERR" };

  let panel = null;
  let toggleBtn = null;
  let list = null;
  let counter = null;
  let lineCount = 0;

  /* ---------- Rendu d'une ligne ---------- */
  function append(message, level) {
    if (!list) return;

    const lvl = level in LEVELS ? level : "info";

    const li = document.createElement("li");
    li.className = "console-line";
    li.setAttribute("data-level", lvl);

    const time = document.createElement("span");
    time.className = "console-time";
    time.textContent = window.ZTXFormat.time();

    const tag = document.createElement("span");
    tag.className = "console-tag";
    tag.textContent = LEVELS[lvl];

    const msg = document.createElement("span");
    msg.className = "console-msg";
    msg.textContent = String(message);

    li.append(time, tag, msg);
    list.appendChild(li);
    lineCount++;

    // Plafond : on retire les plus anciennes
    while (list.children.length > MAX_LINES) {
      list.removeChild(list.firstElementChild);
    }

    if (counter) counter.textContent = String(lineCount);

    // Auto-scroll seulement si le panneau est ouvert
    if (isOpen()) list.scrollTop = list.scrollHeight;
  }

  /* ---------- Ouverture / repli ---------- */
  function isOpen() {
    return !panel || panel.getAttribute("data-open") === "on";
  }

  function setOpen(open) {
    if (!panel) return;
    const on = Boolean(open);
    panel.setAttribute("data-open", on ? "on" : "off");
    if (toggleBtn) toggleBtn.setAttribute("aria-expanded", on ? "true" : "false");
    if (window.ZTXStorage) window.ZTXStorage.set(KEY_OPEN, on);
    if (on && list) list.scrollTop = list.scrollHeight;
  }

  function clear() {
    if (!list) return;
    list.textContent = "";
    lineCount = 0;
    if (counter) counter.textContent = "0";
  }

  /* ---------- Menu "Terminal" de la barre de titre ---------- */
  function bindDropdown() {
    const dropdown = document.getElementById("dropdown-terminal");
    if (!dropdown) return;

    dropdown.addEventListener("click", (e) => {
      const btn = e.target.closest("[data-action]");
      if (!btn) return;

      switch (btn.getAttribute("data-action")) {
        case "console-toggle":
          setOpen(!isOpen());
          break;
        case "console-clear":
          clear();
          append(window.ZTXi18n.tech("log.console.cleared"), "info");
          break;
      }
      if (window.ZTXMenu) window.ZTXMenu.close();
    });
  }

  window.ZTXConsole = {
    log: append,
    info(msg) { append(msg, "info"); },
    ok(msg) { append(msg, "ok"); },
    warn(msg) { append(msg, "warn"); },
    err(msg) { append(msg, "err"); },
    clear,
    setOpen,
    isOpen,
    toggle() { setOpen(!isOpen()); },

    init() {
      panel = document.getElementById("console-panel");
      toggleBtn = document.getElementById("console-toggle");
      list = document.getElementById("console-lines");
      counter = document.getElementById("console-count");

      const saved = window.ZTXStorage ? window.ZTXStorage.get(KEY_OPEN, true) : true;
      setOpen(saved);

      if (toggleBtn) {
        toggleBtn.addEventListener("click", () => setOpen(!isOpen()));
      }

      const clearBtn = document.getElementById("btn-console-clear");
      if (clearBtn) {
        clearBtn.addEventListener("click", (e) => {
          e.stopPropagation();
          clear();
          append(window.ZTXi18n.tech("log.console.cleared"), "info");
        });
      }

      bindDropdown();
    }
  };
})();
