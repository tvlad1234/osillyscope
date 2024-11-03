all : flash

TARGET:=osillyscope

ADDITIONAL_C_FILES+=ssd1306.c gfx.c scope.c ui.c

CH32V003FUN:=../../ch32v003fun/ch32v003fun

include $(CH32V003FUN)/ch32v003fun.mk

flash : cv_flash
clean : cv_clean