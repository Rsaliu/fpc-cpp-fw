import { redirectTo, isLoggedIn } from './utils.js';
import { logout,updateUI,API_URL,loadComponent } from './common.js';
import { loginPageStateMachine } from './state_manager.js';
import { initTabbar } from './tab-bar.js';

window.redirectTo = redirectTo; // Expose redirectTo to the global scope
window.logout = logout; // Expose logout to the global scope
const LOGIN_URL = `${API_URL}/login`;

function login(event) {
  event.preventDefault();
  const form = document.getElementById("loginForm");
  const formData = {
    username: form.username.value.trim(),
    password: form.pwd.value
  };
  fetch(LOGIN_URL, {
    method: "POST",
    headers: {
      "Content-Type": "application/json"
    },
    credentials: "include",       // <-- required for cross-origin cookies
    body: JSON.stringify(formData)
  })
  .then(response => {
    if (!response.ok) {
      throw new Error("Network response was not OK");
    }
    return response.json();
  })
  .then(data => {
    alert("Login successful!");
    localStorage.setItem("isLoggedIn", "true");
    redirectTo("home_ui.html");
  })
  .catch(error => {
    console.error("Error:", error);
    alert("Invalid Credentials!");
  });
}
window.login = login; // Expose login to the global scope for the inline onclick handler

// Initialize the UI after DOM is ready. Load tab-bar first so its
// elements (tab links) are present before updating visibility.
document.addEventListener("DOMContentLoaded", async () => {
  await loadComponent("tab-bar", "tab-bar.html");
  await initTabbar();
  updateUI(loginPageStateMachine);
});
