#include <iostream>
#include "exchange.h"
#include "tui/tui.h"

static constexpr Price DEFAULT_SEED_PRICE = 640000000;  // $64,000.00

static Price parse_seed_price(int argc, char** argv) {
    for (int i = 1; i < argc - 1; i++) {
        if (std::string(argv[i]) == "--seed") {
            try {
                double dollars = std::stod(argv[i + 1]);
                return static_cast<Price>(dollars * 10000.0);
            } catch (...) {}
        }
    }
    return DEFAULT_SEED_PRICE;
}

int main(int argc, char** argv) {
    Price seed_price = parse_seed_price(argc, argv);

    Exchange exchange;

    // Add traders before starting so the registry tick loop has them immediately
    exchange.add_market_maker("mm1",     1'000'000'000);
    exchange.add_informed_trader("inf1", 1'000'000'000);
    exchange.add_informed_trader("inf2", 1'000'000'000);
    exchange.add_noise_trader("noise1",  1'000'000'000, 0.7);
    exchange.add_noise_trader("noise2",  1'000'000'000, 0.7);
    exchange.add_noise_trader("noise3",  1'000'000'000, 0.7);

    exchange.start(seed_price);

    TUI tui(exchange, exchange.registry());
    tui.run();  // blocks until user quits (q or Ctrl-C)

    exchange.stop();
    return 0;
}
