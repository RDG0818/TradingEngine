#pragma once
#include <atomic>
#include <cmath>
#include <random>
#include "order.h"

class LatentPrice {
public:
    explicit LatentPrice(Price seed_price, double sigma = 0.0003)
        : sigma_(sigma)
        , log_price_(std::log(static_cast<double>(seed_price) / 10000.0))
        , rng_(std::random_device{}())
        , dist_(0.0, 1.0)
    {
        price_.store(seed_price, std::memory_order_release);
    }

    void tick() {
        double z = dist_(rng_);
        log_price_ += sigma_ * z;
        Price new_price = static_cast<Price>(std::exp(log_price_) * 10000.0);
        if (new_price == 0) new_price = 1;
        price_.store(new_price, std::memory_order_release);
    }

    Price get() const {
        return price_.load(std::memory_order_acquire);
    }

    void set_sigma(double sigma) { sigma_ = sigma; }
    double sigma() const { return sigma_; }

    void reinit(Price seed_price, double sigma) {
        sigma_ = sigma;
        log_price_ = std::log(static_cast<double>(seed_price) / 10000.0);
        rng_ = std::mt19937{std::random_device{}()};
        price_.store(seed_price, std::memory_order_release);
    }

private:
    std::atomic<Price> price_{};
    double log_price_;
    double sigma_;
    std::mt19937 rng_;
    std::normal_distribution<double> dist_;
};
