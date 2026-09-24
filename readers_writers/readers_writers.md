# Readers-Writers Problem

## 1. Problem Description and Concurrency Dilemma
The Readers-Writers problem formalizes concurrent access to a shared resource (such as a database, memory map, or filesystem) where some entities only inspect the data while others modify it.

* **The Scenario:** A shared data repository is accessed by two distinct classes of concurrent entities: **Readers** and **Writers**.
* **The Concurrency Dilemma:**
  * **Read-Read Concurrency:** Reading does not alter the underlying data structure (it is idempotent and side-effect free). Multiple readers should be allowed to access the data concurrently without contention.
  * **Write-Exclusive Access:** Modifying data is state-altering and destructive. A writer requires **strict mutual exclusion**: no other writer may write concurrently, and no reader may read concurrently while an update is in progress (preventing dirty reads and race conditions).
  * **Arbitration Policy Dilemma:** Determining priority between readers and writers inevitably introduces performance tradeoffs and the danger of thread starvation.

---

## 2. Fundamental Criteria

### 2.1 Active Entities
* **Readers:** Concurrent processes/threads that query the shared dataset without modifying its internal representation.
* **Writers:** Concurrent processes/threads that mutate, overwrite, or delete elements of the shared dataset.

### 2.2 Scarce Resources
* **Exclusive Access Window:** The exclusive lock/token to the underlying shared data structure during a mutation phase. When claimed, it completely excludes all other active entities across the system.

### 2.3 Real-World Example
* **In-Memory Cache Invalidation in Distributed Systems (e.g., Redis):** In web-scale applications, millions of incoming user requests simultaneously inspect cached product metadata, authorization session tokens, or stock inventories without altering memory state (concurrent Readers). When an administrator updates a product price or an inventory level is decremented after checkout (Writer), the write operation must obtain exclusive access to the cache slot to invalidate or overwrite the key. Any concurrent reads during this window risk serving stale or half-written cache lines, requiring read-write locks (`pthread_rwlock`) or atomic compare-and-swap primitives to preserve consistency across worker threads.

---

## 3. Well-Known Solutions

