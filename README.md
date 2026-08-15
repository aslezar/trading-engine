# trading-engine

A simple, low-latency trading engine prototype written in C++.  
It processes mock market data, applies a basic strategy, and simulates order execution.

---

## Features

- Real-time tick processing (mock data)
- Lightweight order book (best bid/ask)
- Simple spread-based trading strategy
- Simulated order execution
- Single-threaded (low overhead baseline)

---

## How it works

```

Market Data → Order Book → Strategy → Order Manager → Exchange

````

- Market data provides ticks  
- Order book updates prices  
- Strategy checks trading condition  
- Orders are generated and sent  

---

## Build & Run

### Compile
```bash
g++ -O2 -std=c++20 main.cpp -o engine
````

### Run

```bash
./engine
```

---

## Example Output

```
BUY id=1 price=100 qty=1
BUY id=2 price=100.1 qty=1
```

---

## Strategy Logic

Trades when spread is large:

```
if (ask - bid > threshold) → BUY
```

---

## Notes

* No networking (mock data only)
* No persistence or risk checks
* Designed for learning low-latency basics

---

## Next Steps

* Add multi-threading
* Use real market data (UDP/FIX)
* Implement full order book
* Add latency measurement

---

## Roadmap

| Stage  | Focus                    | What we build                                     | Benchmarking                  |
| ------ | ------------------------ | ------------------------------------------------- | ----------------------------- |
| **0**  | Current baseline         | Current simulator + engine                        | **Baseline measurements**     |
| **1**  | Clean architecture       | Separate market data, execution, engine           | Basic latency instrumentation |
| **2**  | Data optimization        | Integer prices, compact structs, reusable objects | **Microbenchmarks**           |
| **3**  | Hot-path optimization    | Remove allocations, `std::function`, I/O          | **A/B benchmark suite**       |
| **4**  | Realistic Order Book     | Multiple price levels                             | Order-book benchmarks         |
| **5**  | Benchmark infrastructure | Dedicated benchmark framework + reports           | **Full benchmark harness**    |
| **6**  | Lock-free                | SPSC ring buffer                                  | Queue latency/throughput      |
| **7**  | Multithreading           | Market-data → strategy → execution threads        | End-to-end latency            |
| **8**  | CPU optimization         | Affinity, cache, false sharing, NUMA              | p50/p99/p99.9                 |
| **9**  | Networking               | UDP market-data feed                              | Network → order latency       |
| **10** | OS/network tuning        | Busy polling, kernel tuning, etc.                 | Tail-latency comparison       |
| **11** | Kernel bypass            | DPDK                                              | Full low-latency comparison   |
| **12** | Advanced                 | Optional FPGA/SmartNIC                            | Final comparison              |