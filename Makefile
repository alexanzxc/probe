# Define variables
CC = gcc
CFLAGS = -Wall -Wextra -O0 -ggdb3 -lrt
TARGET = test
SRC = test.c
REPETITIONS = 1000 10000 100000 1000000 10000000 100000000 1000000000

# Default target
all: $(TARGET)

# Compile the program
$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

# Run the program with various repetitions
run: $(TARGET)
	@for rep in $(REPETITIONS); do \
		echo "Running with $$rep repetitions"; \
		./$(TARGET) $$rep; \
	done

# Clean target
clean:
	rm -f $(TARGET)