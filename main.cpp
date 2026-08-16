#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <map>
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
    using BidLevels = std::map<int64_t, int64_t, std::greater<int64_t>>;
    using AskLevels = std::map<int64_t, int64_t, std::less<int64_t>>;

    BidLevels bids_;
    AskLevels asks_;
    int64_t bestBid_ = 0;
    int64_t bestAsk_ = 0;

public:
    inline void update(const Tick &tick)
    {
        bids_.clear();
        asks_.clear();

        bestBid_ = tick.bid;
        bestAsk_ = tick.ask;

        if (bestBid_ > 0)
        {
            bids_[bestBid_] = 1;
        }

        if (bestAsk_ > 0)
        {
            asks_[bestAsk_] = 1;
        }
    }

    inline void addBid(int64_t price, int64_t qty)
    {
        if (qty <= 0)
        {
            return;
        }

        bids_[price] += qty;
        if (price > bestBid_)
        {
            bestBid_ = price;
        }
    }

    inline void addAsk(int64_t price, int64_t qty)
    {
        if (qty <= 0)
        {
            return;
        }

        asks_[price] += qty;
        if (price < bestAsk_ || bestAsk_ == 0)
        {
            bestAsk_ = price;
        }
    }

    inline int64_t bid() const
    {
        return bids_.empty() ? bestBid_ : bids_.begin()->first;
    }

    inline int64_t ask() const
    {
        return asks_.empty() ? bestAsk_ : asks_.begin()->first;
    }

    inline int64_t bidQty() const
    {
        return bids_.empty() ? 0 : bids_.begin()->second;
    }

    inline int64_t askQty() const
    {
        return asks_.empty() ? 0 : asks_.begin()->second;
    }

    inline size_t bidLevels() const
    {
        return bids_.size();
    }

    inline size_t askLevels() const
    {
        return asks_.size();
    }

    inline bool hasLiquidity() const
    {
        return !bids_.empty() && !asks_.empty();
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

        if (spread <= spreadThreshold || !book.hasLiquidity())
        {
            return false;
        }

        order.id = 0;
        order.price = book.bid();
        order.qty = std::max<int64_t>(1, book.bidQty());
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
    uint64_t p999Ns = 0;
    uint64_t p9999Ns = 0;
    uint64_t maxNs = 0;
    double throughput = 0.0;
};

struct BenchmarkSuiteSummary
{
    int repeats = 0;
    double meanThroughput = 0.0;
    double meanMinNs = 0.0;
    double meanP50Ns = 0.0;
    double meanP90Ns = 0.0;
    double meanP95Ns = 0.0;
    double meanP99Ns = 0.0;
    double meanP999Ns = 0.0;
    double meanP9999Ns = 0.0;
    double meanMaxNs = 0.0;
    double stddevThroughput = 0.0;
};

static void printBenchmarkReport(const BenchmarkStats &stats)
{
    std::cout << "==============================\n";
    std::cout << "Stage 5 Benchmark Report\n";
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
    std::cout << "  p99.9: " << stats.p999Ns << '\n';
    std::cout << "  p99.99: " << stats.p9999Ns << '\n';
    std::cout << "  max: " << stats.maxNs << '\n';
    std::cout << "==============================\n";
}

static double meanValue(const std::vector<double> &values)
{
    if (values.empty())
    {
        return 0.0;
    }

    double total = 0.0;
    for (double value : values)
    {
        total += value;
    }

    return total / static_cast<double>(values.size());
}

static double stddevValue(const std::vector<double> &values)
{
    if (values.size() < 2)
    {
        return 0.0;
    }

    const double avg = meanValue(values);
    double variance = 0.0;
    for (double value : values)
    {
        const double diff = value - avg;
        variance += diff * diff;
    }

    return std::sqrt(variance / static_cast<double>(values.size() - 1));
}

static void printBenchmarkSuiteReport(const BenchmarkSuiteSummary &summary)
{
    std::cout << "==============================\n";
    std::cout << "Stage 5 Benchmark Suite\n";
    std::cout << "==============================\n";
    std::cout << "Repeats: " << summary.repeats << '\n';
    std::cout << "Mean throughput: " << std::fixed << std::setprecision(2)
              << summary.meanThroughput << " ticks/sec\n";
    std::cout << "Stddev throughput: " << std::fixed << std::setprecision(2)
              << summary.stddevThroughput << " ticks/sec\n";
    std::cout << "Mean latency (ns):\n";
    std::cout << "  min: " << summary.meanMinNs << '\n';
    std::cout << "  p50: " << summary.meanP50Ns << '\n';
    std::cout << "  p90: " << summary.meanP90Ns << '\n';
    std::cout << "  p95: " << summary.meanP95Ns << '\n';
    std::cout << "  p99: " << summary.meanP99Ns << '\n';
    std::cout << "  p99.9: " << summary.meanP999Ns << '\n';
    std::cout << "  p99.99: " << summary.meanP9999Ns << '\n';
    std::cout << "  max: " << summary.meanMaxNs << '\n';
    std::cout << "==============================\n";
}

