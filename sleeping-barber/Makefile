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
CORE_SRCS = $(SRC_DIR)/barbershop.c
CORE_OBJS = $(BUILD_DIR)/barbershop.o

# Main binary
MAIN_BIN = sleeping_barber
MAIN_SRC = $(SRC_DIR)/main.c
MAIN_OBJ = $(BUILD_DIR)/main.o

# Test binaries
TEST_CASES = $(TEST_CASES_DIR)/case1_basic \
             $(TEST_CASES_DIR)/case2_multi_barber \
             $(TEST_CASES_DIR)/case3_balking_under_saturation \
             $(TEST_CASES_DIR)/case4_high_concurrency

.PHONY: all test clean dirs

all: dirs $(MAIN_BIN) $(TEST_CASES)

dirs:
	@mkdir -p $(BUILD_DIR)
	@mkdir -p $(TEST_CASES_DIR)

# Compilation of core objects
$(BUILD_DIR)/barbershop.o: $(SRC_DIR)/barbershop.c $(INC_DIR)/barbershop.h
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/main.o: $(MAIN_SRC) $(INC_DIR)/barbershop.h
	$(CC) $(CFLAGS) -c $< -o $@

# Link main binary
$(MAIN_BIN): $(CORE_OBJS) $(MAIN_OBJ)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

# Compilation of test binaries in test/cases/
$(TEST_CASES_DIR)/case1_basic: $(TEST_SRC_DIR)/case1_basic.c $(CORE_OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

$(TEST_CASES_DIR)/case2_multi_barber: $(TEST_SRC_DIR)/case2_multi_barber.c $(CORE_OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

$(TEST_CASES_DIR)/case3_balking_under_saturation: $(TEST_SRC_DIR)/case3_balking_under_saturation.c $(CORE_OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

$(TEST_CASES_DIR)/case4_high_concurrency: $(TEST_SRC_DIR)/case4_high_concurrency.c $(CORE_OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

test: all
	@echo "=============================================================="
	@echo "                RUNNING SLEEPING-BARBER SUITE                 "
	@echo "=============================================================="
	@for test_case in $(TEST_CASES); do \
		echo ""; \
		$$test_case || exit 1; \
	done
	@echo ""
	@echo "=============================================================="
	@echo "        ALL SLEEPING-BARBER TEST CASES PASSED!                "
	@echo "=============================================================="

clean:
	rm -rf $(BUILD_DIR) $(MAIN_BIN) $(TEST_CASES_DIR)/*
