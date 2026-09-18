#include <WiFi.h>
#include <esp_now.h>

// Structure to receive data (Must match the Transmitter)
typedef struct struct_message {
    float pitch;
    float yaw;
} struct_message;

struct_message incomingData;

// FIX: Updated callback signature for ESP32 Core Version 3.x.x
void OnDataRecv(const esp_now_recv_info *info, const uint8_t *incomingDataPtr, int len) {
  memcpy(&incomingData, incomingDataPtr, sizeof(incomingData));
  
  // Format as JSON and send directly over USB Serial to Python
  Serial.print("{\"id\":\"Truck_A\", \"pitch\":");
  Serial.print(incomingData.pitch);
  Serial.print(", \"yaw\":");
  Serial.print(incomingData.yaw);
  Serial.println("}");
}

void setup() {
  Serial.begin(115200);
  
  // Set ESP32 as a Wi-Fi Station (Required for ESP-NOW)
  WiFi.mode(WIFI_STA);

  // Initialize ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }
  
  // Register the receive callback
  esp_now_register_recv_cb(OnDataRecv);
}

void loop() {
  // The ESP32 does everything in the background callback.
  delay(1000); 
}