#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include <functional>

#include "./data.cpp"

// ---------- Market Data Feed ----------

class MarketDataFeed
{
private:
    int64_t price = 10000;

public:
    using TickHandler = void (*)(void *context, const Tick &tick);

    static int TICK_RATE;

    Tick makeTick(uint64_t ts)
    {
        // Deterministic tick movement: same input gives same output.
        // This makes the benchmark easier to repeat and compare.
        const int64_t move = static_cast<int64_t>((ts % 11) - 5);
        price += move;

        Tick tick{
            price,
            price + 10,
            ts};

        return tick;
    }

    void start(TickHandler onTick, void *context)
    {
        uint64_t ts = 0;

        while (true)
        {
            Tick tick = makeTick(ts++);
            onTick(context, tick);

            std::this_thread::sleep_for(
                std::chrono::microseconds(1'000'000 / TICK_RATE));
        }
    }
};

int MarketDataFeed::TICK_RATE = 10;

// ---------- Execution Simulator ----------

class ExecutionSimulator
{
private:
    bool verbose = true;

public:
    void setVerbose(bool enabled)
    {
        verbose = enabled;
    }

    void execute(const Order &order)
    {
        if (!verbose)
        {
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

// ---------- Order Book ----------

class OrderBook
{
private:
    int64_t bestBid = 0;
    int64_t bestAsk = 0;

public:
    inline void update(const Tick &tick)
    {
        bestBid = tick.bid;
        bestAsk = tick.ask;
    }

    inline int64_t bid() const
    {
        return bestBid;
    }

    inline int64_t ask() const
    {
        return bestAsk;
    }
};

// ---------- Strategy ----------

class Strategy
{
private:
    int64_t spreadThreshold;

public:
    explicit Strategy(int64_t threshold)
        : spreadThreshold(threshold) {}

    inline bool generate(
        const OrderBook &book,
        Order &order)
    {
        const int64_t spread = book.ask() - book.bid();

        if (spread <= spreadThreshold)
        {
            return false;
        }

        order.id = 0;
        order.price = book.bid();
        order.qty = 1;
        order.isBuy = true;

        return true;
    }
};

// ---------- Order Manager ----------

class OrderManager
{
private:
    int nextId = 1;

public:
    inline void prepare(Order &order)
    {
        order.id = nextId++;
    }
};

// ---------- Trading Engine ----------

class TradingEngine
{
private:
    OrderBook book;
    Strategy strategy;
    OrderManager orderManager;
    MarketDataFeed &feed;
    ExecutionSimulator &execution;
    Order reusableOrder{};

public:
    TradingEngine(
        MarketDataFeed &feed,
        ExecutionSimulator &execution,
        int64_t spreadThreshold)
        : strategy(spreadThreshold),
          feed(feed),
          execution(execution) {}

    static void onTickStatic(void *context, const Tick &tick)
    {
        static_cast<TradingEngine *>(context)->onTick(tick);
    }

    inline void onTick(const Tick &tick)
    {
        book.update(tick);

        if (strategy.generate(book, reusableOrder))
        {
            orderManager.prepare(reusableOrder);
            execution.execute(reusableOrder);
        }
    }

    void start()
    {
        feed.start(&TradingEngine::onTickStatic, this);
    }
};

struct BenchmarkStats
{
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

static void printBenchmarkReport(const BenchmarkStats &stats)
{
    std::cout << "==============================\n";
    std::cout << "Stage 3 Benchmark Report\n";
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

static uint64_t measureNanoseconds(std::function<void()> fn)
{
    const auto start = std::chrono::steady_clock::now();
    fn();
    const auto end = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
}

static void runMicrobenchmarks()
{
    const int iterations = 1000000;

    OrderBook book;
    Tick tick{10000, 10010, 1};
    Strategy strategy(5);
    Order order{};
    OrderManager manager;

    const uint64_t orderBookNs = measureNanoseconds([&]()
                                                    {
        for (int i = 0; i < iterations; ++i) {
            book.update(tick);
        } });

    const uint64_t strategyNs = measureNanoseconds([&]()
                                                   {
        for (int i = 0; i < iterations; ++i) {
            order.id = 0;
            order.price = 0;
            order.qty = 0;
            order.isBuy = true;
            strategy.generate(book, order);
        } });

    const uint64_t orderManagerNs = measureNanoseconds([&]()
                                                       {
        for (int i = 0; i < iterations; ++i) {
            manager.prepare(order);
        } });

    std::cout << "==============================\n";
    std::cout << "Stage 2 Microbenchmarks\n";
    std::cout << "==============================\n";
    std::cout << "Iterations: " << iterations << '\n';
    std::cout << "OrderBook::update: " << orderBookNs << " ns\n";
    std::cout << "Strategy::generate: " << strategyNs << " ns\n";
    std::cout << "OrderManager::prepare: " << orderManagerNs << " ns\n";
    std::cout << "==============================\n";
}

static uint64_t percentileValue(const std::vector<uint64_t> &values, double percent)
{
    if (values.empty())
    {
        return 0;
    }

    std::vector<uint64_t> sorted = values;
    std::sort(sorted.begin(), sorted.end());

    const double ratio = std::clamp(percent / 100.0, 0.0, 1.0);
    const size_t index = static_cast<size_t>(std::ceil(ratio * sorted.size()) - 1);

    return sorted[std::min(index, sorted.size() - 1)];
}

static BenchmarkStats runBenchmark(int tickCount, int64_t spreadThreshold)
{
    MarketDataFeed feed;
    ExecutionSimulator execution;
    execution.setVerbose(false);

    TradingEngine engine(feed, execution, spreadThreshold);
    std::vector<uint64_t> latencies;
    latencies.reserve(static_cast<size_t>(tickCount));

    const auto start = std::chrono::steady_clock::now();
    uint64_t ordersGenerated = 0;

    for (int i = 0; i < tickCount; ++i)
    {
        const auto tickStart = std::chrono::steady_clock::now();

        Tick tick = feed.makeTick(static_cast<uint64_t>(i));
        engine.onTick(tick);

        const auto tickEnd = std::chrono::steady_clock::now();
        const uint64_t elapsedNs =
            std::chrono::duration_cast<std::chrono::nanoseconds>(tickEnd - tickStart).count();

        latencies.push_back(elapsedNs);

        if (tick.ask - tick.bid > spreadThreshold)
        {
            ++ordersGenerated;
        }
    }

    const auto end = std::chrono::steady_clock::now();
    const double elapsedMs =
        std::chrono::duration<double, std::milli>(end - start).count();

    BenchmarkStats stats;
    stats.totalTicks = static_cast<uint64_t>(tickCount);
    stats.totalOrders = ordersGenerated;

    if (!latencies.empty())
    {
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

enum class RunMode
{
    Benchmark,
    Live
};

static void runLiveMode()
{
    MarketDataFeed feed;
    ExecutionSimulator execution;
    MarketDataFeed::TICK_RATE = 10;

    TradingEngine engine(feed, execution, 5);
    std::cout << "Live mode: streaming tick data\n";
    engine.start();
}

static int parseTicks(int argc, char **argv)
{
    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg == "--ticks" && i + 1 < argc)
        {
            return std::stoi(argv[i + 1]);
        }
    }

    return 100000;
}

static RunMode parseMode(int argc, char **argv)
{
    if (argc > 1)
    {
        std::string arg = argv[1];
        if (arg == "--live")
        {
            return RunMode::Live;
        }
        if (arg == "--benchmark")
        {
            return RunMode::Benchmark;
        }
    }

    return RunMode::Benchmark;
}

int main(int argc, char **argv)
{
    const RunMode mode = parseMode(argc, argv);

    if (mode == RunMode::Live)
    {
        runLiveMode();
        return 0;
    }

    const int tickCount = parseTicks(argc, argv);
    const int64_t spreadThreshold = 5;
    const BenchmarkStats stats = runBenchmark(tickCount, spreadThreshold);

    std::cout << "Benchmark config: ticks=" << tickCount
              << ", threshold=" << spreadThreshold << '\n';
    printBenchmarkReport(stats);
    runMicrobenchmarks();

    return 0;
}