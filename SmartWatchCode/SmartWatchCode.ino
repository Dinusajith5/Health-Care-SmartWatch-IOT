
#include <Wire.h>
#include <MPU6050.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <math.h>
#include <ArduinoJson.h>

// OLED display size
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

MPU6050 mpu;
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

char ssid[] = "Dinu S";
char pass[] = "123456790";

// MQTT server details
const char* mqtt_server = "broker.hivemq.com";
const char* mqtt_step_topic = "esp32/step_topic";
const char* mqtt_incident_alert = "esp32/Incident_Alert";
const char* mqtt_walking_pattern = "esp32/walking_pattern";
const char* mqtt_ML_pattern = "esp32/walking_pattern_ML";


// ESP32 MQTT client
WiFiClient espClient;
PubSubClient client(espClient);
int stepCount = 0;
float previousGyroX = 0.0;
float threshold = 150.0; // Adjust based on testing
bool stepDetected = false;

// Incident alert
const int MPU_addr = 0x68;
int16_t AcX, AcY, AcZ, Tmp, GyX, GyY, GyZ;
float ax = 0, ay = 0, az = 0, gx = 0, gy = 0, gz = 0;
boolean fall = false;
boolean trigger1 = false;
boolean trigger2 = false;
boolean trigger3 = false;
byte trigger1count = 0;
byte trigger2count = 0;
byte trigger3count = 0;
int angleChange = 0;

//walking pattern

void connectWiFi() {
  Serial.print("Connecting to WiFi...");
  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("Connected to WiFi");
}
void reconnectMQTT() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect("ESP32_StepCounter")) {
      Serial.println("Connected to MQTT broker");
    } else {
      Serial.print("Failed, rc=");
      Serial.print(client.state());
      Serial.println(" trying again in 5 seconds");
      delay(5000);
    }
  }
}
///////////////////////welcome note/////////////////////

void welcomenote() {
  // Display "Welcome" first
  display.clearDisplay();
  display.setTextSize(2);  // Make the text larger
  display.setCursor(0, 0);
  display.print("Welcome");
  display.display();  // Show "Welcome" on the screen
  delay(1500);        // Wait for 1.5 seconds

  // Clear the screen after "Welcome" message
  display.clearDisplay();
}

void displayBootingScreenAndLoadingBar() {
  // Display "Booting..." message
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("Booting...");
  display.display();  // Show the "Booting..." text

  // Show the loading bar
  for (int i = 0; i <= 100; i += 10) {
    // Clear the loading bar area only (so "Booting..." remains)
    display.fillRect(0, 40, SCREEN_WIDTH, 10, SSD1306_BLACK); 
    display.fillRect(0, 40, (i * SCREEN_WIDTH) / 100, 10, SSD1306_WHITE);  // Draw the loading bar
    display.display();
    delay(200);  // Delay for the loading effect
  }

  delay(1000);  // Hold the screen for 1 second after the loading finishes
  display.clearDisplay();  // Finally, clear the screen
}
/////////////////////////////end of welcome note////////////

void setup() {
  Serial.begin(9600);
  Wire.begin(21, 22); 
  mpu.initialize();
  connectWiFi();
  client.setServer(mqtt_server, 1883);
  
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;);
  }

  welcomenote();  // Show the welcome message
  displayBootingScreenAndLoadingBar();  // Show booting with loading bar
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  if (mpu.testConnection()) {
    Serial.println("MPU6050 connection successful");
  } else {
    Serial.println("MPU6050 connection failed");
  }
  
  displayStepCount();
}
void displayStepCount() {
  display.clearDisplay();  // Clear only once before displaying step count
  display.setTextSize(2);
  display.setCursor(0, 0);
  display.print("Steps: ");
  display.println(stepCount);
  display.display();
}
void displayFallStatus(const char* msg) {
  // Do not clear the display, so the step count remains at the top
  display.setTextSize(1);
  display.setCursor(0, 55);  // Display message below step count
  display.fillRect(0, 55, SCREEN_WIDTH, 10, SSD1306_BLACK);
  display.setCursor(0, 55);
  display.println(msg);
  display.display();
  delay(1500);  // Keep the status message for 1.5 seconds before clearing it
}

