#include <Arduino.h>
#include "USB.h"
#include "USBHIDGamepad.h"
#include <hx711_ADC.h>

#define HX711Dout 4
#define HX711Sck 5
#define GasPin 6
#define ClutchPin 7

USBHIDGamepad Joystick;
HX711_ADC LoadCell(HX711Dout, HX711Sck);

const uint16_t DefaultGasMin = 0; //Hall effect
const uint16_t DefaultGasMax = 1023; //esp32 analog max (2^10-1)
const float_t DefaultBrakeMin = -8388608; //Load cell + hx711
const float_t DefaultBrakeMax = 8388607; // 2^23-1
const uint16_t DefaultClutchMin = 0; //Hall effect
const uint16_t DefaultClutchMax = 1023; //esp32 analog max (2^10-1)

void setup() {
  Serial0.begin(115200, SERIAL_8N1, 44, 43);
  LoadCell.begin();
  LoadCell.setSamplesInUse(4);
  LoadCell.start(2000, false);
  LoadCell.setCalFactor(1.0); //factor = 1 makes it raw value
  Joystick.begin();
  USB.begin();
  pinMode(GasPin, INPUT);
  pinMode(ClutchPin, INPUT);
}

void loop() {
  static uint32_t TimeOfLastPrint;
  static uint16_t GasMin = DefaultGasMin;
  static uint16_t GasMax = DefaultGasMax;
  static float_t BrakeMin = DefaultBrakeMin;
  static float_t BrakeMax = DefaultBrakeMax;
  static uint16_t ClutchMin = DefaultClutchMin;
  static uint16_t ClutchMax = DefaultClutchMax;

  uint16_t RawGasHall = analogRead(GasPin);
  uint16_t RawClutchHall = analogRead(ClutchPin);

  uint16_t RawGas = RawGasHall;
  uint16_t RawClutch = RawClutchHall;

  if(RawGas < GasMin) RawGas = GasMin;
  if(RawGas > GasMax) RawGas = GasMax;

  if(RawClutch < ClutchMin) RawClutch = ClutchMin;
  if(RawClutch > ClutchMax) RawClutch = ClutchMax;

  uint16_t Gas = map((long)RawGas, (long)GasMin, (long)GasMax, 0, 65535);

  uint16_t Clutch = map((long)RawClutch, (long)ClutchMin, (long)ClutchMax, 0, 65535);

  float_t RawBrakeForce=0;
  uint16_t Brake=0;

  if(LoadCell.update()){
    RawBrakeForce = LoadCell.getData();
    float_t RawBrake = RawBrakeForce;

    if(RawBrake < BrakeMin) RawBrake = BrakeMin;
    if(RawBrake > BrakeMax) RawBrake = BrakeMax;

    Brake = map((long)RawBrake, (long)BrakeMin, (long)BrakeMax, 0, 65535);
  }

  
  Joystick.leftStick(0, Clutch);
  Joystick.rightTrigger(Gas);
  Joystick.leftTrigger(Brake);
  

  if(Serial0.available() > 0){
    String Input = Serial0.readStringUntil('\n');
    Input.trim();
    Input.toUpperCase();

    if (Input.startsWith("GMIN=")) {
      GasMin = Input.substring(5).toInt();
      Serial0.printf("GasMin updated to: %u\n", GasMin);
    } 
    else if (Input.startsWith("GMAX=")) {
      GasMax = Input.substring(5).toInt();
      Serial0.printf("GasMax updated to: %u\n", GasMax);
    } 
    else if (Input.startsWith("CMIN=")) {
      ClutchMin = Input.substring(5).toInt();
      Serial0.printf("ClutchMin updated to: %u\n", ClutchMin);
    } 
    else if (Input.startsWith("CMAX=")) {
      ClutchMax = Input.substring(5).toInt();
      Serial0.printf("ClutchMax updated to: %u\n", ClutchMax);
    }
    else if (Input.startsWith("BMIN=")) {
      BrakeMin = Input.substring(5).toInt();
      Serial0.printf("BrakeMin updated to: %u\n", BrakeMin);
    } 
    else if (Input.startsWith("BMAX=")) {
      BrakeMax = Input.substring(5).toInt();
      Serial0.printf("BrakeMax updated to: %u\n", BrakeMax);
    }
    else if (Input.startsWith("DEFAULT")){
      GasMin = DefaultGasMin;
      GasMax = DefaultGasMax;
      ClutchMin = DefaultClutchMin;
      ClutchMax = DefaultClutchMax;
      BrakeMin = DefaultBrakeMin;
      BrakeMax = DefaultBrakeMax;
      Serial0.printf("All values set to default.\n");
    }
  }

  if(millis()-TimeOfLastPrint >= 1000){
    TimeOfLastPrint = millis();
    Serial0.printf("RawGas: %d, Gas: %d\n", RawGas, Gas);
    Serial0.printf("RawBrake: %ld, Brake: %d\n", (int32_t)RawBrakeForce, Brake);
    Serial0.printf("RawClutch: %d, Clutch: %d\n", RawClutch, Clutch);
    Serial0.flush();
  }
}