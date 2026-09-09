import { isLoggedIn, redirectTo } from './utils.js';
export const API_URL = "";
export function logout() {
  localStorage.removeItem("isLoggedIn");
  redirectTo("home_ui.html");
}

export function updateUI(callback) {
  const loginState = isLoggedIn();
  callback(loginState);
}

export async function loadComponent(id, path) {
    const response = await fetch(path);
    const html = await response.text();

  const container = document.getElementById(id);
  if (!container) {
    console.error(`loadComponent: element with id "${id}" not found`);
    return;
  }
  container.innerHTML = html;
}