import { redirectTo } from './utils.js';
import { logout } from './common.js';

export async function initTabbar() {
    let container = document.getElementById("tab-bar");
    // If the container isn't present yet (component may be injected), retry briefly
    if (!container) {
        for (let i = 0; i < 10 && !container; i++) {
            await new Promise((r) => setTimeout(r, 50));
            container = document.getElementById("tab-bar");
        }
    }
    if (!container) {
        console.error('initTabbar: element with id "tab-bar" not found');
        return;
    }

    // Attach behavior to elements inside the tab bar
    const loginBtn = container.querySelector("#loginTabLink");
    if (loginBtn) loginBtn.addEventListener("click", () => redirectTo("login_ui.html"));

    const homeBtn = container.querySelector("#homeTabLink");
    if (homeBtn) homeBtn.addEventListener("click", () => redirectTo("home_ui.html"));

    const registerBtn = container.querySelector("#registerTabLink");
    if (registerBtn) registerBtn.addEventListener("click", () => redirectTo("register_ui.html"));

    const configBtn = container.querySelector("#configTabLink");
    if (configBtn) configBtn.addEventListener("click", () => redirectTo("config_ui.html"));

    const logoutBtn = container.querySelector("#logoutTabLink");
    if (logoutBtn) logoutBtn.addEventListener("click", () => logout());
}