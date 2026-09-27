/* ============================================================
   ZTX — file-types.js
   Panneau "Types de fichiers" (rubrique 2).
   ------------------------------------------------------------
   Un "type" = un preset nommé qui regroupe des extensions :
       { id: "ue", name: "UE", ext: [".uasset", ".umap", ".pak"] }

   - Liste sélectionnable (clic = sélectionner / re-clic = désélectionner)
   - Ajout via éditeur inline (jamais prompt() : une boîte native
     figerait le WebView2 et l'UI ne répondrait plus)
   - Suppression du type sélectionné
   - Bouton "Optimiser" : point d'entrée réservé au futur moteur
   ============================================================ */

(function () {
  "use strict";

  const KEY = "fileTypes";

  /** Preset livré par défaut : fichiers Unreal Engine. */
  const DEFAULT_TYPES = [
    { id: "ue", name: "UE", ext: [".uasset", ".umap", ".pak", ".ubulk"] }
  ];

  /** Message de terminal (toujours en anglais).
      Les libellés visibles de ce panneau vivent dans le HTML (data-i18n). */
  const tech = (key) => window.ZTXi18n.tech(key);

  let types = [];
  let selectedId = null;

  let listEl = null;
  let emptyEl = null;
  let countEl = null;
  let editorEl = null;
  let nameInput = null;
  let extInput = null;
  let removeBtn = null;

  /* ---------- Persistance ---------- */
  function load() {
    const raw = window.ZTXStorage ? window.ZTXStorage.get(KEY, null) : null;
    if (!Array.isArray(raw)) return DEFAULT_TYPES.map((t) => ({ ...t, ext: t.ext.slice() }));

    return raw
      .filter((item) => item && typeof item.name === "string")
      .map((item) => ({
        id: String(item.id || makeId()),
        name: item.name,
        ext: Array.isArray(item.ext) ? item.ext : []
      }));
  }

  function save() {
    if (window.ZTXStorage) window.ZTXStorage.set(KEY, types);
  }

  function makeId() {
    return "t" + Date.now().toString(36) + Math.floor(Math.random() * 1e4).toString(36);
  }

  /* ---------- Rendu ---------- */
  function render() {
    if (!listEl) return;
    listEl.textContent = "";

    types.forEach((type) => {
      const li = document.createElement("li");
      li.className = "type-item";
      li.setAttribute("role", "option");
      li.setAttribute("data-id", type.id);
      li.setAttribute("aria-selected", type.id === selectedId ? "true" : "false");
      li.tabIndex = 0;

      const dot = document.createElement("span");
      dot.className = "type-dot";

      const name = document.createElement("span");
      name.className = "type-name";
      name.textContent = type.name;

      const ext = document.createElement("span");
      ext.className = "type-ext";
      ext.textContent = type.ext.length ? type.ext.join(" · ") : "—";

      li.append(dot, name, ext);
      listEl.appendChild(li);
    });

    if (emptyEl) emptyEl.hidden = types.length > 0;
    if (countEl) countEl.textContent = String(types.length);
    if (removeBtn) removeBtn.disabled = selectedId === null;
  }

  /* ---------- Sélection ---------- */
  function select(id) {
    selectedId = selectedId === id ? null : id;
    render();
  }

  /* ---------- Éditeur inline ---------- */
  function openEditor() {
    if (!editorEl) return;
    editorEl.hidden = false;
    if (nameInput) {
      nameInput.value = "";
      nameInput.focus();
    }
    if (extInput) extInput.value = "";
  }

  function closeEditor() {
    if (editorEl) editorEl.hidden = true;
  }

  function submitEditor(e) {
    if (e) e.preventDefault();

    const name = nameInput ? nameInput.value.trim() : "";
    if (!name) {
      window.ZTXConsole.warn(tech("log.types.nameRequired"));
      if (nameInput) nameInput.focus();
      return;
    }

    const exists = types.some((type) => type.name.toLowerCase() === name.toLowerCase());
    if (exists) {
      window.ZTXConsole.warn(tech("log.types.duplicate").replace("{name}", name));
      if (nameInput) nameInput.select();
      return;
    }

    const ext = window.ZTXFormat.parseExtensions(extInput ? extInput.value : "");
    const type = { id: makeId(), name, ext };

    types.push(type);
    selectedId = type.id;
    save();
    render();
    closeEditor();

    window.ZTXConsole.ok(
      tech("log.types.added")
        .replace("{name}", name)
        .replace("{ext}", ext.length ? ext.join(" ") : "—")
    );
  }

  /* ---------- Suppression ---------- */
  function removeSelected() {
    const victim = types.find((type) => type.id === selectedId);
    if (!victim) {
      window.ZTXConsole.warn(tech("log.types.selectFirst"));
      return;
    }

    types = types.filter((type) => type.id !== selectedId);
    selectedId = null;
    save();
    render();
    window.ZTXConsole.info(tech("log.types.removed").replace("{name}", victim.name));
  }

  /* ---------- Optimisation (stub) ---------- */
  async function optimize() {
    const path = window.ZTXTarget ? window.ZTXTarget.getPath() : "";

    if (!path) {
      window.ZTXConsole.warn(tech("log.common.noTarget"));
      return;
    }
    if (!types.length) {
      window.ZTXConsole.warn(tech("log.types.noneDefined"));
      return;
    }

    window.ZTXConsole.info(
      tech("log.optimize.start").replace("{count}", String(types.length))
    );

    // Le moteur n'existe pas encore : l'API Python répond implemented=false.
    await window.ZTXBridge.call("optimize", path, types);
    window.ZTXConsole.warn(tech("log.optimize.notWired"));
  }

  window.ZTXFileTypes = {
    all() {
      return types.map((type) => ({ ...type, ext: type.ext.slice() }));
    },
    selected() {
      return types.find((type) => type.id === selectedId) || null;
    },

    init() {
      listEl = document.getElementById("type-list");
      emptyEl = document.getElementById("types-empty");
      countEl = document.getElementById("types-count");
      editorEl = document.getElementById("type-editor");
      nameInput = document.getElementById("type-name");
      extInput = document.getElementById("type-ext");
      removeBtn = document.getElementById("btn-type-remove");

      types = load();
      render();

      if (listEl) {
        listEl.addEventListener("click", (e) => {
          const item = e.target.closest(".type-item");
          if (item) select(item.getAttribute("data-id"));
        });
        listEl.addEventListener("keydown", (e) => {
          if (e.key !== "Enter" && e.key !== " ") return;
          const item = e.target.closest(".type-item");
          if (!item) return;
          e.preventDefault();
          select(item.getAttribute("data-id"));
        });
      }

      document.getElementById("btn-type-add")?.addEventListener("click", () => {
        if (editorEl && editorEl.hidden) openEditor();
        else closeEditor();
      });
      document.getElementById("type-cancel")?.addEventListener("click", closeEditor);
      if (editorEl) editorEl.addEventListener("submit", submitEditor);

      if (removeBtn) removeBtn.addEventListener("click", removeSelected);
      document.getElementById("btn-optimize")?.addEventListener("click", optimize);

      // Echap ferme l'éditeur inline
      document.addEventListener("keydown", (e) => {
        if (e.key === "Escape" && editorEl && !editorEl.hidden) closeEditor();
      });
    }
  };
})();
