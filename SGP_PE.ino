#include <Wire.h>
#include <Adafruit_INA219.h>
#include <WiFi.h>
#include <PubSubClient.h>

// --- Pins ---
#define MOSFET_GATE_PIN 5
#define FAULT_SWITCH_PIN 4

// --- WiFi & MQTT Settings ---
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";
const char* mqtt_server = "YOUR_MQTT_BROKER_IP"; // e.g., 192.168.1.50
const char* mqtt_user = "admin";
const char* mqtt_password = "admin123";

WiFiClient espClient;
PubSubClient client(espClient);

// --- MQTT Topics ---
const char* TOPIC_TELEMETRY = "smarthub/telemetry";
const char* TOPIC_COMMAND   = "smarthub/command/reset";
const char* TOPIC_FEEDBACK  = "smarthub/feedback";

// --- IDMT Settings ---
const float I_SET = 0.55; // Slightly higher pickup to ignore noise
const float TMS = 0.10;   
const float CURVE_A = 0.14;
const float CURVE_B = 0.02;

// --- Global Variables ---
Adafruit_INA219 ina219;
float current_A = 0;
bool isTripped = false;
bool faultActive = false;
unsigned long faultStartTime = 0;
String tripReason = "";

// IDMT Calculation
float calculateTripTime(float current) {
  if (current <= I_SET) return 0;
  float ratio = current / I_SET;
  return TMS * (CURVE_A / (pow(ratio, CURVE_B) - 1.0));
}

// MQTT Callback Function
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String message;
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  
  Serial.printf("Topic Arrival: [%s] | Msg: [%s]\n", topic, message.c_str());

  if (String(topic) == TOPIC_COMMAND && message == "RESET") {
    // Read switch state multiple times for stability
    int pinValue = digitalRead(FAULT_SWITCH_PIN);
    Serial.printf(">>> PROCESSING RESET. Switch Pin Reading: %s\n", (pinValue == HIGH ? "HIGH (FAULT)" : "LOW (NORMAL)"));

    // --- SAFETY INTERLOCK ---
    if (pinValue == HIGH) {
      Serial.println("!!! REMOTE RESET BLOCKED: HARDWARE FAULT STILL ACTIVE !!!");
      client.publish(TOPIC_FEEDBACK, "REJECTED: CLEAR HARDWARE FAULT");
      return; 
    }

    Serial.println("--- REMOTE RESET EXECUTED: RESTORING POWER ---");
    client.publish(TOPIC_FEEDBACK, "RESET OK: MOTOR RESTARTED");
    isTripped = false;
    faultActive = false;
    tripReason = "";
    digitalWrite(MOSFET_GATE_PIN, HIGH); 
  }
}

// MQTT Reconnect Function
void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect with authentication
    if (client.connect("ESP32Client", mqtt_user, mqtt_password)) {
      Serial.println("connected");
      // Subscribe to command topic
      client.subscribe(TOPIC_COMMAND);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      vTaskDelay(5000 / portTICK_PERIOD_MS); // Use vTaskDelay instead of delay in tasks
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000); 
  Serial.println("\n--- RELAY BOOT: OFFICIAL RELEASE (V1.0) ---");
  
  // 1. Initialize Hardware First
  pinMode(MOSFET_GATE_PIN, OUTPUT);
  pinMode(FAULT_SWITCH_PIN, INPUT_PULLDOWN); 
  digitalWrite(MOSFET_GATE_PIN, HIGH); 

  Wire.begin(21, 22); 
  if (!ina219.begin()) {
    Serial.println("CRITICAL: INA219 sensor not found!");
  }

  // 2. Focused WiFi Connection (Wait until OK)
  Serial.print("WiFi: Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi: OK! Connected.");

  // 3. Start SGP Logic Tasks
  xTaskCreatePinnedToCore(protectionLogic, "ProtTask", 4096, NULL, 1, NULL, 0);
  Serial.println("Protection Logic: ACTIVE (Core 0)");

  xTaskCreatePinnedToCore(telemetryLogic, "TeleTask", 4096, NULL, 1, NULL, 1);
  Serial.println("Telemetry & MQTT: ACTIVE (Core 1)");
}

void protectionLogic(void * pvParameters) {
  for (;;) {
    if (!isTripped) {
      // 1. READ SENSOR
      float current_mA = ina219.getCurrent_mA();
      
      if (current_mA > 10000 || current_mA < -5000) {
        // Simple Serial warning if sensor glitches
        static unsigned long lastError = 0;
        if (millis() - lastError > 1000) {
          Serial.println("I2C Data Out of Range - Attempting Recovery...");
          lastError = millis();
        }
        Wire.begin(21, 22);
        vTaskDelay(10 / portTICK_PERIOD_MS);
        continue;
      }
      
      current_A = abs(current_mA / 1000.0); 

      // 2. CHECK FAULTS
      bool physicalFault = (current_A > I_SET);
      bool switchReading = (digitalRead(FAULT_SWITCH_PIN) == HIGH);
      
      static int switchCounter = 0;
      if (switchReading) {
        switchCounter++;
      } else {
        switchCounter = 0;
      }
      
      bool simulatedFault = (switchCounter > 5); 

      if (physicalFault || simulatedFault) {
        if (!faultActive) {
          faultStartTime = millis();
          faultActive = true;
          tripReason = physicalFault ? "REAL OVERCURRENT" : "SIMULATED FAULT (SWITCH)";
          Serial.print(">>> FAULT DETECTED: "); Serial.println(tripReason);
        }

        float effectiveCurrent = simulatedFault ? (I_SET * 5.0) : current_A;
        float tripDelaySec = calculateTripTime(effectiveCurrent);
        
        if ((millis() - faultStartTime) >= (tripDelaySec * 1000)) {
          digitalWrite(MOSFET_GATE_PIN, LOW); // TRIP!
          isTripped = true;
          Serial.println("--- RELAY TRIPPED! POWER CUT. ---");
        }
      } else {
        faultActive = false;
      }
    }
    vTaskDelay(20 / portTICK_PERIOD_MS); 
  }
}

void telemetryLogic(void * pvParameters) {
  client.setServer(mqtt_server, 1883);
  client.setCallback(mqttCallback);

  for (;;) {
    if (WiFi.status() == WL_CONNECTED) {
      if (!client.connected()) {
        reconnect();
      }
      client.loop(); 
    }

    // --- Periodic Telemetry Status ---
    String stateMsg = "NORMAL";
    if (isTripped) {
      stateMsg = String("TRIPPED: ") + tripReason;
    } else if (faultActive) {
      stateMsg = "FAULT DETECTED";
    }

    Serial.printf("[Tele] Current: %.3f A | State: %s | WiFi: %s\n", 
                  current_A, stateMsg.c_str(), 
                  (WiFi.status() == WL_CONNECTED ? "OK" : "Connecting..."));

    // Publish to MQTT
    if (client.connected()) {
      String payload = "{\"current_A\":" + String(current_A, 3) + ", \"state\":\"" + stateMsg + "\"}";
      client.publish(TOPIC_TELEMETRY, payload.c_str());
    }

    vTaskDelay(1000 / portTICK_PERIOD_MS); 
  }
}

void loop() {}