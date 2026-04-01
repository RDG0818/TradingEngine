// include/traders/noise_trader.h
#pragma once
#include "trader.h"
#include <random>
#include <limits>

class NoiseTrader : public Trader {
public:
    NoiseTrader(TraderId id, std::string name, uint64_t balance,
                double lambda = 0.5);

    void tick(Price last_price, SubmitFn submit, CancelFn cancel) override;
    void on_fill(const Fill& fill) override;

    void   set_lambda(double l)      { lambda_ = l; arrival_ = std::poisson_distribution<int>(l); }
    double lambda() const            { return lambda_; }
    void   set_price_noise(Price n)  { price_noise_ = n; }
    Price  price_noise() const       { return price_noise_; }

private:
    static constexpr Price PRICE_MAX = std::numeric_limits<Price>::max() / 2;
    static constexpr double LIMIT_PROB    = 0.60;
    static constexpr double MEAN_QTY      = 3.0;
    static constexpr double QTY_SIGMA     = 0.5;
    static constexpr uint64_t MAX_QTY     = 20;

    double lambda_;
    Price  price_noise_;

    std::mt19937                         rng_;
    std::poisson_distribution<int>       arrival_;
    std::lognormal_distribution<double>  size_dist_;
    std::bernoulli_distribution          limit_dist_;   // true = limit order
    std::bernoulli_distribution          side_dist_;    // true = buy
    std::normal_distribution<double>     offset_dist_;
};
