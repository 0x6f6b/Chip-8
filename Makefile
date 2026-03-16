CC = gcc
CFLAGS = -Wall -Wextra -O2
LIBS = -lSDL3
TARGET = chip8
SRC = chip8.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET) $(LIBS)

debug: $(SRC)
	$(CC) $(CFLAGS) -g -DDEBUG $(SRC) -o $(TARGET) $(LIBS)

clean:
	rm -f $(TARGET).exe $(TARGET)

.PHONY: all debug clean