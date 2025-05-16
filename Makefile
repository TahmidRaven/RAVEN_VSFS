# Makefile for RAVEN_VSFS

CC = gcc
CFLAGS = -Wall -Wextra -pedantic -std=c99

TARGET = raven_vsfs
SRC = raven_vsfs.c
OBJ = $(SRC:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJ)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

run: $(TARGET)
	./$(TARGET)


clean:
	rm -f $(TARGET) $(OBJ)

.PHONY: all clean
