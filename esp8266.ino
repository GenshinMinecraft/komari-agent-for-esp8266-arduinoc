#include "komari.h"
#include <ESP8266WiFi.h>

const char* ssid = "Demo";
const char* password = "Demo";

void setup() {
  Serial.begin(115200);
  Serial.println("\n--- ESP8266 监控客户端 ---");

  WiFi.begin(ssid, password);
  Serial.print("正在连接.");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi 已连接");
  Serial.print("IP 地址: ");
  Serial.println(WiFi.localIP());

  if (setupKomari()) {
    Serial.println("[Komari] 初始化成功");
  } else {
    Serial.println("[Komari] 初始化失败");
  }
}

void loop() {
  agentPostOnce();
  
  Serial.println("Processing");

  delay(10);
}