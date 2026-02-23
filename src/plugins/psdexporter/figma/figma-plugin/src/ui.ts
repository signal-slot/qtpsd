// UI logic for PSD to Figma Importer plugin

const jsonInput = document.getElementById("jsonInput") as HTMLTextAreaElement;
const fileInput = document.getElementById("fileInput") as HTMLInputElement;
const fileName = document.getElementById("fileName") as HTMLSpanElement;
const importBtn = document.getElementById("importBtn") as HTMLButtonElement;
const cancelBtn = document.getElementById("cancelBtn") as HTMLButtonElement;

let loadedJson = "";

fileInput.addEventListener("change", () => {
  const file = fileInput.files?.[0];
  if (!file) return;

  fileName.textContent = file.name;
  const reader = new FileReader();
  reader.onload = () => {
    loadedJson = reader.result as string;
    jsonInput.value = loadedJson;
  };
  reader.readAsText(file);
});

importBtn.addEventListener("click", () => {
  const data = jsonInput.value.trim() || loadedJson;
  if (!data) {
    alert("Please paste JSON or upload a file.");
    return;
  }

  // Validate JSON
  try {
    JSON.parse(data);
  } catch {
    alert("Invalid JSON format. Please check the input.");
    return;
  }

  parent.postMessage({ pluginMessage: { type: "import", data } }, "*");
});

cancelBtn.addEventListener("click", () => {
  parent.postMessage({ pluginMessage: { type: "cancel" } }, "*");
});

// Allow drag & drop on textarea
jsonInput.addEventListener("dragover", (e) => {
  e.preventDefault();
  jsonInput.style.borderColor = "#18a0fb";
});

jsonInput.addEventListener("dragleave", () => {
  jsonInput.style.borderColor = "#ddd";
});

jsonInput.addEventListener("drop", (e) => {
  e.preventDefault();
  jsonInput.style.borderColor = "#ddd";

  const file = e.dataTransfer?.files[0];
  if (file) {
    fileName.textContent = file.name;
    const reader = new FileReader();
    reader.onload = () => {
      loadedJson = reader.result as string;
      jsonInput.value = loadedJson;
    };
    reader.readAsText(file);
  }
});
