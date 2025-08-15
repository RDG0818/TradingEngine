#pragma once
#include <vector>
#include <iostream>
#include <map>
#include <queue>
#include <utility>
#include <stack>
#include <string>
#include <algorithm>
using str = std::string;


enum class Side { BUY, SELL };


struct MarketData {
    int price;
    int quantity;

    MarketData(int p, int q) :
    price(p), quantity(q) {};
};


struct Order {
    int orderID;
    Side side;
    int price;
    int quantity;
    int timestamp;
    int traderID;

    Order(int o, Side s, int p, int q, int ti, int tr) : 
    orderID(o), side(s), price(p), quantity(q), timestamp(ti), traderID(tr) {};

};


class OrderBook {
    private:
        std::map<int, std::deque<int>> bids;
        std::map<int, std::deque<int>> asks;

    public:
    
    void addOrder(Order order);
    void cancelOrder(Order order);
    void modifyOrder(Order old_order, Order new_order);
    MarketData getBid();
    MarketData getAsk();
};