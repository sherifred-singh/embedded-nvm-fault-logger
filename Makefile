CC ?= gcc
CFLAGS ?= -std=c99 -Wall -Wextra -Werror -pedantic -Iinclude -Itests/unity

SRC = src/nvm_fault_logger.c
TEST_SRC = tests/test_nvm_fault_logger.c tests/unity/unity.c
TARGET = test_runner

.PHONY: all test clean

all: test

$(TARGET): $(SRC) $(TEST_SRC)
	$(CC) $(CFLAGS) $(SRC) $(TEST_SRC) -o $(TARGET)

test: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET) $(TARGET).exe
