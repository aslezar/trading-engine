#include <bits/stdc++.h>

using namespace std;

struct Tick
{
    double bid;
    double ask;
    uint64_t ts;
};

struct Order
{
    int id;
    double price;
    int qty;
    bool isBuy;
};

// ---------- Market Data ----------
class MarketDataHandler
{
private:
    vector<Tick> data;
    size_t idx = 0;

public:
    MarketDataHandler()
    {
        // mock ticks
        data = {
            {100.0, 100.2, 1},
            {100.1, 100.4, 2},
            {100.2, 100.6, 3},
            {100.3, 100.35, 4},
        };
    }

    bool getNextTick(Tick &tick)
    {
        if (idx >= data.size())
            return false;
        tick = data[idx++];
        return true;
    }
};

class OrderBook
{
private:
    double bestBid = 0.0;
    double bestAsk = 0.0;

public:
    inline void update(const Tick &tick)
    {
        bestBid = tick.bid;
        bestAsk = tick.ask;
    }

    inline double getBid() const { return bestBid; }
    inline double getAsk() const { return bestAsk; }
};

class Strategy
{
private:
    double spreadThreshold;

public:
    Strategy(double threshold) : spreadThreshold(threshold) {}

    inline bool generateSignal(const OrderBook &book, Order &order)
    {
        double bid = book.getBid();
        double ask = book.getAsk();

        if (ask - bid > spreadThreshold)
        {
            order = {0, bid, 1, true}; // buy example
            return true;
        }
        return false;
    }
};

class OrderManager
{
private:
    int nextOrderId = 1;

public:
    inline void prepare(Order &order)
    {
        order.id = nextOrderId++;
    }
};

class Exchange
{
public:
    inline void send(const Order &o)
    {
        cout << (o.isBuy ? "BUY " : "SELL ")
             << "id=" << o.id
             << " price=" << o.price
             << " qty=" << o.qty << "\n";
    }
};

class TradingEngine
{
private:
    MarketDataHandler md;
    OrderBook book;
    Strategy strategy;
    OrderManager om;
    Exchange ex;

public:
    TradingEngine(double threshold)
        : strategy(threshold) {}

    void run()
    {
        Tick tick;
        Order order;

        while (md.getNextTick(tick))
        {

            book.update(tick);

            if (strategy.generateSignal(book, order))
            {
                om.prepare(order);
                ex.send(order);
            }
        }
    }
};

int main(int argc, char const *argv[])
{

    TradingEngine engine(0.2); // spread threshold
    engine.run();

    return 0;
}
