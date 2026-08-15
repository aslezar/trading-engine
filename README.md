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

```
```
