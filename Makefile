# Define variables
CC = gcc
CFLAGS = -Wall -Wextra -O0 -ggdb3 -lrt
TARGET = test
SRC = test.c

# Default target
all: $(TARGET)

# Compile the program
$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

# Run the program with a specified number of repetitions
# run: $(TARGET)
# 	./$(TARGET)

# Clean target
clean:
	rm -f $(TARGET)
