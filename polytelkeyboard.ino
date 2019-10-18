#include <Arduino.h>
#include "usbdrv/usbdrv.h"

#define DATA_PIN 0
#define ENABLE_PIN 5
#define COLUMN_PIN 1
#define ROW_PIN 2

#define DEBOUNCE_LIMIT 128

uint8_t rowHit;
uint8_t debounce;

PROGMEM const char usbHidReportDescriptor[USB_CFG_HID_REPORT_DESCRIPTOR_LENGTH] = { /* USB report descriptor */
  0x05, 0x01,                    // USAGE_PAGE (Generic Desktop)
  0x09, 0x06,                    // USAGE (Keyboard)
  0xa1, 0x01,                    // COLLECTION (Application)
  0x05, 0x07,                    //   USAGE_PAGE (Keyboard)
  0x19, 0xe0,                    //   USAGE_MINIMUM (Keyboard LeftControl)
  0x29, 0xe7,                    //   USAGE_MAXIMUM (Keyboard Right GUI)
  0x15, 0x00,                    //   LOGICAL_MINIMUM (0)
  0x25, 0x01,                    //   LOGICAL_MAXIMUM (1)
  0x75, 0x01,                    //   REPORT_SIZE (1)
  0x95, 0x08,                    //   REPORT_COUNT (8)
  0x81, 0x02,                    //   INPUT (Data,Var,Abs)
  0x19, 0x00,                    //   USAGE_MINIMUM (Reserved (no event indicated))
  0x29, 0xa4,                    //   USAGE_MAXIMUM (Keyboard ExSel)
  0x75, 0x08,                    //   REPORT_SIZE (8)
  0x95, 0x1c,                    //   REPORT_COUNT (28)
  0x26, 0xa4, 0x00,              //   LOGICAL_MAXIMUM (164)
  0x81, 0x00,                    //   INPUT (Data,Ary,Abs)
  0xc0                           // END_COLLECTION
};

void setup()
{
  pinMode(DATA_PIN, INPUT);
  pinMode(ENABLE_PIN, OUTPUT);
  pinMode(COLUMN_PIN, OUTPUT);
  pinMode(ROW_PIN, OUTPUT);

  cli();
  usbDeviceDisconnect();
  usbDeviceConnect();
  usbInit();
  sei();

  digitalWrite(ENABLE_PIN, LOW);
  digitalWrite(COLUMN_PIN, LOW);
  digitalWrite(ROW_PIN, LOW);

  delayMicroseconds(100);

  digitalWrite(ENABLE_PIN, HIGH);
  digitalWrite(COLUMN_PIN, HIGH);
  digitalWrite(ROW_PIN, HIGH);
}

void loop()
{
  usbPoll();
  
  if (digitalRead(DATA_PIN) && DEBOUNCE_LIMIT > debounce)
    ++debounce;
  else if(debounce > 0)
    --debounce;

  if (debounce == DEBOUNCE_LIMIT || rowHit) {
    digitalWrite(ENABLE_PIN, LOW);
    readRow();
    digitalWrite(ENABLE_PIN, HIGH);
    debounce = 0;
  }
}

void readRow () {
  for (uint8_t i = 0; i < 8; ++i) {
    digitalWrite(ROW_PIN, LOW);  
    rowHit |= digitalRead(DATA_PIN) << i;
    digitalWrite(ROW_PIN, HIGH);

    if (rowHit & (1 << i)) {
      uint8_t columns = readColumn();
      if (columns == 0)
        rowHit &= ~(1 << i);
    }
  }
}

uint8_t readColumn () {
  uint8_t value = 0;

  for (uint8_t i = 0; i < 8; ++i) {
    digitalWrite(COLUMN_PIN, LOW);
    value |= digitalRead(DATA_PIN) << i;
    digitalWrite(COLUMN_PIN, HIGH);
  }

  return value;
}