#include "FS.h"
#include "SD_MMC.h"
#include "WiFi.h"
#include "WebServer.h"
#include "esp_camera.h"
const char* fileNameCounterPath = "/filecounter.txt";

// WiFi settings
const char* ssid = "GT-AX11000";
const char* password = "0901230528";

// Create a WebServer object listening on HTTP port 80
WebServer server(80);

// Camera pins configuration
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

// Function declarations
void handleRoot();
void captureImage();
void handleFileList();
void handleFileView();
void handleFileDelete();
int getFileNumber();
void saveFileNumber(int fileNumber);

void setup() {
  Serial.begin(115200);

  // Connect to WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("WiFi connected");

  // Print the IP address
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  // Initialize SD card
  if (!SD_MMC.begin()) {
    Serial.println("SD card initialization failed");
    return;
  }
  Serial.println("SD card initialized");

  // Initialize file counter if not exists
  if (!SD_MMC.exists(fileNameCounterPath)) {
    File counterFile = SD_MMC.open(fileNameCounterPath, FILE_WRITE);
    if (counterFile) {
      counterFile.println("0");
      counterFile.close();
    } else {
      Serial.println("Failed to create file counter");
    }
  }

  // Initialize camera
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;  // Updated deprecated variable
  config.pin_sccb_scl = SIOC_GPIO_NUM;  // Updated deprecated variable
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  if (psramFound()) {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }

  // Initialize camera
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera initialization failed with error 0x%x", err);
    return;
  }

  // Set up server routes
  server.on("/", HTTP_GET, handleRoot);
  server.on("/capture", HTTP_GET, captureImage);
  server.on("/list", HTTP_GET, handleFileList);
  server.on("/view", HTTP_GET, handleFileView);
  server.on("/delete", HTTP_GET, handleFileDelete);

  server.begin();
  Serial.println("Web server started");
}
  
void loop() {
  server.handleClient();
}

void handleRoot() {
  String html = "<html><head><style>";
  html += "body { font-family: Arial, sans-serif; text-align: center; background-color: #f4f4f9; margin: 0; padding: 0; }";
  html += "h1 { color: #333; margin: 20px 0; }";
  html += "button { padding: 10px 20px; margin: 10px; background-color: #4CAF50; color: white; border: none; border-radius: 5px; cursor: pointer; font-size: 16px; }";
  html += "button:hover { background-color: #45a049; }";
  html += "img { max-width: 100%; height: auto; margin-top: 20px; }";
  html += "ul { list-style-type: none; padding: 0; }";
  html += "li { padding: 5px 0; }";
  html += "a { text-decoration: none; color: #007BFF; }";
  html += "a:hover { text-decoration: underline; }";
  html += "header { background-color: #333; color: white; padding: 10px 0; }";
  html += ".container { width: 80%; margin: 0 auto; padding: 20px; background-color: white; box-shadow: 0 0 10px rgba(0, 0, 0, 0.1); border-radius: 10px; }";
  html += ".file-list { text-align: left; margin-top: 20px; }";
  html += ".file-list h2 { margin-bottom: 10px; }";
  html += ".file-list button { display: block; margin: 10px auto; }";
  html += ".file-list ul { margin-top: 10px; }";
  html += ".file-list li { border-bottom: 1px solid #ccc; padding: 8px 0; }";
  html += ".preview-container { text-align: center; margin-top: 20px; }";
  html += ".preview-buttons { margin-top: 10px; }";
  html += ".preview-buttons button, .preview-buttons a { margin: 5px; display: inline-block; }";
  html += "</style></head><body>";
  html += "<header><h1>ESP32-CAM Control Page</h1></header>";
  html += "<div class='container'>";
  html += "<button onclick=\"captureImage()\">Capture</button>";
  html += "<div class='preview-container' id='image-container'></div>";
  html += "<div class='file-list'>";
  html += "<h2>SD Card File List</h2>";
  html += "<button onclick=\"listFiles()\">Refresh List</button>";
  html += "<ul id=\"fileList\"></ul>";
  html += "</div>";
  html += "</div>";
  html += "<script>";
  html += "function captureImage() {";
  html += "  fetch('/capture').then(response => response.blob()).then(blob => {";
  html += "    var img = document.createElement('img');";
  html += "    img.src = URL.createObjectURL(blob);";
  html += "    var container = document.getElementById('image-container');";
  html += "    container.innerHTML = '';";
  html += "    container.appendChild(img);";
  html += "    listFiles();";
  html += "  }).catch(error => console.error('Error:', error));";
  html += "}";
  html += "function listFiles() {";
  html += "  fetch('/list').then(response => response.text()).then(data => {";
  html += "    const fileList = document.getElementById('fileList');";
  html += "    fileList.innerHTML = '';";
  html += "    const files = data.split('\\n');";
  html += "    files.forEach(file => {";
  html += "      if (file) {";
  html += "        const listItem = document.createElement('li');";
  html += "        const link = document.createElement('a');";
  html += "        link.href = '#';";
  html += "        link.textContent = decodeURIComponent(file);";
  html += "        link.onclick = () => viewFile(file);";
  html += "        listItem.appendChild(link);";
  html += "        fileList.appendChild(listItem);";
  html += "      }";
  html += "    });";
  html += "  }).catch(error => console.error('Error:', error));";
  html += "}";
  html += "function viewFile(fileName) {";
  html += "  fetch('/view?name=' + encodeURIComponent(fileName)).then(response => response.blob()).then(blob => {";
  html += "    var img = document.createElement('img');";
  html += "    img.src = URL.createObjectURL(blob);";
  html += "    var container = document.getElementById('image-container');";
  html += "    container.innerHTML = '';";
  html += "    container.appendChild(img);";
  html += "    var buttonsContainer = document.createElement('div');";
  html += "    buttonsContainer.className = 'preview-buttons';";
  html += "    var deleteButton = document.createElement('button');";
  html += "    deleteButton.textContent = 'Delete Photo';";
  html += "    deleteButton.onclick = () => deleteFile(fileName);";
  html += "    buttonsContainer.appendChild(deleteButton);";
  html += "    var downloadLink = document.createElement('a');";
  html += "    downloadLink.href = URL.createObjectURL(blob);";
  html += "    downloadLink.download = fileName;";
  html += "    downloadLink.textContent = 'Download Photo';";
  html += "    buttonsContainer.appendChild(downloadLink);";
  html += "    container.appendChild(buttonsContainer);";
  html += "  }).catch(error => console.error('Error:', error));";
  html += "}";
  html += "function deleteFile(fileName) {";
  html += "  fetch('/delete?name=' + encodeURIComponent(fileName)).then(response => {";
  html += "    listFiles();";
  html += "    document.getElementById('image-container').innerHTML = '';";
  html += "  }).catch(error => console.error('Error:', error));";
  html += "}";
  html += "</script></body></html>";
  server.send(200, "text/html", html);
}