//Stepcount function using gyroX
void StepCount() {
  // Step detection based on gyroscope x-axis threshold crossing
  if (gx > threshold && previousGyroX < threshold && !stepDetected) {
    stepCount++;
    stepDetected = true;
    Serial.print("Step detected! Total steps: ");
    Serial.println(stepCount);
    displayStepCount(); // Only update the step count when it changes
  }
  // Reset step detection when gyroX drops below threshold
  if (gx < threshold) {
    stepDetected = false;
  }
  previousGyroX = gx;
  if (!client.connected()) {
    reconnectMQTT();
  }
  client.loop();
  char stepCountStr[10];
  snprintf(stepCountStr, sizeof(stepCountStr), "%d", stepCount);
  client.publish(mqtt_step_topic, stepCountStr);
  delay(50);
}

//////////////////////////////////Step cont end///////////////////////////////////////////////

/////////////////////////////////walking pattern//////////////////////////////////////////////

// Function to detect walking patterns
void detectWalkingPattern() {
  // Read current acceleration and gyroscope data
  mpu_read();
  float accMagnitude = sqrt(ax * ax + ay * ay + az * az);  
  float gyroMagnitude = sqrt(gx * gx + gy * gy + gz * gz);

float restThresholdAcc = 0.1; 
float restThresholdGyro = 0.5;
float runThresholdAcc = 2.0;
float runThresholdGyro = 60.0;

  if (accMagnitude < restThresholdAcc && gyroMagnitude < restThresholdGyro) {
  
    Serial.println("Running");
    displayWalkingPattern("Running");
    client.publish(mqtt_walking_pattern, "Running");
  }

  else if (accMagnitude > runThresholdAcc || gyroMagnitude > runThresholdGyro) {
    Serial.println("Walking");
    displayWalkingPattern("Walking");
    client.publish(mqtt_walking_pattern, "Walking");
  }
  else {
       Serial.println("Resting");
    displayWalkingPattern("Resting");
    client.publish(mqtt_walking_pattern, "Resting");
    
  }
}

// Function to display walking pattern on OLED
void displayWalkingPattern(const char* pattern) {
  display.setTextSize(1);
  display.setCursor(0, 30);  // Display pattern below step count and fall status
  display.fillRect(0, 30, SCREEN_WIDTH, 10, SSD1306_BLACK);  // Clear previous walking pattern display
  display.setCursor(0, 30);
  display.println(pattern);
  display.display();
}

/////////////////////////////////end walking pattern///////////////////////////////////


