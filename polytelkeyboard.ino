#include <Arduino.h>
#include "DigiKeyboard.h" 

#define DATA_PIN 0
#define ENABLE_PIN 5
#define COLUMN_PIN 1
#define ROW_PIN 2

#define DEBOUNCE_LIMIT 128

uint8_t rowHit;
uint8_t debounce;

void setup()
{
	pinMode(DATA_PIN, INPUT);
  pinMode(ENABLE_PIN, OUTPUT);
  pinMode(COLUMN_PIN, OUTPUT);
  pinMode(ROW_PIN, OUTPUT);

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
  if (digitalRead(DATA_PIN) && DEBOUNCE_LIMIT > debounce)
    ++debounce;
  else if(debounce > 0)
    --debounce;

  if (debounce == DEBOUNCE_LIMIT || rowHit) {
    digitalWrite(ENABLE_PIN, LOW);
    readRow();
    digitalWrite(ENABLE_PIN, HIGH);
    DigiKeyboard.println(rowHit, BIN);
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