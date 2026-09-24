# SCP - Practical Assignment 2: Classic Concurrency Problems

This repository is dedicated to the theoretical analysis, formal criteria, and practical C implementation of classic synchronization and concurrency problems using **POSIX Threads (`pthread`)**.

---

## 1. Covered Problems

The project is structured into modular directories, one for each concurrency problem:

| Directory | Problem | Status |
| :--- | :--- | :--- |
| [`dining_philosophers/`](./dining_philosophers/) | Dining Philosophers | Implemented (Dijkstra's Asymmetric Algorithm + FIFO Forks) |
| [`producer-consumer/`](./producer-consumer/) | Producer-Consumer | Implemented (Dijkstra's Counting Semaphore Bounded Buffer) |
| [`readers_writers/`](./readers_writers/) | Readers-Writers | Implemented (Starvation-Free Fair Turnstile Algorithm) |
| [`sleeping-barber/`](./sleeping-barber/) | Sleeping Barber | Implemented (Downey Multi-Barber Private Semaphore Algorithm) |

---

## 2. Standardized Directory Structure

Every problem directory follows the exact same architectural layout:

```
<problem_name>/
├── <problem_name>.md        # Theoretical specification, criteria, and solution details
├── Makefile                 # Standardized build system (targets: all, test, clean)
├── include/                 # Header files (.h) defining structs and function interfaces
├── src/                     # Core solution source files (.c) and main.c entry point
└── test/
    ├── test_runner.h        # Testing harness, invariant assertions, and watchdog timer
    ├── src/                 # Test case source code (.c)
    └── cases/               # Pre-compiled executable binaries ready to run
```

---

## 3. How to Build and Run

Each problem is completely self-contained with its own `Makefile`. To interact with any problem, navigate to its directory:

```bash
cd <problem_name>  
```

### 3.1 Build Everything (`make all`)
Compiles the main application binary as well as all test case executables in `test/cases/`:

```bash
make all
```

### 3.2 Running the Main Application
The main executable is produced in the root of the problem folder and accepts command-line arguments:

```bash
# Default execution
./<problem_name>

# Example with options (Dining Philosophers):
./dining_philosophers -n 5 -m 10 -v

# Supported options:
#   -n <num>       Number of entities/threads (default: 5)
#   -m <meals>     Number of meals/cycles per entity (default: 5)
#   -t <seconds>   Timed continuous duration mode (runs for T seconds)
#   -v             Verbose output (logs every state transition)
#   -help, -h      Display help message
```

### 3.3 Running the Automated Test Suite (`make test`)
```bash
make test
```

### 3.4 Running Individual Test Cases
```bash
cd test/cases
./case1_basic
./case2_starvation_prevention
./case3_high_contention
./case4_two_philosophers
```