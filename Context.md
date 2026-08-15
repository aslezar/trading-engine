# Low-Latency Trading Engine — Project Context

## Objective

Build a **minimal but realistic high-performance trading engine in C++** that processes real-time market data, makes trading decisions, and executes orders with low latency.

The project is primarily for learning and experimentation with:

* Low-latency C++
* Market data processing
* Order books
* Trading strategies
* Lock-free programming
* Multithreading
* CPU/cache optimization
* Network programming
* Kernel bypass
* Benchmarking and profiling
* HFT-style system architecture

This is **not intended to be a production trading system**.

---

# Development Philosophy

We will not optimize blindly.

The development loop is:

```text
IMPLEMENT
    ↓
BENCHMARK
    ↓
PROFILE
    ↓
IDENTIFY BOTTLENECK
    ↓
OPTIMIZE
    ↓
BENCHMARK AGAIN
```

Every significant optimization should have a measurable before/after comparison.

We will prefer:

* Simple designs
* Explicit data flow
* Low allocation
* Cache-friendly structures
* Deterministic behavior
* Minimal abstraction in hot paths
* Measurement over assumptions

We will **not jump directly to multithreading or DPDK**.

The system will evolve incrementally.

---

# Current Architecture

The current system is single-threaded.

```text
Market Data
     ↓
Order Book
     ↓
Strategy
     ↓
Order Manager
     ↓
Exchange Execution
```

The current flow is:

```text
Tick
 ↓
update OrderBook
 ↓
run Strategy
 ↓
generate Order
 ↓
assign Order ID
 ↓
execute Order
```

---

# Current Code

```cpp
#include <iostream>
#include <thread>
#include <chrono>
#include <functional>
#include <cstdint>
#include <cstdlib>
#include "./data.cpp"

// ---------- Exchange Simulator ----------

class ExchangeSimulator {
private:
    double price = 100.0;

public:
    static int TICK_RATE;

    void start(const std::function<void(const Tick&)>& onTick) {
        uint64_t ts = 0;

        while (true) {
            price += ((std::rand() % 100) - 50) * 0.001;

            Tick tick{
                price,
                price + 0.1,
                ts++
            };

            onTick(tick);

            std::this_thread::sleep_for(
                std::chrono::microseconds(1'000'000 / TICK_RATE)
            );
        }
    }

    void execute(const Order& order) {
        std::cout << "EXECUTED "
                  << (order.isBuy ? "BUY " : "SELL ")
                  << "id=" << order.id
                  << " price=" << order.price
                  << " qty=" << order.qty
                  << '\n';
    }
};

int ExchangeSimulator::TICK_RATE = 10;

// ---------- Order Book ----------

class OrderBook {
private:
    double bestBid = 0;
    double bestAsk = 0;

public:
    inline void update(const Tick& tick) {
        bestBid = tick.bid;
        bestAsk = tick.ask;
    }

    inline double bid() const {
        return bestBid;
    }

    inline double ask() const {
        return bestAsk;
    }
};

// ---------- Strategy ----------

class Strategy {
private:
    double spreadThreshold;

public:
    explicit Strategy(double threshold)
        : spreadThreshold(threshold) {}

    inline bool generate(
        const OrderBook& book,
        Order& order
    ) {
        const double spread = book.ask() - book.bid();

        if (spread > spreadThreshold) {
            order = {
                0,
                book.bid(),
                1,
                true
            };

            return true;
        }

        return false;
    }
};

// ---------- Order Manager ----------

class OrderManager {
private:
    int nextId = 1;

public:
    inline void prepare(Order& order) {
        order.id = nextId++;
    }
};

// ---------- Trading Engine ----------

class TradingEngine {
private:
    OrderBook book;
    Strategy strategy;
    OrderManager orderManager;
    ExchangeSimulator& exchange;

public:
    TradingEngine(
        ExchangeSimulator& exchange,
        double spreadThreshold
    )
        : strategy(spreadThreshold),
          exchange(exchange) {}

    inline void onTick(const Tick& tick) {
        book.update(tick);

        Order order;

        if (strategy.generate(book, order)) {
            orderManager.prepare(order);
            exchange.execute(order);
        }
    }

    void start() {
        exchange.start(
            [this](const Tick& tick) {
                onTick(tick);
            }
        );
    }
};

// ---------- Main ----------

int main() {
    ExchangeSimulator exchange;

    // 10 ticks/sec
    ExchangeSimulator::TICK_RATE = 10;

    TradingEngine engine(
        exchange,
        0.05
    );

    engine.start();

    return 0;
}
```

