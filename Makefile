
# Compiler to use
CC ?= gcc

# Compiler flags
CFLAGS = -Wall -static

# Target binary name
TARGET = abm-init

# Source files
SRC = main.c

# Rule to build the binary
$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

# Clean rule to remove the compiled binary
clean:
	rm -f $(TARGET)
