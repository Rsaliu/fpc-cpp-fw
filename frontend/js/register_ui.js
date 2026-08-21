import { isLoggedIn,redirectTo } from './utils.js';
import { logout,updateUI,API_URL,loadComponent } from './common.js';
import { registerPageStateMachine } from './state_manager.js';
import { initTabbar } from './tab-bar.js';
window.redirectTo = redirectTo; // Expose redirectTo to the global scope
window.logout = logout; // Expose logout to the global scope

const REGISTER_URL = `${API_URL}/register`;

function register(event) {
  event.preventDefault();
  const form = document.getElementById("registerForm");
  const formData = {
    username: form.username.value.trim(),
    password1: form.pwd1.value,
    password2: form.pwd2.value
  };

  if (formData.password1 !== formData.password2) {
    alert("Passwords do not match.");
    console.log("Passwords do not match.");
    return;
  }
  console.log("will send data, passwords match");
  fetch(REGISTER_URL, {
    method: "POST",
    headers: {
      "Content-Type": "application/json"
    },
    body: JSON.stringify(formData)
  })
  .then(response => {
    if (!response.ok) {
      throw new Error("Network response was not OK");
    }
    return response.json();
  })
  .then(data => {
    alert("Registration successful!");
    window.location.href = "home_ui.html";
  })
  .catch(error => {
    console.error("Error:", error);
    alert("Registration failed!");
  });
}

// Initialize the UI after DOM is ready. Load tab-bar first so its
// elements (tab links) are present before updating visibility.
document.addEventListener("DOMContentLoaded", async () => {
  await loadComponent("tab-bar", "tab-bar.html");
  await initTabbar();
  updateUI(registerPageStateMachine);
});
