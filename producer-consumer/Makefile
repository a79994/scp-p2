CC = gcc
CFLAGS = -Wall -Wextra -O2 -pthread -Iinclude -Itest
LDFLAGS = -pthread

# Directories
SRC_DIR = src
INC_DIR = include
TEST_SRC_DIR = test/src
TEST_CASES_DIR = test/cases
BUILD_DIR = build

# Core object files
CORE_SRCS = $(SRC_DIR)/buffer.c
CORE_OBJS = $(BUILD_DIR)/buffer.o

# Main binary
MAIN_BIN = producer_consumer
MAIN_SRC = $(SRC_DIR)/main.c
MAIN_OBJ = $(BUILD_DIR)/main.o

# Test binaries
TEST_CASES = $(TEST_CASES_DIR)/case1_basic \
             $(TEST_CASES_DIR)/case2_fast_producers_slow_consumers \
             $(TEST_CASES_DIR)/case3_slow_producers_fast_consumers \
             $(TEST_CASES_DIR)/case4_high_concurrency

.PHONY: all test clean dirs

all: dirs $(MAIN_BIN) $(TEST_CASES)

dirs:
	@mkdir -p $(BUILD_DIR)
	@mkdir -p $(TEST_CASES_DIR)

# Compilation of core objects
$(BUILD_DIR)/buffer.o: $(SRC_DIR)/buffer.c $(INC_DIR)/buffer.h
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/main.o: $(MAIN_SRC) $(INC_DIR)/buffer.h
	$(CC) $(CFLAGS) -c $< -o $@

# Link main binary
$(MAIN_BIN): $(CORE_OBJS) $(MAIN_OBJ)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

# Compilation of test binaries in test/cases/
$(TEST_CASES_DIR)/case1_basic: $(TEST_SRC_DIR)/case1_basic.c $(CORE_OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

$(TEST_CASES_DIR)/case2_fast_producers_slow_consumers: $(TEST_SRC_DIR)/case2_fast_producers_slow_consumers.c $(CORE_OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

$(TEST_CASES_DIR)/case3_slow_producers_fast_consumers: $(TEST_SRC_DIR)/case3_slow_producers_fast_consumers.c $(CORE_OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

$(TEST_CASES_DIR)/case4_high_concurrency: $(TEST_SRC_DIR)/case4_high_concurrency.c $(CORE_OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

test: all
	@echo "=============================================================="
	@echo "               RUNNING PRODUCER-CONSUMER SUITE                "
	@echo "=============================================================="
	@for test_case in $(TEST_CASES); do \
		echo ""; \
		$$test_case || exit 1; \
	done
	@echo ""
	@echo "=============================================================="
	@echo "       ALL PRODUCER-CONSUMER TEST CASES PASSED!               "
	@echo "=============================================================="

clean:
	rm -rf $(BUILD_DIR) $(MAIN_BIN) $(TEST_CASES_DIR)/*
