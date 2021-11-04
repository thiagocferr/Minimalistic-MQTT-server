GCC       := gcc
CPP_FLAGS := -std=c11 -ggdb -Wall -Wextra -pedantic


INCLUDE_PATH := include/
SRC_FILES := $(wildcard src/*.c)

# External libraries
LIB_FLAGS = -lpthread

# Name of program
TARGET =   broker

all: $(TARGET)

$(TARGET): $(SRC_FILES)
	$(GCC) $(CPP_FLAGS) -I $(INCLUDE_PATH) $^ -o $@ $(LIB_FLAGS)

run:
	./$(TARGET) 1883

clean:
	rm -rf ./$(TARGET)