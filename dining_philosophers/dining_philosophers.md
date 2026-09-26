# Dining Philosophers Problem

## 1. Problem Description and Concurrency Dilemma
Originally formulated by Edsger Dijkstra in 1965 as an examination question regarding resource contention in operating systems, the Dining Philosophers problem is the classic abstraction illustrating **deadlock** and **starvation** in concurrent systems.

* **The Scenario:** $N$ philosophers sit around a circular table. In front of each philosopher is a plate of food, and between each adjacent pair of philosophers lies exactly one fork, with a total of $N$ forks.
* **Behavior:** Each philosopher independently cycles through two states: **thinking** and **eating**. To eat, a philosopher requires exclusive access to **two forks**: specifically, the fork immediately to their left and the fork immediately to their right.
* **The Concurrency Dilemma:**
  * A philosopher cannot eat with only one fork, and no fork can be shared simultaneously.
  * If every philosopher becomes hungry at the exact same moment and picks up the fork to their left (or right) simultaneously, every philosopher holds one fork while waiting indefinitely for the neighboring fork to be released. This circular dependency results in an  **deadlock**.
  * Even if deadlock is avoided, an unfair scheduling policy can cause **starvation**, where one or more philosophers are perpetually bypassed and unable to eat.

---

## 2. Fundamental Criteria

### 2.1 Active Entities
* **Philosophers:** $N$ autonomous threads running concurrently. Each philosopher transitions dynamically between three internal states: *Thinking*, *Hungry* (attempting to acquire forks), and *Eating* (holding both resources).

### 2.2 Scarce Resources
* **Forks:** Exactly $N$ discrete, shared, mutually exclusive logical resources. The scarcity stems from the constraint that each entity must simultaneously acquire **two adjacent items** ($fork_i$ and $fork_{(i+1) \pmod N}$) to perform its critical section (*Eating*), precluding concurrent execution with immediate neighbors.

### 2.3 Real-World Example
* **Dijkstra's Original 1965 Hardware Dilemma (Dual-Drive Data Transfers):**
  * In Dijkstra's original hardware scenario, five mainframe computers arranged in a ring shared five magnetic tape drive peripherals positioned between them. A batch processing job on computer $i$ required reading from tape drive $i$ while simultaneously writing output to tape drive $(i+1) \pmod 5$. If every computer mounted its left tape drive and waited for its right tape drive to become free, a total system freeze occurred.

---

## 3. Well-Known Solutions

