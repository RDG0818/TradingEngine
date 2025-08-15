#pragma once
#include <vector>
#include <iostream>
#include <map>
#include <queue>
#include <utility>
#include <stack>
#include <string>
#include <algorithm>
#include <cmath>
using str = std::string;


enum class Side { BUY, SELL };


struct MarketData {
    int price;
    int quantity;

    MarketData(int p, int q) :
    price(p), quantity(q) {};
};


inline bool isValidPrice(const str& price) {
    if (price.find('-') != std::string::npos) return false;
    auto dot = price.find('.');
    if (dot == std::string::npos) return false; 
    return (price.size() - dot - 1) == 2; 
}

inline unsigned int convertPriceToInt(const std::string& priceStr) {
    str tempStr = priceStr;
    tempStr.erase(std::remove(tempStr.begin(), tempStr.end(), '.'), tempStr.end());
    unsigned int priceInt = std::stoul(tempStr);
    return priceInt;
}


struct Order {
    unsigned int orderID;
    Side side;
    str price;
    unsigned int intPrice;
    unsigned int quantity;
    unsigned int timestamp;
    unsigned int traderID;

    Order(int o, Side s, str p, int q, int ti, int tr) : 
    orderID(o), side(s), price(p), quantity(q), timestamp(ti), traderID(tr) {        
        if (quantity == 0) {
            throw std::invalid_argument("Quantity must be positive.");
        }
        if (!isValidPrice(price)) {
            throw std::invalid_argument("Price must be have at most 2 decimal places and be positive. (Ex. 10.00, 123.45)");
        }
        intPrice = convertPriceToInt(price);
    };
};


class OrderBook {
    private:
        std::map<int, std::deque<int>> bids;
        std::map<int, int> bid_quantities;
        std::map<int, std::deque<int>> asks;
        std::map<int, int> ask_quantities;
        std::unordered_map<int, Order> allOrders;
    
    friend class OrderBookTest;
    
    public:
    
    void addOrder(Order order);
    void cancelOrder(Order order);
    void modifyOrder(Order old_order, Order new_order);
    MarketData getBid();
    MarketData getAsk();
};