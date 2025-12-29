# Compiler und Programmer
CC = avr-gcc
OBJCOPY = avr-objcopy
AVRDUDE = avrdude

# Mikrocontroller-Einstellungen
MCU = atmega328p
F_CPU = 16000000UL  # Taktfrequenz des Mikrocontrollers (16 MHz)
PROGRAMMER = arduino  # Name des Programmers (z.B. usbasp, arduino, etc.)
PORT = /dev/ttyACM0 # Nur für bestimmte Programmer notwendig

# Compiler-Einstellungen
CFLAGS = -Wall -Os -DF_CPU=$(F_CPU) -mmcu=$(MCU)

# Source-Dateien
SRC = main.c

# Output-Datei
TARGET = main

# Default-Build-Ziel
all: $(TARGET).hex

# Compilieren und Linken
$(TARGET).elf: $(SRC)
	$(CC) $(CFLAGS) -o $@ $^

# Erzeugen der HEX-Datei
$(TARGET).hex: $(TARGET).elf
	$(OBJCOPY) -O ihex -R .eeprom $< $@

# Flashen des Mikrocontrollers
flash: $(TARGET).hex
	$(AVRDUDE) -p $(MCU) -c $(PROGRAMMER) -P $(PORT) -U flash:w:$(TARGET).hex:i

# Aufräumen
clean:
	rm -f *.o *.elf *.hex

# Weitere Optionen
fuse:
	$(AVRDUDE) -p $(MCU) -c $(PROGRAMMER) -P $(PORT) -U lfuse:w:0xff:m -U hfuse:w:0xd9:m -U efuse:w:0xfd:m

.PHONY: all clean flash fuse