---

# Current Data Structures

The current `Tick` and `Order` structures are defined in `data.cpp`.

Conceptually:

```cpp
struct Tick {
    double bid;
    double ask;
    uint64_t ts;
};

struct Order {
    int id;
    double price;
    int qty;
    bool isBuy;
};
```

These structures are intentionally simple.

They will be improved later.

---

# Important Current Limitations

The current implementation is intentionally basic.

### ExchangeSimulator has two responsibilities

It currently:

1. Generates market data
2. Executes orders

These should eventually be separated into:

```text
MarketDataFeed
ExecutionSimulator
```

---

### `std::function` is currently used

```cpp
std::function<void(const Tick&)>
```

This is convenient but not ideal for a low-latency hot path.

It should eventually be removed.

---

### `double` is used for prices

We eventually want integer/tick-based prices.

Example:

```text
100.25 USD
    ↓
10025 price units
```

This avoids floating-point concerns and makes price comparisons cheaper and deterministic.

---

### `std::cout` is in the execution path

This is only for demonstration.

Console I/O must eventually be removed from the hot path because it introduces enormous and unpredictable latency.

We will replace it with counters/statistics or a separate logging path.

---

### An `Order` is created on every tick

Eventually we should reuse/preallocate objects where appropriate.

---

### `ts` is not actually a timestamp

Currently:

```cpp
uint64_t ts = 0;
ts++;
```

This is effectively a sequence number.

We eventually want both:

```text
sequence number
receive timestamp
```

---

### Tick rate currently uses `sleep_for`

This is useful for the simulator but must not be used for serious performance benchmarking.

Performance benchmarks should feed a large number of messages as quickly as possible.

---

# Performance Roadmap

## Stage 0 — Baseline

Start from the current implementation.

Goals:

* Keep the architecture simple
* Add latency instrumentation
* Measure current performance
* Establish the baseline

Measure:

```text
tick → order generated
tick → order executed
throughput
```

Latency statistics:

```text
min
p50
p90
p95
p99
p99.9
p99.99
max
```

This is the most important immediate step.

---

# Stage 1 — Clean Architecture

Separate responsibilities:

```text
MarketDataFeed
        ↓
TradingEngine
        ↓
OrderBook
        ↓
Strategy
        ↓
OrderManager
        ↓
ExecutionSimulator
```

Remove the unnecessary coupling between market-data generation and execution.

Benchmark after the refactor to ensure performance hasn't degraded.

---

# Stage 2 — Data Optimization

Improve the data representation.

Potential changes:

* `double` → integer price representation
* Compact structs
* Better struct layout
* Reduce unnecessary copies
* Reuse objects
* Avoid dynamic allocation
* Improve cache locality

Introduce microbenchmarks.

Benchmark:

```text
OrderBook::update()
Strategy::generate()
OrderManager::prepare()
```

---

# Stage 3 — Hot-Path Optimization

Remove unnecessary overhead:

* `std::function`
* Console I/O
* Heap allocations
* Unnecessary copies
* Unnecessary abstraction
* Expensive operations inside hot paths

Compile optimized builds:

```bash
-O3 -march=native
```

Create A/B benchmarks.

Example:

```text
Version A: baseline
Version B: optimization

Compare:
p50
p99
p99.9
throughput
```

An optimization should not be considered successful unless the benchmark demonstrates an improvement or a useful trade-off.

