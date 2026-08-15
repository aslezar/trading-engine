#include <cstdint>
#include <iostream>

struct Tick {
    int64_t bid;
    int64_t ask;
    uint64_t ts;
};

struct Order {
    int id;
    int64_t price;
    int qty;
    bool isBuy;
};
