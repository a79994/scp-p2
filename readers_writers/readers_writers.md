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
* **Flight Reservation and Ticketing System:** Thousands of users across the globe (Readers) continuously browse seat maps and check ticket availability in real time. At the exact moment a customer finalizes a purchase for a specific seat (Writer), the booking system must lock the seat record exclusively to verify payment and mark the seat as occupied, ensuring that two customers cannot purchase the identical seat simultaneously.

---

## 3. Well-Known Solutions

1. **First Readers-Writers (Reader-Preference / Reader Priority):**
   * *Mechanism:* Readers are prioritized. As long as at least one reader is inspecting the resource, incoming readers are immediately granted access, even if writers are waiting. The first reader locks the resource against writers, and the last reader releases it.
   * *Tradeoff:* Delivers maximum read throughput, but can result in **severe writer starvation** if a continuous stream of readers arrives.

2. **Second Readers-Writers (Writer-Preference / Writer Priority):**
   * *Mechanism:* As soon as a writer signals its intent to write, subsequent readers are queued and blocked from entering, even if existing readers are still active. Once all active readers exit, the waiting writer immediately executes.
   * *Tradeoff:* Ensures fresh data updates and prevents writer starvation, but can lead to **reader starvation** in write-heavy systems.

3. **Third Readers-Writers (Fair / FIFO Ordering):**
   * *Mechanism:* Employs a turnstile or FIFO queue mechanism. Requests from readers and writers are served strictly in order of arrival. Readers can read concurrently only if they arrive in an uninterrupted consecutive batch.
   * *Tradeoff:* Completely **starvation-free** for both classes of entities, with a predictable latency profile at the cost of slightly lower peak read throughput.

4. **Native OS Read-Write Locks (RW-Locks):**
   * *Mechanism:* Abstract data types provided by the operating system kernel (e.g., POSIX `pthread_rwlock_t`) that allow threads to request either shared (`rdlock`) or exclusive (`wrlock`) access, allowing the kernel scheduler to balance throughput and fairness.