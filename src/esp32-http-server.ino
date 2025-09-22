#include <WiFi.h>
#include <WebServer.h>
#include "RTClib.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <ESP32Servo.h>
#include <base64.h>
#include "photo.h"   // збережене фото у масиві

// ==== Servo pins ====
const int servoPin1 = 2;   // шасі
const int servoPin2 = 4;   // шасі
const int servoPin3 = 15;  // шасі
const int servoPin4 = 16;  // маніпулятор (рука)
const int servoPin5 = 5;   // маніпулятор (захват)

Servo servo1, servo2, servo3, servo4, servo5;

// ==== Wi-Fi ====
const char* SSID = "Wokwi-GUEST";
const char* password = "";
const char* openaiApiKey = "b453666eb3d04c1fa7c608375d88f7fc";

// ==== Peripherals ====
RTC_DS1307 rtc;
WebServer server(80);
String dataValue = "None";

// ==== Matrix (територія) ====
const int rows = 10;
const int cols = 10;
int territory[rows][cols] = {0};
int last_x = 0, last_y = 0;

// ==== Web UI ====  
String htmlPage() {
  String page = "<!DOCTYPE html><html><head><meta charset='UTF-8'><title>Robot Control</title>";
  page += "<style>";
  page += "table{border-collapse:collapse;}td{width:30px;height:30px;text-align:center;border:1px solid #333;}";
  page += ".done{background:green;color:white;}";
  page += ".todo{background:lightgray;}";
  page += "button{margin:5px;padding:10px 20px;}";
  page += "</style>";

  page += "</head><body>";
  page += "<h2>🤖 Smart Demining Robot</h2>";

  // Матриця
  page += "<h3>Territory Map</h3><table>";
  for (int i = 0; i < rows; i++) {
    page += "<tr>";
    for (int j = 0; j < cols; j++) {
      if (i == last_x && j == last_y) {
        page += "<td style='background:orange;'>R</td>";  // Робот
      } else if (territory[i][j] == 1) {
        page += "<td class='done'>1</td>";
      } else {
        page += "<td class='todo'>0</td>";
      }
    }
    page += "</tr>";
  }
  page += "</table>";

  // Кнопки управління рухом
  page += "<h3>Movement</h3>";
  page += "<form action='/forward'><button>⬆️ Forward</button></form>";
  page += "<form action='/left'><button>⬅️ Left</button></form>";
  page += "<form action='/right'><button>➡️ Right</button></form>";
  page += "<form action='/backward'><button>⬇️ Backward</button></form>";
  page += "<form action='/stop'><button>🛑 Stop</button></form>";

  // Кнопки маніпулятора
  page += "<h3>Manipulator</h3>";
  page += "<form action='/open'><button>🔓 Open</button></form>";
  page += "<form action='/close'><button>🔒 Close</button></form>";

  page += "</body></html>";
  return page;
}


// ==== Web handlers ====  
void handleRoot() { server.send(200, "text/html", htmlPage()); }
void handleForward() {
  int new_x = last_x + 1;  // рух вниз по матриці
  int new_y = last_y;
  moveToCell(new_x, new_y);
  server.send(200, "text/html", htmlPage());
}

void handleBackward() {
  int new_x = last_x - 1;  // рух вгору
  int new_y = last_y;
  moveToCell(new_x, new_y);
  server.send(200, "text/html", htmlPage());
}

void handleLeft() {
  int new_x = last_x;
  int new_y = last_y - 1;  // рух вліво
  moveToCell(new_x, new_y);
  server.send(200, "text/html", htmlPage());
}

void handleRight() {
  int new_x = last_x;
  int new_y = last_y + 1;  // рух вправо
  moveToCell(new_x, new_y);
  server.send(200, "text/html", htmlPage());
}

void handleStop() { stopMove(); server.send(200, "text/html", htmlPage()); }
void handleOpen() { openGripper(); server.send(200, "text/html", htmlPage()); }
void handleClose() { closeGripper(); server.send(200, "text/html", htmlPage()); }

// ==== Matrix helpers ====
String getMatrix() {
  String str_arr = "";
  Serial.println("Стан території:");
  for (int i = 0; i < rows; i++) {
    str_arr += "(";
    for (int j = 0; j < cols; j++) {
      str_arr += String(territory[i][j]) + " ";
    }
    str_arr += ")\n";
  }
  return str_arr;
}

void markProcessed(int x, int y) {
  if (x >= 0 && x < rows && y >= 0 && y < cols) {
    territory[x][y] = 1;
    last_x = x;
    last_y = y;
    Serial.printf("Клітинка (%d,%d) позначена як оброблена.\n", x, y);
  }
}
void moveToCell(int new_x, int new_y) {
  int dx = new_x - last_x;
  int dy = new_y - last_y;

  if (dx == 1 && dy == 0) {
    Serial.println("Рух вниз");
    moveForward();
    delay(1000);
    stopMove();
  } 
  else if (dx == -1 && dy == 0) {
    Serial.println("Рух вгору");
    moveBackward();
    delay(1000);
    stopMove();
  } 
  else if (dx == 0 && dy == 1) {
    Serial.println("Рух вправо");
    turnRight();
    delay(500);
    moveForward();
    delay(1000);
    stopMove();
  } 
  else if (dx == 0 && dy == -1) {
    Serial.println("Рух вліво");
    turnLeft();
    delay(500);
    moveForward();
    delay(1000);
    stopMove();
  } 
  else {
    Serial.println("Невідомий рух або стоїмо на місці");
  }

  // після руху оновлюємо матрицю
  markProcessed(new_x, new_y);
}