void captureImage() {
  camera_fb_t * fb = NULL;
  fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Camera capture failed");
    server.send(500, "text/plain", "Camera capture failed");
    return;
  }

  // Get the current file number
  int fileNumber = getFileNumber();
  if (fileNumber < 0) {
    server.send(500, "text/plain", "Failed to read file counter");
    return;
  }

  // Create the file path
  String path = "/picture" + String(fileNumber) + ".jpg";
  
  // Open the file on SD card
  File file = SD_MMC.open(path, FILE_WRITE);
  if (!file) {
    Serial.println("Failed to open file");
    server.send(500, "text/plain", "Failed to open file");
    return;
  }

  // Write the image data to the file
  file.write(fb->buf, fb->len);
  file.close();
  esp_camera_fb_return(fb);

  // Increment and save the file number
  saveFileNumber(fileNumber + 1);

  server.send_P(200, "image/jpeg", (const char *)fb->buf, fb->len);
}

int getFileNumber() {
  File counterFile = SD_MMC.open(fileNameCounterPath);
  if (!counterFile) {
    Serial.println("Failed to open file counter");
    return -1;
  }

  String fileNumberStr = counterFile.readStringUntil('\n');
  counterFile.close();

  return fileNumberStr.toInt();
}

void saveFileNumber(int fileNumber) {
  File counterFile = SD_MMC.open(fileNameCounterPath, FILE_WRITE);
  if (counterFile) {
    counterFile.println(fileNumber);
    counterFile.close();
  } else {
    Serial.println("Failed to open file counter for writing");
  }
}

void handleFileList() {
  String fileList = "";
  File root = SD_MMC.open("/");
  File file = root.openNextFile();
  while (file) {
    if (!file.isDirectory()) {
      fileList += String(file.name()) + "\n";
    }
    file = root.openNextFile();
  }
  server.send(200, "text/plain", fileList);
}

void handleFileView() {
  if (server.hasArg("name")) {
    String fileName = server.arg("name");
    File file = SD_MMC.open("/" + fileName);
    if (file) {
      server.streamFile(file, "image/jpeg");
      file.close();
    } else {
      server.send(404, "text/plain", "File not found");
    }
  } else {
    server.send(400, "text/plain", "Missing file name parameter");
  }
}

void handleFileDelete() {
  if (server.hasArg("name")) {
    String fileName = server.arg("name");
    if (SD_MMC.remove("/" + fileName)) {
      server.send(200, "text/plain", "File deleted");
    } else {
      server.send(404, "text/plain", "Failed to delete file");
    }
  } else {
    server.send(400, "text/plain", "Missing file name parameter");
  }
}
