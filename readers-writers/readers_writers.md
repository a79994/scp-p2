# Readers-Writers Problem (Distributed Message Broker Architecture)

## 1. Problem Description and Distributed Concurrency Dilemma
The Readers-Writers problem formalizes concurrent access to a shared resource (such as a database, distributed cache, or data repository) where some entities only inspect the data while others mutate it.

* **The Scenario:** A shared data repository is accessed by two distinct classes of concurrent entities: **Readers** and **Writers**.
* **Zero Shared Memory Constraint (Distributed Nodes):** The entities operate as completely isolated POSIX processes (as if located on physically distinct computers across a local area network or WAN). No shared memory segments (`shmget`, `mmap(MAP_SHARED)`, `shm_open`), kernel semaphores, or shared mutexes are permitted.
* **The Concurrency Dilemma:**
  * **Read-Read Concurrency:** Reading does not alter the underlying data structure (it is idempotent and side-effect free). Multiple readers must be allowed to query the data concurrently over the network without mutual contention.
  * **Write-Exclusive Access:** Modifying data is state-altering. A writer requires **strict mutual exclusion**: no other writer may write concurrently, and no reader may read concurrently while an update is in progress (preventing dirty reads, torn reads, and race conditions).
  * **Starvation-Freedom Dilemma:** In high-throughput distributed systems, an uninterrupted stream of reader queries can permanently starve writers, while an aggressive write stream can starve readers.

---

## 2. Fundamental Criteria

### 2.1 Active Entities
* **Message Broker:** Centralized message-oriented middleware and state custodian listening on a TCP/IP port (`AF_INET`). It manages the shared resource payload, enforces concurrency arbitration, and maintains real-time invariant telemetry.
* **Reader Processes:** Concurrent client processes that issue read requests (`MSG_FETCH`) to obtain data snapshots.
* **Writer Processes:** Concurrent client processes that publish mutation messages (`MSG_PUBLISH`) with new data payloads to the Broker's ingestion pipeline.

### 2.2 Scarce Resources
* **Exclusive Mutation Window on the Broker:** The exclusive access window during which the Broker serializes and commits an incoming write payload into the data store, excluding all concurrent queries.

### 2.3 Real-World Example
* **Distributed Key-Value Stores and Cache Brokers (e.g., Redis, Kafka, Distributed Caches):** In distributed microservices architectures, hundreds of services query shared catalog or user state across TCP sockets without holding local locks. When an inventory level or pricing configuration is updated, the write mutation is sent to the Message Broker, which atomically commits the new state and ensures no reader observes a partially updated record.

---

## 3. Well-Known Solutions

