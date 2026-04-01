#pragma once
#include <vector>
#include "trader.h"
#include "latent_price.h"

class MarketMaker : public Trader {
public:
    MarketMaker(TraderId id, std::string name, uint64_t balance,
                const LatentPrice& latent,
                Price half_spread = 20000);

    void tick(Price last_price, SubmitFn submit, CancelFn cancel) override;
    void on_fill(const Fill& fill) override;

    void set_half_spread(Price spread) { half_spread_ = spread; }
    Price half_spread() const { return half_spread_; }

private:
    const LatentPrice& latent_;
    Price half_spread_;
    std::vector<OrderId> resting_bids_;
    std::vector<OrderId> resting_asks_;

    uint64_t fills_in_window_{0};
    uint64_t ticks_in_window_{0};
    Price    effective_spread_{0};  // persists across window boundary
    static constexpr uint64_t WINDOW_TICKS = 20;
    static constexpr double MAX_SPREAD_MULTIPLIER = 3.0;
};
