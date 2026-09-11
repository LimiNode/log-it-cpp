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
        return toggles.length === 1 &&
            Boolean(document.querySelector("#logit-theme-toggle > doxygen-awesome-dark-mode-toggle"));
    });
    await page.waitForTimeout(500);

    const layout = await page.evaluate(() => {
        const sideNav = document.querySelector("#side-nav")?.getBoundingClientRect();
        const top = document.querySelector("#top")?.getBoundingClientRect();
        const search = document.querySelector("#MSearchBox")?.getBoundingClientRect();
        const toggle = document.querySelector("doxygen-awesome-dark-mode-toggle")?.getBoundingClientRect();
        const root = document.documentElement;
        return {
            viewportWidth: window.innerWidth,
            scrollWidth: root.scrollWidth,
            sidebarWidth: sideNav?.width ?? 0,
            sidebarLeft: sideNav?.left ?? 0,
            sidebarRight: sideNav?.right ?? 0,
            topHeight: top?.height ?? 0,
            topBottom: top?.bottom ?? 0,
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
            searchbarHeight: parseFloat(getComputedStyle(root).getPropertyValue("--searchbar-height")),
            toggleCount: document.querySelectorAll("doxygen-awesome-dark-mode-toggle").length,
            targetContainsToggle: Boolean(document.querySelector("#logit-theme-toggle > doxygen-awesome-dark-mode-toggle")),
            cssSidebarWidth: parseFloat(getComputedStyle(root).getPropertyValue("--side-nav-fixed-width")),
        };
    });

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
    assert(layout.targetContainsToggle,
        `${name}: dark-mode toggle is not anchored in the header area`);
    assert(layout.toggleCount === 1,
        `${name}: expected one dark-mode toggle, got ${layout.toggleCount}`);

    if (viewport.width >= 768) {
        assert(layout.searchBottom <= layout.topBottom + 1,
            `${name}: search box is clipped by #top (search bottom ${layout.searchBottom}px, #top bottom ${layout.topBottom}px)`);
        assert(Math.abs(layout.cssSidebarWidth - 335) <= 1,
            `${name}: expected --side-nav-fixed-width to be 335px, got ${layout.cssSidebarWidth}px`);
        assert(Math.abs(layout.sidebarWidth - 335) <= 1,
            `${name}: expected sidebar width to be 335px, got ${layout.sidebarWidth}px`);
        const expectedSearchWidth = layout.sidebarWidth - 2 * layout.spacingMedium - layout.searchbarHeight - 1;
        assert(layout.searchWidth >= expectedSearchWidth - 2,
            `${name}: search box is too narrow (${layout.searchWidth}px; expected about ${expectedSearchWidth}px)`);
        assert(Math.abs(layout.toggleTop - layout.searchTop) <= 5,
            `${name}: dark-mode toggle is not opposite the search box`);
        const searchCenter = (layout.searchTop + layout.searchBottom) / 2;
        const toggleCenter = (layout.toggleTop + layout.toggleBottom) / 2;
        assert(Math.abs(toggleCenter - searchCenter) <= 1,
            `${name}: dark-mode toggle is not vertically centered with the search box`);
        assert(layout.searchLeft >= layout.sidebarLeft - 1 &&
            layout.searchRight <= layout.sidebarRight + 1,
            `${name}: search box is not contained by the sidebar`);
        assert(layout.toggleLeft >= layout.sidebarLeft - 1 &&
            layout.toggleRight <= layout.sidebarRight + 1,
            `${name}: dark-mode toggle is not contained by the sidebar`);
    }

    console.log(`${name}: layout OK (top height ${layout.topHeight}px, search ${layout.searchTop}-${layout.searchBottom}px, top bottom ${layout.topBottom}px)`);
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
