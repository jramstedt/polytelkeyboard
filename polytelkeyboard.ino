#include <Arduino.h>
#include <util/delay.h>

extern "C" {
  #include "usbdrv/osccal.h"
  #include "usbdrv/usbdrv.h"
}

#include "USB_scan_codes.h"

#define DATA_PIN 0
#define ENABLE_PIN 5
#define COLUMN_PIN 1
#define ROW_PIN 2

#define DEBOUNCE_LIMIT 128

#define PROTOCOL_BOOT 0
#define PROTOCOL_REPORT 1

uint8_t rowHit;
uint8_t debounce;

uint8_t protocol = PROTOCOL_REPORT;

void readRow ();
uint8_t readColumn (const uint8_t column[]);
void registerKey (const uint8_t pressed, const uint8_t usbScanCode);
void flushBuffer ();

/*
 Visual:

 Q W E R T Y U I O P Å BKSC
 A S D F G H J K L Ö Ä ENTER
SH Z X C V B N M , . - SHIFT
FN1 CTRL CM1 SPACE CM2 MENU PHONE FN2

 Indexes:
  0,0 0,1 0,2 0,3 0,4 0,5 6,0 6,1 1,0 1,1 1,2 1,3
   1,4 1,5 7,1 7,0 2,0 2,1 2,2 2,3 2,4 2,5 7,3 7,2
 3,0 3,1 3,2 3,3 3,4 3,5 6,5 6,4 4,0 4,1 4,2 4,3
  4,4   4,5   5,5   5,4      5,0   5,1   5,2   5,3
*/

// TODO support multiple layers

PROGMEM const uint8_t charMap[8][6] = {
  {KEY_Q, KEY_W, KEY_E, KEY_R, KEY_T, KEY_Y},
  {KEY_O, KEY_P, KEY_LEFTBRACE, KEY_BACKSPACE, KEY_A, KEY_S},
  {KEY_G, KEY_H, KEY_J, KEY_K, KEY_L, KEY_SEMICOLON},
  {KEY_LEFTSHIFT, KEY_Z, KEY_X, KEY_C, KEY_V, KEY_B},

  {KEY_COMMA, KEY_DOT, KEY_SLASH, KEY_RIGHTSHIFT, KEY_FN1, KEY_LEFTCONTROL},
  {KEY_RIGHTALT, KEY_MENU, KEY_FN2, KEY_RIGHTCONTROL, KEY_SPACEBAR, KEY_LEFTALT},
  {KEY_U, KEY_I, 0x00, 0x00, KEY_M, KEY_N},
  {KEY_F, KEY_D, KEY_ENTER, KEY_APOSTROPHE, 0x00, 0x00},
};

struct CommonUsbMsg {
  uint8_t modifierMask;
};

/** boot supported */
PROGMEM const char usbHidReportDescriptor[] = {
    0x05, 0x01,                    // USAGE_PAGE (Generic Desktop)
    0x09, 0x06,                    // USAGE (Keyboard)
    0xa1, 0x01,                    // COLLECTION (Application)
    0x05, 0x07,                    //   USAGE_PAGE (Keyboard)
    0x19, 0xe0,                    //   USAGE_MINIMUM (Keyboard LeftControl)
    0x29, 0xe7,                    //   USAGE_MAXIMUM (Keyboard Right GUI)
    0x15, 0x00,                    //   LOGICAL_MINIMUM (0)
    0x25, 0x01,                    //   LOGICAL_MAXIMUM (1)
    0x95, 0x08,                    //   REPORT_COUNT (8)
    0x75, 0x01,                    //   REPORT_SIZE (1)
    0x81, 0x02,                    //   INPUT (Data,Var,Abs)
    0x95, 0x01,                    //   REPORT_COUNT (1)
    0x75, 0x08,                    //   REPORT_SIZE (8)
    0x81, 0x03,                    //   INPUT (Cnst,Var,Abs)
    0x95, 0x06,                    //   REPORT_COUNT (6)
    0x75, 0x08,                    //   REPORT_SIZE (8)
    0x15, 0x00,                    //   LOGICAL_MINIMUM (0)
    0x25, 0x65,                    //   LOGICAL_MAXIMUM (101)
    0x19, 0x00,                    //   USAGE_MINIMUM (Reserved (no event indicated))
    0x29, 0x65,                    //   USAGE_MAXIMUM (Keyboard Application)
    0x81, 0x00,                    //   INPUT (Data,Ary,Abs)
    0xc0                           // END_COLLECTION
};

