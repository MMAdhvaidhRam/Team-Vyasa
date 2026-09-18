#include <WiFi.h>
#include <esp_now.h>
#include <Wire.h>
#include <math.h>

// Broadcast MAC address (Sends to any listening ESP32 nearby)
uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

typedef struct struct_message {
    float pitch;
    float yaw;
} struct_message;

struct_message outgoingData;
esp_now_peer_info_t peerInfo;

const int MPU_ADDR = 0x68; 
unsigned long lastTime = 0;
float currentYaw = 0.0;
float gyroZ_Error = 0.0; 

void setup() {
  Serial.begin(115200);
  
  // Set as Wi-Fi Station for ESP-NOW
  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }
  
  // Register the Receiver peer
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 0;  
  peerInfo.encrypt = false;
  esp_now_add_peer(&peerInfo);

  // Initialize MPU6500
  Wire.begin();
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x6B); 
  Wire.write(0);    
  Wire.endTransmission(true);
  
  // Auto-Calibration
  Serial.println("Calibrating Gyro... KEEP STILL!");
  delay(2000); 
  long errorSum = 0;
  for (int i = 0; i < 500; i++) {
    Wire.beginTransmission(MPU_ADDR);
    Wire.write(0x47);
    Wire.endTransmission(false);
    Wire.requestFrom(MPU_ADDR, 2, true);
    
    uint8_t highByte = Wire.read(), lowByte = Wire.read();
    errorSum += (highByte << 8) | lowByte;
    delay(3); 
  }
  gyroZ_Error = (errorSum / 500.0) / 131.0; 
  lastTime = millis();
}

void loop() {
  // 1. Read Accelerometer (Pitch)
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x3B); 
  Wire.endTransmission(false);
  Wire.requestFrom(MPU_ADDR, 6, true); 
  
  int16_t accX = (Wire.read() << 8) | Wire.read();
  int16_t accY = (Wire.read() << 8) | Wire.read();
  int16_t accZ = (Wire.read() << 8) | Wire.read();
  float currentPitch = atan2(-accX, sqrt(pow(accY, 2) + pow(accZ, 2))) * (180.0 / M_PI);

  // 2. Read Gyroscope (Yaw)
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x47);
  Wire.endTransmission(false);
  Wire.requestFrom(MPU_ADDR, 2, true);
  
  int16_t gyroZ_raw = (Wire.read() << 8) | Wire.read();
  
  unsigned long currentTime = millis();
  float dt = (currentTime - lastTime) / 1000.0; 
  lastTime = currentTime;

  float gyroZ_deg = (gyroZ_raw / 131.0) - gyroZ_Error;
  if (abs(gyroZ_deg) > 1.0) currentYaw -= (gyroZ_deg * dt); 

  // 3. Send over ESP-NOW
  outgoingData.pitch = currentPitch;
  outgoingData.yaw = currentYaw;
  esp_now_send(broadcastAddress, (uint8_t *) &outgoingData, sizeof(outgoingData));

  delay(50); // 20Hz Loop
}