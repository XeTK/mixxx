
(function () {
  "use strict";
  var PREFIX = "mixxx-a11y-testplan:";

  function restore() {
    document.querySelectorAll("[data-persist-key]").forEach(function (el) {
      var key = PREFIX + el.getAttribute("data-persist-key");
      var stored = localStorage.getItem(key);
      if (stored === null) return;
      if (el.type === "radio") {
        if (el.value === stored) el.checked = true;
      } else {
        el.value = stored;
      }
    });
  }

  function persistOne(el) {
    var key = PREFIX + el.getAttribute("data-persist-key");
    if (el.type === "radio") {
      if (el.checked) localStorage.setItem(key, el.value);
    } else {
      localStorage.setItem(key, el.value);
    }
  }

  function wire() {
    document.querySelectorAll("[data-persist-key]").forEach(function (el) {
      var evt = el.tagName === "TEXTAREA" ? "input" : "change";
      el.addEventListener(evt, function () { persistOne(el); });
    });
  }

  function addFilter() {
    var scenarios = document.querySelectorAll(".scenario");
    if (scenarios.length < 2) return;
    var container = document.querySelector("nav[aria-label='Scenarios in this file']");
    if (!container) return;
    var box = document.createElement("div");
    box.className = "filter-box";
    box.innerHTML =
      '<label for="tag-filter">Filter scenarios by tag (e.g. "@blocking"). Leave blank to show all.</label>' +
      '<input type="text" id="tag-filter" autocomplete="off">' +
      '<p class="filter-status" id="filter-status" role="status"></p>';
    container.parentNode.insertBefore(box, container.nextSibling);
    var input = box.querySelector("#tag-filter");
    var status = box.querySelector("#filter-status");
    input.addEventListener("input", function () {
      var q = input.value.trim().toLowerCase();
      var shown = 0;
      scenarios.forEach(function (el) {
        var tags = (el.getAttribute("data-tags") || "").toLowerCase();
        var match = q === "" || tags.indexOf(q) !== -1;
        el.hidden = !match;
        if (match) shown++;
      });
      status.textContent = q === "" ? "" : (shown + " of " + scenarios.length + " scenarios match.");
    });
  }

  document.addEventListener("DOMContentLoaded", function () {
    restore();
    wire();
    addFilter();
  });
})();
