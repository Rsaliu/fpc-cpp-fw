

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
  fetch("http://fpc-webserver.local/register", {
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