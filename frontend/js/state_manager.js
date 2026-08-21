import { isLoggedIn, redirectTo } from "./utils.js";

// homepage state management 

export function homePageStateMachine(loginState) {
    if (loginState) {
        // User is logged in, show the config tab and logout tab
        setDisplay("configTabLink", "inline-block");
        setDisplay("logoutTabLink", "inline-block");
        setDisplay("homeTabLink", "none");
        setDisplay("loginTabLink", "none");
        setDisplay("registerTabLink", "none");
    } else {
        // User is not logged in, show the login and register tabs
        setDisplay("configTabLink", "none");
        setDisplay("logoutTabLink", "none");
        setDisplay("homeTabLink", "none");
        setDisplay("loginTabLink", "inline-block");
        setDisplay("registerTabLink", "inline-block");
    }
}

// login page state management

export function loginPageStateMachine(loginState) {
    if (loginState) {
        // User is logged in, redirect to home page
        redirectTo("home_ui.html");
    }else {
        // User is not logged in, show the login and register tabs
        setDisplay("configTabLink", "none");
        setDisplay("logoutTabLink", "none");
        setDisplay("loginTabLink", "none");
        setDisplay("registerTabLink", "inline-block");
        setDisplay("homeTabLink", "inline-block");
    }  
}

// config page state management

export function configPageStateMachine(loginState) {
    if (loginState) {
        // User is logged in, show the config tab and logout tab
        setDisplay("homeTabLink", "inline-block");
        setDisplay("logoutTabLink", "inline-block");
        setDisplay("configTabLink", "none");
        setDisplay("loginTabLink", "none");
        setDisplay("registerTabLink", "none");
    }else{
        // User is not logged in, redirect to login page
        redirectTo("login_ui.html");
    }
}

// register page state management

export function registerPageStateMachine(loginState) {
    if (loginState) {
        // User is logged in, redirect to home page
        redirectTo("home_ui.html");
    }else {
        // User is not logged in, show the login and register tabs
        setDisplay("configTabLink", "none");
        setDisplay("logoutTabLink", "none");
        setDisplay("loginTabLink", "inline-block");
        setDisplay("registerTabLink", "none");
        setDisplay("homeTabLink", "inline-block");
    }
}

function setDisplay(id, value) {
    const el = document.getElementById(id);
    if (!el) return;
    el.style.display = value;
}