#include "komari.h"
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>
#include <FS.h>

const char* server_host = "192.168.31.135";
const uint16_t server_port = 25774;
const char* token = "mUOZ7W8UGQXBgnMH";
const char* post_path = "/api/clients/uploadBasicInfo";
const char* ws_path = "/api/clients/report";

WebSocketsClient webSocket;
WiFiClient client;

bool basicInfoSentSuccessfully = false;
const unsigned long SEND_INTERVAL = 1000;
unsigned long previousMillis = 0;

uint32_t initialFreeHeap = 0;
unsigned long loopStartTime = 0;
unsigned long loopWorkTime = 0;
float cpuLoad = 0.0;

bool sendBasicInfo() {
  HTTPClient http;
  
  String url = String("http://") + server_host + ":" + server_port + post_path + "?token=" + token;
  Serial.print("[Komari-HTTP] 准备发送 POST 请求到: ");
  Serial.println(url);

  if (http.begin(client, url)) {
    http.addHeader("Content-Type", "application/json");

    StaticJsonDocument<512> doc;

    doc["arch"] = "xtensa";
    doc["cpu_cores"] = 1;
    doc["cpu_name"] = "Tensilica Diamond Standard 106Micro @ 80 MHz";
    doc["disk_total"] = ESP.getFlashChipSize();
    doc["gpu_name"] = "N/A";
    doc["ipv4"] = WiFi.localIP().toString();
    doc["ipv6"] = "N/A";
    doc["mem_total"] = initialFreeHeap;
    doc["os"] = "Arduino/ESP8266 SDK";
    doc["kernel_version"] = ESP.getSdkVersion();
    doc["swap_total"] = 0;
    doc["version"] = "3.1.2-esp8266";
    doc["virtualization"] = "None";
    
    String output;
    serializeJson(doc, output);
    Serial.println("[Komari-HTTP] 发送的 JSON: " + output);

    int httpCode = http.POST(output);

    if (httpCode > 0) {
      Serial.printf("[Komari-HTTP] POST 响应码: %d\n", httpCode);
      if (httpCode == HTTP_CODE_OK) {
        String payload = http.getString();
        Serial.println("[HTTP] 响应内容: " + payload);
        http.end();
        return true;
      }
    } else {
      Serial.printf("[Komari-HTTP] POST 请求失败: %s\n", http.errorToString(httpCode).c_str());
    }
    http.end();
  } else {
    Serial.println("[Komari-HTTP] 无法连接到服务器");
  }
  return false;
}

void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      Serial.println("[Komari-WS] 已断开连接!");
      break;
    case WStype_CONNECTED:
      Serial.printf("[Komari-WS] 已连接到 url: %s\n", (const char*)payload);
      Serial.println(">>> Komari Agent Websocket 推送已启动 <<<");
      break;
    case WStype_TEXT:
      Serial.printf("[Komari-WS] 收到文本: %s\n", (const char*)payload);
      break;
    default:
      break;
  }
}

void sendServerStatus() {
  uint32_t freeHeap = ESP.getFreeHeap();
  uint32_t usedHeap = initialFreeHeap > freeHeap ? initialFreeHeap - freeHeap : 0;
  
  cpuLoad = (float)loopWorkTime / (SEND_INTERVAL * 1000.0f) * 100.0f;
  if(cpuLoad > 100.0) cpuLoad = 100.0;

  StaticJsonDocument<512> doc;

  JsonObject cpu = doc.createNestedObject("cpu");
  cpu["usage"] = cpuLoad;

  JsonObject ram = doc.createNestedObject("ram");
  ram["total"] = initialFreeHeap;
  ram["used"] = usedHeap;

  JsonObject swap = doc.createNestedObject("swap");
  swap["total"] = 0;
  swap["used"] = 0;

  JsonObject load = doc.createNestedObject("load");
  load["load1"] = cpuLoad;
  load["load5"] = cpuLoad;
  load["load15"] = cpuLoad;

  JsonObject disk = doc.createNestedObject("disk");
  disk["total"] = ESP.getFlashChipSize();
  disk["used"] = ESP.getSketchSize();

  JsonObject network = doc.createNestedObject("network");
  network["up"] = 0;
  network["down"] = 0;
  network["totalUp"] = 0;
  network["totalDown"] = 0;

  JsonObject connections = doc.createNestedObject("connections");
  connections["tcp"] = 0;
  connections["udp"] = 0;

  doc["uptime"] = millis() / 1000;
  doc["process"] = 1;
  doc["message"] = "";

  String output;
  serializeJson(doc, output);
  Serial.println("[Komari-WS] 发送的 JSON: " + output);
  webSocket.sendTXT(output);
}

bool setupKomari() {
  initialFreeHeap = ESP.getFreeHeap();
  
  Serial.println("\n--- 发送基础信息 ---");
  if (sendBasicInfo()) {
    basicInfoSentSuccessfully = true;
    Serial.println("[OK] 基础信息发送成功!");
  } else {
    Serial.println("[错误] 基础信息发送失败, 请检查服务器或网络。");
    return false;
  }

  if (basicInfoSentSuccessfully) {
    Serial.println("\n--- 启动 WebSocket 状态推送 ---");
    String ws_url = String(ws_path) + "?token=" + token;
    webSocket.begin(server_host, server_port, ws_url.c_str());
    webSocket.onEvent(webSocketEvent);
    webSocket.setReconnectInterval(5000);
  }

  return true;
}

bool agentPostOnce() {
  loopStartTime = micros();

  if (basicInfoSentSuccessfully) {
    webSocket.loop();

    unsigned long currentMillis = millis();
    if (currentMillis - previousMillis >= SEND_INTERVAL) {
      previousMillis = currentMillis;
      if (webSocket.isConnected()) {
        sendServerStatus();
      }
    }
  } else {
    return false;
  }

  loopWorkTime = micros() - loopStartTime;

  return true;
}