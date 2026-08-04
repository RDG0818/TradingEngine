#pragma once
#include <random>
#include "market/trader.h"
#include "market/latent_price.h"

// Simulates a trader with a noisy signal about true value. Submits an
// IOC limit order at the signal price whenever it diverges from the
// last trade price beyond a threshold — directional pressure that
// drives price discovery. Never rests an order.
class InformedTrader : public Trader {
public:
    InformedTrader(TraderId id, std::string name, uint64_t balance,
                   const LatentPrice& latent,
                   double signal_noise = 0.001,
                   double threshold    = 0.002);

    void tick(Price last_price, SubmitFn submit, CancelFn cancel) override;
    void on_fill(const Fill& fill) override;

    void   set_signal_noise(double n) { signal_noise_ = n; }
    double signal_noise()       const { return signal_noise_; }

    void   set_threshold(double t) { threshold_ = t; }
    double threshold()       const { return threshold_; }

private:
    const LatentPrice& latent_;
    double signal_noise_;
    double threshold_;

    static constexpr uint64_t mean_qty_ = 5;
    static constexpr uint64_t max_qty_  = 50;

    std::mt19937 rng_;
    std::normal_distribution<double> noise_dist_;
    std::lognormal_distribution<double> qty_dist_;
};
