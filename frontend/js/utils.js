export function isLoggedIn() {
  return localStorage.getItem("isLoggedIn") === "true";
}  

export function redirectTo(page) {
  window.location.href = page;
}