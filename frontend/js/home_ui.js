function redirectTo(page) {

  window.location.href = page;
}

function logout() {
  localStorage.removeItem("isLoggedIn");
  window.location.href = "home_ui.html";
}

function updateUI() {
  const isLoggedIn = localStorage.getItem("isLoggedIn") === "true";

  const loginTabLink = document.getElementById("loginTabLink");
  const registerTabLink = document.getElementById("registerTabLink");
  const configTabLink = document.getElementById("configTabLink");
  const logoutTabLink = document.getElementById("logoutTabLink");
  const homeTabLink = document.getElementById("homeTabLink");



  if (isLoggedIn) {
    loginTabLink.style.display = "none";
    registerTabLink.style.display = "none";
    configTabLink.style.display = "inline-block";
    homeTabLink.style.display = "inline-block";
    logoutTabLink.style.display = "inline-block";


  } else {
    loginTabLink.style.display = "inline-block";
    registerTabLink.style.display = "inline-block";
    configTabLink.style.display = "none";
    logoutTabLink.style.display = "none";
    homeTabLink.style.display = "none";

  }
}

// Initialize the UI
document.addEventListener("DOMContentLoaded", updateUI);