// ==== GPT Request (matrix only) ====
void request_gpt() {
  DynamicJsonDocument doc(2048);
  doc["model"] = "gpt-3.5-turbo";
  JsonArray messages = doc.createNestedArray("messages");

  JsonObject sys = messages.createNestedObject();
  sys["role"] = "system";
  sys["content"] = "Smart demining robot. Explore matrix grid step by step.";

  JsonObject user = messages.createNestedObject();
  user["role"] = "user";
  user["content"] =
    "Matrix:\n" + getMatrix() +
    "\nLast position: (" + String(last_x) + "," + String(last_y) + ")\n"
    "Write next step in format (x,y)";

  HTTPClient http;
  String apiUrl = "https://artificialintelligence.openai.azure.com/openai/deployments/test/chat/completions?api-version=2023-05-15";
  http.begin(apiUrl);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("api-key", String(openaiApiKey));

  String requestBody;
  serializeJson(doc, requestBody);
  int httpCode = http.POST(requestBody);

  if (httpCode == 200) {
    String response = http.getString();
    deserializeJson(doc, response);
    String content = doc["choices"][0]["message"]["content"].as<String>();
    Serial.println("GPT reply: " + content);

    content.replace("(", "");
    content.replace(")", "");
    int commaIndex = content.indexOf(',');
    int x = content.substring(0, commaIndex).toInt();
    int y = content.substring(commaIndex + 1).toInt();
    moveToCell(x, y);
    
  } else {
    Serial.println("GPT request failed");
  }
  http.end();
}

void request_gpt_with_photo(const String& prompt) {
  DynamicJsonDocument doc(8192);  // збільшив буфер для великої картинки
  doc["model"] = "gpt-4o-mini";

  JsonArray messages = doc.createNestedArray("messages");

  JsonObject sys = messages.createNestedObject();
  sys["role"] = "system";
  sys["content"] = "You are a demining robot. Analyze photos for dangerous objects.";

  JsonObject user = messages.createNestedObject();
  user["role"] = "user";
  JsonArray userContent = user.createNestedArray("content");

  JsonObject textPart = userContent.createNestedObject();
  textPart["type"] = "text";
  textPart["text"] = prompt;

  JsonObject imagePart = userContent.createNestedObject();
  imagePart["type"] = "image_url";
  JsonObject imageUrl = imagePart.createNestedObject("image_url");
  imageUrl["url"] = String("data:image/png;base64,") + photo_base64;

  HTTPClient http;
  String apiUrl = "https://artificialintelligence.openai.azure.com/openai/deployments/test/chat/completions?api-version=2023-05-15";
  http.begin(apiUrl);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("api-key", String(openaiApiKey));

  String requestBody;
  serializeJson(doc, requestBody);

  int httpCode = http.POST(requestBody);

  if (httpCode == 200) {
    String response = http.getString();
    Serial.println("GPT response:");
    Serial.println(response);
  } else {
    Serial.print("Error sending photo request, code=");
    Serial.println(httpCode);
    Serial.println(http.getString());
  }
  http.end();
}


// ==== Robot movements ====
void moveForward() { servo1.write(90); servo2.write(90); servo3.write(90); }
void moveBackward() { servo1.write(0); servo2.write(0); servo3.write(0); }
void turnLeft() { servo1.write(0); servo2.write(90); servo3.write(90); }
void turnRight() { servo1.write(90); servo2.write(0); servo3.write(90); }
void stopMove() { servo1.write(90); servo2.write(90); servo3.write(90); }

void openGripper() { servo4.write(0); servo5.write(0); }
void closeGripper() { servo4.write(90); servo5.write(90); }

// ==== Setup ====
void setup() {
  Serial.begin(115200);
  servo1.attach(servoPin1);
  servo2.attach(servoPin2);
  servo3.attach(servoPin3);
  servo4.attach(servoPin4);
  servo5.attach(servoPin5);

  WiFi.begin(SSID, password);
  while (WiFi.status() != WL_CONNECTED) delay(1000);
  Serial.println("WiFi connected: " + WiFi.localIP().toString());

  server.on("/", handleRoot);
  server.on("/forward", handleForward);
  server.on("/backward", handleBackward);
  server.on("/left", handleLeft);
  server.on("/right", handleRight);
  server.on("/stop", handleStop);
  server.on("/open", handleOpen);
  server.on("/close", handleClose);
  server.begin();

  rtc.begin();
  memset(territory, 0, sizeof(territory));
}

// ==== Loop ====
int counter = 0;
void loop() {
  server.handleClient();
  DateTime now = rtc.now();
  dataValue = "Time: " + String(now.hour()) + ":" + String(now.minute()) + ":" + String(now.second());

if (counter < 2) {
  request_gpt();
  request_gpt_with_photo("Check area for mines or bombs.");
  counter++;
}

  delay(2000);
}