1. **Courtois-Heymans-Parnas First Readers-Writers Algorithm (Reader-Preference)**
   * *Proposed by:* Pierre-Jacques Courtois, Frans Heymans, and David L. Parnas (1971)
   * *Bibliographic Reference:* Courtois, P. J., Heymans, F., & Parnas, D. L. (1971). *Concurrent Control with "Readers" and "Writers"*. Communications of the ACM, 14(10), 667–668. [https://doi.org/10.1145/362759.362813](https://doi.org/10.1145/362759.362813)
   * *Mechanism:* Employs two binary semaphores (`mutex` and `wsem`, both initialized to 1) and an integer counter `readcount`. The first reader to arrive (`readcount == 1`) executes `P(wsem)`, locking the resource against writers. Subsequent readers increment `readcount` and proceed without touching `wsem`. The last reader to exit (`readcount == 0`) executes `V(wsem)`, releasing the resource.
   * *Tradeoff:* Delivers maximum read concurrency and throughput, but causes **severe writer starvation** if a continuous stream of overlapping readers arrives.

2. **Courtois-Heymans-Parnas Second Readers-Writers Algorithm (Writer-Preference)**
   * *Proposed by:* Pierre-Jacques Courtois, Frans Heymans, and David L. Parnas (1971)
   * *Bibliographic Reference:* Courtois, P. J., Heymans, F., & Parnas, D. L. (1971). *Concurrent Control with "Readers" and "Writers"*. Communications of the ACM, 14(10), 667–668. [https://doi.org/10.1145/362759.362813](https://doi.org/10.1145/362759.362813)
   * *Mechanism:* Introduces a `writecount` tracker alongside `readcount`, coordinated via five semaphores (`mutex1`, `mutex2`, `mutex3`, `wsem`, and `rsem`). As soon as the first writer signals its intention (`writecount == 1`), incoming readers are blocked at `rsem`. Existing active readers finish, the waiting writer executes, and readers only resume when all waiting writers have finished.
   * *Tradeoff:* Guarantees prompt data updates and prevents writer starvation, but can cause **reader starvation** in write-heavy environments.

3. **Starvation-Free Readers-Writers Algorithm (Third Problem / Turnstile Pattern)**
   * *Proposed by:* Kenneth A. Reek (2004), Allen B. Downey (2005/2008), and Jalal Kawash (2004)
   * *Bibliographic Reference:*
     * Reek, K. A. (2004). *Design Patterns for Semaphores*. Proceedings of the 35th SIGCSE Technical Symposium on Computer Science Education, 36(1), 288–292. [https://doi.org/10.1145/971300.971399](https://doi.org/10.1145/971300.971399)
     * Downey, A. B. (2008). *The Little Book of Semaphores* (2nd ed.). Green Tea Press. Section 4.2.5: "No-starve readers-writers". [https://greenteapress.com/semaphores/](https://greenteapress.com/semaphores/)
     * Kawash, J. (2004). *Process Synchronization with Readers and Writers Revisited*. Proceedings of the International Conference on Parallel and Distributed Processing Techniques and Applications (PDPTA'04), Las Vegas, Nevada.
   * *Mechanism:* Introduces a "turnstile" binary semaphore (initialized to 1) at the entry point. Every incoming entity (reader or writer) must pass through the turnstile. If a writer is waiting, it holds the turnstile, preventing newly arriving readers from jumping ahead of the waiting writer.
   * *Tradeoff:* Completely **starvation-free** for both readers and writers, offering predictable service latency with a minor reduction in peak read concurrency.

4. **POSIX Read-Write Locks Standard (`pthread_rwlock`)**
   * *Proposed by:* IEEE Portable Applications Standards Committee & The Open Group (POSIX.1c-1995 / IEEE Std 1003.1-2001)
   * *Bibliographic Reference:*
     * IEEE Computer Society. (1995). *IEEE Std 1003.1c-1995: Information Technology - Portable Operating System Interface (POSIX) - Part 1: System Application Program Interface (API) - Amendment 2: Threads Extension (C Language)*. IEEE. [https://standards.ieee.org/ieee/1003.1c/1760/](https://standards.ieee.org/ieee/1003.1c/1760/)
     * IEEE & The Open Group. (2001). *The Open Group Base Specifications Issue 6 / IEEE Std 1003.1-2001 (POSIX.1)*. Section: Read-Write Locks (`pthread_rwlock_init`, `pthread_rwlock_rdlock`, `pthread_rwlock_wrlock`). [https://pubs.opengroup.org/onlinepubs/009695399/functions/pthread_rwlock_init.html](https://pubs.opengroup.org/onlinepubs/009695399/functions/pthread_rwlock_init.html)
   * *Mechanism:* Modern OS kernels provide native synchronization types (e.g., POSIX `pthread_rwlock_t`) configured with scheduling policies such as `PTHREAD_RWLOCK_PREFER_READER_NP` or `PTHREAD_RWLOCK_PREFER_WRITER_NONRECURSIVE_NP`, allowing kernel schedulers to select optimal fairness versus throughput trade-offs.

---

## 4. References and Original Problem

* **Original Problem Formulation (First & Second Variations):**
  * Courtois, P. J., Heymans, F., & Parnas, D. L. (1971). *Concurrent Control with "Readers" and "Writers"*. Communications of the ACM, 14(10), 667–668. [https://doi.org/10.1145/362759.362813](https://doi.org/10.1145/362759.362813)
* **Fair / Starvation-Free Synchronization Patterns:**
  * Reek, K. A. (2004). *Design Patterns for Semaphores*. Proceedings of the 35th SIGCSE Technical Symposium on Computer Science Education, 36(1), 288–292. [https://doi.org/10.1145/971300.971399](https://doi.org/10.1145/971300.971399)
  * Downey, A. B. (2008). *The Little Book of Semaphores* (2nd ed.). Green Tea Press. Section 4.2: Readers-Writers Problem and "No-starve readers-writers". [https://greenteapress.com/semaphores/](https://greenteapress.com/semaphores/)
  * Kawash, J. (2004). *Process Synchronization with Readers and Writers Revisited*. Proceedings of the International Conference on Parallel and Distributed Processing Techniques and Applications (PDPTA'04), Las Vegas, Nevada.
* **POSIX Synchronization Standards:**
  * IEEE & The Open Group. (2001). *The Open Group Base Specifications Issue 6 / IEEE Std 1003.1-2001 (POSIX.1)*. [https://pubs.opengroup.org/onlinepubs/009695399/](https://pubs.opengroup.org/onlinepubs/009695399/)
* **Queueing and Operating System Synchronization:**
  * Stallings, W. (2008). *Operating Systems: Internals and Design Principles* (6th ed.). Prentice Hall. Chapter 5: Concurrency: Mutual Exclusion and Synchronization.

---

## 5. Implemented Solution

The implemented solution is based on the **Starvation-Free Fair (Turnstile) Readers-Writers Algorithm (WKS 3)**:

* **Fair Ordering & Starvation Prevention (`turnstile`):**
  * `sem_t turnstile`: Initialized to 1. All incoming readers and writers must pass through the turnstile.
  * When a writer arrives, it holds `turnstile` while awaiting current readers to drain. Any subsequent readers that arrive block at the turnstile, preventing newly arriving readers from indefinitely bypassing the waiting writer and eliminating writer starvation.

* **Mutual Exclusion & Room Protection (`room_empty`):**
  * `sem_t room_empty`: Initialized to 1. Ensures that writers have exclusive access to the shared resource.
  * The first reader to enter (`readers_count == 1`) locks `room_empty`, barring writers from entering while readers are inspecting data.
  * The last reader to exit (`readers_count == 0`) unlocks `room_empty`, allowing a waiting writer to proceed.

* **Reader Counter Protection (`read_mutex`):**
  * `pthread_mutex_t read_mutex`: Guards increments and decrements of `readers_count` against concurrent modifications.

* **Invariant Guarantees:**
  * **Writer Mutual Exclusion:** At most one writer may modify the shared resource at any given time.
  * **No Dirty Reads:** Readers never inspect data during a writer's critical section.
  * **Concurrent Reading:** When no writer is present, multiple readers can concurrently access the resource.