1. **Dijkstra's Asymmetric Solution (Direction-Alternating Symmetry Breaking)**
   * *Proposed by:* Edsger W. Dijkstra (1971)
   * *Bibliographic Reference:* Dijkstra, E. W. (1971). *Hierarchical ordering of sequential processes*. Acta Informatica, 1(2), 115–138. [https://doi.org/10.1007/BF00289519](https://doi.org/10.1007/BF00289519) (Also transcribed as [EWD310](http://www.cs.utexas.edu/users/EWD/transcriptions/EWD03xx/EWD310.html)).
   * *Mechanism:* The inherent symmetry of identical processes is broken. Even-numbered philosophers are instructed to pick up their left fork first and then their right fork, whereas odd-numbered philosophers pick up their right fork first and then their left.
   * *Outcome:* This eliminates the circular wait condition (one of Coffman's four deadlock conditions), ensuring at least one philosopher can acquire both forks and proceed.

2. **Havender-Dijkstra Resource Hierarchy Solution (Partial Order Allocation)**
   * *Proposed by:* J. W. Havender (1968) and Edsger W. Dijkstra (1971)
   * *Bibliographic Reference:*
     * Havender, J. W. (1968). *Avoiding Deadlock in Multitasking Systems*. IBM Systems Journal, 7(2), 74–84. [https://doi.org/10.1147/sj.72.0074](https://doi.org/10.1147/sj.72.0074)
     * Dijkstra, E. W. (1971). *Hierarchical ordering of sequential processes*. Acta Informatica, 1(2), 115–138. [https://doi.org/10.1007/BF00289519](https://doi.org/10.1007/BF00289519)
   * *Mechanism:* All resources (forks) are globally indexed from $0$ to $N-1$. Every philosopher must strictly request forks in ascending order of index (always acquire $\min(left, right)$ before attempting to acquire $\max(left, right)$).
   * *Outcome:* Prevents cycles in the resource allocation graph (RAG), mathematically proving the impossibility of a deadlock.

3. **Dijkstra's Concurrency Limiter (Multiplex / Bouncer Semaphore Solution)**
   * *Proposed by:* Edsger W. Dijkstra (1965)
   * *Bibliographic Reference:*
     * Dijkstra, E. W. (1965). *Cooperating Sequential Processes* (EWD123). Technological University, Eindhoven. Section 4.3. [http://www.cs.utexas.edu/users/EWD/transcriptions/EWD01xx/EWD123.html](http://www.cs.utexas.edu/users/EWD/transcriptions/EWD01xx/EWD123.html)
     * Downey, A. B. (2008). *The Little Book of Semaphores* (2nd ed.). Green Tea Press. Section 5.1: "Dining Philosophers". [https://greenteapress.com/semaphores/](https://greenteapress.com/semaphores/)
   * *Mechanism:* An arbiter or counting semaphore limits the maximum number of philosophers allowed to sit at the table simultaneously to $N-1$.
   * *Outcome:* By the Pigeonhole Principle, with at most $N-1$ entities competing for $N$ resources, at least one philosopher is guaranteed to obtain both forks, complete the eating phase, and release their resources.

4. **Tanenbaum's State-Based Two-Fork Semaphore Algorithm**
   * *Proposed by:* Andrew S. Tanenbaum (1987)
   * *Bibliographic Reference:*
     * Tanenbaum, A. S. (1987). *Operating Systems: Design and Implementation* (1st ed.). Prentice-Hall. Chapter 2: "Processes", pp. 70–74.
     * Tanenbaum, A. S., & Bos, H. (2015). *Modern Operating Systems* (4th ed.). Pearson. Chapter 2.5: "Classical IPC Problems", pp. 167–172.
   * *Mechanism:* Explicit states are tracked for every philosopher (*THINKING*, *HUNGRY*, *EATING*). A philosopher is only permitted to transition to *EATING* if neither of their immediate neighbors is currently eating (`test()` function). If blocked, the philosopher releases any claims and waits on a condition variable or individual synchronization barrier without retaining partial resources.
   * *Outcome:* Completely eliminates the "hold-and-wait" condition.

5. **Chandy-Misra Distributed Token Algorithm (Hygienic Philosophers)**
   * *Proposed by:* K. Mani Chandy and Jayadev Misra (1984)
   * *Bibliographic Reference:*
     * Chandy, K. M., & Misra, J. (1984). *The Drinking Philosophers Problem*. ACM Transactions on Programming Languages and Systems (TOPLAS), 6(4), 632–646. [https://doi.org/10.1145/863.864](https://doi.org/10.1145/863.864)
   * *Mechanism:* Completely eliminates centralized arbiters and asymmetric philosopher IDs by assigning dynamic clean/dirty tokens to each shared edge. When contending, an eating philosopher dirties their fork; a hungry neighbor requests it, causing the dirty fork to be cleaned and transferred.
   * *Outcome:* Completely distributed, starvation-free, and deadlock-free across arbitrary conflict graphs without global state.

---

## 4. References and Original Problem

* **Original Problem Formulation & Asymmetric Ordering:**
  * Dijkstra, E. W. (1971). *Hierarchical ordering of sequential processes*. Acta Informatica, 1(2), 115–138. [https://doi.org/10.1007/BF00289519](https://doi.org/10.1007/BF00289519) (Also transcribed as [EWD310](http://www.cs.utexas.edu/users/EWD/transcriptions/EWD03xx/EWD310.html)).
  * Dijkstra, E. W. (1965). *Cooperating Sequential Processes* (EWD123). Technological University, Eindhoven. [http://www.cs.utexas.edu/users/EWD/transcriptions/EWD01xx/EWD123.html](http://www.cs.utexas.edu/users/EWD/transcriptions/EWD01xx/EWD123.html)
* **The Dining Philosophers Metaphor:**
  * Hoare, C. A. R. (1978). *Communicating Sequential Processes*. Communications of the ACM, 21(8), 666–677. [https://doi.org/10.1145/359576.359585](https://doi.org/10.1145/359576.359585)
* **Resource Hierarchy & Deadlock Prevention Theory:**
  * Havender, J. W. (1968). *Avoiding Deadlock in Multitasking Systems*. IBM Systems Journal, 7(2), 74–84. [https://doi.org/10.1147/sj.72.0074](https://doi.org/10.1147/sj.72.0074)
  * Coffman, E. G., Elphick, M., & Shoshani, A. (1971). *System Deadlocks*. ACM Computing Surveys (CSUR), 3(2), 67–78. [https://doi.org/10.1145/356586.356588](https://doi.org/10.1145/356586.356588)
* **State-Based Mutex & Monitor Formulations:**
  * Tanenbaum, A. S. (1987). *Operating Systems: Design and Implementation*. Prentice-Hall. Chapter 2.
  * Tanenbaum, A. S., & Bos, H. (2015). *Modern Operating Systems* (4th ed.). Pearson. Chapter 2.5.
* **Distributed Synchronization & Graph Priority:**
  * Chandy, K. M., & Misra, J. (1984). *The Drinking Philosophers Problem*. ACM Transactions on Programming Languages and Systems (TOPLAS), 6(4), 632–646. [https://doi.org/10.1145/863.864](https://doi.org/10.1145/863.864)
* **Parallel Computing Context & Shared-Memory Contention:**
  * Eijkhout, V. (2022). *The Art of HPC, Book 1: The Science of Computing*. [https://theartofhpc.com/istc.html](https://theartofhpc.com/istc.html)
  * Eijkhout, V. (2022). *The Art of HPC, Book 2: Parallel Programming for Science and Engineering*. [https://theartofhpc.com/pcse.html](https://theartofhpc.com/pcse.html)

---

## 5. Implemented Solution

The implemented solution is based on an **autonomous process-based architecture** using **UNIX Domain Sockets (`AF_UNIX` via `socketpair`)** and a **Fair FIFO Request Ordering** mechanism managed by the Table Coordinator:

* **Concurrency Model (Processes via `fork()`):**
  * Philosophers are completely isolated child processes created via `fork()`.
  * **Zero Shared Memory:** There are no shared heap pointers or memory segments. Each process maintains its own address space.
  * Inter-Process Communication (IPC) is strictly message-passing over UNIX Domain sockets.

* **Deadlock Prevention (Coordinator Allocation):**
  * The Table Coordinator process arbitrates fork allocations based on non-preemptive availability and global request tickets.
  * A philosopher only transitions to the eating state when **both** adjacent forks are acquired atomically by the coordinator on its behalf, eliminating circular deadlocks.

* **Starvation Prevention (Fair FIFO Arrival Tickets):**
  * When a philosopher sends a request (`MSG_REQ_FORKS`), it is assigned a sequential ticket number.
  * The coordinator evaluates fork allocation strictly prioritizing earlier ticket requests, ensuring adjacent aggressive neighbors cannot bypass an intermediate hungry philosopher.
  * Guarantees bounded waiting time and eliminates starvation (empirically verified by test case 2).