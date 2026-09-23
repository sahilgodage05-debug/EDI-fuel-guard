#include <OneWire.h>
#include <DallasTemperature.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// ==========================================
// Fuel-Guard 3X Pro (Rev 2) - Pin Mapping
// ==========================================

// 1. I2C LCD Screen Mapping
// ESP32 default I2C pins: SDA = 21, SCL = 22
LiquidCrystal_I2C lcd(0x27, 16, 2); 

// 2. Optoelectronic Sensor (LDR + Laser)
#define LDR_PIN 34         // Pure Input ADC1 Channel
#define LASER_PIN 27       // Safe GPIO assigned for Laser

// 3. Thermal Network (DS18B20)
#define TEMP_SENSOR_PIN 4  // 1-Wire Data

// 4. TCS3200 Control Interface (Color Sensor)
#define S0 18              
#define S1 19              
#define S2 23              
#define S3 5               
#define TCS_OUT 16         // Dedicated safe hardware interrupt capture

// ==========================================

OneWire oneWire(TEMP_SENSOR_PIN);
DallasTemperature tempSensor(&oneWire);

// "Golden Range" - Pre-Calibrated Software Logic
int goldenLdrThreshold = 2000; // Baseline LDR value
int goldenBlueThreshold = 50;  // Baseline Blue value (Kerosene Dye detection)

void setup() {
  Serial.begin(115200);
  
  // Initialize LCD
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Fuel-Guard 3X");
  lcd.setCursor(0, 1);
  lcd.print("Booting System..");
  delay(2000); 
  
  // Configure Pins
  pinMode(LASER_PIN, OUTPUT);
  pinMode(LDR_PIN, INPUT);
  
  pinMode(S0, OUTPUT);
  pinMode(S1, OUTPUT);
  pinMode(S2, OUTPUT);
  pinMode(S3, OUTPUT);
  pinMode(TCS_OUT, INPUT);
  
  // TCS3200 Frequency Scaling (Set to 20%)
  digitalWrite(S0, HIGH);
  digitalWrite(S1, LOW);

  // Initialize Thermal Network
  tempSensor.begin();
  digitalWrite(LASER_PIN, LOW); 
  
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("System Ready");
  lcd.setCursor(0, 1);
  lcd.print("Waiting 4 Flow..");
  Serial.println("Fuel-Guard 3X Pro (Rev 2) - Ready & Waiting for Fluid Flow");
}

void loop() {
  // The first 1.0 second of fluid entry is used to stabilize mechanical flow boundaries
  delay(1000); 

  // --- 1. Thermal Coefficient & Density Adjustment ---
  tempSensor.requestTemperatures(); 
  float currentTemp = tempSensor.getTempCByIndex(0);
  
  int currentLdrThreshold = goldenLdrThreshold;
  
  // Thermal offset for liquid viscosity changes based on weather
  if (currentTemp < 15.0) {
    currentLdrThreshold -= 200; 
  } else if (currentTemp > 35.0) {
    currentLdrThreshold += 200; 
  }

  // --- 2. Optoelectronic Turbidity Test (Water / Impurities) ---
  digitalWrite(LASER_PIN, HIGH);
  delay(100); 
  int ldrValue = analogRead(LDR_PIN);
  digitalWrite(LASER_PIN, LOW);
  
  bool fuelIsPure = true;
  String failReason = "";

  if (ldrValue > currentLdrThreshold) { 
     fuelIsPure = false;
     failReason = "WATER/DIRT";
  }

  // --- 3. Color Forensics (Blue Dye / Kerosene) ---
  digitalWrite(S2, LOW);
  digitalWrite(S3, HIGH);
  delay(50);
  int blueFreq = pulseIn(TCS_OUT, LOW); // High-speed capture
  
  if (blueFreq > 0 && blueFreq < goldenBlueThreshold) { 
    fuelIsPure = false;
    if (failReason == "") {
      failReason = "KEROSENE";
    } else {
      failReason = "MULTIPLE FAULT";
    }
  }

  // --- 4. Result Dashboard Output (Local I2C Display) ---
  lcd.clear();
  if (fuelIsPure) {
    lcd.setCursor(0, 0);
    lcd.print("FUEL: PURE 100%");
    lcd.setCursor(0, 1);
    lcd.print("Temp: "); lcd.print(currentTemp, 1); lcd.print(" C");
    Serial.println("Result: PURE FUEL (Matches Golden Baseline)");
  } else {
    lcd.setCursor(0, 0);
    lcd.print("FAIL: "); lcd.print(failReason);
    lcd.setCursor(0, 1);
    lcd.print("DO NOT REFUEL!");
    Serial.print("Result: FAILED - Adulteration Type: "); Serial.println(failReason);
  }

  // Wait until next cycle (completing the 5-second dynamic flow window)
  delay(4000); 
}
