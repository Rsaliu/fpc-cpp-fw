function login(event) {
  event.preventDefault();
  const form = document.getElementById("loginForm");
  const formData = {
    username: form.username.value.trim(),
    password: form.pwd.value
  };

  fetch("http://fpc-webserver.local/login", {
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
    window.location.href = "home_ui.html";
  })
  .catch(error => {
    console.error("Error:", error);
    alert("Invalid Credentials!");
  });
}