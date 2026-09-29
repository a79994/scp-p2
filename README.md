# SCP - Practical Assignment 2: Classic Concurrency Problems

This repository is dedicated to the theoretical analysis, formal criteria, and practical C implementation of classic synchronization and concurrency problems using modern concurrency paradigms in Linux (both multi-process architectures with IPC and POSIX Threads).

---

## 1. Covered Problems

The project is structured into modular directories, one for each concurrency problem:

| Directory | Problem | Concurrency Model | Synchronization / IPC Mechanism |
| :--- | :--- | :--- | :--- |
| [`dining_philosophers/`](./dining_philosophers/) | Dining Philosophers | **Processes (`fork`)** | UNIX Domain Sockets (`AF_UNIX`) + Fair FIFO Coordinator |
| [`producer-consumer/`](./producer-consumer/) | Producer-Consumer | **Processes (`fork`)** | Dual UNIX Pipes IPC + Credit-Token Flow Control (Zero Shared Memory) |
| [`readers-writers/`](./readers-writers/) | Readers-Writers | **Processes (`fork` / Distributed)** | TCP Message Broker (`AF_INET`) + Ingestion Queue & Fair Turnstile (Zero Shared Memory) |
| [`sleeping-barber/`](./sleeping-barber/) | Sleeping Barber | Threads (`pthread`) | Downey Multi-Barber Private Semaphore Algorithm |

---

## 2. Standardized Directory Structure

Every problem directory follows the exact same architectural layout:

```
<problem_name>/
├── <problem_name>.md        # Theoretical specification, criteria, solutions, and bibliography
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

# Example with options (Producer-Consumer):
./producer_consumer -p 3 -c 3 -b 5 -i 30 -v

# Supported options (Producer-Consumer):
#   -p <num>       Number of producer processes (default: 2)
#   -c <num>       Number of consumer processes (default: 2)
#   -b <size>      Buffer capacity (default: 5)
#   -i <items>     Total items to produce (default: 20)
#   -v             Verbose output (logs each produce/consume with PID)
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
./case2_fast_producers_slow_consumers
./case3_slow_producers_fast_consumers
./case4_high_concurrency
```