const { chromium } = require("playwright");

const baseUrl = process.env.DOCS_BASE_URL || "http://127.0.0.1:8765";

function assert(condition, message) {
    if (!condition) {
        throw new Error(message);
    }
}

function within(value, lower, upper) {
    return value >= lower - 1 && value <= upper + 1;
}

async function inspectViewport(page, name, viewport) {
    await page.setViewportSize(viewport);
    await page.goto(`${baseUrl}/index.html`, { waitUntil: "networkidle" });
    await page.locator("#MSearchBox").waitFor();
    await page.waitForFunction(() => {
        const toggles = document.querySelectorAll("doxygen-awesome-dark-mode-toggle");
        return toggles.length === 1 && toggles[0].parentElement?.id === "logit-theme-toggle";
    });
    await page.waitForTimeout(500);

    const layout = await page.evaluate(() => {
        const sideNav = document.querySelector("#side-nav")?.getBoundingClientRect();
        const search = document.querySelector("#MSearchBox")?.getBoundingClientRect();
        const toggle = document.querySelector("doxygen-awesome-dark-mode-toggle")?.getBoundingClientRect();
        const root = document.documentElement;
        return {
            viewportWidth: window.innerWidth,
            scrollWidth: root.scrollWidth,
            sidebarWidth: sideNav?.width ?? 0,
            sidebarLeft: sideNav?.left ?? 0,
            sidebarRight: sideNav?.right ?? 0,
            searchLeft: search?.left ?? 0,
            searchRight: search?.right ?? 0,
            toggleLeft: toggle?.left ?? 0,
            toggleRight: toggle?.right ?? 0,
            searchTop: search?.top ?? 0,
            searchBottom: search?.bottom ?? 0,
            toggleTop: toggle?.top ?? 0,
            toggleBottom: toggle?.bottom ?? 0,
            searchWidth: search?.width ?? 0,
            spacingMedium: parseFloat(getComputedStyle(root).getPropertyValue("--spacing-medium")),
            toggleCount: document.querySelectorAll("doxygen-awesome-dark-mode-toggle").length,
            toggleParentId: toggle?.parentElement?.id ?? "",
            cssSidebarWidth: parseFloat(getComputedStyle(root).getPropertyValue("--side-nav-fixed-width")),
        };
    });
    console.log(`${name}: metrics ${JSON.stringify(layout)}`);

    assert(layout.scrollWidth <= layout.viewportWidth + 1,
        `${name}: horizontal overflow (${layout.scrollWidth}px > ${layout.viewportWidth}px)`);
    assert(within(layout.searchLeft, 0, layout.viewportWidth) &&
        within(layout.searchRight, 0, layout.viewportWidth),
        `${name}: search box is outside the viewport`);
    assert(within(layout.toggleLeft, 0, layout.viewportWidth) &&
        within(layout.toggleRight, 0, layout.viewportWidth),
        `${name}: dark-mode toggle is outside the viewport`);
    const separated = layout.searchRight <= layout.toggleLeft + 1 ||
        layout.toggleRight <= layout.searchLeft + 1 ||
        layout.searchBottom <= layout.toggleTop + 1 ||
        layout.toggleBottom <= layout.searchTop + 1;
    assert(separated,
        `${name}: search box and dark-mode toggle overlap`);
    assert(layout.toggleTop < layout.searchTop,
        `${name}: dark-mode toggle is not above the search box`);
    assert(layout.toggleParentId === "logit-theme-toggle",
        `${name}: dark-mode toggle is not in the header container`);
    assert(layout.toggleCount === 1,
        `${name}: expected one dark-mode toggle, got ${layout.toggleCount}`);

    if (viewport.width >= 768) {
        assert(Math.abs(layout.cssSidebarWidth - 335) <= 1,
            `${name}: expected --side-nav-fixed-width to be 335px, got ${layout.cssSidebarWidth}px`);
        assert(Math.abs(layout.sidebarWidth - 335) <= 1,
            `${name}: expected sidebar width to be 335px, got ${layout.sidebarWidth}px`);
        const expectedSearchWidth = layout.sidebarWidth - 2 * layout.spacingMedium;
        assert(layout.searchWidth >= expectedSearchWidth - 2,
            `${name}: search box is too narrow (${layout.searchWidth}px; expected about ${expectedSearchWidth}px)`);
        assert(layout.searchLeft >= layout.sidebarLeft - 1 &&
            layout.searchRight <= layout.sidebarRight + 1,
            `${name}: search box is not contained by the sidebar`);
        assert(layout.toggleLeft >= layout.sidebarLeft - 1 &&
            layout.toggleRight <= layout.sidebarRight + 1,
            `${name}: dark-mode toggle is not contained by the sidebar`);
    }

    console.log(`${name}: layout OK`);
}

(async () => {
    const browser = await chromium.launch({ headless: true });
    try {
        const page = await browser.newPage({ viewport: { width: 1440, height: 900 } });
        await inspectViewport(page, "desktop", { width: 1440, height: 900 });
        await inspectViewport(page, "wide-desktop", { width: 1745, height: 864 });
        await inspectViewport(page, "mobile", { width: 390, height: 844 });
    } finally {
        await browser.close();
    }
})().catch((error) => {
    console.error(error.stack || error);
    process.exitCode = 1;
});
