# Makefile - build with gcc / MinGW
CC      = gcc
CFLAGS  = -std=c99 -Wall -Wextra -O2
TARGET  = car

all: $(TARGET)

$(TARGET): main.c car.c car.h
	$(CC) $(CFLAGS) -o $(TARGET) main.c car.c

clean:
	rm -f $(TARGET) $(TARGET).exe

.PHONY: all clean