///////////////////////////////Start Incident alert //////////////////////////////////////////
void alertFallDetected() {
   mpu_read();
   ax = (AcX - 2050) / 16384.00;
   ay = (AcY - 77) / 16384.00;
   az = (AcZ - 1947) / 16384.00;
   gx = (GyX + 270) / 131.07;
   gy = (GyY - 351) / 131.07;
   gz = (GyZ + 136) / 131.07;
   
   float Raw_Amp = pow(pow(ax, 2) + pow(ay, 2) + pow(az, 2), 0.5);
   int Amp = Raw_Amp * 10;  // Multiplied by 10 because values are between 0 to 1
   Serial.println(Amp);
   if (Amp <= 2 && trigger2 == false) { //if AM breaks lower threshold (0.4g)
     trigger1 = true;
     Serial.println("TRIGGER 1 ACTIVATED");
     displayFallStatus("TRIGGER 1 ACTIVATED");
   }
   if (trigger1 == true) {     
     trigger1count++;     
     if (Amp >= 12) { //if AM breaks upper threshold (3g)
       trigger2 = true;
       Serial.println("TRIGGER 2 ACTIVATED");
       displayFallStatus("TRIGGER 2 ACTIVATED");
       trigger1 = false;
       trigger1count = 0;
     }
   }
   if (trigger2 == true) {
     trigger2count++;
     angleChange = pow(pow(gx, 2) + pow(gy, 2) + pow(gz, 2), 0.5);
     Serial.println(angleChange);
     if (angleChange >= 30 && angleChange <= 400) { //if orientation changes by between 80-100 degrees
       trigger3 = true;
       trigger2 = false;
       trigger2count = 0;
       Serial.println("TRIGGER 3 ACTIVATED");
       displayFallStatus("TRIGGER 3 ACTIVATED");
     }
   }
   if (trigger3 == true) {
     trigger3count++;
     if (trigger3count >= 10) {
       angleChange = pow(pow(gx, 2) + pow(gy, 2) + pow(gz, 2), 0.5);
       Serial.println(angleChange);
       if ((angleChange >= 0) && (angleChange <= 10)) { //if orientation remains between 0-10 degrees
         fall = true;
         trigger3 = false;
         trigger3count = 0;
         Serial.println(angleChange);
         const char* fallmsg = "FALL DETECTED";
         client.publish(mqtt_incident_alert, fallmsg);
         displayFallStatus("FALL DETECTED");
       } else {
         trigger3 = false;
         trigger3count = 0;
         Serial.println("TRIGGER 3 DEACTIVATED");
         displayFallStatus("TRIGGER 3 DEACTIVATED");
       }
     }
   }
}
void mpu_read() {
   Wire.beginTransmission(MPU_addr);
   Wire.write(0x3B);  // starting with register 0x3B (ACCEL_XOUT_H)
   Wire.endTransmission(false);
   Wire.requestFrom(MPU_addr, 14, true); // request a total of 14 registers
   AcX = Wire.read() << 8 | Wire.read(); // 0x3B (ACCEL_XOUT_H) & 0x3C (ACCEL_XOUT_L)
   AcY = Wire.read() << 8 | Wire.read(); // 0x3D (ACCEL_YOUT_H) & 0x3E (ACCEL_YOUT_L)
   AcZ = Wire.read() << 8 | Wire.read(); // 0x3F (ACCEL_ZOUT_H) & 0x40 (ACCEL_ZOUT_L)
   Tmp = Wire.read() << 8 | Wire.read(); // 0x41 (TEMP_OUT_H) & 0x42 (TEMP_OUT_L)
   GyX = Wire.read() << 8 | Wire.read(); // 0x43 (GYRO_XOUT_H) & 0x44 (GYRO_XOUT_L)
   GyY = Wire.read() << 8 | Wire.read(); // 0x45 (GYRO_YOUT_H) & 0x46 (GYRO_YOUT_L)
   GyZ = Wire.read() << 8 | Wire.read(); // 0x47 (GYRO_ZOUT_H) & 0x48 (GYRO_ZOUT_L)
   
   // Convert raw gyroscope values to degrees per second (dps) for ±250dps range
   gx = GyX / 131.0;
   gy = GyY / 131.0;
   gz = GyZ / 131.0;

   ax = AcX / 16384.00;
   ay = AcY / 16384.00;
   az = AcZ / 16384.00;

}
////////////////////////////// End Incident Alert //////////////////////////////////////



void Mlmodel(){

// Create JSON object
   StaticJsonDocument<200> jsonDoc;
   jsonDoc["gyro_x"] = GyX;
   jsonDoc["gyro_y"] = GyY;
   jsonDoc["gyro_z"] = GyZ;

   // Convert JSON object to string
   char jsonBuffer[200];
   serializeJson(jsonDoc, jsonBuffer);

   // Publish JSON data to MQTT topic
   client.publish(mqtt_ML_pattern, jsonBuffer);

}


void loop() {
  mpu_read();

  Mlmodel();

  StepCount();

  alertFallDetected();

  detectWalkingPattern();

  if (!client.connected()) {
    reconnectMQTT();
  }
  client.loop();
}
