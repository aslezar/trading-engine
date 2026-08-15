#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <iomanip>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "./data.cpp"

// ---------- Exchange Simulator ----------

class ExchangeSimulator {
private:
    double price = 100.0;
    bool verbose = true;

public:
    static int TICK_RATE;

    void setVerbose(bool enabled) {
        verbose = enabled;
    }

    Tick makeTick(uint64_t ts) {
        double move = ((std::rand() % 101) - 50) * 0.001;
        price += move;

        Tick tick{
            price,
            price + 0.1,
            ts
        };

        return tick;
    }

    void start(const std::function<void(const Tick&)>& onTick) {
        uint64_t ts = 0;

        while (true) {
            Tick tick = makeTick(ts++);
            onTick(tick);

            std::this_thread::sleep_for(
                std::chrono::microseconds(1'000'000 / TICK_RATE)
            );
        }
    }

    void execute(const Order& order) {
        if (!verbose) {
            return;
        }

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
    double bestBid = 0.0;
    double bestAsk = 0.0;

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

struct BenchmarkStats {
    uint64_t totalTicks = 0;
    uint64_t totalOrders = 0;
    uint64_t minNs = 0;
    uint64_t p50Ns = 0;
    uint64_t p90Ns = 0;
    uint64_t p95Ns = 0;
    uint64_t p99Ns = 0;
    uint64_t maxNs = 0;
    double throughput = 0.0;
};

static void printBenchmarkReport(const BenchmarkStats& stats) {
    std::cout << "==============================\n";
    std::cout << "Stage 0 Benchmark Report\n";
    std::cout << "==============================\n";
    std::cout << "Ticks: " << stats.totalTicks << '\n';
    std::cout << "Orders: " << stats.totalOrders << '\n';
    std::cout << "Throughput: " << std::fixed << std::setprecision(2)
              << stats.throughput << " ticks/sec\n";
    std::cout << "Latency (ns):\n";
    std::cout << "  min: " << stats.minNs << '\n';
    std::cout << "  p50: " << stats.p50Ns << '\n';
    std::cout << "  p90: " << stats.p90Ns << '\n';
    std::cout << "  p95: " << stats.p95Ns << '\n';
    std::cout << "  p99: " << stats.p99Ns << '\n';
    std::cout << "  max: " << stats.maxNs << '\n';
    std::cout << "==============================\n";
}

static uint64_t percentileValue(const std::vector<uint64_t>& values, double percent) {
    if (values.empty()) {
        return 0;
    }

    std::vector<uint64_t> sorted = values;
    std::sort(sorted.begin(), sorted.end());

    const double ratio = std::clamp(percent / 100.0, 0.0, 1.0);
    const size_t index = static_cast<size_t>(std::ceil(ratio * sorted.size()) - 1);

    return sorted[std::min(index, sorted.size() - 1)];
}

static BenchmarkStats runBenchmark(int tickCount) {
    ExchangeSimulator exchange;
    exchange.setVerbose(false);

    TradingEngine engine(exchange, 0.05);
    std::vector<uint64_t> latencies;
    latencies.reserve(static_cast<size_t>(tickCount));

    const auto start = std::chrono::steady_clock::now();
    uint64_t ordersGenerated = 0;

    for (int i = 0; i < tickCount; ++i) {
        const auto tickStart = std::chrono::steady_clock::now();

        Tick tick = exchange.makeTick(static_cast<uint64_t>(i));
        engine.onTick(tick);

        const auto tickEnd = std::chrono::steady_clock::now();
        const uint64_t elapsedNs =
            std::chrono::duration_cast<std::chrono::nanoseconds>(tickEnd - tickStart).count();

        latencies.push_back(elapsedNs);

        if (tick.ask - tick.bid > 0.05) {
            ++ordersGenerated;
        }
    }

    const auto end = std::chrono::steady_clock::now();
    const double elapsedMs =
        std::chrono::duration<double, std::milli>(end - start).count();

    BenchmarkStats stats;
    stats.totalTicks = static_cast<uint64_t>(tickCount);
    stats.totalOrders = ordersGenerated;

    if (!latencies.empty()) {
        std::vector<uint64_t> sorted = latencies;
        std::sort(sorted.begin(), sorted.end());

        stats.minNs = sorted.front();
        stats.p50Ns = percentileValue(latencies, 50.0);
        stats.p90Ns = percentileValue(latencies, 90.0);
        stats.p95Ns = percentileValue(latencies, 95.0);
        stats.p99Ns = percentileValue(latencies, 99.0);
        stats.maxNs = sorted.back();
    }

    stats.throughput = elapsedMs > 0.0
                           ? (static_cast<double>(tickCount) * 1000.0) / elapsedMs
                           : 0.0;

    return stats;
}

// ---------- Main ----------

enum class RunMode {
    Benchmark,
    Live
};

static void runLiveMode() {
    ExchangeSimulator exchange;
    ExchangeSimulator::TICK_RATE = 10;

    TradingEngine engine(exchange, 0.05);
    std::cout << "Live mode: streaming tick data\n";
    engine.start();
}

static RunMode parseMode(int argc, char** argv) {
    if (argc > 1) {
        std::string arg = argv[1];
        if (arg == "--live") {
            return RunMode::Live;
        }
        if (arg == "--benchmark") {
            return RunMode::Benchmark;
        }
    }

    return RunMode::Benchmark;
}

int main(int argc, char** argv) {
    const RunMode mode = parseMode(argc, argv);

    if (mode == RunMode::Live) {
        runLiveMode();
        return 0;
    }

    // Stage 1 boundary: later we will split feed, strategy, order book, and execution.
    const int tickCount = 100000;
    const BenchmarkStats stats = runBenchmark(tickCount);
    printBenchmarkReport(stats);

    return 0;
}