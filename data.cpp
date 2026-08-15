#include <iostream>

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
