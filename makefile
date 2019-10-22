BOARD_TAG = digispark-tiny
ALTERNATE_CORE_PATH = /usr/local/share/digistump-avr/1.6.7

ASFLAGS += -D__ASSEMBLER__
CXXFLAGS += -funsigned-char

LOCAL_C_SRCS += $(wildcard usbdrv/*.c)
LOCAL_AS_SRCS += $(wildcard usbdrv/*.S)
include /usr/share/arduino/Arduino.mk