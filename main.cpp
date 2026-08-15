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