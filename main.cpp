#include <iostream>
#include <string>

#include "./engine_core.hpp"

enum class RunMode
{
    Live
};

static RunMode parseMode(int argc, char **argv)
{
    if (argc > 1)
    {
        const std::string arg = argv[1];
        if (arg == "--live")
        {
            return RunMode::Live;
        }
    }

    return RunMode::Live;
}

static void runLiveMode()
{
    MarketDataFeed feed;
    ExecutionSimulator execution;
    MarketDataFeed::TICK_RATE = 10;

    TradingEngine engine(feed, execution, 5);
    std::cout << "Live mode: streaming tick data\n";
    engine.start();
}

int main(int argc, char **argv)
{
    const RunMode mode = parseMode(argc, argv);

    if (mode == RunMode::Live)
    {
        runLiveMode();
    }

    return 0;
}
