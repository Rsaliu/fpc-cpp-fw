import { homePageStateMachine } from './state_manager.js';
import { updateUI, loadComponent } from './common.js';
import { redirectTo } from './utils.js';
import { logout } from './common.js';
import { initTabbar } from './tab-bar.js';

window.redirectTo = redirectTo; // Expose redirectTo to the global scope
window.logout = logout; // Expose logout to the global scope

// Initialize the UI after DOM is ready. Load tab-bar first so its
// elements (tab links) are present before updating visibility.
document.addEventListener("DOMContentLoaded", async () => {
	await loadComponent("tab-bar", "../html/tab-bar.html");
	await initTabbar();
	updateUI(homePageStateMachine);
});


