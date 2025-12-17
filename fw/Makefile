all : flash

TARGET:=osillyscope
TARGET_MCU?=CH32V003

ADDITIONAL_C_FILES+=ssd1306.c gfx.c scope.c ui.c

CH32FUN:=../ch32fun/ch32fun

include $(CH32FUN)/ch32fun.mk

flash : cv_flash
clean : cv_clean