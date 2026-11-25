#include <Arduino.h>
#include "USB.h"
#include "USBHID.h"
#include <hx711_ADC.h>

#define HX711Dout 4
#define HX711Sck 5
#define GasPin 6
#define ClutchPin 7

static const uint8_t ReportDescriptor[] = {
  0x05, 0x01,                    // Usage Page (Generic Desktop)
  0x09, 0x04,                    // Usage (Joystick)
  0xA1, 0x01,                    // Collection (Application)
  0x85, 0x01,                    //   Report ID (1)
  0x09, 0x30,                    //   Usage (X - Gas)
  0x09, 0x31,                    //   Usage (Y - Brake)
  0x09, 0x32,                    //   Usage (Z - Clutch)
  0x15, 0x00,                    //   Logical Minimum (0)
  0x27, 0xFF, 0xFF, 0x00, 0x00,  //   Logical Maximum (65535)
  0x75, 0x10,                    //   Report Size (16 bits)
  0x95, 0x03,                    //   Report Count (3 axes)
  0x81, 0x02,                    //   Input (Data, Variable, Absolute)
  0xC0                           // End Collection
};

USBHID HID;

class SimPedals : public USBHIDDevice {
  public:
    SimPedals(){
      static bool init = false;
      if (!init){
        init = true;
        HID.addDevice(this, sizeof(ReportDescriptor));
      }
    }

    uint16_t _onGetDescriptor(uint8_t* buffer) override {
      memcpy(buffer, ReportDescriptor, sizeof(ReportDescriptor));
      return sizeof(ReportDescriptor);
    }

    bool send(uint16_t Gas, uint16_t Brake, uint16_t Clutch){
      if (!HID.ready()) return false;
      uint16_t Report[3] = {Gas, Brake, Clutch};
      return HID.SendReport(1, Report, sizeof(Report));
    }
};

SimPedals Pedals;

HX711_ADC LoadCell(HX711Dout, HX711Sck);  

const uint16_t DefaultGasMin = 0;
const uint16_t DefaultGasMax = 4095;
const float_t DefaultBrakeMin = 8375000;
const float_t DefaultBrakeMax = 8450000;
const uint16_t DefaultClutchMin = 0;
const uint16_t DefaultClutchMax = 4095;

void setup() {
  Serial0.begin(115200, SERIAL_8N1, 44, 43);

  LoadCell.begin(64);
  LoadCell.setSamplesInUse(4);
  LoadCell.start(2000, false);
  LoadCell.setCalFactor(1.0);

  USB.PID(0x8602);
  USB.productName("DIY esp32 Sim Pedals.");
  HID.begin();
  USB.begin();
  
  pinMode(GasPin, INPUT);
  pinMode(ClutchPin, INPUT);
}

void loop() {
  static uint32_t TimeOfLastPrint;
  static uint32_t TimeOfLastHid;
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

  uint16_t Gas = (uint16_t)(((float_t)(RawGas - GasMin) / (GasMax - GasMin)) * 65535.0f);
  uint16_t Clutch = (uint16_t)(((float_t)(RawClutch - ClutchMin) / (ClutchMax - ClutchMin)) * 65535.0f);

  static float_t RawBrakeForce=0;
  static uint16_t Brake=0;

  if(LoadCell.update()){
    RawBrakeForce = LoadCell.getData();
    float_t RawBrake = RawBrakeForce;

    if(RawBrake < BrakeMin) RawBrake = BrakeMin;
    if(RawBrake > BrakeMax) RawBrake = BrakeMax;

    Brake = (uint16_t)(((RawBrake - BrakeMin) / (BrakeMax - BrakeMin)) * 65535.0f);
  }

  if (micros() - TimeOfLastHid >= 500){
    TimeOfLastHid = micros();
    Pedals.send(Gas, Brake, Clutch);
  };

  if(Serial0.available() > 0){
    String Input = Serial0.readStringUntil('\n');
    Input.trim();
    Input.toUpperCase();

    if (Input.startsWith("GMIN=")) {
      GasMin = Input.substring(5).toInt();
      Serial0.printf("GasMin updated to: %0.f\n", GasMin);
    } 
    else if (Input.startsWith("GMAX=")) {
      GasMax = Input.substring(5).toInt();
      Serial0.printf("GasMax updated to: %0.f\n", GasMax);
    } 
    else if (Input.startsWith("CMIN=")) {
      ClutchMin = Input.substring(5).toInt();
      Serial0.printf("ClutchMin updated to: %0.f\n", ClutchMin);
    } 
    else if (Input.startsWith("CMAX=")) {
      ClutchMax = Input.substring(5).toInt();
      Serial0.printf("ClutchMax updated to: %0.f\n", ClutchMax);
    }
    else if (Input.startsWith("BMIN=")) {
      BrakeMin = Input.substring(5).toInt();
      Serial0.printf("BrakeMin updated to: %0.f\n", BrakeMin);
    } 
    else if (Input.startsWith("BMAX=")) {
      BrakeMax = Input.substring(5).toInt();
      Serial0.printf("BrakeMax updated to: %0.f\n", BrakeMax);
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