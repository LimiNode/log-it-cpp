(function () {
    function moveThemeToggle() {
        var target = document.getElementById("logit-theme-toggle");
        var toggles = document.querySelectorAll("doxygen-awesome-dark-mode-toggle");
        if (!target || toggles.length === 0) {
            return;
        }

        target.appendChild(toggles[0]);
        for (var i = 1; i < toggles.length; ++i) {
            toggles[i].remove();
        }
    }

    function scheduleMove() {
        window.requestAnimationFrame(moveThemeToggle);
    }

    document.addEventListener("DOMContentLoaded", function () {
        moveThemeToggle();
        window.addEventListener("resize", scheduleMove);

        var observer = new MutationObserver(scheduleMove);
        observer.observe(document.body, { childList: true, subtree: true });
    });
})();