struct BootUsbMsg : CommonUsbMsg {
  uint8_t oemReserved;
  uint8_t scanCodes[6];
};

/** nkro supported
PROGMEM const char usbHidReportDescriptor[] = {
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
    0x15, 0x00,                    //   LOGICAL_MINIMUM (0)
    0x25, 0x01,                    //   LOGICAL_MAXIMUM (1)
    0x75, 0x01,                    //   REPORT_SIZE (1)
    0x95, 0xe8,                    //   REPORT_COUNT (232)
    0x81, 0x02,                    //   INPUT (Data,Var,Abs)
    0xc0                           // END_COLLECTION
};
*/

struct NKROUsbMsg : CommonUsbMsg {
  uint8_t keyMask[29];
};

uint8_t emptyFlushed;
uint8_t keysPressed;
uint8_t modifiersDirty;

size_t reportBufferSize;
uint8_t *reportBuffer;

void setup()
{
  pinMode(DATA_PIN, INPUT);
  pinMode(ENABLE_PIN, OUTPUT);
  pinMode(COLUMN_PIN, OUTPUT);
  pinMode(ROW_PIN, OUTPUT);

  cli();
  usbDeviceDisconnect();

  reportBuffer = (uint8_t *)malloc(max(sizeof(BootUsbMsg), sizeof(NKROUsbMsg)));
  reportBufferSize = sizeof(BootUsbMsg);  
  memset(reportBuffer, 0, reportBufferSize);

  _delay_ms(250);
  usbDeviceConnect();
  usbInit();
  sei();

  digitalWrite(ENABLE_PIN, LOW);
  digitalWrite(COLUMN_PIN, LOW);
  digitalWrite(ROW_PIN, LOW);
  _delay_ms(1);
  digitalWrite(ENABLE_PIN, HIGH);
  digitalWrite(COLUMN_PIN, HIGH);
  digitalWrite(ROW_PIN, HIGH);
}

void loop()
{ 
  if (digitalRead(DATA_PIN) && DEBOUNCE_LIMIT > debounce)
    ++debounce;
  else if(debounce > 0)
    --debounce;

  if (debounce == DEBOUNCE_LIMIT || rowHit) {
    CommonUsbMsg *commonReportBuffer = (CommonUsbMsg *)reportBuffer;
    uint8_t prevModifiers = commonReportBuffer->modifierMask;

    digitalWrite(ENABLE_PIN, LOW);
    readRow();
    digitalWrite(ENABLE_PIN, HIGH);
    debounce = 0;
    
    modifiersDirty |= commonReportBuffer->modifierMask ^ prevModifiers;
  }

  flushBuffer();
}

void readRow () {
  for (uint8_t i = 0; i < 8; ++i) {
    digitalWrite(ROW_PIN, LOW);  
    rowHit |= digitalRead(DATA_PIN) << i;
    digitalWrite(ROW_PIN, HIGH);

    if (rowHit & (1 << i)) {
      uint8_t columns = readColumn(charMap[i]);
      if (columns == 0)
        rowHit &= ~(1 << i);
    }
  }
}

uint8_t readColumn (const uint8_t column[]) {
  uint8_t value = 0;

  for (uint8_t i = 0; i < 6; ++i) {
    digitalWrite(COLUMN_PIN, LOW);
    uint8_t pressed = digitalRead(DATA_PIN);
    registerKey(pressed, pgm_read_byte(&(column[i])));
    value |= pressed << i;
    digitalWrite(COLUMN_PIN, HIGH);
  }

  digitalWrite(COLUMN_PIN, LOW); // Seventh bit is not used,
  digitalWrite(COLUMN_PIN, HIGH);

  digitalWrite(COLUMN_PIN, LOW); //  Eight bit is always HIGH
  digitalWrite(COLUMN_PIN, HIGH);

  return value;
}

