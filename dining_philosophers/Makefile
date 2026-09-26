CC = gcc
CFLAGS = -Wall -Wextra -O2 -pthread -Iinclude -Itest
LDFLAGS = -pthread -lm

# Directories
SRC_DIR = src
INC_DIR = include
TEST_SRC_DIR = test/src
TEST_CASES_DIR = test/cases
BUILD_DIR = build

# Core object files
CORE_SRCS = $(SRC_DIR)/fork.c $(SRC_DIR)/philosopher.c
CORE_OBJS = $(BUILD_DIR)/fork.o $(BUILD_DIR)/philosopher.o

# Main binary
MAIN_BIN = dining_philosophers
MAIN_SRC = $(SRC_DIR)/main.c
MAIN_OBJ = $(BUILD_DIR)/main.o

# Test binaries
TEST_CASES = $(TEST_CASES_DIR)/case1_basic \
             $(TEST_CASES_DIR)/case2_starvation_prevention \
             $(TEST_CASES_DIR)/case3_high_contention \
             $(TEST_CASES_DIR)/case4_two_philosophers

.PHONY: all test clean dirs

all: dirs $(MAIN_BIN) $(TEST_CASES)

dirs:
	@mkdir -p $(BUILD_DIR)
	@mkdir -p $(TEST_CASES_DIR)

# Compilation of core objects
$(BUILD_DIR)/fork.o: $(SRC_DIR)/fork.c $(INC_DIR)/fork.h
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/philosopher.o: $(SRC_DIR)/philosopher.c $(INC_DIR)/philosopher.h $(INC_DIR)/fork.h $(INC_DIR)/protocol.h
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/main.o: $(MAIN_SRC) $(INC_DIR)/philosopher.h $(INC_DIR)/protocol.h
	$(CC) $(CFLAGS) -c $< -o $@

# Link main binary
$(MAIN_BIN): $(CORE_OBJS) $(MAIN_OBJ)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

# Compilation of test binaries in test/cases/
$(TEST_CASES_DIR)/case1_basic: $(TEST_SRC_DIR)/case1_basic.c $(CORE_OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

$(TEST_CASES_DIR)/case2_starvation_prevention: $(TEST_SRC_DIR)/case2_starvation_prevention.c $(CORE_OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

$(TEST_CASES_DIR)/case3_high_contention: $(TEST_SRC_DIR)/case3_high_contention.c $(CORE_OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

$(TEST_CASES_DIR)/case4_two_philosophers: $(TEST_SRC_DIR)/case4_two_philosophers.c $(CORE_OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

test: all
	@echo "=============================================================="
	@echo "               RUNNING DINING PHILOSOPHERS SUITE              "
	@echo "=============================================================="
	@for test_case in $(TEST_CASES); do \
		echo ""; \
		$$test_case || exit 1; \
	done
	@echo ""
	@echo "=============================================================="
	@echo "       ALL DINING PHILOSOPHERS TEST CASES PASSED!             "
	@echo "=============================================================="

clean:
	rm -rf $(BUILD_DIR) $(MAIN_BIN) $(TEST_CASES_DIR)/*
