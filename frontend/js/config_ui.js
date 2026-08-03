function loadConfig() {
  if (localStorage.getItem("isLoggedIn") !== "true") {
    alert("Please log in to access configuration.");
    window.location.href = "login_ui.html";
    return;
  }

  fetch("http://fpc-webserver.local/config", {
    method: "GET",
    headers: {
      "Content-Type": "application/json"
    
    },
    credentials: "include"
  })
  .then(response => {
    if (!response.ok) {
      const err = new Error(`Failed to load configuration: ${response.statusText}`);
      err.status = response.status;
      throw err;

    }
    return response.json();
  })
  .then(data => {
    document.getElementById("configInput").value = JSON.stringify(data, null, 2);
    document.getElementById("status").textContent = "Configuration loaded successfully!";
  })
  .catch(error => {
    console.error("Error:", error);
    if(error.status === 401) {
      localStorage.removeItem("isLoggedIn");
      document.getElementById("status").textContent = "Unauthorized access. Please log in.";
      window.location.href = "login_ui.html";
      return;
    }
    document.getElementById("status").textContent = `Error loading configuration: ${error.message}`;
  });
}

function saveConfig() {
  if (localStorage.getItem("isLoggedIn") !== "true") {
    alert("Please log in to access configuration.");
    window.location.href = "login_ui.html";
    return;
  }

  const configInput = document.getElementById("configInput").value;
  let configData;
  try {
    configData = JSON.parse(configInput);
  } catch (error) {
    document.getElementById("status").textContent = "Invalid JSON format. Please check your input.";
    return;
  }

  fetch("http://fpc-webserver.local/config", {
    method: "POST",
    headers: {
      "Content-Type": "application/json"
    },
    credentials: "include", 
    body: JSON.stringify(configData)
  })
  .then(response => {
    if (!response.ok) {
      const err = new Error(`Failed to save configuration: ${response.statusText}`);
      err.status = response.status;
      throw err;
    }
    return response.json();
  })
  .then(data => {
    document.getElementById("status").textContent = "Configuration saved successfully!";
  })
  .catch(error => {
    console.error("Error:", error);
    if(error.status === 401) {
      localStorage.removeItem("isLoggedIn");
      document.getElementById("status").textContent = "Unauthorized access. Please log in.";
      window.location.href = "login_ui.html";
      return;
    }
    document.getElementById("status").textContent = `Error saving configuration: ${error.message}`;
  });
}

