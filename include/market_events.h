// include/market_events.h
#pragma once
#include <string>
#include <vector>

enum class MarketEventType {
    FlashCrash,
    BullRun,
    LiquiditySqueeze,
    MeanReversionTrap,
};

struct MarketEventConfig {
    MarketEventType type;
    int             duration_ticks{30};  // how long the event lasts
};

// Human-readable metadata for each event (used by the API).
struct MarketEventInfo {
    std::string id;
    std::string name;
    std::string description;
    int         default_duration_s;
};

inline std::vector<MarketEventInfo> all_market_events() {
    return {
        {"flash_crash",         "Flash Crash",         "Spawns panic sellers, suspends market makers.", 30},
        {"bull_run",            "Bull Run",            "Amplifies momentum trader aggression.",         30},
        {"liquidity_squeeze",   "Liquidity Squeeze",   "Removes limit order providers from the book.",  20},
        {"mean_reversion_trap", "Mean Reversion Trap", "Momentum spike into oversold conditions.",      15},
    };
}
