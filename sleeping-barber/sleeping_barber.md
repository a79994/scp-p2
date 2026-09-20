# Sleeping-Barber Problem

## 1. Problem Description and Concurrency Dilemma
Introduced by Edsger Dijkstra, the Sleeping-Barber problem models a single-server queuing system with bounded waiting capacity and client balking (rejection).

* **The Scenario:** A barbershop consists of a cutting room with a single barber and a single barber chair, plus a waiting room equipped with $M$ customer chairs.
* **Behavioral Dynamics:**
  * If no customers are present, the barber sits down in the barber chair and goes to sleep (*Idle/Blocked*).
  * When a customer arrives:
    * If the barber is asleep, the customer wakes the barber up and sits in the barber chair to be served.
    * If the barber is busy:
      * If there is an empty chair in the waiting room, the customer sits down and waits.
      * If all $M$ waiting chairs are occupied, the customer immediately leaves without a haircut (**balking**).
  * Upon finishing a haircut, the barber inspects the waiting room: if customers are waiting, the barber summons one; otherwise, the barber returns to sleep.
* **The Concurrency Dilemma:**
  * **Lost Wakeups & Race Conditions:** If a customer checks the shop status while the barber is transitioning between awake and asleep, the wakeup signal can be lost, causing both the barber and the customer to remain blocked indefinitely.
  * **Waiting Room State Integrity:** Concurrently arriving customers must inspect and modify the available chair count atomically to avoid overbooking the waiting room.

---

## 2. Fundamental Criteria

### 2.1 Active Entities
* **The Barber:** A single service provider (worker thread) that alternates between sleeping (blocked waiting for work) and serving (executing customer haircuts).
* **Customers:** Asynchronous client entities (threads) arriving at unpredictable intervals that either enter the queue, get served immediately, or balk.

### 2.2 Scarce Resources
* **The Barber Chair:** Exactly 1 unit; the strictly exclusive resource where the actual service transaction takes place.
* **Waiting Room Chairs:** A finite set of $M$ slots; temporary capacity that prevents immediate client rejection.
* **Barber Processing Bandwidth:** A single sequential service pipeline.

---

## 2.3. Real-World Example
* **Helpdesk Call Center with Finite Hold Capacity:** A single customer support agent (the Barber) handles incoming telephone calls. When there are no callers, the agent is on standby. The telephony switchboard can hold up to 10 incoming calls in a waiting queue (Waiting Chairs). If an 11th caller dials in while the agent is busy and all 10 queue slots are occupied, the caller receives a busy tone or the call drops immediately (*balking*).

---

## 3. Well-Known Solutions

1. **Dijkstra's Three-Semaphore Pattern:**
   * *Mechanism:* Uses three synchronization constructs:
     1. `customers` semaphore (initialized to 0): tracks waiting customers and wakes up the barber.
     2. `barber` semaphore (initialized to 0): indicates the barber is ready to begin service.
     3. `mutex`: protects the shared variable `waiting_chairs_count` ensuring atomic check-and-decrement/increment decisions.
   * *Outcome:* Resolves state inspection race conditions and correctly models the sleep/wake cycle without busy waiting.

2. **Bidirectional Handshake (Rendezvous / Double Handshake):**
   * *Mechanism:* Introduces an explicit completion signal. The customer waits until the haircut is done, and the barber waits until the current customer acknowledges leaving before calling the next customer.
   * *Outcome:* Prevents desynchronization where an eager customer enters the chair before the previous occupant has departed.

3. **Monitor-Based Solution with Explicit Waiting Queues:**
   * *Mechanism:* The barbershop state is encapsulated within an object monitor. Methods such as `customer_arrives()` and `get_next_customer()` handle thread queuing using condition variables (`barber_available`, `customer_ready`, `chair_vacated`).
   * *Outcome:* Replaces low-level semaphore signaling with clear object-oriented state transitions and structured queue policies (such as strict FIFO customer dispatching).