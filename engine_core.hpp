#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <iostream>
#include <map>
#include <thread>

#include "./data.cpp"

class MarketDataFeed
{
private:
    int64_t price = 10000;

public:
    using TickHandler = void (*)(void *context, const Tick &tick);

    static int TICK_RATE;

    Tick makeTick(uint64_t ts)
    {
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

inline int MarketDataFeed::TICK_RATE = 10;

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
