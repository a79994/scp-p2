# Sleeping-Barber Problem

## 1. Problem Description and Concurrency Dilemma
The Sleeping-Barber problem models a multi-server queueing system with bounded waiting capacity, dynamic worker sleep/wake scheduling, and client balking (rejection upon saturation).

* **Original Formulation vs. Modern Generalization:**
  * **Original Dijkstra Formulation (1965):** In his foundational manuscript EWD123 (*Cooperating Sequential Processes*), Edsger W. Dijkstra introduced the problem with a **single barber** ($K = 1$), a single barber chair, and a waiting room with $M$ chairs, protected by an exclusive entry/exit sliding door.
  * **The Generalized Multi-Barber Problem ($K \ge 1$):** In modern operating systems textbooks (e.g., Stallings, Tanenbaum, Downey's *Little Book of Semaphores*), the scenario is standardly generalized to **$K$ barbers**, **$K$ barber chairs**, and **$M$ waiting chairs**. This models concurrent multi-server worker pools (such as thread pools with bounded task queues).

* **The Scenario:** A barbershop consists of a cutting room with $K$ barbers and $K$ barber chairs, plus an adjacent waiting room equipped with a finite capacity of $M$ customer chairs.
* **Behavioral Dynamics:**
  * **Barber Behavior:** If there are no customers waiting, any idle barber sits down in a barber chair and falls asleep (*Idle/Blocked*). When a customer arrives or is waiting, a sleeping barber is awakened to perform the haircut. Upon finishing a haircut, the barber checks the waiting room: if customers remain, the barber summons the next customer; otherwise, the barber goes back to sleep.
  * **Customer Behavior:**
    * If at least one barber is asleep/idle, the customer wakes one barber up and sits in an available barber chair to be served immediately.
    * If all $K$ barbers are busy cutting hair:
      * If an empty chair exists in the waiting room ($count < M$), the customer sits down and waits for their turn.
      * If all $M$ waiting chairs are occupied ($count = M$), the customer immediately leaves without a haircut (**balking**).
* **The Concurrency Dilemmas:**
  1. **Lost Wakeups & Sleep/Wake Race Conditions:** If a customer inspects the shop status while a barber is actively transitioning from awake to asleep, the wakeup signal can be lost, leaving both the barber and the waiting customer permanently blocked.
  2. **Waiting Room Atomic Integrity:** Concurrently arriving customers must inspect and increment/decrement the available chair count atomically to prevent race conditions that overbook the waiting room.
  3. **Barber Contention (Multi-Barber Setting, $K > 1$):** When multiple barbers are idle or finish simultaneously, they must not contend destructively for the same waiting customer; exactly one barber must claim the customer.
  4. **Customer-to-Barber Rendezvous (Multi-Barber Setting, $K > 1$):** In a shop with multiple barbers, a customer cannot simply wait on a single global "haircut done" signal, as multiple haircuts occur in parallel. A customer must be deterministically matched to a specific barber chair $k \in \{0, \dots, K-1\}$ and synchronize exclusively with their assigned barber through a private double handshake (sit $\to$ cut $\to$ finished $\to$ depart).

---

## 2. Fundamental Criteria

### 2.1 Active Entities
* **The Barbers:** A pool of $K$ concurrent service provider threads ($K \ge 1$). Each barber alternates between sleeping (blocked waiting for customers) and serving (executing haircuts in their designated chair).
* **Customers:** Asynchronous client threads arriving at non-deterministic intervals that either enter service immediately, join the bounded waiting queue, or balk.

### 2.2 Scarce Resources
* **Barber Chairs:** Exactly $K$ units; the strictly exclusive physical execution slots where the service transaction occurs.
* **Waiting Room Chairs:** A finite set of $M$ slots; temporary buffer capacity that prevents immediate client rejection.
* **Barber Service Bandwidth:** Up to $K$ concurrent execution pipelines operating in parallel.

---

### 2.3 Real-World Example
* **Thread Pool Work Queues with Bounded Capacity and Rejection Execution Handlers:**
  * In production network runtimes and server frameworks (such as Java's `ThreadPoolExecutor` or Netty event loops), a pool of $K$ worker threads represents the **$K$ Barbers**.
  * Incoming client requests or HTTP connections represent the **Customers**.
  * When no tasks are incoming, worker threads block on an internal condition variable or queue head (`WAITING`/asleep).
  * Arriving tasks wake an idle worker thread. If all $K$ worker threads are fully occupied processing long-running requests, subsequent tasks are enqueued in a bounded blocking queue of capacity $M$ (the **Waiting Chairs**).
  * Once this internal queue reaches capacity ($M$), any subsequent arriving request triggers an explicit rejection policy (e.g., `AbortPolicy` throwing `RejectedExecutionException`, or HTTP 503 Service Unavailable), which directly mirrors customer **balking**.

---

## 3. Well-Known Solutions

1. **Dijkstra's Canonical Three-Semaphore Pattern (Single Barber, $K=1$):**
   * *Mechanism:* Uses three synchronization primitives:
     1. `customers` semaphore (initialized to 0): increments when a customer enters the waiting room and wakes the barber.
     2. `barber` semaphore (initialized to 0): signals that the barber is ready to accept a customer.
     3. `mutex` binary semaphore (initialized to 1): guarantees mutual exclusion around the shared counter `waiting_chairs_count`.
   * *Outcome:* Elegantly solves lost wakeups and atomic waiting room capacity tracking for the single-server baseline without busy waiting.

2. **Multi-Barber Synchronization with Queue Rendezvous ($K > 1$):**
   * *Mechanism:* Extends the model to coordinate multiple barbers:
     1. A counting semaphore `barbers_available` (initialized to $K$) tracking idle barbers.
     2. A counting semaphore `customers_waiting` (initialized to 0).
     3. An array of private semaphores (or a thread-safe FIFO queue of barber IDs) where a customer receives the specific ID of the barber assigned to them, establishing a 1-to-1 rendezvous.
     4. A bidirectional double handshake (`customer_seated`, `haircut_completed`) between the specific barber-customer pair.
   * *Outcome:* Completely eliminates cross-barber race conditions and ensures that each customer binds to exactly one chair and one barber.

3. **Monitor-Based Solution with Explicit Condition Variables:**
   * *Mechanism:* Encapsulates the entire shop state inside a monitor object using mutex locks and condition variables (`cond_barber_sleep`, `cond_customer_wait`, `cond_haircut_done[K]`). State variables explicitly record which chairs are occupied and which barbers are free.
   * *Outcome:* Avoids low-level semaphore signaling errors and simplifies implementing fair FIFO dispatching policies.

---

## 4. References and Original Problem

* **Original Single-Barber Formulation (Dijkstra, 1965):** Edsger W. Dijkstra introduced the problem in Section 4.2 ("The Superfluity of the General Semaphore") of his seminal manuscript, defining it with one barber, one barber chair, and an entry/exit sliding door:
  * Dijkstra, E. W. (1965). *Cooperating Sequential Processes* (EWD123). Technological University, Eindhoven. [http://www.cs.utexas.edu/users/EWD/transcriptions/EWD01xx/EWD123.html](http://www.cs.utexas.edu/users/EWD/transcriptions/EWD01xx/EWD123.html)
* **Multi-Barber Extensions and Semaphore Patterns:**
  * Downey, A. B. (2008). *The Little Book of Semaphores* (2nd ed.). Green Tea Press. Section 5.5: The Barbershop Problem and Multi-Barber generalizations. [https://greenteapress.com/semaphores/](https://greenteapress.com/semaphores/)
* **Queueing and Operating System Synchronization:**
  * Stallings, W. (2008). *Operating Systems: Internals and Design Principles* (6th ed.). Prentice Hall. Chapter 5: Concurrency: Mutual Exclusion and Synchronization.
* **Workload Distribution & Overdecomposition:**
  * Eijkhout, V. (2022). *The Art of HPC, Book 2: Parallel Programming for Science and Engineering*. [https://theartofhpc.com/pcse.html](https://theartofhpc.com/pcse.html)