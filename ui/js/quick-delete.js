/* ============================================================
   ZTX — quick-delete.js
   Panneau "Suppression rapide" (rubrique 3) + modale de confirmation.
   ------------------------------------------------------------
   Parcours :
     1. seuil = valeur + unité (100 Mo par défaut)
     2. "Lancer" -> scan RÉEL du dossier cible (lecture seule)
     3. interrupteur ON  -> modale listant les fichiers trouvés
        interrupteur OFF -> enchaîne directement
     4. la suppression passe par ZTXApi.delete_files(), verrouillée
        côté Python tant que DELETION_ENABLED vaut False.
   ------------------------------------------------------------
   Aucun confirm() natif : une boîte modale JS bloquerait WebView2.
   ============================================================ */

(function () {
  "use strict";

  /* ------------------------------------------------------------------
     MOTEUR DÉBRANCHÉ — état voulu.
     Le bouton "Lancer la suppression rapide" ne déclenche AUCUNE action :
     ni scan, ni modale, ni appel Python. Le moteur sera écrit à la main.

     Tout le parcours ci-dessous reste en place et se rebranche en passant
     ENGINE_WIRED à true — rien d'autre à modifier.
     Pour l'essayer sans toucher au drapeau, depuis la console DevTools
     (F12, lancer avec ZTX_DEBUG=1) :   ZTXQuickDelete.run()
     ------------------------------------------------------------------ */
  const ENGINE_WIRED = false;

  const KEY_VALUE = "clean.value";
  const KEY_UNIT = "clean.unit";
  const KEY_CONFIRM = "clean.confirm";

  let valueInput = null;
  let unitPicker = null;
  let confirmToggle = null;
  let confirmSub = null;
  let hintEl = null;
  let runBtn = null;
  let stateEl = null;

  let modal = null;
  let modalSub = null;
  let modalList = null;
  let modalTotal = null;
  let modalConfirm = null;

  let unit = "Mo";
  let pendingFiles = [];
  let lastFocus = null;
  let busy = false;

  /** Message d'interface (langue courante). */
  function t(key) {
    return window.ZTXi18n.t(key);
  }

  /** Message de terminal (toujours en anglais). */
  function tech(key) {
    return window.ZTXi18n.tech(key);
  }

  /* ---------- Seuil ---------- */
  function currentValue() {
    const v = valueInput ? Number(valueInput.value) : 0;
    return isFinite(v) && v > 0 ? v : 0;
  }

  function minBytes() {
    return window.ZTXFormat.toBytes(currentValue(), unit);
  }

  function refreshHint() {
    if (!hintEl) return;
    const bytes = minBytes();
    hintEl.textContent = bytes
      ? t("ws.clean.hint").replace("{size}", window.ZTXFormat.bytes(bytes))
      : t("ws.clean.hintInvalid");
  }

  function refreshConfirmSub() {
    if (!confirmSub) return;
    const on = confirmToggle ? confirmToggle.checked : true;
    confirmSub.textContent = on ? t("ws.clean.confirmOn") : t("ws.clean.confirmOff");
  }

  function setUnit(next) {
    unit = window.ZTXFormat.UNITS[next] ? next : "Mo";
    if (unitPicker) {
      unitPicker.querySelectorAll(".seg-btn").forEach((btn) => {
        const key = btn.getAttribute("data-unit");
        const active = key === unit;
        btn.classList.toggle("active", active);
        btn.setAttribute("aria-checked", active ? "true" : "false");
        // Le libellé suit la langue ; la clé (data-unit) ne bouge jamais.
        btn.textContent = window.ZTXFormat.unitLabel(key);
      });
    }
    if (window.ZTXStorage) window.ZTXStorage.set(KEY_UNIT, unit);
    refreshHint();
  }

  function setState(key, tone) {
    if (!stateEl) return;
    stateEl.textContent = t(key);
    stateEl.classList.toggle("badge-muted", tone !== "active");
  }

  function setBusy(on) {
    busy = on;
    if (runBtn) runBtn.disabled = on;
    setState(on ? "ws.clean.scanning" : "ws.clean.idle", on ? "active" : "muted");
  }

  /* ---------- Modale ---------- */
  function openModal(scan) {
    if (!modal) return;

    pendingFiles = scan.files || [];
    lastFocus = document.activeElement;

    if (modalSub) {
      modalSub.textContent = t("ws.modal.sub")
        .replace("{count}", window.ZTXFormat.count(scan.matched))
        .replace("{size}", window.ZTXFormat.bytes(scan.min_bytes))
        .replace("{root}", scan.root);
    }

    if (modalList) {
      modalList.textContent = "";
      pendingFiles.forEach((file) => {
        const li = document.createElement("li");
        li.className = "modal-row";

        const name = document.createElement("span");
        name.className = "file-name";
        name.textContent = file.name;

        const dir = document.createElement("span");
        dir.className = "file-dir";
        dir.textContent = file.dir;
        dir.title = file.path;

        const size = document.createElement("span");
        size.className = "file-size";
        size.textContent = window.ZTXFormat.bytes(file.size);

        li.append(name, dir, size);
        modalList.appendChild(li);
      });
    }

    if (modalTotal) {
      let text = t("ws.modal.total")
        .replace("{count}", window.ZTXFormat.count(pendingFiles.length))
        .replace("{size}", window.ZTXFormat.bytes(scan.total_bytes));
      if (scan.truncated) text += " " + t("ws.modal.truncated");
      modalTotal.textContent = text;
    }

    modal.hidden = false;
    if (modalConfirm) modalConfirm.focus();
  }

  function closeModal() {
    if (!modal) return;
    modal.hidden = true;
    pendingFiles = [];
    if (lastFocus && typeof lastFocus.focus === "function") lastFocus.focus();
  }

  /* ---------- Suppression (verrouillée côté Python) ---------- */
  async function performDelete(files) {
    if (!files.length) return;

    const paths = files.map((file) => file.path);
    const res = await window.ZTXBridge.call("delete_files", paths);

    if (!res || res.ok !== true) {
      window.ZTXConsole.err(tech("log.clean.deleteFailed"));
      return;
    }

    if (res.simulated) {
      window.ZTXConsole.warn(
        tech("log.clean.locked")
          .replace("{count}", window.ZTXFormat.techCount(res.planned))
          .replace("{size}", window.ZTXFormat.techBytes(res.freed_bytes))
      );
      return;
    }

    window.ZTXConsole.ok(
      tech("log.clean.deleted")
        .replace("{count}", window.ZTXFormat.techCount(res.deleted))
        .replace("{size}", window.ZTXFormat.techBytes(res.freed_bytes))
    );
  }

  /* ---------- Bouton "Lancer" ---------- */
  function onRunClick() {
    if (!ENGINE_WIRED) {
      // Une ligne de terminal : le clic est reçu, rien ne se passe côté disque.
      window.ZTXConsole.info(tech("log.clean.notWired"));
      return;
    }
    run();
  }

  /* ---------- Parcours complet (réactivé par ENGINE_WIRED) ---------- */
  async function run() {
    if (busy) return;

    const path = window.ZTXTarget ? window.ZTXTarget.getPath() : "";
    if (!path) {
      window.ZTXConsole.warn(tech("log.common.noTarget"));
      return;
    }
    if (!currentValue()) {
      window.ZTXConsole.warn(tech("log.clean.badThreshold"));
      if (valueInput) valueInput.focus();
      return;
    }
    if (!window.ZTXBridge.available()) {
      window.ZTXConsole.warn(tech("log.bridge.absent"));
      return;
    }

    const threshold = minBytes();
    setBusy(true);
    window.ZTXConsole.info(
      tech("log.clean.scanStart")
        .replace("{path}", path)
        .replace("{size}", window.ZTXFormat.techBytes(threshold))
    );

    const scan = await window.ZTXBridge.call("scan_large_files", path, threshold);
    setBusy(false);

    if (!scan || scan.ok !== true) {
      window.ZTXConsole.err(tech("log.clean.scanFailed"));
      return;
    }

    window.ZTXConsole.info(
      tech("log.clean.scanDone")
        .replace("{scanned}", window.ZTXFormat.techCount(scan.scanned))
        .replace("{ms}", window.ZTXFormat.techCount(scan.elapsed_ms))
        .replace("{matched}", window.ZTXFormat.techCount(scan.matched))
    );

    if (scan.errors) {
      window.ZTXConsole.warn(
        tech("log.clean.unreadable").replace("{count}", window.ZTXFormat.techCount(scan.errors))
      );
    }
    if (scan.budget_reached) {
      window.ZTXConsole.warn(tech("log.clean.budget"));
    }

    if (!scan.matched) {
      window.ZTXConsole.ok(tech("log.clean.nothing"));
      return;
    }

    if (confirmToggle && confirmToggle.checked) {
      openModal(scan);
    } else {
      window.ZTXConsole.info(tech("log.clean.autoMode"));
      await performDelete(scan.files || []);
    }
  }

  /* ---------- Traductions dynamiques ---------- */
  function refreshTexts() {
    setUnit(unit);          // re-libelle les unités dans la langue courante
    refreshHint();
    refreshConfirmSub();
    setState(busy ? "ws.clean.scanning" : "ws.clean.idle", busy ? "active" : "muted");
  }

  window.ZTXQuickDelete = {
    run,
    init() {
      valueInput = document.getElementById("size-value");
      unitPicker = document.getElementById("unit-picker");
      confirmToggle = document.getElementById("confirm-toggle");
      confirmSub = document.getElementById("confirm-sub");
      hintEl = document.getElementById("size-hint");
      runBtn = document.getElementById("btn-quick-delete");
      stateEl = document.getElementById("clean-state");

      modal = document.getElementById("delete-modal");
      modalSub = document.getElementById("modal-sub");
      modalList = document.getElementById("modal-list");
      modalTotal = document.getElementById("modal-total");
      modalConfirm = document.getElementById("modal-confirm");

      /* -- Restauration des préférences -- */
      const storedValue = window.ZTXStorage ? window.ZTXStorage.get(KEY_VALUE, 100) : 100;
      const storedUnit = window.ZTXStorage ? window.ZTXStorage.get(KEY_UNIT, "Mo") : "Mo";
      const storedConfirm = window.ZTXStorage ? window.ZTXStorage.get(KEY_CONFIRM, true) : true;

      if (valueInput) valueInput.value = storedValue;
      if (confirmToggle) confirmToggle.checked = Boolean(storedConfirm);
      setUnit(storedUnit);

      /* -- Écouteurs -- */
      if (valueInput) {
        valueInput.addEventListener("input", () => {
          if (window.ZTXStorage) window.ZTXStorage.set(KEY_VALUE, currentValue() || 1);
          refreshHint();
        });
      }

      if (unitPicker) {
        unitPicker.addEventListener("click", (e) => {
          const btn = e.target.closest(".seg-btn[data-unit]");
          if (btn) setUnit(btn.getAttribute("data-unit"));
        });
      }

      if (confirmToggle) {
        confirmToggle.addEventListener("change", () => {
          if (window.ZTXStorage) window.ZTXStorage.set(KEY_CONFIRM, confirmToggle.checked);
          refreshConfirmSub();
          window.ZTXConsole.info(
            t(confirmToggle.checked ? "log.clean.modeConfirm" : "log.clean.modeAuto")
          );
        });
      }

      if (runBtn) runBtn.addEventListener("click", onRunClick);

      /* -- Modale -- */
      document.getElementById("modal-cancel")?.addEventListener("click", () => {
        window.ZTXConsole.info(tech("log.clean.cancelled"));
        closeModal();
      });
      document.getElementById("modal-close")?.addEventListener("click", closeModal);

      if (modalConfirm) {
        modalConfirm.addEventListener("click", async () => {
          const files = pendingFiles.slice();
          closeModal();
          await performDelete(files);
        });
      }

      if (modal) {
        modal.addEventListener("click", (e) => {
          if (e.target === modal) closeModal();
        });
      }

      document.addEventListener("keydown", (e) => {
        if (e.key === "Escape" && modal && !modal.hidden) closeModal();
      });

      window.addEventListener("ztx:language-changed", refreshTexts);
      refreshTexts();
    }
  };
})();
