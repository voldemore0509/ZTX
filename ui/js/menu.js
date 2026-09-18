/* ============================================================
   ZTX — menu.js
   Gère l'ouverture des menus déroulants sous la barre de titre.
   Un seul menu ouvert à la fois. Fermeture au clic extérieur ou
   à la touche Escape.
   ============================================================ */

(function () {
  "use strict";

  const items = document.querySelectorAll(".menu-item[data-menu]");
  const dropdowns = new Map();

  function openMenu(name, anchor) {
    closeAll();
    const dd = dropdowns.get(name);
    if (!dd) return;

    dd.hidden = false;
    // Réinitialise l'animation à chaque ouverture
    dd.style.animation = "none";
    // eslint-disable-next-line no-unused-expressions
    dd.offsetHeight;
    dd.style.animation = "";

    // Positionne le dropdown sous le bouton cliqué
    const rect = anchor.getBoundingClientRect();
    dd.style.left = rect.left + "px";

    // Anti-débordement : si le dropdown sort à droite de la fenêtre,
    // on le décale vers la gauche pour rester dans le cadre.
    requestAnimationFrame(() => {
      const dropRect = dd.getBoundingClientRect();
      const overflow = dropRect.right - (window.innerWidth - 10);
      if (overflow > 0) {
        dd.style.left = Math.max(10, rect.left - overflow) + "px";
      }
    });

    anchor.classList.add("active");
  }

  function closeAll() {
    dropdowns.forEach((dd) => (dd.hidden = true));
    items.forEach((i) => i.classList.remove("active"));
  }

  function onDocClick(e) {
    if (e.target.closest(".dropdown")) return;
    if (e.target.closest(".menu-item")) return;
    closeAll();
  }

  function onKey(e) {
    if (e.key === "Escape") closeAll();
  }

  window.ZTXMenu = {
    close: closeAll,
    init() {
      // Récupère les dropdowns par convention d'id : dropdown-<name>
      items.forEach((btn) => {
        const name = btn.getAttribute("data-menu");
        const dd = document.getElementById("dropdown-" + name);
        if (dd) dropdowns.set(name, dd);

        btn.addEventListener("click", (e) => {
          e.stopPropagation();
          const isOpen = !dd?.hidden && btn.classList.contains("active");
          if (isOpen) {
            closeAll();
          } else {
            openMenu(name, btn);
          }
        });
      });

      document.addEventListener("click", onDocClick);
      document.addEventListener("keydown", onKey);
    }
  };
})();
