(function () {
    function moveThemeToggle() {
        var target = document.getElementById("logit-theme-toggle");
        var toggle = document.querySelector("doxygen-awesome-dark-mode-toggle");
        if (target && toggle && toggle.parentNode !== target) {
            target.appendChild(toggle);
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