void registerKey (const uint8_t pressed, const uint8_t usbScanCode) {
  if(usbScanCode < KEY_LEFTCONTROL) {
    if (!pressed) return;

      BootUsbMsg *bootReportBuffer = (BootUsbMsg *)reportBuffer; 

      if (keysPressed == sizeof(bootReportBuffer->scanCodes))
        memset(bootReportBuffer->scanCodes, KEY_ErrorRollOver, sizeof(bootReportBuffer->scanCodes));
      else
        bootReportBuffer->scanCodes[keysPressed++] = usbScanCode;

    /*
    if (protocol == PROTOCOL_BOOT) {
      BootUsbMsg *bootReportBuffer = (BootUsbMsg *)reportBuffer; 

      if (keysPressed == sizeof(bootReportBuffer->scanCodes))
        memset(bootReportBuffer->scanCodes, KEY_ErrorRollOver, sizeof(bootReportBuffer->scanCodes));
      else
        bootReportBuffer->scanCodes[keysPressed++] = usbScanCode;
    } else {
      NKROUsbMsg *nkroReportBuffer = (NKROUsbMsg *)reportBuffer;

      nkroReportBuffer->keyMask[usbScanCode >> 3] |= 1 << (usbScanCode & 0b111);
      ++keysPressed;
    }*/

  } else if (usbScanCode <= KEY_RIGHTGUI) { // Modifier
    uint8_t bitIndex = usbScanCode - KEY_LEFTCONTROL;
    
    CommonUsbMsg *commonReportBuffer = (CommonUsbMsg *)reportBuffer;

    if (pressed)
      commonReportBuffer->modifierMask |= 1 << bitIndex;
    else
      commonReportBuffer->modifierMask &= ~(1 << bitIndex);

    /*
    CommonUsbMsg *commonReportBuffer = (CommonUsbMsg *)reportBuffer;

    if (pressed)
      commonReportBuffer->modifierMask |= 1 << bitIndex;
    else
      commonReportBuffer->modifierMask &= ~(1 << bitIndex);
    */
  }
}

void flushBuffer () {
  usbPoll();

  uint8_t keysActive = keysPressed || modifiersDirty;

  if (!keysActive && emptyFlushed) return;

  if (usbInterruptIsReady()) {
    usbSetInterrupt(reportBuffer, reportBufferSize);
    emptyFlushed = !keysActive;
    modifiersDirty = 0;
  }

  // Zero data portion of the buffer
  memset(reportBuffer+sizeof(CommonUsbMsg), 0, reportBufferSize - sizeof(CommonUsbMsg));
  keysPressed = 0;
}

USB_PUBLIC usbMsgLen_t usbFunctionSetup(uchar data[8]) {
  usbRequest_t *rq = (usbRequest_t *)(data);
  
  if ((rq->bmRequestType & USBRQ_TYPE_MASK) == USBRQ_TYPE_CLASS) {
    if (rq->bRequest == USBRQ_HID_GET_REPORT) {
      usbMsgPtr = reportBuffer;
      return reportBufferSize;
    } else if (rq->bRequest == USBRQ_HID_GET_IDLE) {
      return 0;
    } else if (rq->bRequest == USBRQ_HID_GET_PROTOCOL) {
      usbMsgPtr = &protocol;
      return sizeof(protocol);
    } else if (rq->bRequest == USBRQ_HID_SET_REPORT) {
      return 0;
    } else if (rq->bRequest == USBRQ_HID_SET_IDLE) {
      return 0;
    } else if (rq->bRequest == USBRQ_HID_SET_PROTOCOL) {
      protocol = rq->wValue.bytes[1];

      /*
      if (protocol == PROTOCOL_BOOT)
        reportBufferSize = sizeof(BootUsbMsg);
      else
        reportBufferSize = sizeof(NKROUsbMsg);
      */
     
      memset(reportBuffer, 0, reportBufferSize);

      return 0;
    }
  }

  return 0;
}

/*
USB_PUBLIC usbMsgLen_t usbFunctionDescriptor(struct usbRequest *rq) {
  if ((rq->bmRequestType & USBRQ_TYPE_MASK) == USBRQ_TYPE_STANDARD) {
    if (rq->bRequest == USBRQ_GET_DESCRIPTOR) {
      if (rq->wValue.bytes[1] == USBDESCR_HID) {
        if (protocol == PROTOCOL_BOOT) {

        } else {

        }
      } else if (rq->wValue.bytes[1] == USBDESCR_HID_REPORT) {
        if (protocol == PROTOCOL_BOOT) {
          usbMsgPtr = (usbMsgPtr_t)usbDescriptorHidReportBoot;
          return sizeof(usbDescriptorHidReportBoot);
        } else {
          usbMsgPtr = (usbMsgPtr_t)usbDescriptorHidReportNKRO;
          return sizeof(usbDescriptorHidReportNKRO);
        }
      }
    }
  }

  return 0;
}
*/