static uint64_t measureNanoseconds(std::function<void()> fn)
{
    const auto start = std::chrono::steady_clock::now();
    fn();
    const auto end = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
}

static BenchmarkStats runBenchmark(int tickCount, int64_t spreadThreshold);

static BenchmarkSuiteSummary runBenchmarkSuite(int tickCount, int64_t spreadThreshold, int repeats)
{
    std::vector<BenchmarkStats> runs;
    runs.reserve(static_cast<size_t>(repeats));

    std::vector<double> throughputValues;
    throughputValues.reserve(static_cast<size_t>(repeats));
    std::vector<double> minValues;
    minValues.reserve(static_cast<size_t>(repeats));
    std::vector<double> p50Values;
    p50Values.reserve(static_cast<size_t>(repeats));
    std::vector<double> p90Values;
    p90Values.reserve(static_cast<size_t>(repeats));
    std::vector<double> p95Values;
    p95Values.reserve(static_cast<size_t>(repeats));
    std::vector<double> p99Values;
    p99Values.reserve(static_cast<size_t>(repeats));
    std::vector<double> p999Values;
    p999Values.reserve(static_cast<size_t>(repeats));
    std::vector<double> p9999Values;
    p9999Values.reserve(static_cast<size_t>(repeats));
    std::vector<double> maxValues;
    maxValues.reserve(static_cast<size_t>(repeats));

    for (int i = 0; i < repeats; ++i)
    {
        const BenchmarkStats stats = runBenchmark(tickCount, spreadThreshold);
        runs.push_back(stats);

        throughputValues.push_back(stats.throughput);
        minValues.push_back(static_cast<double>(stats.minNs));
        p50Values.push_back(static_cast<double>(stats.p50Ns));
        p90Values.push_back(static_cast<double>(stats.p90Ns));
        p95Values.push_back(static_cast<double>(stats.p95Ns));
        p99Values.push_back(static_cast<double>(stats.p99Ns));
        p999Values.push_back(static_cast<double>(stats.p999Ns));
        p9999Values.push_back(static_cast<double>(stats.p9999Ns));
        maxValues.push_back(static_cast<double>(stats.maxNs));
    }

    BenchmarkSuiteSummary summary;
    summary.repeats = repeats;
    summary.meanThroughput = meanValue(throughputValues);
    summary.meanMinNs = meanValue(minValues);
    summary.meanP50Ns = meanValue(p50Values);
    summary.meanP90Ns = meanValue(p90Values);
    summary.meanP95Ns = meanValue(p95Values);
    summary.meanP99Ns = meanValue(p99Values);
    summary.meanP999Ns = meanValue(p999Values);
    summary.meanP9999Ns = meanValue(p9999Values);
    summary.meanMaxNs = meanValue(maxValues);
    summary.stddevThroughput = stddevValue(throughputValues);

    for (const BenchmarkStats &stats : runs)
    {
        printBenchmarkReport(stats);
    }

    printBenchmarkSuiteReport(summary);

    return summary;
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
    std::cout << "Stage 5 Microbenchmarks\n";
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
        stats.p999Ns = percentileValue(latencies, 99.9);
        stats.p9999Ns = percentileValue(latencies, 99.99);
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

static int parseRepeats(int argc, char **argv)
{
    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg == "--repeats" && i + 1 < argc)
        {
            return std::max(1, std::stoi(argv[i + 1]));
        }
    }

    return 5;
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
    const int repeats = parseRepeats(argc, argv);
    const int64_t spreadThreshold = 5;

    std::cout << "Benchmark config: ticks=" << tickCount
              << ", threshold=" << spreadThreshold
              << ", repeats=" << repeats << '\n';

    const BenchmarkSuiteSummary suite = runBenchmarkSuite(tickCount, spreadThreshold, repeats);
    (void)suite;
    runMicrobenchmarks();

    return 0;
}