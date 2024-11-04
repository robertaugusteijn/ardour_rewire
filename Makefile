# Compiler
CC = gcc

# Compiler flags
CFLAGS = -Wall -Wextra -g `pkg-config --cflags libxml-2.0`

# Linker flags
LDFLAGS = `pkg-config --libs libxml-2.0`

# Source files
SRC = rewire.c

# Output binary name
TARGET = rewire

# Default target
all: $(TARGET)

# Build the target
$(TARGET): $(SRC)
	$(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS)

# Clean up generated files
clean:
	rm -f $(TARGET)

# Phony targets
.PHONY: all clean