---

# Stage 4 — Realistic Order Book

Current order book:

```text
bestBid
bestAsk
```

Move toward multiple price levels:

```text
Bids                 Asks

100.00 × 50          100.01 × 30
 99.99 × 20          100.02 × 45
 99.98 × 70          100.03 × 15
```

Initially keep the implementation simple.

Benchmark:

```text
update
lookup
insert
remove
```

---

# Stage 5 — Benchmark Infrastructure

Create dedicated benchmark infrastructure.

Possible structure:

```text
benchmarks/
    micro/
        market_data_bench
        order_book_bench
        strategy_bench
        order_manager_bench
        queue_bench

    end_to_end/
        trading_engine_bench
```

Maintain results:

```text
results/
    baseline.txt
    v1.txt
    v2.txt
    ...
```

Each benchmark report should contain:

```text
Version
Hardware
CPU
OS
Compiler
Compiler flags
Workload
Message count
Throughput
p50
p90
p99
p99.9
p99.99
max
```

---

# Stage 6 — Lock-Free Queues

Introduce a fixed-size SPSC ring buffer.

Architecture:

```text
Market Data
     ↓
SPSC Ring Buffer
     ↓
Strategy
```

Avoid locks where possible.

Learn:

* Atomics
* Memory ordering
* Ring buffers
* Cache-line alignment
* False sharing

Benchmark:

```text
push latency
pop latency
round-trip latency
throughput
```

Compare:

```text
mutex-based queue
vs
lock-free SPSC queue
```

---

# Stage 7 — Multithreading

Move toward:

```text
Market Data Thread
        ↓
     SPSC Queue
        ↓
Strategy Thread
        ↓
     SPSC Queue
        ↓
Execution Thread
```

Important:

Multithreading is **not automatically an optimization**.

It may:

* Increase throughput
* Increase latency
* Increase tail latency
* Introduce synchronization overhead

Therefore compare directly against the single-threaded version.

Primary metrics:

```text
p50
p99
p99.9
p99.99
throughput
```

---

# Stage 8 — CPU and Cache Optimization

Investigate:

* CPU affinity
* Thread pinning
* Cache misses
* False sharing
* Cache-line alignment
* Branch prediction
* NUMA
* CPU frequency
* Context switches
* CPU migrations

Linux tools:

```bash
perf stat
perf record
perf report
```

Potentially use flamegraphs.

At this stage, tail latency becomes increasingly important.

---

# Stage 9 — Real Networking

Replace the direct simulator callback with real networking.

Initially:

```text
UDP
 ↓
MarketDataFeed
 ↓
TradingEngine
```

Investigate:

* UDP
* Binary market-data messages
* Packet parsing
* Multicast concepts
* Socket buffers
* `recvmmsg`
* Busy polling

Measure:

```text
packet received
      ↓
parsed
      ↓
order book
      ↓
strategy
      ↓
order generated
```

---

# Stage 10 — OS / Network Tuning

Experiment with:

* CPU pinning
* IRQ affinity
* Socket configuration
* Busy polling
* Huge pages
* NUMA placement
* Scheduler behavior
* CPU frequency behavior

Benchmark every change individually.

---

# Stage 11 — Kernel Bypass

Only after understanding conventional networking.

Architecture:

```text
Normal:

NIC
 ↓
Kernel
 ↓
Socket
 ↓
Application


Kernel bypass:

NIC
 ↓
DPDK
 ↓
Application
```

Investigate:

* DPDK
* Huge pages
* Polling
* Zero-copy concepts
* NIC queues
* CPU affinity

Compare conventional networking against DPDK.

---

# Stage 12 — Optional Hardware Acceleration

Optional advanced stage:

```text
CPU
 ↓
FPGA / SmartNIC
```

This is not required for the core project.

The purpose is to understand where hardware acceleration fits into extremely latency-sensitive systems.

---

# Benchmarking Strategy

There is no single universal HFT benchmarking standard.