1. **Courtois-Heymans-Parnas First Readers-Writers Algorithm (Reader-Preference)**
   * *Proposed by:* Pierre-Jacques Courtois, Frans Heymans, and David L. Parnas (1971)
   * *Bibliographic Reference:* Courtois, P. J., Heymans, F., & Parnas, D. L. (1971). *Concurrent Control with "Readers" and "Writers"*. Communications of the ACM, 14(10), 667–668. [https://doi.org/10.1145/362759.362813](https://doi.org/10.1145/362759.362813)
   * *Mechanism:* Prioritizes incoming readers; subsequent readers bypass waiting writers.
   * *Tradeoff:* Maximizes read throughput, but causes **severe writer starvation**.

2. **Courtois-Heymans-Parnas Second Readers-Writers Algorithm (Writer-Preference)**
   * *Proposed by:* Pierre-Jacques Courtois, Frans Heymans, and David L. Parnas (1971)
   * *Bibliographic Reference:* Courtois, P. J., Heymans, F., & Parnas, D. L. (1971). *Concurrent Control with "Readers" and "Writers"*. Communications of the ACM, 14(10), 667–668. [https://doi.org/10.1145/362759.362813](https://doi.org/10.1145/362759.362813)
   * *Mechanism:* Blocks incoming readers as soon as a writer signals intention to write.
   * *Tradeoff:* Eliminates writer starvation, but causes **reader starvation** under continuous write workloads.

3. **Starvation-Free Fair (Turnstile) Readers-Writers Algorithm**
   * *Proposed by:* Kenneth A. Reek (2004), Allen B. Downey (2005/2008), and Jalal Kawash (2004)
   * *Bibliographic Reference:*
     * Reek, K. A. (2004). *Design Patterns for Semaphores*. Proceedings of the 35th SIGCSE Technical Symposium on Computer Science Education, 36(1), 288–292. [https://doi.org/10.1145/971300.971399](https://doi.org/10.1145/971300.971399)
     * Downey, A. B. (2008). *The Little Book of Semaphores* (2nd ed.). Green Tea Press. Section 4.2.5: "No-starve readers-writers". [https://greenteapress.com/semaphores/](https://greenteapress.com/semaphores/)
     * Kawash, J. (2004). *Process Synchronization with Readers and Writers Revisited*. Proceedings of the PDPTA'04 Conference, Las Vegas, Nevada. [https://doi.org/10.2498/cit.2005.01.05](https://doi.org/10.2498/cit.2005.01.05)
   * *Mechanism:* Implements a fair entrance turnstile through which all incoming entities must pass. A waiting writer holds the turnstile, preventing newly arriving readers from overtaking it.
   * *Tradeoff:* Completely **starvation-free** for both readers and writers.

4. **Message-Oriented Middleware and Distributed Coordination**
   * *Proposed by:* Birman & Joseph (1987); Coulouris et al. (2011)
   * *Bibliographic Reference:*
     * Coulouris, G., Dollimore, J., Kindberg, T., & Blair, G. (2011). *Distributed Systems: Concepts and Design* (5th ed.). Addison-Wesley. Chapter 6: Indirect Communication (Message Queuing and Publish-Subscribe). [http://www.cdk5.net/](http://www.cdk5.net/)
     * Birman, K. P., & Joseph, T. A. (1987). *Reliable communication in the presence of failures*. ACM Transactions on Computer Systems (TOCS), 5(1), 47–76. [https://doi.org/10.1145/7351.7478](https://doi.org/10.1145/7351.7478)
   * *Mechanism:* Eliminates reliance on shared physical memory by routing structured data packets over stream-oriented transport protocols (TCP/IP).

---

## 4. References and Bibliographic Citations

* **Original Problem Formulation (First & Second Variations):**
  * Dijkstra, E. W. (1965). *Cooperating Sequential Processes* (EWD123). Technological University, Eindhoven. [http://www.cs.utexas.edu/users/EWD/transcriptions/EWD01xx/EWD123.html](http://www.cs.utexas.edu/users/EWD/transcriptions/EWD01xx/EWD123.html)
  * Courtois, P. J., Heymans, F., & Parnas, D. L. (1971). *Concurrent Control with "Readers" and "Writers"*. Communications of the ACM, 14(10), 667–668. [https://doi.org/10.1145/362759.362813](https://doi.org/10.1145/362759.362813)
* **Starvation-Free Synchronization Patterns & The Turnstile Paradigm:**
  * Downey, A. B. (2008). *The Little Book of Semaphores* (2nd ed.). Green Tea Press. Section 4.2: Readers-Writers Problem and "No-starve readers-writers". [https://greenteapress.com/semaphores/](https://greenteapress.com/semaphores/)
* **Message-Oriented Middleware & Distributed Communication:**
  * Tanenbaum, A. S., & Van Steen, M. (2017). *Distributed Systems: Principles and Paradigms* (3rd ed.). CreateSpace Independent Publishing Platform. Chapter 4: Communication. [https://www.distributed-systems.net/index.php/books/ds3/](https://www.distributed-systems.net/index.php/books/ds3/)
* **Operating System Concurrency & Synchronization:**
  * Silberschatz, A., Galvin, P. B., & Gagne, G. (2018). *Operating System Concepts* (10th ed.). Wiley. Chapters 6 & 7: Synchronization Tools and Examples. [https://os-book.com/](https://os-book.com/)

---

## 5. Implemented Solution: Distributed Message Broker

The implemented architecture is based on **Process Isolation over TCP/IP** coupled with **Downey's Starvation-Free Fair Turnstile Algorithm** hosted within the Message Broker.

### 5.1 Zero Shared Memory Protocol
All interactions take place through structured fixed-size network frames over TCP stream sockets:

| Message Type | Direction | Payload Contents | Description |
| :--- | :--- | :--- | :--- |
| `MSG_REGISTER` | Client $\rightarrow$ Broker | Role (`READER`, `WRITER`, `ADMIN`), Client ID | Handshake upon connection. |
| `MSG_FETCH` | Reader $\rightarrow$ Broker | Client ID | Query request for the current resource snapshot. |
| `MSG_FETCH_RESP` | Broker $\rightarrow$ Reader | Data Array (`values[8]`), Version, Timestamp | Snapshot returned during concurrent read lease. |
| `MSG_PUBLISH` | Writer $\rightarrow$ Broker | Data Array (`values[8]`), Client ID | Write mutation submitted to Broker ingestion pipeline. |
| `MSG_WRITE_ACK` | Broker $\rightarrow$ Writer | Committed Version Number, Status | Confirmation that write was atomically committed. |
| `MSG_STATS_REQ` | Admin $\rightarrow$ Broker | - | Query real-time invariant telemetry and counters. |
| `MSG_SHUTDOWN` | Admin $\rightarrow$ Broker | - | Instructs the Broker to stop accepting queries. |

### 5.2 Starvation-Free Fairness on the Broker
1. **Entrance Turnstile (`turnstile`):** Every worker thread handling a client connection must pass through the turnstile mutex.
2. **Concurrent Reads (`readers_count` & `room_empty_cv`):**
   * Readers pass through the turnstile and increment `readers_count`.
   * The first reader locks `room_mutex`. Subsequent readers read concurrently without blocking each other.
   * Readers release `turnstile` immediately upon entry, allowing other readers to join concurrently.
3. **Starvation Prevention for Writers:**
   * When a writer arrives, it claims `turnstile` and awaits all existing readers in the room to exit.
   * Because the writer holds `turnstile`, **no newly arriving readers can pass**. They queue up behind the writer.
   * When the last existing reader exits, the writer enters the room, mutates the array payload, commits the new version, signals `room_empty_cv`, and unlocks `turnstile`.
4. **Starvation Prevention for Readers:**
   * Once the writer unlocks `turnstile`, waiting readers are admitted in FIFO order.

### 5.3 Invariant Verification
The Broker continuously monitors and asserts three critical invariants on every state transition:
* **Writer Mutual Exclusion:** `active_writers <= 1`.
* **Reader-Writer Exclusion:** `!(active_writers > 0 && active_readers > 0)`.
* **Write Atomicity & Data Consistency:** Payload elements are validated on every read to prove that no torn reads occur during writes.