Different exchanges and trading systems use different latency definitions and reporting methodologies.

Therefore this project will define its own consistent measurements.

The most important metric for our engine is:

```text
Tick-to-Trade Latency
```

Conceptually:

```text
Market Data Received
        ↓
Order Book Update
        ↓
Strategy Decision
        ↓
Order Creation
        ↓
Order Sent
```

Measure:

```text
T_order - T_tick
```

---

# Benchmark Workloads

Do not benchmark using:

```cpp
sleep_for(...)
```

The simulator may use sleep to model real-time behavior, but performance benchmarks should process data as fast as possible.

Use workloads such as:

### Normal

```text
100K messages/sec
```

### Heavy

```text
1M messages/sec
```

### Stress

```text
Several million messages/sec
```

### Burst

```text
100K
100K
5M
5M
100K
```

Burst workloads are important for exposing queueing and tail-latency problems.

---

# Benchmark Types

## Microbenchmark

Measure individual components:

```text
OrderBook::update()
Strategy::generate()
Queue::push()
Queue::pop()
```

Google Benchmark can be used for this.

## End-to-End Benchmark

Measure the complete pipeline:

```text
Tick
 ↓
OrderBook
 ↓
Strategy
 ↓
Order
```

This is more representative of the actual engine.

## Stress Benchmark

Run millions of messages and determine:

* Maximum sustainable throughput
* Queue growth
* Latency degradation
* Tail latency

---

# Benchmark Output

A benchmark should eventually produce something similar to:

```text
================================================
Trading Engine Benchmark
================================================

Version: V2
CPU: <CPU>
Cores: <cores>
OS: <OS>
Compiler: <compiler>
Build: -O3 -march=native

Messages:        10,000,000
Throughput:      X msg/sec

Tick → Order latency:

p50:             X ns
p90:             X ns
p99:             X ns
p99.9:           X ns
p99.99:          X ns
max:             X ns
================================================
```

Results should be compared between versions.

Example:

```text
                  p50       p99       throughput

Baseline          420ns     1.2us     1.8M/s
Integer price     350ns     900ns     2.1M/s
No std::function  310ns     700ns     2.4M/s
SPSC queue        280ns     600ns     3.1M/s
CPU pinning       280ns     410ns     3.1M/s
```

These numbers are examples only, not expected results.

---

# Repository Target Structure

Eventually:

```text
trading-engine/
│
├── src/
│   ├── market_data/
│   ├── orderbook/
│   ├── strategy/
│   ├── execution/
│   ├── engine/
│   └── common/
│
├── tests/
│
├── benchmarks/
│   ├── micro/
│   └── end_to_end/
│
├── simulator/
│
├── results/
│
├── docs/
│
├── CMakeLists.txt
│
└── CONTEXT.md
```

---

# Important Design Decisions

## Start single-threaded

The initial engine should remain single-threaded.

Reasons:

* Easier to reason about
* No synchronization
* No locks
* Easier debugging
* Establishes a strong baseline

Multithreading comes later.

## Avoid premature optimization

Do not add:

* DPDK
* lock-free queues
* CPU pinning
* NUMA tuning
* FPGA

until the previous stage has been measured and understood.

## Measure tail latency

Average latency is insufficient.

Always focus on:

```text
p50
p99
p99.9
p99.99
max
```

Especially after introducing concurrency and networking.

---

# Immediate Next Step

We are currently at **Stage 0**.

Do NOT redesign the whole system yet.

First:

1. Add proper latency measurement.
2. Add throughput measurement.
3. Remove/disable console output during benchmarking.
4. Run a fixed workload instead of an infinite real-time loop.
5. Produce a baseline benchmark report.
6. Then begin Stage 1 refactoring.

The first objective is:

```text
Current Code
     ↓
Baseline Benchmark
     ↓
Known Numbers
     ↓
Architecture Improvements
     ↓
Benchmark Again
```

This baseline will be the reference point for the entire project.
